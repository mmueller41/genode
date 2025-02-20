/*
 * \brief  Mapping of Genode's capability names to capabilities selectors.
 * \author Alexander Boettcher
 * \date   2013-08-26
 *
 * This header is public to allow user-level VMMs to manually allocate windows
 * of consecutive selectors (for virtualization event portals) in the
 * component's capability space.
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NOVA__CAP_MAP_H_
#define _INCLUDE__NOVA__CAP_MAP_H_

/* Genode includes */
#include <base/stdint.h>

#include <base/mutex.h>

#include <base/log.h>

#include <util/avl_tree.h>
#include <util/noncopyable.h>

namespace Genode {

	class Cap_range : public Avl_node<Cap_range> {

		private:

			Mutex  _mutex { };
			addr_t _base = 0;
			addr_t _last = 0;

			enum
			{
				HEADER = sizeof(_base) + sizeof(_mutex) + sizeof(_last),
				CAP_RANGE_SIZE = 131072,
				WORDS = (CAP_RANGE_SIZE - HEADER - sizeof(Avl_node<Cap_range>)) / sizeof(addr_t),
			};

			uint16_t _cap_array[WORDS * sizeof(addr_t) / 2];

			bool _match(addr_t id) {
				return _base <= id && id < _base + elements(); };

		public:

			Cap_range(addr_t base) : _base(base)
			{
				//static_assert(sizeof(*this) == CAP_RANGE_SIZE,
				              //"Cap_range misconfigured");

				for (unsigned i = 0; i < elements(); i++)
					_cap_array[i] = 0;
			}

			addr_t   base()     const { return _base; }
			unsigned elements() const { return sizeof(_cap_array) / sizeof(_cap_array[0]); }

			Cap_range *find_by_id(addr_t);

			void inc(unsigned id);
			void dec(unsigned id, bool revoke = true, unsigned num_log2 = 0);

			addr_t alloc(size_t const num_log2);

			/************************
			 ** Avl node interface **
			 ************************/

			bool higher(Cap_range *n) { return n->_base > _base; }

	};


	class Cap_index
	{
		private:

			Cap_range * _range;
			addr_t      _local_name;

		public:

			Cap_index(Cap_range *range, addr_t local_name)
			: _range(range), _local_name(local_name) {}

			bool     valid() const   { return _range; }

			inline void inc()
			{
				if (_range)
					_range->inc((unsigned)(_local_name - _range->base()));
			}

			inline void dec()
			{
				if (_range)
					_range->dec((unsigned)(_local_name - _range->base()));
			}
	};


	class Capability_map : private Noncopyable
	{
		private:

			Avl_tree<Cap_range> _tree { };

		public:

			Cap_index find(addr_t local_sel);

			void insert(Cap_range &range) { _tree.insert(&range); }

			addr_t insert(size_t num_log_2 = 0, addr_t cap = ~0UL);

			void remove(addr_t sel, uint8_t num_log_2 = 0, bool revoke = true);
	};


	/**
	 * Get the global Capability_map of the process.
	 */
	Capability_map &cap_map();
}

#endif /* _INCLUDE__NOVA__CAP_MAP_H_ */
