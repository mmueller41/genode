
#include <sandbox/child.h>
#include <sandbox/alias.h>
#include <sandbox/server.h>
#include <sandbox/heartbeat.h>
#include <sandbox/config_model.h>
#include <sandbox/sandbox.h>

#include <base/env.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <util/xml_node.h>
#include <util/noncopyable.h>
#include <base/registry.h>
#include <base/service.h>

#pragma once
namespace Sandbox {
    class Library;
}

namespace Genode {
    class Sandbox;
}
struct Sandbox::Library : ::Sandbox::State_reporter::Producer,
								  ::Sandbox::Child::Default_route_accessor,
								  ::Sandbox::Child::Default_caps_accessor,
								  ::Sandbox::Child::Ram_limit_accessor,
								  ::Sandbox::Child::Cap_limit_accessor,
								  ::Sandbox::Child::Cpu_limit_accessor,
								  ::Sandbox::Child::Cpu_quota_transfer,
								  ::Sandbox::Start_model::Factory,
								  ::Sandbox::Parent_provides_model::Factory
{
	using Routed_service = ::Sandbox::Routed_service;
	using Parent_service = ::Sandbox::Parent_service;
	using Local_service = ::Genode::Sandbox::Local_service_base;
	using Report_detail = ::Sandbox::Report_detail;
	using Child_registry = ::Sandbox::Child_registry;
	using Verbose = ::Sandbox::Verbose;
	using State_reporter = ::Sandbox::State_reporter;
	using Heartbeat = ::Sandbox::Heartbeat;
	using Server = ::Sandbox::Server;
	using Alias = ::Sandbox::Alias;
	using Child = ::Sandbox::Child;
	using Prio_levels = ::Sandbox::Prio_levels;
	using Ram_info = ::Sandbox::Ram_info;
	using Cap_info = ::Sandbox::Cap_info;
	using Cpu_quota = ::Sandbox::Cpu_quota;
	using Config_model = ::Sandbox::Config_model;
	using Start_model = ::Sandbox::Start_model;
	using Preservation = ::Sandbox::Preservation;
	using Pd_intrinsics = Genode::Sandbox::Pd_intrinsics;

	using State_handler = Genode::Sandbox::State_handler;

	Env &_env;
	Heap &_heap;

	Pd_intrinsics &_pd_intrinsics;

	Registry<Parent_service> _parent_services{};
	Registry<Routed_service> _child_services{};
	Registry<Local_service> &_local_services;
	Child_registry _children{};

	/*
	 * Global parameters obtained from config
	 */
	Reconstructible<Verbose> _verbose{};
	Config_model::Version _version{};
	Constructible<Buffered_xml> _default_route{};
	Cap_quota _default_caps{0};
	Prio_levels _prio_levels{};
	Constructible<Affinity::Space> _affinity_space{};
	Preservation _preservation{};

	Affinity::Space _effective_affinity_space() const
	{
		return _affinity_space.constructed() ? *_affinity_space
											 : Affinity::Space{1, 1};
	}

	State_reporter _state_reporter;

	Heartbeat _heartbeat{_env, _children, _state_reporter};

	/*
	 * Internal representation of the XML configuration
	 */
	Config_model _config_model{};

	/*
	 * Variables for tracking the side effects of updating the config model
	 */
	bool _server_appeared_or_disappeared = false;
	bool _state_report_outdated = false;

	unsigned _child_cnt = 0;

	Cpu_quota _avail_cpu{.percent = 100};
	Cpu_quota _transferred_cpu{.percent = 0};

	Ram_quota _avail_ram() const
	{
		Ram_quota avail_ram = _env.pd().avail_ram();

		if (_preservation.ram.value > avail_ram.value)
		{
			error("RAM preservation exceeds available memory");
			return Ram_quota{0};
		}

		/* deduce preserved quota from available quota */
		return Ram_quota{avail_ram.value - _preservation.ram.value};
	}

	Cap_quota _avail_caps() const
	{
		Cap_quota avail_caps{_env.pd().avail_caps().value};

		if (_preservation.caps.value > avail_caps.value)
		{
			error("Capability preservation exceeds available capabilities");
			return Cap_quota{0};
		}

		/* deduce preserved quota from available quota */
		return Cap_quota{avail_caps.value - _preservation.caps.value};
	}

	/**
	 * Child::Ram_limit_accessor interface
	 */
	Ram_quota resource_limit(Ram_quota const &) const override
	{
		return _avail_ram();
	}

	/**
	 * Child::Cap_limit_accessor interface
	 */
	Cap_quota resource_limit(Cap_quota const &) const override { return _avail_caps(); }

	/**
	 * Child::Cpu_limit_accessor interface
	 */
	Cpu_quota resource_limit(Cpu_quota const &) const override { return _avail_cpu; }

	/**
	 * Child::Cpu_quota_transfer interface
	 */
	void transfer_cpu_quota(Capability<Pd_session> pd_cap, Pd_session &pd,
							Capability<Cpu_session> cpu, Cpu_quota quota) override
	{
		Cpu_quota const remaining{100 - min(100u, _transferred_cpu.percent)};

		/* prevent division by zero in 'quota_lim_upscale' */
		if (remaining.percent == 0)
			return;

		size_t const fraction =
			Cpu_session::quota_lim_upscale(quota.percent, remaining.percent);

		Child::with_pd_intrinsics(_pd_intrinsics, pd_cap, pd, [&](auto &intrinsics)
								  { intrinsics.ref_cpu.transfer_quota(cpu, fraction); });

		_transferred_cpu.percent += quota.percent;
	}

	/**
	 * State_reporter::Producer interface
	 */
	void produce_state_report(Xml_generator &xml, Report_detail const &detail) const override
	{
		if (detail.init_ram())
			xml.node("ram", [&]()
					 { Ram_info::from_pd(_env.pd()).generate(xml); });

		if (detail.init_caps())
			xml.node("caps", [&]()
					 { Cap_info::from_pd(_env.pd()).generate(xml); });

		if (detail.children())
			_children.report_state(xml, detail);
	}

	/**
	 * State_reporter::Producer interface
	 */
	Child::Sample_state_result sample_children_state() override
	{
		return _children.sample_state();
	}

	/**
	 * Default_route_accessor interface
	 */
	Xml_node default_route() override
	{
		return _default_route.constructed() ? _default_route->xml()
											: Xml_node("<empty/>");
	}

	/**
	 * Default_caps_accessor interface
	 */
	Cap_quota default_caps() override { return _default_caps; }

	void _update_aliases_from_config(Xml_node const &);
	void _update_parent_services_from_config(Xml_node const &);
	void _update_children_config(Xml_node const &);
	void _destroy_abandoned_parent_services();
	void _destroy_abandoned_children();

	Server _server{_env, _heap, _child_services, _state_reporter};

	/**
	 * Sandbox::Start_model::Factory
	 */
	Child &create_child(Xml_node const &) override;

	/**
	 * Sandbox::Start_model::Factory
	 */
	void update_child(Child &, Xml_node const &) override;

	/**
	 * Sandbox::Start_model::Factory
	 */
	Alias &create_alias(Child_policy::Name const &name) override
	{
		Alias &alias = *new (_heap) Alias(name);
		_children.insert_alias(&alias);
		return alias;
	}

	/**
	 * Sandbox::Start_model::Factory
	 */
	void destroy_alias(Alias &alias) override
	{
		_children.remove_alias(&alias);
		destroy(_heap, &alias);
	}

	/**
	 * Sandbox::Start_model::Factory
	 */
	bool ready_to_create_child(Start_model::Name const &,
							   Start_model::Version const &) const override;

	/**
	 * Sandbox::Parent_provides_model::Factory
	 */
	Parent_service &create_parent_service(Service::Name const &name) override
	{
		return *new (_heap) Parent_service(_parent_services, _env, name);
	}

	/**
	 * Default way of using the 'Env::pd' as the child's 'ref_pd' and accessing
	 * the child's address space via RPC.
	 */
	struct Default_pd_intrinsics : Pd_intrinsics
	{
		Env &_env;

		void with_intrinsics(Capability<Pd_session>, Pd_session &pd, Fn const &fn) override
		{
			Region_map_client region_map(pd.address_space());

			Intrinsics intrinsics{_env.pd(), _env.pd_session_cap(),
								  _env.cpu(), _env.cpu_session_cap(), region_map};
			fn.call(intrinsics);
		}

		void start_initial_thread(Capability<Cpu_thread> cap, addr_t ip) override
		{
			Cpu_thread_client(cap).start(ip, 0);
		}

		Default_pd_intrinsics(Env &env) : _env(env) {}

	} _default_pd_intrinsics{_env};

	Library(Env &env, Heap &heap, Registry<Local_service> &local_services,
			State_handler &state_handler, Pd_intrinsics &pd_intrinsics)
		: _env(env), _heap(heap), _pd_intrinsics(pd_intrinsics),
		  _local_services(local_services), _state_reporter(_env, *this, state_handler)
	{
	}

	Library(Env &env, Heap &heap, Registry<Local_service> &local_services,
			State_handler &state_handler)
		: Library(env, heap, local_services, state_handler, _default_pd_intrinsics)
	{
	}

	void apply_config(Xml_node const &);

	void generate_state_report(Xml_generator &xml) const
	{
		_state_reporter.generate(xml);
	}
};