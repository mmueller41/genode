/*
 * \brief  Test for yielding resources
 * \author Norman Feske
 * \date   2013-10-05
 *
 * This test exercises the protocol between a parent and child, which is used
 * by the parent to regain resources from a child subsystem.
 *
 * The program acts in either one of two roles, the parent or the child. The
 * role is determined by reading a config argument.
 *
 * The child periodically allocates chunks of RAM until its RAM quota is
 * depleted. Once it observes a yield request from the parent, however, it
 * cooperatively releases as much resources as requested by the parent.
 *
 * The parent wait a while to give the child the chance to allocate RAM. It
 * then sends a yield request and waits for a response. When getting the
 * response, it validates whether the child complied to the request or not.
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/arg_string.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/log.h>
#include <timer_session/connection.h>
#include <os/static_parent_services.h>
#include <os/dynamic_rom_session.h>
#include <base/child.h>
#include <trace/timestamp.h>

namespace Test {
	class Child;
	class Parent;
	using namespace Genode;
}


/****************
 ** Child role **
 ****************/

/**
 * The child eats more and more RAM. However, when receiving a yield request,
 * it releases the requested amount of resources.
 */
class Test::Child
{
	private:

		struct Ram_chunk : List<Ram_chunk>::Element
		{
			Env &env;

			size_t const size;

			Ram_dataspace_capability ds_cap;

			Ram_chunk(Env &env, size_t size)
			:
				env(env),size(size), ds_cap(env.ram().alloc(size))
			{ }

			~Ram_chunk() { env.ram().free(ds_cap); }
		};

		Env                  &_env;
		Heap                  _heap { _env.ram(), _env.rm() };
		bool            const _expand;
		List<Ram_chunk>       _ram_chunks { };
		Timer::Connection     _timer { _env };
		Signal_handler<Child> _grant_handler;
		Genode::uint64_t        const _period_ms;

		void _handle_grant();

		

	public:

		Child(Env &, Xml_node);
		void main();
};


void Test::Child::_handle_grant()
{
	/* request yield request arguments */
		unsigned long start = Genode::Trace::timestamp();
		[[maybe_unused]] Genode::Parent::Resource_args const args = _env.parent().gained_resources();
		unsigned long end = Genode::Trace::timestamp();
		// Genode::Parent::Resource_args const args = _env.parent().yield_request();

		_env.parent().yield_response();
		log("{\"grant-handle-et\": ", (end - start)/2000, "}");
		// size_t const gained_ram_quota = Arg_string::find_arg(args.string(), "ram_quota").ulong_value(0);

		// log("Gained RAM quota: ", gained_ram_quota);
		/*
			log("yield request: ", args.string());

			size_t const requested_ram_quota =
				Arg_string::find_arg(args.string(), "ram_quota").ulong_value(0);

			log("got request to free ", requested_ram_quota, " MB of RAM");

			size_t const requested_cpu_quota =
				Arg_string::find_arg(args.string(), "cpu_quota").ulong_value(0);

			log("released ", requested_cpu_quota, " portions of cpu_quota");

			size_t const requested_gpu_quota =
				Arg_string::find_arg(args.string(), "gpus").ulong_value(0);

			log("got request to release ", requested_gpu_quota, " gpus");*/
}


Test::Child::Child(Env &env, Xml_node config)
:
	_env(env),
	_expand(config.attribute_value("expand", false)),
	_grant_handler(_env.ep(), *this, &Child::_handle_grant),
	_period_ms(config.attribute_value("period_ms", (Genode::uint64_t)500))
{
	/* register yield signal handler */
	_env.parent().resource_avail_sigh(_grant_handler);
}


/*****************
 ** Parent role **
 *****************/

/**
 * The parent grants resource requests as long as it has free resources.
 * Once in a while, it politely requests the child to yield resources.
 */
class Test::Parent
{
	private:

		Env &_env;

		Timer::Connection _timer { _env };

		void _print_status()
		{
			log("quota: ", _child.pd().ram_quota().value / 1024, " KiB  "
			    "used: ",  _child.pd().used_ram().value  / 1024, " KiB");
		}

		size_t _used_ram_prior_yield = 0;

		/* perform the test three times */
		unsigned _cnt = 5000;

		unsigned long _start = 0;

		unsigned long _end = 0;
		unsigned long _sent = 0;

		enum State { WAIT, YIELD_REQUESTED, YIELD_GOT_RESPONSE };
		State _state = WAIT;

		void _schedule_one_second_timeout()
		{
			//log("wait ", _wait_cnt, "/", _wait_secs);
			_timer.trigger_once(10000);
		}

		void _init()
		{
			_state = WAIT;
			_schedule_one_second_timeout();
		}

		void _bestow_resources()
		{
			/* remember quantum of resources used by the child */
			//_used_ram_prior_yield = _child.pd().used_ram().value;

			//log("request yield (ram prior yield: ", _used_ram_prior_yield);

			/* issue yield request */
			Genode::Parent::Resource_args award("ram_quota=5M,cpu_quota=10,gpus=1");

			_start = Genode::Trace::timestamp();
			_child.accept(award);
			_sent = Genode::Trace::timestamp();

			_state = YIELD_REQUESTED;
		}

		void _handle_timeout()
		{
			//_print_status();
			_bestow_resources();
			_schedule_one_second_timeout();
		}

		void _yield_response()
		{
			_end = Genode::Trace::timestamp();
			log("{\"bestow-rtt\": ", (_end-_start)/2000, ", \"bestow-transmit\": ", (_sent-_start)/2000, ",\"bestow-acked\":", (_end-_sent)/2000,"}");

			_state = YIELD_GOT_RESPONSE;

			//_print_status();

			if (_cnt-- > 0) {
				_init();
			} else {
				log("--- test-resource_yield finished ---");
				_env.parent().exit(0);
			}
		}

		Signal_handler<Parent> _timeout_handler {
			_env.ep(), *this, &Parent::_handle_timeout };

		struct Policy : public Genode::Child_policy
		{
			Env &_env;

			Parent &_parent;

			Static_parent_services<Pd_session, Cpu_session, Rom_session,
			                       Log_session, Timer::Session, Topo_session>
				_parent_services { _env };

			Cap_quota   const _cap_quota { 50 };
			Ram_quota   const _ram_quota { 10*1024*1024 };
			Binary_name const _binary_name { "benchmark_resource_award" };

			/*
			 * Config ROM service
			 */

			struct Config_producer : Dynamic_rom_session::Content_producer
			{
				void produce_content(char *dst, Genode::size_t dst_len) override
				{
					Xml_generator xml(dst, dst_len, "config", [&] () {
						xml.attribute("child", "yes"); });
				}
			} _config_producer { };

			Dynamic_rom_session _config_session { _env.ep().rpc_ep(),
			                                      ref_pd(), _env.rm(),
			                                      _config_producer };

			typedef Genode::Local_service<Dynamic_rom_session> Config_service;

			Config_service::Single_session_factory _config_factory { _config_session };
			Config_service                         _config_service { _config_factory };

			void yield_response() override
			{
				_parent._yield_response();
			}

			Policy(Parent &parent, Env &env) : _env(env), _parent(parent) { }

			Name name() const override { return "child"; }

			Binary_name binary_name() const override { return _binary_name; }

			Pd_session           &ref_pd()           override { return _env.pd(); }
			Pd_session_capability ref_pd_cap() const override { return _env.pd_session_cap(); }

			void init(Pd_session &pd, Pd_session_capability pd_cap) override
			{
				pd.ref_account(ref_pd_cap());
				ref_pd().transfer_quota(pd_cap, _cap_quota);
				ref_pd().transfer_quota(pd_cap, _ram_quota);
			}

			Route resolve_session_request(Service::Name const &service_name,
			                              Session_label const &label,
			                              Session::Diag const  diag) override
			{
				auto route = [&] (Service &service) {
					return Route { .service = service,
					               .label   = label,
					               .diag    = diag }; };

				if (service_name == "ROM" && label == "child -> config")
					return route(_config_service);

				Service *service_ptr = nullptr;
				_parent_services.for_each([&] (Service &s) {
					if (!service_ptr && service_name == s.name())
						service_ptr = &s; });

				if (!service_ptr)
					throw Service_denied();

				return route(*service_ptr);
			}
		};

		Policy _policy { *this, _env };

		Genode::Child _child { _env.rm(), _env.ep().rpc_ep(), _policy };

	public:

		class Insufficient_yield { };

		/**
		 * Constructor
		 */
		Parent(Env &env) : _env(env)
		{
			_timer.sigh(_timeout_handler);
			_init();
		}
};


/***************
 ** Component **
 ***************/

void Component::construct(Genode::Env &env)
{
	using namespace Genode;

	/*
	 * Read value '<config child="" />' attribute to decide whether to perform
	 * the child or the parent role.
	 */
	static Attached_rom_dataspace config(env, "config");
	bool const is_child = config.xml().attribute_value("child", false);

	if (is_child) {
		log("--- test-resource_yield child role started ---");
		static Test::Child child(env, config.xml());
	} else {
		log("--- test-resource_yield parent role started ---");
		static Test::Parent parent(env);
	}
}
