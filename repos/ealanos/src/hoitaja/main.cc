/*
 * \brief  Init component
 * \author Norman Feske
 * \date   2010-04-27
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include "base/capability.h"
#include "base/ipc.h"
#include "base/ram_allocator.h"
#include "region_map/client.h"
#include "region_map/region_map.h"
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/attached_ram_dataspace.h>
#include <sandbox/sandbox.h>
#include <os/reporter.h>
#include <base/log.h>
#include <tukija/syscall-generic.h>
#include <child.h>

#include <ealanos/laucher/component.h>

#include <ealanos/shell/component.h>

#include <base/mutex.h>
namespace Ealan {

	using namespace Genode;

	struct Hoitaja;
}


class Ealan::Hoitaja : Genode::Sandbox::State_handler, Genode::Sandbox::Local_service_base::Wakeup
{
	private:

		friend class Ealan::Shell::Session_component;

		Env &_env;

		Genode::Sandbox _sandbox { _env, *this };

		Attached_rom_dataspace _config { _env, "config" };

		Genode::Xml_node *_habitat_config{nullptr};

		Genode::Ram_dataspace_capability _config_ds{};
		char *_config_dataspace{nullptr};

		void _handle_resource_avail() { }

		Signal_handler<Hoitaja> _resource_avail_handler {
			_env.ep(), *this, &Hoitaja::_handle_resource_avail };

		Constructible<Reporter> _reporter { };

		Genode::Sliced_heap _md_alloc{_env.ram(), _env.rm()};

		Genode::Heap _heap{_env.ram(), _env.rm()};

		Genode::Mutex _config_lock{};

		using Launcher_service = Genode::Sandbox::Local_service<Ealan::Launcher_session_component>;
		Launcher_service _launcher_service{_sandbox, *this};

		using Shell_service = Genode::Sandbox::Local_service<Ealan::Shell::Session_component>;
		Shell_service _shell_service{_sandbox, *this};
		
		size_t _report_buffer_size = 0;

		void _handle_config()
		{
			_config_lock.acquire();
			_config.update();

			if (_habitat_config) {
				delete _habitat_config;
				delete _config_dataspace;
			}

			_config_ds = _env.ram().alloc(32 * _config.size());

			Genode::Region_map::Attr attr{};
			attr.writeable = true;

			_config_dataspace = _env.rm()
			                        .attach(_config_ds, attr)
			                        .convert<char *>(
										[&](Genode::Region_map::Range r) {
											return reinterpret_cast<char*>(r.start);
										},
										[&](Genode::Region_map::Attach_error) { return nullptr; });
			//_config_dataspace = static_cast<char*>(_heap.alloc(_config.size()*32));
			Genode::memcpy(_config_dataspace, _config.local_addr<char*>(), _config.size());
			_habitat_config = new (_sandbox._heap) Genode::Xml_node(_config_dataspace);

			Xml_node const config = *_habitat_config;

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
			_config_lock.release();
		}

		Signal_handler<Hoitaja> _config_handler {
		_env.ep(), *this, &Hoitaja::_handle_config };

		Hoitaja(const Hoitaja &);

		Hoitaja &operator=(const Hoitaja &);

	public:

		/**
		 * Sandbox::State_handler interface
		 */
		void handle_sandbox_state() override
		{
			Genode::log("Sandbox state changed");
			try
			{
				Reporter::Xml_generator xml(*_reporter, [&] () {
					_sandbox.generate_state_report(xml); });
			}
			catch (Xml_generator::Buffer_exceeded) {

				error("state report exceeds maximum size");

				/* try to reflect the error condition as state report */
				try {
					Reporter::Xml_generator xml(*_reporter, [&] () {
						xml.attribute("error", "report buffer exceeded"); });
				}
				catch (...) { }
			}
		}

		void handle_child_state(::Sandbox::Child &child) override {
			bool repeat = false;
			do
			{
				try {
					_config_lock.acquire();
					Genode::log("Updating state of child ", child.name());
					_habitat_config = _sandbox.update(child, _habitat_config);
					Genode::log("Updated config length:", _habitat_config->content_size());
					_config_lock.release();
				}
				catch (Genode::Quota_guard<Genode::Cap_quota>::Limit_exceeded)
				{
					Genode::log("Caps exceeded while handling child state");
					_config_lock.release();
					_env.parent().exit(1);
				}
				catch (Genode::Ipc_error)
				{
					Genode::error("Failed to update child state for <", child.name(), ">");
					_config_lock.release();
					repeat = true;
				}
			} while (repeat);
		}

		void wakeup_local_service() override {
			_launcher_service.for_each_requested_session([&](Launcher_service::Request &req) {
				req.deliver_session(*new (_md_alloc) Ealan::Launcher_session_component(
					*this, _env.ep(), req.resources, "", req.diag));
			});
			_shell_service.for_each_requested_session([&](Shell_service::Request &req) {
				req.deliver_session(*new (_md_alloc) Ealan::Shell::Session_component(*this, _env.ep(), req.resources, "", req.diag));
			});
		}

		void add_cell_from_xml(const char *start_node){
			char *dest = nullptr;

			_config_lock.acquire();
			_habitat_config->with_raw_content([&](char const *content, Genode::size_t)
											  { dest = const_cast<char*>(content); });
			dest += _habitat_config->content_size();


			Genode::memcpy(dest, start_node, Genode::strlen(start_node));
			dest += Genode::strlen(start_node);
			Genode::memcpy(dest, "</config>", sizeof("</config>"));

			_sandbox._heap.free(_habitat_config, sizeof(Xml_node));

			_habitat_config = new (_sandbox._heap) Xml_node(_config_dataspace);

			bool repeat = false;
			do {
				try {
					_sandbox.apply_config(*_habitat_config);
				} catch (Genode::Ipc_error) {
					Genode::error("IPC error while creating cell. Retrying.");
					repeat = true;
				}
			} while (repeat);
			_config_lock.release();
		}

		void update_config()
		{
			_config_lock.acquire();
			_sandbox._heap.free(_habitat_config, sizeof(Xml_node));
			_habitat_config = new (_sandbox._heap) Xml_node(_config_dataspace);

			bool repeat = false;
			do {
				try {
					_sandbox.apply_config(*_habitat_config);
				} catch (Genode::Ipc_error) {
					Genode::warning("IPC error while applying new configuration.");
					repeat = true;
				}
			} while (repeat);
			_config_lock.release();
		}
		
		Hoitaja(Env &env) : _env(env)
		{
			_config.sigh(_config_handler);

			Genode::log("Hoitaja starting ...");

			/* prevent init to block for resource upgrades (never satisfied by core) */
			_env.parent().resource_avail_sigh(_resource_avail_handler);

			Tukija::Tip const *tip = Tukija::Tip::tip();

			Genode::log("Found topology model of size ", tip->length, " at ", static_cast<const void *>(tip));

			_handle_config();
		}
};

void Ealan::Launcher_session_component::launch(Genode::String<640> start_node)
{
	_hoitaja.add_cell_from_xml(start_node.string());
}

/*******************************
 * Shell server implementation *
 *******************************/

Genode::Ram_dataspace_capability
	Ealan::Shell::Session_component::connect()
{
	return _hoitaja._config_ds;
}

void Ealan::Shell::Session_component::disconnect()
{
}

void Ealan::Shell::Session_component::commit()
{
	_hoitaja.update_config();
}



void Component::construct(Genode::Env &env) { static Ealan::Hoitaja main(env); }

