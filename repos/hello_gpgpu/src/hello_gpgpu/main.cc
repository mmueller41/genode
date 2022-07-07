#include <base/component.h>
#include <base/log.h>
#include <hello_gpgpu_session/connection.h>

void Component::construct(Genode::Env &env)
{
	gpgpu::Connection gpgpu(env);

	gpgpu.say_hello();

	Genode::log("hello gpgpu completed");
}
