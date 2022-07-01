#include <base/component.h>
#include <thread>
#include <iostream>
#include <sched.h>
#include <unistd.h>

namespace ThreadTest {
    class Test;
}

class ThreadTest::Test
{

    public:

    void execute() 
    {
        while(true) {
            Genode::log("Hello world");
            //std::cout << "Hello world" << std::endl;
            //std::this_thread::sleep_for(std::chrono::seconds(5));
            sleep(2);
        }
    }
};

int main(void)
{
    /* Create test posix thread via std::thread API */
    ThreadTest::Test test;
    auto test_thread = std::thread([&] { test.execute(); });
    
    /* Print native threads affinity */

    test_thread.join();
    return 0;
}

//void Component::construct(Genode::Env &env)
//{
    //static ThreadTest::Test test;
    //Genode::log("ThreadTest constructed.");
    //test.execute();
    ////std::thread([&]
                ////{ main.execute(); });
//}
