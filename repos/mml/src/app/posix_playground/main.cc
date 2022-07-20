#include <thread>
#include <libc/component.h>
#include <base/log.h>
#include <chrono>
#include <memory>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <unistd.h>
#include <iostream>

namespace Posix_playground {
    class Chrono_thread;
}

/* Thread using std::chrono to measure the time it sleeps */
class Posix_playground::Chrono_thread {

    private:
        std::uint16_t _id;
    
    public:
        Chrono_thread(std::uint16_t id) : _id(id) { }

        void execute()
        {
            std::chrono::time_point<std::chrono::steady_clock> start;
            std::chrono::time_point<std::chrono::steady_clock> end;
            
            while (true) {
                std::cout << "Pong from Thread " << this->_id << std::endl;
                start = std::chrono::steady_clock::now();
                
                sleep(this->_id);
                
                end = std::chrono::steady_clock::now();
                std::cout << "Thread " << _id << " woke up after " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << std::endl;
            }
        }
};

int main(void) {
    std::cout << "Starting POSIX stdcxx playground" << std::endl;

    std::cout << "Let's start some threads" << std::endl;

    std::vector<Posix_playground::Chrono_thread*> thread_objs(5);
    std::vector<std::thread*> thread_list(4);

    std::cout << "Let's use aligned memory for threads objects." << std::endl;

    for (int i = 0; i < 3; i++) {
        Posix_playground::Chrono_thread *thread_obj;
        if(posix_memalign((void**)(&thread_obj), 64, sizeof(Posix_playground::Chrono_thread))) {
            std::cerr << "Could not allocate thread object " << i << std::endl;
            continue;
        }
        std::cout << "Thread object " << (i + 1) << " is at address " << (void *)(thread_obj) << std::endl;

        thread_objs[i] = new (thread_obj) Posix_playground::Chrono_thread((std::uint16_t)(i+1));
        
        auto thread =  new std::thread([thread_objs, i]
                                      { thread_objs[i]->execute(); });
        thread_list[i] = thread;
    }

    for (auto thread : thread_list) {
        thread->join();
    }
    
    while(true);

    return 0;
}
