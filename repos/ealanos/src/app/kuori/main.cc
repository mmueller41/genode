#include "base/attached_ram_dataspace.h"
#include "base/ram_allocator.h"
#include "region_map/region_map.h"
#include "sys/_pthreadtypes.h"
#include "util/xml_node.h"
#include <base/log.h>
#include <base/attached_rom_dataspace.h>
#include <cstring>
#include <libc/component.h>
#include <internal/thread_create.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include <ealanos/shell/connection.h>

#include "protocol.h"
namespace Ealan {
    namespace Kuori {
        class Server;
    }
}

static void print(Genode::Output &output, sockaddr_in const &addr)
{
	Genode::print(output, (ntohl(addr.sin_addr.s_addr) >> 24) & 0xff);
	output.out_string(".");
	Genode::print(output, (ntohl(addr.sin_addr.s_addr) >> 16) & 0xff);
	output.out_string(".");
	Genode::print(output, (ntohl(addr.sin_addr.s_addr) >> 8) & 0xff);
	output.out_string(".");
	Genode::print(output, (ntohl(addr.sin_addr.s_addr) >> 0) & 0xff);
	output.out_string(":");
	Genode::print(output, ntohs(addr.sin_port));
}
class Ealan::Kuori::Server
{
	private:

		Libc::Env                     &_env;
		Genode::Attached_rom_dataspace _config_rom {_env, "config"};
		Genode::Xml_node               _config{_config_rom.xml()};
		Ealan::Shell::Connection       _shell{_env};

		void _die(const char *reason){ perror(reason); _env.parent().exit(1); }

		struct handler {
				pthread_t *thread;
				Server    *server;
				int        cd;
		};

	    char *_config_ptr;

	public:

		using Command = Ealan::Kuori::Package_header::Command;
		using Header  = Ealan::Kuori::Package_header;
		using State   = Ealan::Kuori::Package_header::State;

		
		Server(Libc::Env &env) : _env(env) {}

		static void *handle(void *args)
		{
			struct handler *h = reinterpret_cast<struct handler *>(args);
			h->server->handle_connection(h->cd);
			delete h->thread;
			delete h;

			return nullptr;
		}

		void handle_connection(int cd)
		{
			Header header;
			
			while (true) {
				int ret = read(cd, &header, sizeof(header));

				if (!ret) break;

				if (ret > 0) {
					Request request{header.command, header.length, cd};
					Genode::log("size of command: ", sizeof(Command));
					Genode::log("CMD=", static_cast<unsigned int>(request.command), " CD=", cd, " LEN=", header.length);
					process_request(request);
				}
			}

		}

		void process_request(Request &request)
		{
			switch (request.command) {
			case Command::BEGIN:
				{
					
					/* Acknowledge the new transaction */

					Header *ack = new Header {Command::ACK, State::ACCEPT, static_cast<std::uint32_t>(::strlen(_config_ptr)) };
					write(request.cd, ack, sizeof(Header));

					/* Send habitat configuration to client */
					write(request.cd, _config_ptr, ::strlen(_config_ptr));
					break;
				}

			case Command::COMMIT:
				{
                    char *buf = new char[request.length];
					Genode::log("Commit: Awaiting ", request.length, " bytes");
					std::uint32_t bytes = 0;
					int rc    = 0;

					while (bytes < request.length) {
						rc = read(request.cd, &buf[bytes], request.length);
						if (rc == 0)
							break;
						Genode::log("Read ", rc, " bytes.");
						bytes += rc;
					}


					memcpy(_config_ptr, buf, request.length);
					_config_ptr[bytes] = '\0';

					printf("Read new configuration:\n %s\n", _config_ptr);

                    _shell.commit();
					break;
				}
			default:
				{
					return;
				}
			}
		}

		void run()
		{
			int rc = 0;

			Genode::log("Kuori v1.0 - Remote Shell for EalánOS");

			Genode::Ram_dataspace_capability cap = _shell.connect();
			Genode::Region_map::Attr         attr{};
			attr.writeable = true;

			_config_ptr = _env.rm().attach(cap, attr).convert<char *>(
				[&](Genode::Region_map::Range r) { return reinterpret_cast<char*>(r.start); }, [&] (Genode::Region_map::Attach_error) { return nullptr; });

			if (!_config_ptr) _die("Failed mapping habitat config");

			Genode::log("Mapped habitat configuration");

			printf("%s\n", _config_ptr);
			
			int const sd = socket(AF_INET, SOCK_STREAM, 0);
			if (sd == -1) { _die("socket"); }

			unsigned const port = _config.attribute_value("port", 8080U);

			sockaddr_in const addr{0, AF_INET, htons(port), {INADDR_ANY}, {INADDR_ANY}};
			sockaddr const   *paddr = reinterpret_cast<sockaddr const *>(&addr);

			int const on = 1;
			rc           = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

			if (rc) _die("setsockopt");

			rc = bind(sd, paddr, sizeof(addr));
			if (rc == -1) _die("bind");

			rc = listen(sd, SOMAXCONN);
			if (rc == -1) _die("listen");

			Genode::log("Kuori service up and running.");

			while (true) {
				sockaddr_in caddr;
				socklen_t   scaddr = sizeof(caddr);
				sockaddr   *pcaddr = reinterpret_cast<sockaddr *>(&caddr);

				int const cd = accept(sd, pcaddr, &scaddr);
				if (cd == -1) _die("accept");

				Genode::log("New connection from ", caddr);
				struct handler *handler = new struct handler();
				handler->cd             = cd;
				handler->server         = this;
				handler->thread         = new pthread_t();

                Genode::String<32> const name {"kuori#", caddr};
				
				Libc::pthread_create_from_session(handler->thread, Server::handle, handler, 8*4096, name.string(), &_env.cpu(), _env.cpu().affinity_space().location_of_index(0));
			}
		}
};

void Libc::Component::construct(Libc::Env &env)
{
	static Ealan::Kuori::Server kuori(env);

	Libc::with_libc([&]() {
		kuori.run();
	});
}
