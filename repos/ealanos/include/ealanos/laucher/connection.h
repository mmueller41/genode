#ifndef __EALANOS__INCLUDE__LAUNCHER__CONNECTION_H_
#define __EALANOS__INCLUDE__LAUNCHER__CONNECTION_H_

#include <base/rpc_args.h>
#include <base/connection.h>
#include <session/session.h>


#include <ealanos/laucher/client.h>

namespace Ealan {
    struct Launcher_connection;
}

struct Ealan::Launcher_connection : Genode::Connection<Ealan::Launcher_session>, Launcher_client
{
    Launcher_connection(Genode::Env &env, Genode::Affinity &affinity, Label const &label = Label())
    : Connection<Launcher_session>(env, label, Genode::Ram_quota { RAM_QUOTA }, affinity, Args("")), Launcher_client(cap()) {}

    void launch(Genode::String<640> start_node) override
    {
        Launcher_client::launch(start_node);
    }
};
#endif // __EALANOS__INCLUDE__LAUNCHER__CONNECTION_H_