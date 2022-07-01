#include <base/component.h>
#include <timer_session/connection.h>
#include <base/heap.h>
#include <cstdint>
namespace Thread_test {
    class Tester;
    class Test_thread;
}

using namespace Genode;

class Thread_test::Test_thread : public Thread
{
    private:
        Env &_env;
        uint16_t _id;
        Timer::Connection _timer{_env};

    public:
        List_element<Test_thread> _list_element{this};

        Test_thread(Env &env, uint16_t id, Location const &location)
            : Thread(env, Name("test_", location.xpos(), "x", location.ypos()), 4 * 4096, location, Weight(), env.cpu()),
            _env(env),
            _id(id) 
        { }

        void entry() override
        {
            while(true) {
                Genode::log("Pong from thread ", _id);
                _timer.msleep(_id * 1000);
            }
        }
};

class Thread_test::Tester
{
    typedef List<List_element<Thread_test::Test_thread>> Thread_list;

private:
    Env &_env;
    Heap _heap{_env.ram(), _env.rm()};
    Thread_list _threads{};

public:
    Tester(Env &env) : _env(env)
    {
        Affinity::Space space = env.cpu().affinity_space();

        Genode::log("Size of Affinity space is ", space.total());
        Genode::log("-----------------------------");
        for (unsigned i = 1; i < space.total(); i++)
        {
            Affinity::Location loc = space.location_of_index(i);
            Genode::log("1: x = ", loc.xpos(), " y = ", loc.ypos());
        }
        Genode::log("-----------------------------");

        for (unsigned i = 1; i < space.total(); i++)
        {
            Affinity::Location location = env.cpu().affinity_space().location_of_index(i);
            Test_thread *thread = new (_heap) Test_thread(env, (uint16_t)i, location);
            thread->start();

            _threads.insert(&thread->_list_element);
        }
}
};

void Component::construct(Genode::Env &env)
{
static Thread_test::Tester tester(env);
Genode::log("Thread tester constructed.");
}
