/*
 * \brief  Connection to GPGPU session
 * \author Michael Müller
 * \date   2022-07-17
 */

/*
 * Copyright (C) 2022 Michael Müller
 *
 * This file is distributed under the terms of the
 * GNU Affero General Public License version 3.
 */

#include "client.h"
#include <base/connection.h>

namespace Kiihdytin::GPGPU { struct Connection; }


struct Kiihdytin::GPGPU::Connection : Genode::Connection<Session>, Session_client
{
	Connection(Genode::Env &env)
	:
		/* create session */
		Genode::Connection<Kiihdytin::GPGPU::Session>(env, session(env.parent(),
		                                                "ram_quota=6K, cap_quota=4")), // TODO: determine correct ram and cap quota

		/* initialize RPC interface */
		Session_client(cap()) { }
};
