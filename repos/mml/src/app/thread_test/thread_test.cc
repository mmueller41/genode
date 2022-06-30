#include <base/component.h>
#include <thread>
#include <iostream>
#include <chrono>

namespace ThreadTest {
    struct Main;
}

struct ThreadTest::Main
{
    Genode::Env &_env;

    Main(env) : _env(env) {}

    void execute() 
    {
        while(true) {
            std::cout << "Hello world" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
};

void Component::construct(Genode::Env &env)
{
    static ThreadTest::Main main(env);
    std::thread([&]
                { main.execute(); });
}