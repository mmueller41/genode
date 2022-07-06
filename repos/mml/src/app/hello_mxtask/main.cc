/* Hello world appication from https://github.com/jmuehlig/mxtasking/src/application/hello_world */
/* MIT License

Copyright (c) 2021 Jan Mühlig <jan.muehlig@tu-dortmund.de>
Modified by Michael Müller <michael.mueller@uos.de> for use on Genode OS.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE. */

#include <base/component.h>
#include <mx/tasking/runtime.h>

class HelloWorldTask : public mx::tasking::TaskInterface
{
public:
    constexpr HelloWorldTask() = default;
    ~HelloWorldTask() override = default;

    mx::tasking::TaskResult execute(const std::uint16_t /*core_id*/, const std::uint16_t /*channel_id*/) override
    {
        std::cout << "Hello World" << std::endl;

        // Stop MxTasking runtime after this task.
        return mx::tasking::TaskResult::make_stop();
    }
};

void Component::construct(Genode::Env &env)
{
    // Define which cores will be used (1 core here).
    const auto cores = mx::util::core_set::build(1);

    mx::system::Environment::env = env;
    { // Scope for the MxTasking runtime.

        // Create a runtime for the given cores.
        mx::tasking::runtime_guard _{cores};

        // Create an instance of the HelloWorldTask with the current core as first
        // parameter. The core is required for memory allocation.
        auto *hello_world_task = mx::tasking::runtime::new_task<HelloWorldTask>(cores.front());

        // Annotate the task to run on the first core.
        hello_world_task->annotate(cores.front());

        // Schedule the task.
        mx::tasking::runtime::spawn(*hello_world_task);
    }
}
