/*
 * \brief  Example app to utilize ACPICA library
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/allocator_avl.h>
#include <base/component.h>
#include <base/log.h>
#include <base/signal.h>
#include <base/heap.h>
#include <irq_session/connection.h>
#include <io_port_session/connection.h>

#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>

#include <util/reconstructible.h>
#include <util/xml_node.h>

#include <acpica/acpica.h>


extern "C" {
#include "acpi.h"
#include "accommon.h"
#include "acevents.h"
#include "acnamesp.h"
}

namespace Acpica {
	struct Main;
	struct Statechange;
	class Reportstate;
};

#include "util.h"
#include "reporter.h"
#include "fixed.h"


struct Acpica::Statechange
{
	Genode::Signal_handler<Acpica::Statechange> _dispatcher;
	Genode::Attached_rom_dataspace _system_state;
	bool _enable_reset;
	bool _enable_poweroff;

	Statechange(Genode::Env &env, bool reset, bool poweroff)
	:
		_dispatcher(env.ep(), *this, &Statechange::state_changed),
		_system_state(env, "system"),
		_enable_reset(reset), _enable_poweroff(poweroff)
	{
		_system_state.sigh(_dispatcher);

		state_changed();
	}

	void state_changed() {

		_system_state.update();

		if (!_system_state.valid()) return;

		Genode::Xml_node system(_system_state.local_addr<char>(),
		                       _system_state.size());

		typedef Genode::String<32> State;
		State const state = system.attribute_value("state", State());

		if (_enable_poweroff && state == "poweroff") {
			ACPI_STATUS res0 = AcpiEnterSleepStatePrep(5);
			ACPI_STATUS res1 = AcpiEnterSleepState(5);
			Genode::error("system poweroff failed - "
			              "res=", Genode::Hex(res0), ",", Genode::Hex(res1));
			return;
		}

		if (_enable_reset && state == "reset") {
			ACPI_STATUS res = AE_OK;
			try {
				res = AcpiReset();
			} catch (...) { }

			Genode::uint64_t const space_addr = AcpiGbl_FADT.ResetRegister.Address;
			Genode::error("system reset failed - "
			              "err=", res, " "
			              "reset=", !!(AcpiGbl_FADT.Flags & ACPI_FADT_RESET_REGISTER), " "
			              "spaceid=", Genode::Hex(AcpiGbl_FADT.ResetRegister.SpaceId), " "
			              "addr=", Genode::Hex(space_addr));
		}
	}
};

struct Acpica::Main
{
	Genode::Env  &env;
	Genode::Heap  heap { env.ram(), env.rm() };

	Genode::Attached_rom_dataspace config { env, "config" };

	Genode::Signal_handler<Acpica::Main>          sci_irq;
	Genode::Constructible<Genode::Irq_connection> sci_conn;

	Acpica::Reportstate * report { nullptr };

	unsigned unchanged_state_count { 0 };
	unsigned unchanged_state_max;

	static struct Irq_handler {
		UINT32 irq;
		Genode::Irq_session::Trigger  trigger;
		Genode::Irq_session::Polarity polarity;

		ACPI_OSD_HANDLER handler;
		void *context;
	} irq_handler;

	void init_acpica();

	Main(Genode::Env &env)
	:
		env(env),
		sci_irq(env.ep(), *this, &Main::acpi_irq),
		unchanged_state_max(config.xml().attribute_value("update_unchanged", 20U))
	{
		bool const enable_reset    = config.xml().attribute_value("reset", false);
		bool const enable_poweroff = config.xml().attribute_value("poweroff", false);
		bool const enable_report   = config.xml().attribute_value("report", false);

		if (enable_report)
			report = new (heap) Acpica::Reportstate(env);

		init_acpica();

		if (enable_report)
			report->enable();

		if (enable_reset || enable_poweroff)
			new (heap) Acpica::Statechange(env, enable_reset, enable_poweroff);

		/* setup IRQ */
		if (!irq_handler.handler) {
			Genode::warning("no IRQ handling available");
			return;
		}

		sci_conn.construct(env, irq_handler.irq, irq_handler.trigger, irq_handler.polarity);

		Genode::log("SCI IRQ: ", irq_handler.irq,
		            " (", irq_handler.trigger, "-", irq_handler.polarity, ")");

		sci_conn->sigh(sci_irq);
		sci_conn->ack_irq();
	}

	void acpi_irq()
	{
		if (!irq_handler.handler)
			return;

		UINT32 res = irq_handler.handler(irq_handler.context);

		sci_conn->ack_irq();

		AcpiOsWaitEventsComplete();

		if (report) {
			bool const changed = report->generate_report();

			if (unchanged_state_max) {
				if (changed)
					unchanged_state_count = 0;
				else
					unchanged_state_count ++;

				if (unchanged_state_count >= unchanged_state_max) {
					Genode::log("generate report because of ",
					            unchanged_state_count, " irqs without state "
					            "changes");
					report->generate_report(true);
					unchanged_state_count = 0;
				}
			}
		}

		if (res == ACPI_INTERRUPT_HANDLED)
			return;
	}
};

#include "ac.h"
#include "lid.h"
#include "sb.h"
#include "ec.h"
#include "fujitsu.h"

ACPI_STATUS init_pic_mode()
{
	ACPI_OBJECT_LIST arguments;
	ACPI_OBJECT      argument;

	arguments.Count = 1;
	arguments.Pointer = &argument;

	enum { PIC = 0, APIC = 1, SAPIC = 2};

	argument.Type = ACPI_TYPE_INTEGER;
	argument.Integer.Value = APIC;

	return AcpiEvaluateObject(ACPI_ROOT_OBJECT, ACPI_STRING("_PIC"),
	                          &arguments, nullptr);
}


void Acpica::Main::init_acpica()
{
	Acpica::init(env, heap);

	/* enable debugging: */
	if (false) {
		AcpiDbgLevel |= ACPI_LV_IO | ACPI_LV_INTERRUPTS | ACPI_LV_INIT_NAMES;
		AcpiDbgLayer |= ACPI_TABLES;
		Genode::log("debugging level=", Genode::Hex(AcpiDbgLevel),
		            " layers=", Genode::Hex(AcpiDbgLayer));
	}

	ACPI_STATUS status = AcpiInitializeSubsystem();
	if (status != AE_OK) {
		Genode::error("AcpiInitializeSubsystem failed, status=", status);
		return;
	}

	status = AcpiInitializeTables(nullptr, 0, true);
	if (status != AE_OK) {
		Genode::error("AcpiInitializeTables failed, status=", status);
		return;
	}

	status = AcpiLoadTables();
	if (status != AE_OK) {
		Genode::error("AcpiLoadTables failed, status=", status);
		return;
	}

	{
		using Genode::Irq_session;

		/*
		 * ACPI Spec 2.1 General ACPI Terminology
		 *
		 * System Control Interrupt (SCI) A system interrupt used by hardware
		 * to notify the OS of ACPI events. The SCI is an active, low,
		 * shareable, level interrupt.
		 */
		irq_handler.irq      = AcpiGbl_FADT.SciInterrupt;
		irq_handler.trigger  = Irq_session::TRIGGER_LEVEL;
		irq_handler.polarity = Irq_session::POLARITY_LOW;

		/* apply potential override in MADT */
		ACPI_TABLE_MADT *madt = nullptr;

		ACPI_STATUS status = AcpiGetTable(ACPI_STRING(ACPI_SIG_MADT), 0, (ACPI_TABLE_HEADER **)&madt);
		if (status == AE_OK) {
			using Genode::String;

			for_each_element(madt, (ACPI_SUBTABLE_HEADER *) nullptr,
			                 [&](ACPI_SUBTABLE_HEADER const * const s) {

				if (s->Type != ACPI_MADT_TYPE_INTERRUPT_OVERRIDE)
					return;

				ACPI_MADT_INTERRUPT_OVERRIDE const * const irq =
					reinterpret_cast<ACPI_MADT_INTERRUPT_OVERRIDE const * const>(s);

				auto polarity_from_flags = [] (UINT16 flags) {
					switch (flags & 0b11) {
					case 0b01: return Irq_session::POLARITY_HIGH;
					case 0b11: return Irq_session::POLARITY_LOW;
					case 0b00:
					default:
						return Irq_session::POLARITY_UNCHANGED;
					}
				};

				auto trigger_from_flags = [] (UINT16 flags) {
					switch ((flags & 0b1100) >> 2) {
					case 0b01: return Irq_session::TRIGGER_EDGE;
					case 0b11: return Irq_session::TRIGGER_LEVEL;
					case 0b00:
					default:
						return Irq_session::TRIGGER_UNCHANGED;
					}
				};

				if (irq->SourceIrq == AcpiGbl_FADT.SciInterrupt) {
					irq_handler.irq      = irq->GlobalIrq;
					irq_handler.trigger  = trigger_from_flags(irq->IntiFlags);
					irq_handler.polarity = polarity_from_flags(irq->IntiFlags);

					AcpiGbl_FADT.SciInterrupt = irq->GlobalIrq;
				}
			}, [](ACPI_SUBTABLE_HEADER const * const s) { return s->Length; });
		}
	}

	status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
	if (status != AE_OK) {
		Genode::error("AcpiEnableSubsystem failed, status=", status);
		return;
	}

	status = AcpiInitializeObjects(ACPI_NO_DEVICE_INIT);
	if (status != AE_OK) {
		Genode::error("AcpiInitializeObjects (no devices) failed, status=", status);
		return;
	}

	/* set APIC mode */
	status = init_pic_mode();
	if (status != AE_OK) {
		Genode::error("Setting PIC mode failed, status=", status);
		return;
	}

	/* Embedded controller */
	status = AcpiGetDevices(ACPI_STRING("PNP0C09"), Ec::detect, this, nullptr);
	if (status != AE_OK) {
		Genode::error("AcpiGetDevices failed, status=", status);
		return;
	}

	status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
	if (status != AE_OK) {
		Genode::error("AcpiInitializeObjects (full init) failed, status=", status);
		return;
	}

	status = AcpiUpdateAllGpes();
	if (status != AE_OK) {
		Genode::error("AcpiUpdateAllGpes failed, status=", status);
		return;
	}

	status = AcpiEnableAllRuntimeGpes();
	if (status != AE_OK) {
		Genode::error("AcpiEnableAllRuntimeGpes failed, status=", status);
		return;
	}

	/* note: ACPI_EVENT_PMTIMER claimed by nova kernel - not usable by us */
	Fixed * acpi_fixed = new (heap) Fixed(report);

	status = AcpiInstallFixedEventHandler(ACPI_EVENT_POWER_BUTTON,
	                                      Fixed::handle_power_button,
	                                      acpi_fixed);
	if (status != AE_OK)
		Genode::log("failed   - power button registration - error=", status);

	status = AcpiInstallFixedEventHandler(ACPI_EVENT_SLEEP_BUTTON,
	                                      Fixed::handle_sleep_button,
	                                      acpi_fixed);
	if (status != AE_OK)
		Genode::log("failed   - sleep button registration - error=", status);


	/* AC Adapters and Power Source Objects */
	status = AcpiGetDevices(ACPI_STRING("ACPI0003"), Ac::detect, this, nullptr);
	if (status != AE_OK) {
		Genode::error("AcpiGetDevices (ACPI0003) failed, status=", status);
		return;
	}

	/* Smart battery control devices */
	status = AcpiGetDevices(ACPI_STRING("PNP0C0A"), Battery::detect, this, nullptr);
	if (status != AE_OK) {
		Genode::error("AcpiGetDevices (PNP0C0A) failed, status=", status);
		return;
	}

	/* LID device */
	status = AcpiGetDevices(ACPI_STRING("PNP0C0D"), Lid::detect, this, nullptr);
	if (status != AE_OK) {
		Genode::error("AcpiGetDevices (PNP0C0D) failed, status=", status);
		return;
	}

	/* Fujitsu HID device */
	status = AcpiGetDevices(ACPI_STRING("FUJ02E3"), Fuj02e3::detect, this, nullptr);
	if (status != AE_OK) {
		Genode::error("AcpiGetDevices (FUJ02E3) failed, status=", status);
		return;
	}
}


struct Acpica::Main::Irq_handler Acpica::Main::irq_handler;


ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 irq, ACPI_OSD_HANDLER handler,
                                          void *context)
{
	if (irq != Acpica::Main::irq_handler.irq) {
		Genode::error("SCI interrupt is ", Acpica::Main::irq_handler.irq,
		              " but library requested ", irq);
		return AE_BAD_PARAMETER;
	}

	Acpica::Main::irq_handler.handler = handler;
	Acpica::Main::irq_handler.context = context;
	return AE_OK;
}


void Component::construct(Genode::Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	static Acpica::Main main(env);
}
