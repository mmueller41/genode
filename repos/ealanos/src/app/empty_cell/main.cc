#include <base/component.h>
#include <base/env.h>
#include <base/log.h>

namespace Hoitaja_test
{
    class Empty_cell;
}

class Hoitaja_test::Empty_cell
{
    private:
        Genode::Env &_env;
    
    public:
        Empty_cell(Genode::Env &env) : _env(env)
        {
            Genode::log("Empty dummy cell started.");
            while (true);
        }
};

void Component::construct(Genode::Env &env) { static Hoitaja_test::Empty_cell cell(env);}