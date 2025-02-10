#include <base/component.h>
#include <base/env.h>
#include <base/log.h>
#include <timer_session/connection.h>

namespace Hoitaja_test
{
    class Empty_cell;
}

class Hoitaja_test::Empty_cell
{
    private:
        Genode::Env &_env;
        Timer::Connection _timer{_env};

    public:
        Empty_cell(Genode::Env &env) : _env(env)
        {
            Genode::log("Volatile dummy cell started.");
            _timer.msleep(10000);
            Genode::log("Exiting..");
            _env.parent().exit(0);
        }
};

void Component::construct(Genode::Env &env) { static Hoitaja_test::Empty_cell cell(env);}