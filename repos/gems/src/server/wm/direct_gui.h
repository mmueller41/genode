/*
 * \brief  Pass-through GUI service announced to the outside world
 * \author Norman Feske
 * \date   2015-09-29
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DIRECT_GUI_H_
#define _DIRECT_GUI_H_

/* Genode includes */
#include <os/session_policy.h>
#include <gui_session/connection.h>

namespace Wm { class Direct_gui_session; }


class Wm::Direct_gui_session : public Session_object<Gui::Session>
{
	private:

		Env &_env;

		Connection<Gui::Session> _connection {
			_env, _label, Ram_quota { 36*1024 }, /* Args */ { } };

		Gui::Session_client _session { _connection.cap() };

		using View_capability = Gui::View_capability;
		using View_id         = Gui::View_id;

	public:

		Direct_gui_session(Env &env, auto &&... args)
		:
			Session_object<Gui::Session>(env.ep(), args...),
			_env(env)
		{ }

		void upgrade(char const *args)
		{
			size_t const ram_quota = Arg_string::find_arg(args, "ram_quota").ulong_value(0);
			_connection.upgrade_ram(ram_quota);
		}


		/***************************
		 ** GUI session interface **
		 ***************************/
		
		Framebuffer::Session_capability framebuffer() override
		{
			return _session.framebuffer();
		}

		Input::Session_capability input() override
		{
			return _session.input();
		}

		View_result view(View_id id, View_attr const &attr) override
		{
			return _session.view(id, attr);
		}

		Child_view_result child_view(View_id id, View_id parent, View_attr const &attr) override
		{
			return _session.child_view(id, parent, attr);
		}

		void destroy_view(View_id view) override
		{
			_session.destroy_view(view);
		}

		Associate_result associate(View_id id, View_capability view_cap) override
		{
			return _session.associate(id, view_cap);
		}

		View_capability_result view_capability(View_id view) override
		{
			return _session.view_capability(view);
		}

		void release_view_id(View_id view) override
		{
			_session.release_view_id(view);
		}

		Dataspace_capability command_dataspace() override
		{
			return _session.command_dataspace();
		}

		void execute() override
		{
			_session.execute();
		}

		Info_result info() override
		{
			return _session.info();
		}

		Buffer_result buffer(Framebuffer::Mode mode) override
		{
			return _session.buffer(mode);
		}

		void focus(Capability<Gui::Session> session) override
		{
			_session.focus(session);
		}
};

#endif /* _DIRECT_GUI_H_ */
