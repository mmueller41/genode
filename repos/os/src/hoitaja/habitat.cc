#include "habitat.h"
#include <sandbox/utils.h>
#include <suoritin/request_task.h>

::Sandbox::Child &Hoitaja::Habitat::create_child(Genode::Xml_node const &start_node)
{
    if (_affinity_space.constructed() && !_core_allocator.constructed())
        _core_allocator.construct(*_affinity_space, _prio_levels);

    Genode::Affinity::Location allocation = _core_allocator->allocate_cores_for_cell(start_node);



    if (allocation.width() < 1) {
        Genode::error("failed to create child ", start_node.attribute_value("name", Child_policy::Name()), ": not enough CPU cores left.");
        throw ::Sandbox::Start_model::Factory::Creation_failed();
    }

    // Allocate `cores_share` cores from the Core Allocator and set the childs affinity accordingly
    // TODO: Implement core allocation

	try {
		Hoitaja::Cell &child = *new (_heap)
			Hoitaja::Cell(_env, _heap, *_verbose,
			      Child::Id { ++_child_cnt }, _state_reporter,
			      start_node, *this, *this, _children, *this, *this, *this, *this,
			      _prio_levels, _env.topo().global_affinity_space(), allocation,
			      _parent_services, _child_services, _local_services, _habitat_handler);
        _children.insert(static_cast<::Sandbox::Child *>(&child));
    
		maintain_cells();

        _avail_cpu.percent -= min(_avail_cpu.percent, child.cpu_quota().percent);

		if (start_node.has_sub_node("provides"))
			_server_appeared_or_disappeared = true;

		_state_report_outdated = true;

		return static_cast<::Sandbox::Child&>(child);
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

void Hoitaja::Habitat::_destroy_abandoned_children()
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
			_core_allocator->free_cores_from_cell(static_cast<Hoitaja::Cell &>(child));
			_children.remove(&child);

			Cpu_quota const child_cpu_quota = child.cpu_quota();

			destroy(_heap, &child);

			/* replenish available CPU quota */
			_avail_cpu.percent       += child_cpu_quota.percent;
			_transferred_cpu.percent -= min(_transferred_cpu.percent,
			                                child_cpu_quota.percent);
		}
	});
	
	/* We might have formerly occupied resources again now, so redistribute them */
	//maintain_cells();
}

void Hoitaja::Habitat::maintain_cells()
{
	int xpos = _affinity_space->total();
	_children.for_each_child([&](Child &child)
							 {
                                //log(child.name(), " ram: ", child.ram_quota());
                                Cell &cell = static_cast<Cell&>(child);
                                _core_allocator->update(cell, &xpos); });
	/*suoritin.for_each([&](Tukija::Suoritin::Session_component &client)
					  { Genode::log("Cell ", client.label(), "\n------------");
		for (unsigned long channel_id = 0; channel_id < client.channels(); channel_id++) 
		{
			Tukija::Suoritin::Channel &channel = client.channels_if()[channel_id];

			Genode::log("\t", "Channel ", channel_id, ": length=", channel.length(), " worker=", client.worker(channel._worker).name(), ",", client.worker(channel._worker).cap() );
			if (channel.length() > 0xFFFF) {
				Genode::Parent::Resource_args grant_args("cpu_quota=10");
				client.send_request(grant_args);
			}
		}
					   });*/
}

void Hoitaja::Habitat::update(Cell &cell)
{
	if (cell._exited) {
		if (cell._exit_value != 0)
			Genode::error(cell.name(), " exited with exit status ", cell._exit_value);
		
		_children.remove(static_cast<Sandbox::Child *>(&cell));
		_core_allocator->free_cores_from_cell(cell);

		/* Update resource allocations, as there are new resources available */	
		maintain_cells();
	}
}

void Hoitaja::Core_allocation_request::handle(Hoitaja::Cell&, Hoitaja::Habitat&) 
{
	Genode::Parent::Resource_args grant_args("cpu_quota=10");
	client().send_request(grant_args);
}