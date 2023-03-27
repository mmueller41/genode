# EalánOS — An Operating System for Heterogeneous Many-core Systems

EalánOS is a research operating system, based on the [Genode OS Framework](https://genode.org/), that explores new architectural designs and resource management strategies for many-core systems with heterogeneous computing and memory resources. It is a reference implementation of the [MxKernel](https://mxkernel.org/) architecture.

## MxKernel Architecture
The MxKernel is a new operating system architecture inspired by many-core operating systems, such as [FOS](https://dl.acm.org/doi/abs/10.1145/1531793.1531805) and [Tesselation](https://www.usenix.org/event/hotpar09/tech/full_papers/liu/liu_html/), as well as hypervisors, exokernels and unikernels.
Novel approaches of the MxKernel include the use of tasks, short-lived closed units of work, instead of threads as control-flow abstraction, and the concept of elastic cells as process abstraction. The architecture has first been described in the paper [MxKernel: Rethinking Operating System Architecture for Many-core Hardware](https://sites.google.com/site/sfma2019eurosys/Program/sfma-mxkernel.pdf?attredirects=0) presented at the [9th Workshop on Systems for Multi-core and Heterogeneous Architectures](https://sites.google.com/site/sfma2019eurosys/). 

## Task-based programming
EalánOS promotes task-parallel programming by including the [MxTasking](https://github.com/jmuehlig/mxtasking.git) task-parallel runtime library. MxTasking improves on the common task-parallel programming paradigm by allowing tasks to be annotated with hints about the tasks behavior, such as memory accesses. These annotations are used by the runtime environment to implement advanced features, like automatic prefetching of data and automatic synchronization of concurrent memory accesses.

## Documentation
Because EalánOS is based on Genode, the primary documentation, for now, can be found in the book [Genode Foundations](https://genode.org/documentation/genode-foundations-22-05.pdf).

## Features added to Genode
EalánOS extends the Genode OS framework by functionality needed and helpful for many-core systems with non-uniform memory access (NUMA), such as
- A topology service that allows to query NUMA information from within a Genode component.
- A port of [MxTasking](https://github.com/jmuehlig/mxtasking.git), a task-based framework designed to aid in developing parallel applications.
- (WiP) A extension of Genode's RAM service that enables applications to allocate memory from a specific NUMA region, similar to libnuma's `numa_alloc_on_node`, and thus improve NUMA-locality of internal data objects.
- (WiP) An interface for using Hardware Performance Monitoring Counters inside Genode components. Currently, performance counters are only implemented for AMD's Zen1 microarchitecture.

### Acknowledgement
The work on EalánOS and the MxKernel architecture is supported by the German Research Foundation (DFG) as part of the priority program 2037 "[Scalable Data Management on Future Hardware](https://dfg-spp2037.de/)" under Grant numbers SP968/9-1 and SP968/9-2. 
The MxTasking framework is developed as part of the same DFG project at the [DBIS group at TU Dortmund Universitiy](http://dbis.cs.tu-dortmund.de/cms/de/home/index.html)  and funded under Grant numbers TE1117/2-1.