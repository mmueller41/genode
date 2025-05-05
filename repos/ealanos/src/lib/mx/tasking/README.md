# Tasking

## Task
Each task has to implement the [task interface](task.h) which inherits annotation to subclasses. 

#### Annotation
Annotations provide the possibility to express runtime information about the behavior of a task to the scheduling and execution layer.
Such information include
* the [priority](priority.h) a task should run with,
* the target (a specific worker id, a [resource](../resource/resource_interface.h) that needs synchronized access, `local` or `anywhere`) the task should execute on,
* and the [resource](../resource/ptr.h) a task is accessing that could be load from memory into cache before the task executes.

## Worker
Upon start, *MxTasking* will launch one [worker](worker.h) thread on each (specified) logical hardware thread.
The worker will fetch tasks from the task pool and executes them.

#### Task Pool
Every worker has its own [task pool](task_pool.h) which has different backend-queues (a queue for normal priority and local dispatches, a queue for normal priority and remote dispatches from the same numa region, a queue for normal priority and dispatches from remote numa regions, a queue for low priority ...).
Tasks will be fetched from the task pool and stored in a buffer before executed.

#### Task Buffer
The [task buffer](task_buffer.h) is a buffer for a specific amount of tasks (i.e., `64` tasks).
Whenever the buffer becomes empty, the worker will contact the task pool and fill up the buffer with tasks from the pool.
The main reason for using a buffer is to get a precise view of future-tasks.
Tasks in the pool are stored in linked lists which are very costly to iterate whereas the buffer can be accessed like an array.
This is important to *prefetch* tasks that will be executed in the near future.

#### Task Stack
The [task stack](task_stack.h) is used to persist the state of a task before executing the task optimistically.
Executing a task optimistically may fail and the state of the task has to be reset to the state before executing (to execute again at a later time).

## Scheduler
The [scheduler](scheduler.h) dispatches tasks to task pools on different worker threads.

## Configuration
The configuration is specified by a [config file](config.h).
* `max_cores`: Specifies the maximal number of cores the tasking will spawn worker threads on.
* `task_size`: The size that will be allocated for every task.
* `task_buffer_size`: The size allocated in the worker-local [task buffer](task_buffer.h) which is filled by the task pools.
* `is_use_task_counter`: If enabled, *MxTasking*  will collect information about the number of executed and disptached tasks. *Should be disabled for measurements.*
* `is_collect_task_traces`: If enabled, *MxTasking* will collect information about which task executed at what times at which worker thread. *Should be disabled for measurements.*
* `memory_reclamation`: Specifies if reclamation should be done periodically, after every task execution, or never.
* `worker_mode`: When running in `PowerSave` mode, every worker will sleep for a small amount of time to reduce power. *Should be `Performance` for measurements.*