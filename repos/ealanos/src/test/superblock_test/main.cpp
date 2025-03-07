/**
 * @file main.cpp
 * @author Michael Müller
 * @brief Component for testing superblocks
 * @version 0.1
 * @date 2025-03-06
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <base/component.h>
#include <base/env.h>
#include <base/log.h>
#include <ealanos/memory/superblock.h>
#include <base/attached_ram_dataspace.h>
#include <base/heap.h>

namespace Ealan::Memory {
    class SuperblockTest;
}

class Ealan::Memory::SuperblockTest
{
    private:
        Genode::Env &_env;

        class SuperblockTestThread : public Genode::Thread
        {
            private:
                Genode::Env &_env;
                Superblock<8192, 64> *_sb;
                Genode::size_t _blocks;

                SuperblockTestThread(const SuperblockTestThread &);
                SuperblockTest &operator=(SuperblockTestThread &);

            public:
                SuperblockTestThread(Genode::Env &env, Superblock<8192,64> *sb, Genode::size_t blocks, Genode::Affinity::Location loc) : Genode::Thread(env, Genode::Thread::Name("superblocktest"), 4 * 4096, loc, Genode::Cpu_session::Weight(), env.cpu()), _env(env), _sb(sb), _blocks(blocks)
                {}

                void entry() override {
                    void *blocks[_blocks];

                    Genode::log("Capacity of superblock ", _sb, " is ", _sb->free_blocks());

                    for (unsigned i = 0; i < _blocks; i++) {
                        blocks[i] = _sb->alloc();
                        Genode::log("blocks[", i, "]=", blocks[i]);
                        _sb->free(blocks[i]);
                        Genode::log("Freed ", blocks[i]);
                    }
                }
        };

    public:
        SuperblockTest(Genode::Env &env) : _env(env)
        {
            Genode::Ram_dataspace_capability ds = _env.ram().alloc(8192);
            Genode::Region_map::Attr attr{};
            attr.writeable = true;
            Genode::Region_map::Attach_result const result = _env.rm().attach(ds, attr);
            Genode::Heap heap(_env.ram(), _env.rm());

            void *sb_address = result.convert<void *>(
                [&](Genode::Region_map::Range r)
                { return reinterpret_cast<void *>(r.start); },
                [&](Genode::Region_map::Attach_error)
                {
                    return nullptr;
                });

            Genode::log("Attached dataspace for superblock");
            Superblock<8192, 64> *sb = new (sb_address) Superblock<8192, 64>(128);

            Genode::log("Superblock has ", sb->free_blocks(), " free blocks.");
            Genode::log("Superblock blocks begin at ", sb->start());

            Genode::size_t capacity = sb->free_blocks();

            Genode::log("Allocating blocks until superblock is full");

            void *blocks[capacity];

            for (unsigned i = 0; i < capacity; i++)
            {
                blocks[i] = sb->alloc();
                Genode::log("block[", i, "]=", blocks[i], " left=", sb->free_blocks());
            }

            Genode::log("Test if allocating from full block yields nullptr");
            if (!sb->alloc()) {
                Genode::log("Success.");
            } else {
                Genode::error("Got block despite superblock being full.");
            }

            Genode::log("Freeing 1/2 of the blocks (every second block) and checking if capacity is ", capacity / 2);
            for (unsigned i = 1; i < capacity; i+=2) {
                sb->free(blocks[i]);
            }
            if (sb->free_blocks() == capacity/2) {
                Genode::log("Success.");
            } else {
                Genode::error("Capacity of superblock is: ", sb->free_blocks(), " but should be ", capacity / 2);
            }

            Genode::log("The next block we allocate should be right after the first allocated block.");
            blocks[1] = sb->alloc();
            if (blocks[1] != reinterpret_cast<void*>(reinterpret_cast<Genode::addr_t>(blocks[0])+128)) {
                Genode::error("Allocator yielded wrong block: ", blocks[1]);
            } else {
                Genode::log("Success.");
            }

            Genode::log("Testing aligned alloc with default alignment=64");
            blocks[3] = sb->aligned_alloc();
            if (reinterpret_cast<Genode::addr_t>(blocks[3]) % 64 == 0) {
                Genode::log("Success");
            } else {
                Genode::error("Alignment error: ", blocks[3], " is not 64-bytes aligned");
            }

            Genode::log("Freeing all remaining blocks");

            sb->free(blocks[3]);
            sb->free(blocks[1]);

            for (unsigned i = 0; i < capacity; i+=2) {
                sb->free(blocks[i]);
            }

            if (sb->free_blocks() == capacity) {
                Genode::log("Success. Full capacitiy restored.");
            } else {
                Genode::error("Could not free all blocks. Capacity is now: ", sb->free_blocks());
            }

            Genode::log("Thread-safety test ");
            SuperblockTestThread *threads[8];
            for (unsigned i = 0; i < 8; i++)
            {
                threads[i] = new (heap) SuperblockTestThread(env, sb, capacity / 8, env.cpu().affinity_space().location_of_index(i));
                threads[i]->start();
            }

            Genode::log("Threads started. Waiting for completion..");
            for (unsigned i = 0; i < 8; i++)
            {
                threads[i]->join();
            }
            Genode::log("Threads finished. There should be ", capacity, " blocks left now.");
            if (sb->free_blocks() == capacity) {
                Genode::log("Success.");
            } else {
                Genode::error("Failed: ", sb->free_blocks(), "left.");
            }
        }
};

void Component::construct(Genode::Env &env)
{
    static Ealan::Memory::SuperblockTest test(env);
}