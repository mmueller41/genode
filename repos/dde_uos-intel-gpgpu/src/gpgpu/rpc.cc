#include <base/log.h>
#include <base/heap.h>
#include <root/component.h>
#include <base/rpc_server.h>
#include <gpgpu/session.h>

namespace gpgpu {
	struct Session_component;
	struct Root_component;
	struct Main;
}

struct gpgpu::Session_component : Genode::Rpc_object<Session>
{
	void say_hello() override
	{
		Genode::log("Hello from uos-intel-gpgpu!");
	}
};

class gpgpu::Root_component
:
	public Genode::Root_component<Session_component>
{
	protected:
		Session_component *_create_session(const char *) override
		{
			return new (md_alloc()) Session_component();
		}

	public:

		Root_component(Genode::Entrypoint &ep,
		               Genode::Allocator &alloc)
		:
			Genode::Root_component<Session_component>(ep, alloc)
		{

		}
};


struct gpgpu::Main
{
	Genode::Env &env;

	/*
	 * A sliced heap is used for allocating session objects - thereby we
	 * can release objects separately.
	 */
	Genode::Sliced_heap sliced_heap { env.ram(), env.rm() };

	gpgpu::Root_component root { env.ep(), sliced_heap };

	Main(Genode::Env &env) : env(env)
	{
		/*
		 * Create a RPC object capability for the root interface and
		 * announce the service to our parent.
		 */
		env.parent().announce(env.ep().manage(root));
	}
};

void construct_RPC(Genode::Env &env)
{
	static gpgpu::Main main(env);
}
