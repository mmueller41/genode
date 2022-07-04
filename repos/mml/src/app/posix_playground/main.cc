#include <thread>
#include <base/component.h>
#include <chrono>
#include <memory>
#include <cstdint>
#include <vector>

namespace Posix_playground {
    class Chrono_thread;
}

class Posix_playground::Chrono_thread {

    private:
        std::uint16_t _id;
    
    public:
        Chrono_thread(std::uint16_t id) : _id(id) { }

        void execute()
        {
            while (true) {
                Genode::log("Pong from Thread ", _id);
                auto start = std::chrono::steady_clock::now();
                std::this_thread::sleep_for(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::milliseconds(_id * 1000)));
                auto end = std::chrono::steady_clock::now();
                Genode::log("Thread ", _id, " woke up after ", std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
            }
        }
};

int main(void) {
    Genode::log("Starting POSIX stdcxx playground");

    Genode::log("Let's start some threads");

    std::vector<Posix_playground::Chrono_thread*> thread_objs(4);
    std::vector<std::thread*> thread_list(4);

    for (std::uint16_t i = 1; i < 4; i++) {
        thread_objs[i] = new Posix_playground::Chrono_thread(i);
        auto thread =  new std::thread([&]
                                      { thread_objs[i]->execute(); });
        thread_list.push_back(thread);
        thread->join();
    }
    
    return 0;
}