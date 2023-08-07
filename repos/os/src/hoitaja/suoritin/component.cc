/* Genode includes */
#include <base/session_object.h>
#include <base/affinity.h>
#include <base/attached_dataspace.h>
#include <base/heap.h>
#include <base/session_object.h>

#include <suoritin/session.h>
#include <suoritin/client.h>
#include <suoritin/connection.h>



namespace Tukija {
    namespace Suoritin {
        class Session_component;
        template <class T> class Allocator;
    }
}

template <class T>
class Tukija::Suoritin::Allocator : public Genode::Allocator
{
    using size_t = Genode::size_t;

private:
    Genode::Region_map::Local_addr _dataspace{};
    size_t _interface_size;
    Genode::Region_map::Local_addr _pos {_dataspace};

public:
    Allocator(Genode::Env &env, Genode::Ram_dataspace_capability *_interface_cap, size_t interface_size) : _interface_size(interface_size)
    {
        *_interface_cap = env.ram().alloc(interface_size);
        _dataspace = static_cast<T *>(env.rm().attach(*_interface_cap));
        }

        Alloc_result try_alloc(size_t) override
        {
        T *pos = _pos;
        if (pos >= static_cast<T*>(_dataspace) + _interface_size)
            return Alloc_result(Genode::Ram_allocator::Alloc_error::OUT_OF_RAM);

        pos++;
        return Alloc_result(static_cast<void *>(pos));
        }

        void free(void *, size_t) override
        { }

        size_t overhead(size_t) const override { return 0; }

        bool need_size_for_free() const override { return false; }

        T *interface() { return _dataspace; }
};

class Tukija::Suoritin::Session_component : public Genode::Session_object<Tukija::Suoritin::Session>
{
    private:
        Genode::Affinity _affinity;
        Genode::Env &_env;
        Genode::Ram_dataspace_capability _workers_interface_cap{};
        Genode::Ram_dataspace_capability _channels_interface_cap{};

        Allocator<Genode::Registered<Worker>> _worker_allocator;
        Allocator<Genode::Registered<Channel>> _channel_allocator;

        unsigned long no_channels{0};
        unsigned long no_workers{0};

        template <class T, typename FUNC>
        void construct(FUNC const &fn, Allocator<Genode::Registered<T>> &alloc, Genode::Registry<Genode::Registered<T>> &registry) {
            T* object = nullptr;

            try {
                try {
                    object = new (alloc) Genode::Registered<T>(registry);
                    fn(object);
                } catch (Genode::Allocator::Out_of_memory) {
                    Genode::error("Out of RAM on registering worker.");
                    throw;
                }
            } catch (...) {
                if (object)
                    destroy(alloc, object);
                Genode::error("Exception caught registering worker");
                throw;
            }
        }


    public:
        Session_component(Genode::Rpc_entrypoint &session_ep,
                          Genode::Session::Resources const &resources,
                          Genode::Session::Label const &label,
                          Genode::Session::Diag const &diag,
                          Genode::Env &env,
                          Genode::Affinity &affinity)
        : Session_object(session_ep, resources, label, diag), _affinity(affinity.space().total() ? affinity : Genode::Affinity(Genode::Affinity::Space(1,1), Genode::Affinity::Location(0,0,1,1))),
        _env(env), _worker_allocator(env, &_workers_interface_cap, _affinity.space().total()*sizeof(Genode::Registered<Worker>)),
        _channel_allocator(env, &_channels_interface_cap, _affinity.space().total()*sizeof(Genode::Registered<Channel>))
        {
        }

        void create_channel() override
        {
            try {
                construct<Channel>([&](Channel *) {}, _channel_allocator, _channels);
            }
            catch (...)
            {
                Genode::error("Faild to create channel");
            }
        }
        void register_worker(Genode::Thread::Name const &name, Genode::Thread_capability cap) override {
            try {
                construct<Worker>([&](Worker *worker)
                                  { worker->_cap = cap;
                                    worker->_name = name; }, _worker_allocator, _workers);
            }
            catch (...)
            {
                Genode::error("Failed to register worker");
            }
        }

        Capability interface_cap() override {
            return Capability{_workers_interface_cap, _channels_interface_cap};
        }
};