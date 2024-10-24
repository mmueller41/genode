
#include "synthetic_worker.h"

#include <chrono>
#include <iostream>
#include <thread>

#include <nova/syscall-generic.h>
#include <nova/syscalls.h>
#include <libc/component.h>

#include <mx/system/environment.h>
#include <mx/tasking/runtime.h>
#include <mx/util/core_set.h>

#include <base/log.h>

namespace
{

    int threads;
    uint64_t n;
    std::string worker_spec;

    class SyntheticWork : public mx::tasking::TaskInterface
    {
        private:
            SyntheticWorker *_w{nullptr};
            uint64_t *_cnt;

        public:
            SyntheticWork(SyntheticWorker *w, uint64_t *cnt) : _w(w), _cnt(cnt) {}
            ~SyntheticWork() override = default;

            mx::tasking::TaskResult execute(const std::uint16_t , const std::uint16_t) override
            {
                _w->Work(n);
                (*_cnt)++;
                //mx::tasking::runtime::scheduler().allocate_cores(64);
                return mx::tasking::TaskResult::make_succeed(this);
            }
    };

    void
    MainHandler(void *arg)
    {
        std::vector<uint64_t> cnt(threads);

        auto cores = mx::util::core_set::build(threads);
        std::cout << "Core set to use: " << cores << std::endl;
        mx::tasking::runtime::init(cores, 0, false);

        for (int i = 0; i < threads; ++i)
        {
            Genode::log("Creating synthetic worker ", i);
            auto *w = SyntheticWorkerFactory(worker_spec);
            if (w == nullptr) {
                std::cerr << "Failed to create worker." << std::endl;
                exit(1);
            }
            auto *work = mx::tasking::runtime::new_task<SyntheticWork>(i, w, &cnt[i]);
            work->annotate(static_cast<mx::tasking::TaskInterface::channel>(i));
            mx::tasking::runtime::spawn(*work, mx::system::topology::core_id());
        }

        auto monitor = std::thread([&]()
                                   {
                        uint64_t last_total = 0;
                        auto last = std::chrono::steady_clock::now();
                        while (1) {
                        std::chrono::seconds sec(1);
                        std::this_thread::sleep_for(sec);
                        auto now = std::chrono::steady_clock::now();
                        uint64_t total = 0;
                        double duration =
                            std::chrono::duration_cast<std::chrono::duration<double>>(now - last)
                                .count();
                        for (int i = 0; i < threads; i++) total += cnt[i];
                        std::cerr << static_cast<double>(total - last_total) / duration
                                    << std::endl;
                        last_total = total;
                        last = now;
                    } });
        mx::tasking::runtime::start_and_wait();
        monitor.join();

        // never returns
    }

} // anonymous namespace

void PrintUsage()
{
    std::cerr << "usage: [#threads] [#n] [worker_spec] <use_barrier>"
              << std::endl;
}

int main(int argc, char *argv[])
{
    int ret;
    if (argc < 4)
    {
        PrintUsage();
        return -EINVAL;
    }

    threads = std::stoi(argv[1], nullptr, 0);
    n = std::stoul(argv[2], nullptr, 0);
    worker_spec = std::string(argv[3]);

    // ret = base_init();
    if (ret)
        return ret;

    // ret = base_init_thread();
    if (ret)
        return ret;

    MainHandler(NULL);

    return 0;
}

void Libc::Component::construct(Libc::Env &env) {

    mx::system::Environment::set_env(&env);

    auto sys_cores = mx::util::core_set::build(64);
    mx::system::Environment::set_cores(&sys_cores);

    mx::memory::GlobalHeap::myself();
    std::uint16_t cores = 64;
     //env.cpu().affinity_space().total();

    char cores_arg[10];
    sprintf(cores_arg, "%d", cores);

    char *args[] = {"stress_genode", cores_arg, "1", "cacheantagonist:4090880"};

    Libc::with_libc([&]()
                    { 
                        std::cout << "Starting Cache Antagonist" << std::endl;
                        main(4, args); 
                    });
}