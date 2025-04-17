#ifndef __EALANOS__INCLUDE__LAUNCHER__COMPONENT_H_
#define __EALANOS__INCLUDE__LAUNCHER__COMPONENT_H_

#include <base/rpc_server.h>
#include <base/env.h>
#include <base/session_label.h>
#include <base/log.h>
#include <base/attached_rom_dataspace.h>
#include <util/xml_generator.h>
#include <root/component.h>
#include <base/session_object.h>
#include <sandbox/sandbox.h>

#include <ealanos/laucher/session.h>


namespace Ealan {
    class Launcher_session_component;
    class Launcher_root;
    class Hoitaja;
}

class Ealan::Launcher_session_component : public Genode::Session_object<Ealan::Launcher_session>
{
    private:
        Genode::Entrypoint &_ep;
        Ealan::Hoitaja &_hoitaja;

    public:
        template <typename... ARGS>
        Launcher_session_component(Hoitaja &hoitaja, Genode::Entrypoint &ep, Genode::Session::Resources const &res, ARGS &&...args)
            : Session_object(ep, res, args...), _ep(ep), _hoitaja(hoitaja) { Genode::log("Creating new launcher session"); }

        void launch(Genode::String<640> start_node) override;
};

#endif // __EALANOS__INCLUDE__LAUNCHER__COMPONENT_H_