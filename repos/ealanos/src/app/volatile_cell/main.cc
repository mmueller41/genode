#include <base/component.h>
#include <base/env.h>
#include <base/log.h>
#include <timer_session/connection.h>
#include <tukija/syscalls.h>

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
            Tukija::alloc();
            Genode::log("Found topology model of size ", Tukija::Tip::tip()->length, " mapped at ", static_cast<const void *>(Tukija::Tip::tip()));
            Tukija::Cip *cip = Tukija::Cip::cip();
            Genode::log("CIP value: ", *reinterpret_cast<unsigned long*>(cip));
            long pd_sel = _env.pd().native_pd().local_name();
            Genode::log("pd_sel = ", pd_sel);
            _timer.msleep(10000);
            Genode::log("Exiting..");
            _env.parent().exit(0);

        }
};

void Component::construct(Genode::Env &env) { static Hoitaja_test::Empty_cell cell(env);}