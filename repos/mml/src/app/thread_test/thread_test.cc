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

    void execute() 
    {
        while(true) {
            std::cout << "Hello world" << std::endl;
            std::this_thread::sleep_for(std::chrone::seconds(1));
        }
    }
};

void Component::construct(Genode::Env &env)
{
    static ThreadTest::Main main(env);
    std::thread([main]
                { main->execute(); });
}