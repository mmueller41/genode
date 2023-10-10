#include <base/component.h>
#include <base/log.h>

#include <suoritin/connection.h>

struct Suoritin_tester
{
    Genode::Env &_env;


    Suoritin_tester(Genode::Env &env) : _env(env)
    {
        Genode::log("Hello from Suoritin tester");
    Tukija::Suoritin::Connection suoritin {_env};
    }
};

void Component::construct(Genode::Env &env) {
    static Suoritin_tester tester(env);
}