# Dedos Runtime

## Creating new MSUs

Creation of an MSU requires modifications in three places.

First, in `src/include/dedos_msu_list.h`, choose an ID for your MSU

Second, create a header file in `src/include/modules/` and a c/c++ file in `src/modules`. The header file should contain at least the following line:

```c
#include "generic_msu.h"

struct msu_type <MSU_TYPE>
```

The .c file must define a function that receives data from other msus, with the signature:
```c
int <something>_receive(struct generic_msu *msu, struct generic_msu_queue_item *queue_item);
```

and then must define the `msu_type` declared in the header:

```c
/**
 * All msus of this type ontain a reference to this object
 */
struct msu_type <MSU_TYPE> = {
    .name="<msu type>",
    .layer=DEDOS_LAYER_APPLICATION,
    .type_id=<MSU_TYPE>_MSU_ID, //  This is the IS defined in dedos_msu_list.h
    .proto_number=0,
    .init=NULL,                   // Type-specific allocation of additional variables
    .destroy=NULL                 // Freeing of variables allocated in init
    .receive=<something>_receive, // Reception of messages from other MSUs
    .receive_ctrl=NULL            // Reception of special messages from global controller
    .route=round_robin,           // Which of these MSUs receives input from sending MSUs
    .send_local=default_send_local,   // How to send queue items to local MSUs
    .send_remote=default_send_remote, // How to send queue items to remote MSUs
    .deserialize=default_deserialize  // How to deserialize messages sent from remote MSUs
};
```

(For a full description and signatures for each field in `msu_type`, look in 
[generic\_msu.h](http://dedos.gitlab.io/Dedos/runtime/generic__msu_8h.html))

Finally, in `src/stack/msu_tracker.c`, add your `<MSU_TYPE>` variable to the `MSU_TYPES[]` array, 
and update `N_MSU_TYPES` to reflect the new total number of msu types.

## Configuring and running

### Starting the runtime

To start the runtime, enter:

`./rt  [-j dfg.json -i runtime_id] | [-g global_ctl_ip -p global_ctl_port -P local_listen_port [--same-machine | -s]] -w webserver_port [--db-ip db_ip --db-port db_port --db-load db_max_load]`

Either the JSON configuration flags ( `-j` & `-i` ) or the manual configuration flags ( `-g -p -P [-s]`) must be provided.

In contrast to before, the `--same-machine` flag now takes no arguments. When provided, it implies that the runtime is running on the same machine as the global controller. That flag is unnecessary when starting using the JSON. 

### Configuration files

To create a new JSON configuration file, suitable for use with the global controller or runtime, use the ptyhon script `dfg_from_yml.py`.

To get access to the relevant `pyyaml` library, include the line:
```
export PATH="/media/sdb1/nfs/shared/miniconda2/bin:$PATH"
```
in your `~/.bashrc` file

`dfg_from_yml.py` accepts one flag (`-p`), which pretty-prints the output, and an argument specifying the location of the yml file to parse.

If you are not interested in pre-loading MSUs, you can use the empty.yml file to generate the corresponding json. 

Simply change the text around lines 56-64:
```yml
runtimes:
    1: &rt1
        <<: *dedos01 # Change this line when appropriate
        <<: *default_runtime
        runtime_id: 1
    2: &rt2
        <<: *dedos02 # Change this line when appropriate
        <<: *default_runtime
        runtime_id: 2
```
to specify the actual machines (dedos01-04) that you are using.

If you want to pre-load an MSU configuration, you can copy a file such as `ssl-flood.yml` and modify the `msus` and `routes` list appropriately.

```yml
msus:
    - name: ssl-route-1 # Name your msu something human-readable
      <<: *rt1-msu      # Specify the runtime 
      <<: *ssl-routing  # Specify the msu type
      thread: 2         # Thread number on which the msu is running
      reps: 1           # Number of repetitions of this MSU
                        # each repetition runs on the subsequent thread
                        # (i.e. replica 3 will run on thread 4, in this case)
```

```yml
routes:
    - from: [ssl-route-1, ssl-route-2] # An MSU or list of source MSUs
      to: ssl-read-1                   # An MSU or list of destination MSUs
```

To create the final configuration file:

`python dfg_from_yml.py ssl-flood.yml > ssl_flood.json`

If you are not pre-loading MSUs, you can also use

`python cfg_from_yml.py ssl-flood.yml > ssl_flood.cfg`

to create a config file suitable for loading with loadcfg from the global controller
