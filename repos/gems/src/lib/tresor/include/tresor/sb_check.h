/*
 * \brief  Module for checking all hashes of a superblock and its hash trees
 * \author Martin Stein
 * \date   2023-05-03
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__SB_CHECK_H_
#define _TRESOR__SB_CHECK_H_

/* tresor includes */
#include <tresor/types.h>
#include <tresor/vbd_check.h>
#include <tresor/ft_check.h>
#include <tresor/block_io.h>
#include <tresor/trust_anchor.h>

namespace Tresor { class Sb_check; }

struct Tresor::Sb_check : Noncopyable
{
	class Check : Noncopyable
	{
		public:

			using Module = Sb_check;

		private:

			enum State {
				INIT, COMPLETE, READ_BLK, READ_BLK_SUCCEEDED, CHECK_VBD, CHECK_VBD_SUCCEEDED, CHECK_FT, CHECK_FT_SUCCEEDED,
				CHECK_MT, CHECK_MT_SUCCEEDED, READ_SB_HASH, READ_SB_HASH_SUCCEEDED };

			using Helper = Request_helper<Check, State>;

			Helper _helper;
			Hash _hash { };
			bool _scan_for_highest_gen_sb_done { false };
			Superblock_index _sb_idx { };
			Superblock _sb { };
			Snapshot_index _snap_idx { 0 };
			Constructible<Tree_root> _tree_root { };
			Block _blk { };
			Generatable_request<Helper, State, Trust_anchor::Read_hash> _read_sb_hash { };
			Generatable_request<Helper, State, Vbd_check::Check> _check_vbd { };
			Generatable_request<Helper, State, Ft_check::Check> _check_ft { };
			Generatable_request<Helper, State, Block_io::Read> _read_block { };

			void _check_snap(bool &);

		public:

			Check() : _helper(*this) { }

			~Check() { }

			void print(Output &out) const { Genode::print(out, "check"); }

			bool execute(Vbd_check &, Ft_check &, Block_io &, Trust_anchor &);

			bool complete() const { return _helper.complete(); }
			bool success() const { return _helper.success(); }
	};

	Sb_check() { }

	bool execute(Check &check, Vbd_check &vbd_check, Ft_check &ft_check, Block_io &block_io, Trust_anchor &trust_anchor) { return check.execute(vbd_check, ft_check, block_io, trust_anchor); };

	static constexpr char const *name() { return "sb_check"; }
};

#endif /* _TRESOR__SB_CHECK_H_ */
