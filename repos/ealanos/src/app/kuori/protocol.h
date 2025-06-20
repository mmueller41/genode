#ifndef __EALANOS__SRC__APP__KUORI__PROTOCOL_H_
#define __EALANOS__SRC__APP__KUORI__PROTOCOL_H_

#include <cstdint>

namespace Ealan
{
	namespace Kuori
	{
        using Handle = std::uint16_t;
		
		struct Package_header {
				enum Command { BEGIN, COMMIT, ACK };

				enum State {
					ACCEPT,
					FAILED,
					SUCCESS,
					WAIT
				};

				Command command;
				State   state;
				std::uint32_t length;
		};

		struct Request {
				Package_header::Command command;
				std::uint32_t length;
				int cd;
		};
	}
	// namespace Kuori
}

#endif