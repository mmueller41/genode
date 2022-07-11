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

struct HelloClient {
	Genode::Env &_env;
	Hello::Connection &_hello;

	int _a;
	int _b;

	Genode::Attached_rom_dataspace _config{_env, "config"};


	void _handle_config()
	{
		Genode::log("Reading config");
		_config.update();

		if (!_config.valid()) {
			Genode::log("Config is invalid.");
			return;
		}

		_a = _config.xml().attribute_value("a", (int)2);
		_b = _config.xml().attribute_value("b", (int)5);	
	}

	Genode::Signal_handler<HelloClient> _config_handler {
		_env.ep(), *this, &HelloClient::_handle_config
	};

	void run()
	{
		_hello.say_hello();

		int const sum = _hello.add(_a, _b);
		Genode::log("added ", _a, " + ", _b, " = ", sum);

		Genode::log("hello test completed.");
	}

	HelloClient(Genode::Env &env, Hello::Connection &conn) : _env(env), _hello(conn), _a(5), _b(2)
	{}
};

void
Component::construct(Genode::Env &env)
{
	Hello::Connection hello(env);

	HelloClient client(env, hello);
	client.run();

}
