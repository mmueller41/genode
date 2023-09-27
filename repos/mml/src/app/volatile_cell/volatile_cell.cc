#include <base/component.h>
#include <base/env.h>
#include <timer_session/connection.h>
#include <base/log.h>

namespace Hoitaja_test {
    class Volatile_cell;
}


class Hoitaja_test::Volatile_cell
{
    private:
        Genode::Env &_env;
        Timer::Connection _timer{_env};

        void _handle_timeout()
        {
            Genode::log("My time has come. Exiting ...");
            _env.parent().exit(0);
        }

        Genode::Signal_handler<Volatile_cell> _timeout_handler{
            _env.ep(), *this, &Volatile_cell::_handle_timeout};

    public:
        Volatile_cell(Genode::Env &env) : _env(env) 
        {
            Genode::log("My affinity space is ", _env.cpu().affinity_space());
            _timer.sigh(_timeout_handler);
            _timer.trigger_once(30 * 1000 * 1000);
        }
};

void Component::construct(Genode::Env &env) { static Hoitaja_test::Volatile_cell cell(env); }