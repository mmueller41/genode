#ifndef __EALANOS__INCLUDE__LAUNCHER__SESSION_H_
#define __EALANOS__INCLUDE__LAUNCHER__SESSION_H_

#include <base/rpc_args.h>
#include <session/session.h>
#include <util/xml_node.h>

namespace Ealan {
    struct Launcher_session;
}

struct Ealan::Launcher_session : Genode::Session
{
    static const char *service_name() { return "Launcher"; }

    enum
    {
        CAP_QUOTA = 1,
        RAM_QUOTA = 1024
    };

    virtual void launch(Genode::String<640> start_node) = 0;

    GENODE_RPC(Rpc_launch, void, launch, Genode::String<640>);
    GENODE_RPC_INTERFACE(Rpc_launch);
};

#endif // __EALANOS__INCLUDE__LAUNCHER__SESSION_H_