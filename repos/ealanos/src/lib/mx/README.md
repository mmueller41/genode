# MxTasking

The *MxTasking*  lib implements the task engine.

## Structure
### Memory
The memory component can be found in the [memory](memory) folder. 
It implements different memory allocators (the [fixed size allocator](memory/fixed_size_allocator.h) mainly for tasks with a static size and the [dynamic size allocator](memory/dynamic_size_allocator.h) for variable sized data objects).
Further, epoch-based memory reclamation is implemented by the [epoch manager](memory/reclamation/epoch_manager.h).

### Queue
Different (task-) queues can be found in the [queue](queue) folder.
It provides 
* a [non-synchronized single-core queue](queue/list.h) (for fast core-local dispatching), 
* a [multi-producer single-consumer queue](queue/mpsc.h) (for dispatching tasks to remote cores),
* and a [(bound) multi-producer multi-consumer queue](queue/bound_mpmc.h) (mainly used for memory-reclamation).

### Resource
The resource component can be found in the [resource](resource) folder.
This package encapsulates 
* [annotations for resources](resource/annotation.h),
* the [resource builder](resource/builder.h) which creates and schedules resources,
* a [tagged pointer](resource/ptr.h) instance that links to resources including information (synchronization method and worker id),
* and the [resource interface](resource/resource_interface.h) that has to be implemented by each to synchronized resource.

### Synchronization
The synchronization component can be found in the [synchronization](synchronization) folder.
This package provides different synchronization methods ([optimistic lock](synchronization/optimistic_lock.h), [rw lock](synchronization/rw_spinlock.h), [spinlock](synchronization/spinlock.h)) and [structures to define the required synchronization level](synchronization/synchronization.h).

### Tasking
The tasking core can be found in the [tasking](tasking) folder.
For more information see the tasking-specific [readme](tasking/README.md).

