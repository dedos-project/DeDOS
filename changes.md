# Functionality changes

* Separating the main thread into two threads:
1) blocking epoll thread, handling reading from sockets
2) Main thread handling thread queue, waits on semaphore
 * This will stop the main thread from exhausting 100% of a cpu
 * Other runtimes can send directly to a worker thread, avoiding main thread involvement

* Make DFG de-facto truth for runtime configuration.
 * Changes are not made to the runtime DFG until they are actually implemented
   * (e.g. MSUs are not added until MSU addition occurs succesfully)
   * Not 100% sure how to do so while avoiding locks.
     * DFG modification happens through main thread?
 * Global controller should be able to poll part of runtime DFG (hashsum?) to detect differences
   * Have not thought this part out fully


# Important bugfixes

* Routes should be initialized with a type ID for the route, in addition to a route ID
  * Necessary when referencing route by type from MSUs.
  * If not explicitly defined, needs ugly workaround
* Stats sampling has to be looked at in both global controller and runtime.
  * global controller reports of missing statistics occasionally. This needs to be checked.
  * Stat sampling in runtime is likely expensive. Hard to tell over 100% CPU utilization though.
* Ensure Makefile compiles with optimization. This should fix the `log_custom()` problem
* Webserver error handling needs to be improved, esp. in the case of lack of file descriptors
  * Need more complex handling when failing to access database
  * Need better state freeing on error

* Error with blocking_socket_handler not passing thread semaphore

# New features

* Timestamp MSU_queue_item upon entry into system. Allow within-MSU (or global?) timeouts
  * global timeout could be set in DFG
* MSU hook for clearing states
* A way for an MSU to signal to all other MSUs to clear a particular state
* State management based on 2-tuple of (msg_key, delivery_route_id)
* Tracking msu msg lineage
  * This becomes especially important with respect to state transfer.
    Transfers must occur on per-route basis.
* Flag in MSU to signal default execution as:
  * `on_message_only`: Execute only when a message is delivered to that MSU
  * `on_timeout`: Execute when message is delivered or at interval
  * `once`: Execute once after dedos has started up, only on message delivery afterwards
  * `always`: Execute every chance it gets
* At request of Max, Tavish: Change protocol for enqueing to next MSU. Call a function within
  the msu to enqueue (`msu_enqueue(local_msu *src, int msu_type)`)
* Thread deletion (?). Neets an exit signal in the thread structure
* (low priority) Runtime receives initial DFG from global controller, not input argument

# Configuration changes

* Addition and removal of a few types from the DFG
 * Adding: msu_type, holding information invariant across msu instances
 * Removing: statistics, in favor of storing locally in runtime and controller (different formats)
 * Change: vertex_type to bitwise option so MSU can be both ENTRY and EXIT
 * Change: references to thread_id into references to actual thread object

* Addition of msu_type initialization function
 * Allow structures to be set up once for entire runtime
   * (e.g. OpenSSL structures are set up when ssl_read MSU type is initialized)
 * (Could easily put this in individual MSU initialization function, with checks so that it only
    happens on first initialization, but I think that msu_type initialization is cleaner)

* Registering logged items with statistics module to avoid preallocation
 * MSUs, threads, register their ID with the stats reporting module, listing appropriate stats

* Routing module should use preallocation rather than dynamic allocation
 * Should look up routes based on ID table, avoiding using UT_hash

* MSUs should receive init_data as a string
 * init_data should not be used for "meta" information (blocking/thread_num)

* Messages are delivered to thread by ID. Main thread has ID of 0.
  (All threads, including main, are `dedos_thread`s.

# Code Changes

## Moving
* Separation of local MSU from MSU type. Track MSU Types in msu_type.[ch]
* Move epoll_ops (or something similar) into main runtime codebase.
  May need to modify a bit, but we need a standard way for messages to enter into Dedos.
  Might be useful for managing main thread sockets as well
* I would like to move c and header files to the same directory.
  The makefile can easily generate an `include` directory with only headers.
* Separation of `thread_msg` from `control_msg`

## Renaming
* generic_msu -> local_msu
* Remove 'generic' from everywhere
* Remove 'dedos' from most places (exception: threads, other conflicts with standard libs)
* FORWARD_DATA replaced with SEND_TO_RUNTIME, SEND_TO_CONTROLLER
* Prefix most/all core dfg structures with `dfg` to distinguish from more specific types

## Refactoring
* Make queueing generic, shared across different types of queues
  * Functions such as `dequeue_control_msg()` or `dequeue_msu_msg()` act on generic queue,
    but return their specific type of message
* Remove all global variables from headers
  (If a variable should needs to be global, keep in `static` in .c file with accessor) 
* Remove pointers from "serializable" structures that specify size.
  If a structure has a payload, the payload will follow the structure,
  otherwise the pointer is just extra overhead.
* Remove stat sample from stat_item structure in runtime

## Directory Structure
```
├── common:     Common code between runtime and controller. No longer has its own Makefile.
│   ├── logging.h
│   ├── dfg.c ----  Dedos DFG stuff. Nothing specific to runtime or controller
│   ├── dfg.h
│   ├── dfg_reader.c ------ Reading DFG from JSON
│   ├── dfg_reader.h
│   ├── dfg_writer.c ------ Writing DFG (or portion of DFG) to JSON
│   ├── dfg_writer.h
│   ├── jsmn_parser.c ----- Generic JSON reading
│   ├── jsmn_parser.h
│   ├── control_msg.c ----- Building/serializing/deserializing messages between controller + rt
│   ├── control_msg.h
│   ├── timeseries.c ------ Generic timestamped circular buffer
│   └── timeseries.h
├── global_controller
│   ├── Makefile
│   └── src
│       ├── common -> ../../common/ --- Symlink to top-level common folder.
│       └── controller
│           ├── main.c -------- Should really only contain parsing of args
│           ├── api.c --------- core API: creating/deleting MSUs/routes/runtimes
│           ├── api.h
│           ├── cli.c --------- CLI API: Becoming less necessary at the moment
│           ├── cli.h
│           ├── controller_dfg.c -- Management of and interface to DFG from controller
│           ├── controller_dfg.h
│           ├── runtime_communication.c -- Handles communcation to and from runtimes
│           ├── runtime_communication.h
│           ├── scheduling.c --- Business logic of scheduler
│           └── scheduling.h
└── runtime
    ├── Makefile
    └── src
        ├── common -> ../../common/ --- Symlink to top-level common folder
        ├── legacy ------ Legacy apps ported into DeDoS
        ├── msus   ------ Holds code for individual MSUs
        │   ├── msu_1.c
        │   ├── msu_1.h
        │   ├── msu_2.c
        │   └── msu_2.h
        └── runtime
            ├── main.c --- Mostly parsing of args
            ├── communication.c ---- Generic socket communication
            ├── communication.h
            ├── controller_communication.c --- Socket communication with controller
            ├── controller_communication.h
            ├── runtime_communcation.c  ------ Socket communication with peer runtimes
            ├── runtime_communcation.h
            ├── dedos_threads.c ---- functions for starting/stopping/managing threads
            ├── dedos_threads.h
            ├── local_msu.c ----- Right now, `generic_msu.c`. Info about local MSUs.
            ├── local_msu.h
            ├── msu_type.c  ----- Right now also in `generic_msu.c`. MSU Type definition/management
            ├── msu_type.h
            ├── main_thread.c --- Code just for executing the main thread (now (sorta) `runtime.c`)
            ├── main_thread.h
            ├── worker_thread.c - Code for executing and managing worker threads
            ├── worker_thread.h
            ├── message_queue.c - Generic message queueing/dequeuing/allocation/management
            ├── message_queue.h
            ├── runtime_dfg.c  -- Management of and interface to DFG from runtime
            ├── runtime_dfg.h
            ├── stats.c  -------- Aggregation and sampling of statistics
            └── stats.h
```


# Questions

* What is "proto_number" in terms of MSUs?
* Could we use pthread_signal to easily interrupt blocking calls in MSUs that are marked blocking?
  * I have never been great at signals :( ...
* GET_INIT_CONFIG ?
* SET_DEDOS_RUNTIMES_LIST ?
* How does new runtime addition work?
  Does the controller tell the new runtime where to connect? Or the original one?
* Have we ever handled thread deletion?
* When is it worth it to specify the specific number of bits in int (uint32_t, e.g.)
  when you are not serializing
* generic_msu_queue seems to be doing some sort of manual struct-packing thing. Can we remove it?
* Why set REUSEPORT and REUSEADDR on a socket that is not being bound to a port?

