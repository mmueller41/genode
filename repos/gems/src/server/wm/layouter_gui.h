/*
 * \brief  GUI service provided to layouter
 * \author Norman Feske
 * \date   2015-06-06
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LAYOUTER_GUI_H_
#define _LAYOUTER_GUI_H_

/* Genode includes */
#include <input/component.h>
#include <gui_session/connection.h>

namespace Wm {
	struct Layouter_gui_session;
	struct Layouter_gui_service;
}


struct Wm::Layouter_gui_session : Session_object<Gui::Session>
{
	Input::Session_capability _input_session_cap;

	/*
	 * GUI session solely used to supply the GUI mode to the layouter
	 */
	Gui::Connection _mode_sigh_gui;

	Attached_ram_dataspace _command_ds;

	Layouter_gui_session(Env                      &env,
	                     Resources          const &resources,
	                     Label              const &label,
	                     Diag               const &diag,
	                     Input::Session_capability input_session_cap)
	:
		Session_object<Gui::Session>(env.ep(), resources, label, diag),
		_input_session_cap(input_session_cap),
		_mode_sigh_gui(env), _command_ds(env.ram(), env.rm(), 4096)
	{ }


	/***************************
	 ** GUI session interface **
	 ***************************/
	
	Framebuffer::Session_capability framebuffer() override
	{
		return Framebuffer::Session_capability();
	}

	Input::Session_capability input() override
	{
		return _input_session_cap;
	}

	Info_result info() override
	{
		return _mode_sigh_gui.info_rom_cap();
	}

	View_result view(Gui::View_id, View_attr const &) override
	{
		return View_result::OK;
	}

	Child_view_result child_view(Gui::View_id, Gui::View_id, View_attr const &) override
	{
		return Child_view_result::OK;
	}

	void destroy_view(Gui::View_id) override { }

	Associate_result associate(Gui::View_id, Gui::View_capability) override
	{
		return Associate_result::OK;
	}

	View_capability_result view_capability(Gui::View_id) override
	{
		return Gui::View_capability();
	}

	void release_view_id(Gui::View_id) override { }

	Dataspace_capability command_dataspace() override
	{
		return _command_ds.cap();
	}

	void execute() override { }

	Buffer_result buffer(Framebuffer::Mode) override { return Buffer_result::OK; }

	void focus(Capability<Gui::Session>) override { }
};

#endif /* _LAYOUTER_GUI_H_ */
