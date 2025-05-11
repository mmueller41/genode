#include "mx/util/core_set.h"
#include <cstddef>
#include <iostream>
#include <mx/tasking/runtime.h>

#include <libc/component.h>

class HelloWorldTask : public mx::tasking::TaskInterface
{
public:
    constexpr HelloWorldTask() = default;
    ~HelloWorldTask() override = default;

    mx::tasking::TaskResult execute(const std::uint16_t core_id) override
    {
        std::cout << "Hello World" << std::endl;

        // Stop MxTasking runtime after this task.
        return mx::tasking::TaskResult::make_stop(core_id);
    }
};

int mx_main(Libc::Env &env)
{
    unsigned num_cores = env.cpu().affinity_space().total();
    // Define which cores will be used (1 core here).
    const auto cores = mx::util::core_set::build(num_cores, mx::util::core_set::NUMAAware);

    { // Scope for the MxTasking runtime.

		std::size_t qouta = env.pd().avail_ram().value;
        std::cout << "Remaining memory quota " << qouta << std::endl; 
        // Create a runtime for the given cores.
        mx::tasking::runtime_guard _{env, true,cores};

		std::cout << "MxTasking initialized." << std::endl;
        // Create an instance of the HelloWorldTask with the current core as first
        // parameter. The core is required for memory allocation.
        auto *hello_world_task = mx::tasking::runtime::new_task<HelloWorldTask>(cores.front());

        std::cout << "task object is at " << static_cast<void*>(hello_world_task) << std::endl;
        // Annotate the task to run on the first core.
        hello_world_task->annotate(cores.front());

        // Schedule the task.
        mx::tasking::runtime::spawn(*hello_world_task);
		std::cout << "Remaining memory quota " << env.pd().avail_ram().value << std::endl;
		std::cout << "Consumed RAM qouta " << (qouta - env.pd().avail_ram().value) << std::endl;
	}

    return 0;
}

void Libc::Component::construct(Libc::Env &env)
{
	Libc::with_libc([&]() {
		mx_main(env);
	});
}
