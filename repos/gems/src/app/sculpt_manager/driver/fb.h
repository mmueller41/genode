/*
 * \brief  Sculpt framebuffer-driver management
 * \author Norman Feske
 * \date   2024-03-15
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVER__FB_H_
#define _DRIVER__FB_H_

namespace Sculpt { struct Fb_driver; }


struct Sculpt::Fb_driver : private Noncopyable
{
	Constructible<Child_state> _intel_gpu { },
	                           _intel_fb  { },
	                           _vesa_fb   { },
	                           _boot_fb   { },
	                           _soc_fb    { };

	void gen_start_nodes(Xml_generator &xml) const
	{
		auto gen_capture_route = [&] (Xml_generator &xml)
		{
			gen_service_node<Capture::Session>(xml, [&] {
				xml.node("parent", [&] {
					xml.attribute("label", "global"); }); });
		};

		auto start_node = [&] (auto const &driver, auto const &binary, auto const &fn)
		{
			if (driver.constructed())
				xml.node("start", [&] {
					driver->gen_start_node_content(xml);
					gen_named_node(xml, "binary", binary);
					fn(); });
		};

		start_node(_intel_gpu, "intel_gpu", [&] {
			xml.node("provides", [&] {
				gen_service_node<Gpu::Session>     (xml, [&] { });
				gen_service_node<Platform::Session>(xml, [&] { });
			});
			xml.node("route", [&] {
				gen_parent_route<Platform::Session>(xml);
				gen_parent_rom_route(xml, "intel_gpu");
				gen_parent_rom_route(xml, "config", "config -> gpu");
				gen_parent_rom_route(xml, "system", "config -> managed/system");
				gen_parent_route<Rm_session>(xml);
				gen_common_routes(xml);
			});
		});

		start_node(_intel_fb, "pc_intel_fb", [&] {
			xml.node("heartbeat", [&] { });
			xml.node("route", [&] {
				gen_service_node<Platform::Session>(xml, [&] {
					gen_named_node(xml, "child", "intel_gpu"); });
				gen_capture_route(xml);
				gen_parent_rom_route(xml, "pc_intel_fb");
				gen_parent_rom_route(xml, "config", "config -> managed/fb");
				gen_parent_rom_route(xml, "system", "config -> managed/system");
				gen_parent_rom_route(xml, "intel_opregion", "report -> drivers/intel_opregion");
				gen_parent_route<Rm_session>(xml);
				gen_common_routes(xml);
			});
		});

		start_node(_vesa_fb, "vesa_fb", [&] {
			xml.node("route", [&] {
				gen_parent_route<Platform::Session>(xml);
				gen_capture_route(xml);
				gen_parent_rom_route(xml, "vesa_fb");
				gen_parent_rom_route(xml, "config", "config -> fb");
				gen_parent_route<Io_mem_session>(xml);
				gen_parent_route<Io_port_session>(xml);
				gen_common_routes(xml);
			});
		});

		start_node(_boot_fb, "boot_fb", [&] {
			xml.node("route", [&] {
				gen_parent_rom_route(xml, "config", "config -> fb");
				gen_parent_rom_route(xml, "boot_fb");
				gen_parent_rom_route(xml, "platform_info");
				gen_parent_route<Io_mem_session>(xml);
				gen_capture_route(xml);
				gen_common_routes(xml);
			});
		});

		start_node(_soc_fb, "fb", [&] {
			xml.node("route", [&] {
				gen_parent_route<Platform::Session>   (xml);
				gen_parent_route<Pin_control::Session>(xml);
				gen_parent_route<I2c::Session>(xml);
				gen_capture_route(xml);
				gen_parent_rom_route(xml, "fb");
				gen_parent_rom_route(xml, "config", "config -> fb");
				gen_parent_rom_route(xml, "dtb",    "fb.dtb");
				gen_parent_route<Rm_session>(xml);
				gen_common_routes(xml);
			});
		});
	};

	void update(Registry<Child_state> &registry, Board_info const &board_info,
	            Xml_node const &platform)
	{
		bool const suspending  = board_info.options.suspending;

		bool const use_intel_gpu =  board_info.detected.intel_gfx &&
		                           !board_info.options.suppress.intel_gpu;
		bool const use_intel_fb  =  use_intel_gpu && !suspending;
		bool const use_boot_fb   = !use_intel_fb  && !suspending &&
		                            board_info.detected.boot_fb;
		bool const use_vesa      = !use_intel_fb  && !suspending &&
		                            board_info.detected.vga && !use_boot_fb;

		_intel_gpu.conditional(use_intel_gpu,
		                       registry, "intel_gpu", Priority::MULTIMEDIA,
		                       Ram_quota { 32*1024*1024 }, Cap_quota { 1400 });

		_intel_fb.conditional(use_intel_fb,
		                      registry, "intel_fb", Priority::MULTIMEDIA,
		                      Ram_quota { 16*1024*1024 }, Cap_quota { 800 });

		_vesa_fb.conditional(use_vesa,
		                     registry, "vesa_fb", Priority::MULTIMEDIA,
		                     Ram_quota { 8*1024*1024 }, Cap_quota { 110 });

		_soc_fb.conditional(board_info.soc.fb && board_info.options.display,
		                    registry, Child_state::Attr {
		                        .name      = "fb",
		                        .priority  = Priority::MULTIMEDIA,
		                        .cpu_quota = 20,
		                        .initial   = { Ram_quota { 16*1024*1024 },
		                                       Cap_quota { 250 } },
		                        .max       = { } } );

		if (use_boot_fb && !_boot_fb.constructed())
			Boot_fb::with_mode(platform, [&] (Boot_fb::Mode mode) {
				_boot_fb.construct(registry, "boot_fb", Priority::MULTIMEDIA,
				                   mode.ram_quota(), Cap_quota { 100 }); });
	}

	static bool suspend_supported(Board_info const &board_info)
	{
		/* offer suspend/resume only when using intel graphics */
		return board_info.detected.intel_gfx
		   && !board_info.options.suppress.intel_gpu;
	}
};

#endif /* _DRIVER__FB_H_ */
