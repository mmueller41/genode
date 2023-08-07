#include <base/component.h>
#include <base/env.h>
#include <base/log.h>
#include <timer_session/connection.h>

namespace Hoitaja_test {
    struct Persistent_cell;
}


struct Hoitaja_test::Persistent_cell
{
    Genode::Env &_env;
    Timer::Connection _timer{_env};

    void _handle_timeout() 
    {
        Genode::log("My affinity is ", _env.cpu().affinity_space());
        Genode::log("My PD cap is ", _env.pd_session_cap());
        _timer.trigger_once(5 * 1000 * 1000);
    }

    Genode::Signal_handler<Persistent_cell> _timeout_handler{
        _env.ep(), *this, &Persistent_cell::_handle_timeout};
        
    Persistent_cell(Genode::Env &env) : _env(env)
    {
        Genode::log("My affinity is ", _env.cpu().affinity_space());
        Genode::log("My PD cap is ", _env.pd().address_space());
        _timer.sigh(_timeout_handler);

        _timer.trigger_once(5 * 1000 * 1000);
    }
};

void Component::construct(Genode::Env &env) { static Hoitaja_test::Persistent_cell cell(env); }