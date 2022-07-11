/*
 * \brief  Test client for the Hello RPC interface
 * \author Björn Döbel
 * \author Norman Feske
 * \date   2008-03-20
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/log.h>
#include <base/attached_rom_dataspace.h>
#include <hello_session/connection.h>
#include <timer_session/connection.h>

struct HelloClient {
	Genode::Env &_env;
	Hello::Connection &_hello;
	Timer::Connection _timer{_env};

	unsigned short _a;
	unsigned short _b;

	Genode::Attached_rom_dataspace _config{_env, "config"};


	void _handle_config()
	{
		Genode::log("Reading config");
		_config.update();

		if (!_config.valid()) {
			Genode::log("Config is invalid.");
			return;
		}

		_a = _config.xml().attribute_value("a", (unsigned short)2);
		_b = _config.xml().attribute_value("b", (unsigned short)5);	
	}

	Genode::Signal_handler<HelloClient> _config_handler {
		_env.ep(), *this, &HelloClient::_handle_config
	};

	void run()
	{
		_hello.say_hello();

		while (true) {
			int const sum = _hello.add(_a, _b);
			unsigned short id = _hello.id();

			Genode::log(id, ": added ", _a, " + ", _b, " = ", sum);
			_timer.msleep(_a*1000);
			_b += 2;
		}
		Genode::log("hello test completed.");
	}

	HelloClient(Genode::Env &env, Hello::Connection &conn) : _env(env), _hello(conn), _a(5), _b(2)
	{
		_config.sigh(_config_handler);
		_handle_config();
	}
};

void
Component::construct(Genode::Env &env)
{
	Timer::Connection timer(env);
	Hello::Connection hello(env);

	HelloClient client(env, hello);
	client.run();

}
