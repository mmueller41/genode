#include <thread>
#include <libc/component.h>
#include <base/log.h>
#include <chrono>
#include <memory>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <unistd.h>

namespace Posix_playground {
    class Chrono_thread;
}

/* Thread using std::chrono to measure the time it sleeps */
class Posix_playground::Chrono_thread {

    private:
        std::uint16_t _id;
    
    public:
        Chrono_thread(std::uint16_t id) : _id(id) { }

        [[nodiscard]] void execute()
        {
            std::chrono::time_point<std::chrono::steady_clock> start;
            std::chrono::time_point<std::chrono::steady_clock> end;
            
            while (true) {
                if (&this->_id == nullptr) {
                    Genode::log("WTF? this is a nullptr!");
                    return;
                } 
                Genode::log("Pong from Thread ", this->_id);
                start = std::chrono::steady_clock::now();
                sleep(this->_id);
                end = std::chrono::steady_clock::now();
                Genode::log("Thread ", _id, " woke up after ", std::chrono::duration_cast<std::chrono::seconds>(end - start).count());
            }
        }
};

int main(void) {
    Genode::log("Starting POSIX stdcxx playground");

    Genode::log("Let's start some threads");

    std::vector<Posix_playground::Chrono_thread*> thread_objs(5);
    std::vector<std::thread*> thread_list(4);

    Genode::log("Let's use aligned memory for threads objects.");

    for (int i = 0; i < 3; i++) {
        Posix_playground::Chrono_thread *thread_obj;
        if(posix_memalign((void**)(&thread_obj), 64, sizeof(Posix_playground::Chrono_thread))) {
            Genode::error("Could not allocate thread object ", i);
            continue;
        }
        Genode::log("Thread object ", (i+1), " is at address ", (void*)(thread_obj));

        thread_objs[i] = new (thread_obj) Posix_playground::Chrono_thread((std::uint16_t)(i+1));
        
        auto thread =  new std::thread([thread_objs, i]
                                      { thread_objs[i]->execute(); });
        thread_list[i] = thread;
        //thread->join();
    }

    for (auto thread : thread_list) {
        thread->join();
    }
    
    while(true);

    return 0;
}
