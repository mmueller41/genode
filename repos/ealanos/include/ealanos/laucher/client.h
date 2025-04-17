#ifndef __EALANOS__INCLUDE__LAUNCHER__CLIENT_H_
#define __EALANOS__INCLUDE__LAUNCHER__CLIENT_H_

#include <base/rpc_client.h>
#include <ealanos/laucher/session.h>

namespace Ealan {
    struct Launcher_client;
    using Launcher_capability = Genode::Capability<Ealan::Launcher_session>;
}

struct Ealan::Launcher_client : Genode::Rpc_client<Ealan::Launcher_session>
{
    explicit Launcher_client(Launcher_capability session) : Rpc_client<Launcher_session>(session) {}

    void launch(Genode::String<640> start_node) override {
        call<Rpc_launch>(start_node);
    }
};

#endif // __EALANOS__INCLUDE__LAUNCHER__CLIENT_H_