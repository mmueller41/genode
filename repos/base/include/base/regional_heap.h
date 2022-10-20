/*
 * \brief  Heap partition
 * \author Norman Feske
 * \date   2006-05-15
 */

/*
 * Copyright (C) 2022 Michael Müller
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__REGIONAL_HEAP_H_
#define _INCLUDE__BASE__REGIONAL_HEAP_H_

#include <util/list.h>
#include <util/reconstructible.h>
#include <base/ram_allocator.h>
#include <region_map/region_map.h>
#include <base/allocator_avl.h>
#include <base/mutex.h>
#include <topo_session/node.h>

namespace Genode {

	class Regional_heap;
	class Sliced_regional_heap;
}


/**
 * Heap that uses dataspaces as backing store and has a physical memory range limited to a single NUMA region
 *
 * The heap class provides an allocator that uses a list of dataspaces of a RAM
 * allocator as backing store. One dataspace may be used for holding multiple
 * blocks.
 */
class Genode::Regional_heap : public Allocator
{
	private:

		class Dataspace : public List<Dataspace>::Element
		{
			private:

				/*
				 * Noncopyable
				 */
				Dataspace(Dataspace const &);
				Dataspace &operator = (Dataspace const &);

			public:

				Ram_dataspace_capability cap;
				void  *local_addr;
				size_t size;

				Dataspace(Ram_dataspace_capability c, void *local_addr, size_t size)
				: cap(c), local_addr(local_addr), size(size) { }
		};

		/*
		 * This structure exists only to make sure that the dataspaces are
		 * destroyed after the AVL allocator.
		 */
		class Dataspace_pool : public List<Dataspace>
		{
			private:

				/*
				 * Noncopyable
				 */
				Dataspace_pool(Dataspace_pool const &);
				Dataspace_pool &operator = (Dataspace_pool const &);

			public:

				Ram_allocator *ram_alloc; /* backing store */
				Region_map    *region_map;

				Dataspace_pool(Ram_allocator *ram, Region_map *rm)
				: ram_alloc(ram), region_map(rm) { }

				~Dataspace_pool();

				void remove_and_free(Dataspace &);

				void reassign_resources(Ram_allocator *ram, Region_map *rm) {
					ram_alloc = ram, region_map = rm; }
		};

		Mutex                  mutable _mutex { };
		Reconstructible<Allocator_avl> _alloc;        /* local allocator    */
		Dataspace_pool                 _ds_pool;      /* list of dataspaces */
		size_t                         _quota_limit { 0 };
		size_t                         _quota_used  { 0 };
		size_t                         _chunk_size  { 0 };
		Ram_allocator::Numa_id 		   _numa_id;      /* ID of NUMA region to serve allocations from */

		using Alloc_ds_result = Attempt<Dataspace *, Alloc_error>;

		/**
		 * Allocate a new dataspace of the specified size
		 *
		 * \param size                       number of bytes to allocate
		 * \param enforce_separate_metadata  if true, the new dataspace
		 *                                   will not contain any meta data
		 */
		Alloc_ds_result _allocate_dataspace(size_t size, bool enforce_separate_metadata);

		/**
		 * Try to allocate block at our local allocator
		 */
		Alloc_result _try_local_alloc(size_t size);

		/**
		 * Unsynchronized implementation of 'try_alloc'
		 */
		Alloc_result _unsynchronized_alloc(size_t size);

	public:

		enum { UNLIMITED = ~0 };

		Regional_heap(Ram_allocator *ram_allocator,
		     Region_map    *region_map,
			 Topology::Numa_region &region,
		     size_t         quota_limit = UNLIMITED,
		     void          *static_addr = 0,
		     size_t         static_size = 0);

		Regional_heap(Ram_allocator &ram, Region_map &rm, Topology::Numa_region &region) : Regional_heap(&ram, &rm, region) { }

		~Regional_heap();

		/**
		 * Reconfigure quota limit
		 *
		 * \return  negative error code if new quota limit is higher than
		 *          currently used quota.
		 */
		int quota_limit(size_t new_quota_limit);

		/**
		 * Re-assign RAM allocator and region map
		 */
		void reassign_resources(Ram_allocator *ram, Region_map *rm) {
			_ds_pool.reassign_resources(ram, rm); }

		/**
		 * Call 'fn' with the start and size of each backing-store region
		 */
		template <typename FN>
		void for_each_region(FN const &fn) const
		{
			Mutex::Guard guard(_mutex);
			for (Dataspace const *ds = _ds_pool.first(); ds; ds = ds->next())
				fn(ds->local_addr, ds->size);
		}


		/*************************
		 ** Allocator interface **
		 *************************/

		Alloc_result try_alloc(size_t)           override;
		void free(void *, size_t) override;
		size_t       consumed()            const override { return _quota_used; }
		size_t       overhead(size_t size) const override { return _alloc->overhead(size); }
		bool         need_size_for_free()  const override { return false; }
};


/**
 * Heap that allocates each block at a separate dataspace
 */
class Genode::Sliced_regional_heap : public Allocator
{
	private:

		/**
		 * Meta-data header placed in front of each allocated block
		 */
		struct Block : List<Block>::Element
		{
			Ram_dataspace_capability const ds;
			size_t                   const size;

			Block(Ram_dataspace_capability ds, size_t size) : ds(ds), size(size)
			{ }
		};

		Ram_allocator  &_ram_alloc;     /* RAM allocator for backing store */
		Region_map     &_region_map;    /* region map of the address space */
		size_t          _consumed = 0;  /* number of allocated bytes       */
		List<Block>     _blocks { };    /* list of allocated blocks        */
		Mutex           _mutex  { };    /* serialize allocations           */

	public:

		/**
		 * Return size of header prepended to each allocated block in bytes
		 */
		static constexpr size_t meta_data_size() { return sizeof(Block); }

		/**
		 * Constructor
		 */
		Sliced_regional_heap(Ram_allocator &ram_alloc, Region_map &region_map);

		/**
		 * Destructor
		 */
		~Sliced_regional_heap();


		/*************************
		 ** Allocator interface **
		 *************************/

		Alloc_result try_alloc(size_t)     override;
		void         free(void *, size_t)        override;
		size_t       consumed()            const override { return _consumed; }
		size_t       overhead(size_t size) const override;
		bool         need_size_for_free()  const override { return false; }
};

#endif /* _INCLUDE__BASE__HEAP_H_ */
