/*
 * \brief  Hoitaja — Cell Management Component based on Init
 * \author Michael Müller, Norman Feske (Init)
 * \date   2023-04-20
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 * Copyright (C) 2023 Michael Müller, Osnabrück University
 *
 * This file is part of EalánOS, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <habitat.h>
#include <os/reporter.h>
#include <timer_session/connection.h>

/** Hoitaja components **/
/* Filtering components */
#include "load_controller.h"
#include "cell_controller.h"
#include "hyperthread_controller.h"
#include "memory_controller.h"
#include "numa_controller.h"
/* Core Allocator */
#include "core_allocator.h"
/* State Handler */
#include "state_handler.h"

namespace Hoitaja {

	using namespace Genode;

	struct Main;
}


struct Hoitaja::Main : Genode::Sandbox::State_handler, Hoitaja::State_handler
{
	Env &_env;

	Habitat _sandbox { _env, *this, *this };

	Timer::Connection _timer{_env};

	Attached_rom_dataspace _config { _env, "config" };

	void _handle_resource_avail() { }

	Signal_handler<Main> _resource_avail_handler {
		_env.ep(), *this, &Main::_handle_resource_avail };

	Constructible<Reporter> _reporter { };

	size_t _report_buffer_size = 0;

	void _handle_config()
	{
		_config.update();

		Xml_node const config = _config.xml();

		bool reporter_enabled = false;
		config.with_optional_sub_node("report", [&] (Xml_node report) {

			reporter_enabled = true;

			/* (re-)construct reporter whenever the buffer size is changed */
			Number_of_bytes const buffer_size =
				report.attribute_value("buffer", Number_of_bytes(4096));

			if (buffer_size != _report_buffer_size || !_reporter.constructed()) {
				_report_buffer_size = buffer_size;
				_reporter.construct(_env, "state", "state", _report_buffer_size);
			}
		});

		if (_reporter.constructed())
			_reporter->enabled(reporter_enabled);

		_sandbox.apply_config(config);
	}

	Signal_handler<Main> _config_handler {
		_env.ep(), *this, &Main::_handle_config };

	void _handle_timeout()
	{
		//log("Hoitaja's entering its maintance cycle");
		// For now just print all cells created by Hoitaja
		//_handle_config();
		//_timer.trigger_once(1000 * 1000);
	}

	Signal_handler<Main> _timeout_handler{
		_env.ep(), *this, &Main::_handle_timeout};

	/**
	 * Sandbox::State_handler interface
	 */
	void handle_sandbox_state() override
	{
		Genode::log("Habitat state changed");
		/*
		try {
			Reporter::Xml_generator xml(*_reporter, [&] () {
				_sandbox.generate_state_report(xml); });
		}
		catch (Xml_generator::Buffer_exceeded) {

			error("state report exceeds maximum size");

			 try to reflect the error condition as state report
			try {
				Reporter::Xml_generator xml(*_reporter, [&] () {
					xml.attribute("error", "report buffer exceeded"); });
			}
			catch (...) { }
		}*/
	}

	void handle_habitat_state(Cell &cell) override
	{
		Genode::log("Habitat changed");
		_sandbox.update(cell);
	}

	Main(Env &env) : _env(env)
	{
		_config.sigh(_config_handler);
		_timer.sigh(_timeout_handler);

		/* prevent init to block for resource upgrades (never satisfied by core) */
		_env.parent().resource_avail_sigh(_resource_avail_handler);

		_timer.trigger_once(1000 * 1000);
		_handle_config();
	}
};

void Component::construct(Genode::Env &env) { static Hoitaja::Main main(env); }

