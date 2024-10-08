/*
 * \brief  Sandbox library
 * \author Norman Feske
 * \date   2020-01-10
 */

/*
 * Copyright (C) 2010-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <sandbox/sandbox.h>

/* local includes */
#include <sandbox/library.h>



void Sandbox::Library::_destroy_abandoned_parent_services()
{
	_parent_services.for_each([&] (Parent_service &service) {
		if (service.abandoned())
			destroy(_heap, &service); });
}


void Sandbox::Library::_destroy_abandoned_children()
{
	_children.for_each_child([&] (Child &child) {

		if (!child.abandoned())
			return;

		/* make the child's services unavailable */
		child.destroy_services();
		child.close_all_sessions();
		_state_report_outdated = true;

		/* destroy child once all environment sessions are gone */
		if (child.env_sessions_closed()) {
			_children.remove(&child);

			Cpu_quota const child_cpu_quota = child.cpu_quota();

			destroy(_heap, &child);

			/* replenish available CPU quota */
			_avail_cpu.percent       += child_cpu_quota.percent;
			_transferred_cpu.percent -= min(_transferred_cpu.percent,
			                                child_cpu_quota.percent);
		}
	});
}


bool Sandbox::Library::ready_to_create_child(Start_model::Name    const &name,
                                                     Start_model::Version const &version) const
{
	bool exists = false;

	unsigned num_abandoned = 0;

	_children.for_each_child([&] (Child const &child) {
		if (child.name() == name && child.has_version(version)) {
			if (child.abandoned())
				num_abandoned++;
			else
				exists = true;
		}
	});

	/* defer child creation if corresponding child already exists */
	if (exists)
		return false;

	/* prevent queuing up abandoned children with the same name */
	if (num_abandoned > 1)
		return false;

	return true;
}


::Sandbox::Child &Sandbox::Library::create_child(Xml_node const &start_node)
{
	if (!_affinity_space.constructed() && start_node.has_sub_node("affinity"))
		warning("affinity-space configuration missing, "
		        "but affinity defined for child ",
		        start_node.attribute_value("name", Child_policy::Name()));

	try {
		Child &child = *new (_heap)
			Child(_env, _heap, *_verbose,
			      Child::Id { ++_child_cnt }, _state_reporter,
			      start_node, *this, *this, _children, *this, *this, *this, *this,
			      _prio_levels, _effective_affinity_space(), Affinity::Location(-1, -1, 0, 0),
			      _parent_services, _child_services, _local_services);
		_children.insert(&child);

		_avail_cpu.percent -= min(_avail_cpu.percent, child.cpu_quota().percent);

		if (start_node.has_sub_node("provides"))
			_server_appeared_or_disappeared = true;

		_state_report_outdated = true;

		return child;
	}
	catch (Rom_connection::Rom_connection_failed) {
		/*
		 * The binary does not exist. An error message is printed
		 * by the Rom_connection constructor.
		 */
	}
	catch (Out_of_ram) {
		warning("memory exhausted during child creation"); }
	catch (Out_of_caps) {
		warning("local capabilities exhausted during child creation"); }
	catch (Child::Missing_name_attribute) {
		warning("skipped startup of nameless child"); }
	catch (Region_map::Region_conflict) {
		warning("failed to attach dataspace to local address space "
		        "during child construction"); }
	catch (Region_map::Invalid_dataspace) {
		warning("attempt to attach invalid dataspace to local address space "
		        "during child construction"); }
	catch (Service_denied) {
		warning("failed to create session during child construction"); }

	throw ::Sandbox::Start_model::Factory::Creation_failed();
}


void Sandbox::Library::update_child(Child &child, Xml_node const &start)
{
	if (child.abandoned())
		return;

	switch (child.apply_config(start)) {

	case Child::NO_SIDE_EFFECTS: break;

	case Child::PROVIDED_SERVICES_CHANGED:
		_server_appeared_or_disappeared = true;
		_state_report_outdated = true;
		break;
	};
}


void Sandbox::Library::apply_config(Xml_node const &config)
{
	_server_appeared_or_disappeared = false;
	_state_report_outdated          = false;

	_config_model.update_from_xml(config,
	                              _heap,
	                              _verbose,
	                              _version,
	                              _preservation,
	                              _default_route,
	                              _default_caps,
	                              _prio_levels,
	                              _affinity_space,
	                              *this, *this, _server,
	                              _state_reporter,
	                              _heartbeat);

	/*
	 * After importing the new configuration, servers may have disappeared
	 * (STATE_ABANDONED) or become new available.
	 *
	 * Re-evaluate the dependencies of the existing children.
	 *
	 * - Stuck children (STATE_STUCK) may become alive.
	 * - Children with broken dependencies may have become stuck.
	 * - Children with changed dependencies need a restart.
	 *
	 * Children are restarted if any of their client sessions can no longer be
	 * routed or result in a different route. As each child may be a service,
	 * an avalanche effect may occur. It stops if no child gets scheduled to be
	 * restarted in one iteration over all children.
	 */
	while (true) {

		bool any_restart_scheduled = false;

		_children.for_each_child([&] (Child &child) {

			if (child.abandoned())
				return;

			if (child.restart_scheduled()) {
				any_restart_scheduled = true;
				return;
			}

			if (_server_appeared_or_disappeared || child.uncertain_dependencies())
				child.evaluate_dependencies();

			if (child.restart_scheduled())
				any_restart_scheduled = true;
		});

		/*
		 * Release resources captured by abandoned children before starting
		 * new children. The children must be started in the order of their
		 * start nodes for the assignment of slack RAM.
		 */
		_destroy_abandoned_parent_services();
		_destroy_abandoned_children();

		_config_model.trigger_start_children();

		if (any_restart_scheduled)
			_config_model.apply_children_restart(config);

		if (!any_restart_scheduled)
			break;
	}

	_server.apply_updated_policy();

	/*
	 * (Re-)distribute RAM and capability quota among the children, given their
	 * resource assignments and the available slack memory. We first apply
	 * possible downgrades to free as much resources as we can. These resources
	 * are then incorporated in the subsequent upgrade step.
	 */
	_children.for_each_child([&] (Child &child) { child.apply_downgrade(); });
	_children.for_each_child([&] (Child &child) { child.apply_upgrade(); });

	if (_state_report_outdated)
		_state_reporter.trigger_immediate_report_update();
}


/*********************************
 ** Sandbox::Local_service_base **
 *********************************/

void Genode::Sandbox::Local_service_base::_for_each_requested_session(Request_fn &fn)
{
	_server_id_space.for_each<Session_state>([&] (Session_state &session) {

		if (session.phase == Session_state::CREATE_REQUESTED) {

			Request request(session);

			fn.with_requested_session(request);

			bool wakeup_client = false;

			if (request._denied) {
				session.phase = Session_state::SERVICE_DENIED;
				wakeup_client = true;
			}

			if (request._session_ptr) {
				session.local_ptr = request._session_ptr;
				session.cap       = request._session_cap;
				session.phase     = Session_state::AVAILABLE;
				wakeup_client     = true;
			}

			if (wakeup_client && session.ready_callback)
				session.ready_callback->session_ready(session);
		}
	});
}


void Genode::Sandbox::Local_service_base::_for_each_upgraded_session(Upgrade_fn &fn)
{
	_server_id_space.for_each<Session_state>([&] (Session_state &session) {

		if (session.phase != Session_state::UPGRADE_REQUESTED)
			return;

		if (session.local_ptr == nullptr)
			return;

		bool wakeup_client = false;

		Session::Resources const amount { session.ram_upgrade,
		                                  session.cap_upgrade };

		switch (fn.with_upgraded_session(*session.local_ptr, amount)) {

			case Upgrade_response::CONFIRMED:
				session.phase = Session_state::CAP_HANDED_OUT;
				wakeup_client = true;
				break;

			case Upgrade_response::DEFERRED:
				break;
			}

		if (wakeup_client && session.ready_callback)
			session.ready_callback->session_ready(session);
	});
}


void Genode::Sandbox::Local_service_base::_for_each_session_to_close(Close_fn &close_fn)
{
	/*
	 * Collection of closed sessions to be destructed via callback
	 *
	 * For asynchronous sessions, the 'Session_state' object is destructed by
	 * the 'Closed_callback'. We cannot issue the callback from within
	 * '_server_id_space.for_each()' because the destruction of 'id_at_server'
	 * would deadlock. Instead be collect the 'Session_state' objects to be
	 * called back in the 'pending_callbacks' ID space. This is possible
	 * because the parent ID space is not used for local services.
	 */
	Id_space<Parent::Client> pending_callbacks { };

	_server_id_space.for_each<Session_state>([&] (Session_state &session) {

		if (session.phase != Session_state::CLOSE_REQUESTED)
			return;

		if (session.local_ptr == nullptr)
			return;

		switch (close_fn.close_session(*session.local_ptr)) {

		case Close_response::CLOSED:

			session.phase = Session_state::CLOSED;
			session.id_at_parent.construct(session, pending_callbacks);
			break;

		case Close_response::DEFERRED:
			break;
		}
	});

	/*
	 * Purge 'Session_state' objects by calling 'closed_callback'
	 */
	while (pending_callbacks.apply_any<Session_state>([&] (Session_state &session) {

		session.id_at_parent.destruct();

		if (session.closed_callback)
			session.closed_callback->session_closed(session);
		else
			session.destroy();
	}));
}


Genode::Sandbox::Local_service_base::Local_service_base(Sandbox    &sandbox,
                                                        Name const &name,
                                                        Wakeup     &wakeup)
:
	Service(name),
	_element(sandbox._local_services, *this),
	_session_factory(sandbox._heap, Session_state::Factory::Batch_size{16}),
	_async_wakeup(wakeup),
	_async_service(name, _server_id_space, _session_factory, _async_wakeup)
{ }


/*************
 ** Sandbox **
 *************/

void Genode::Sandbox::apply_config(Xml_node const &config)
{
	_library.apply_config(config);
}


void Genode::Sandbox::generate_state_report(Xml_generator &xml) const
{
	_library.generate_state_report(xml);
}


Genode::Sandbox::Sandbox(Env &env, State_handler &state_handler)
:
	_heap(env.ram(), env.rm()),
	_library(*new (_heap) ::Sandbox::Library(env, _heap, _local_services, state_handler))
{ }
