var config = {};

///////////////
// SSH CONFIGURATION
//////////////

/** Username for SSH access to remote nodes */
config.user_name = "iped";
/** SSH key to access remote nodes without password */
config.ssh_key = `/home/${config.user_name}/.ssh/id_rsa_internal`
/** Port for SSH access to remote nodes */
config.ssh_port = 2324;


/////////////
// NODE.JS CONFIGURATION
////////////

/** Port on which node.js listens for connections */
config.listen_port = 8000;
/** Local directory where DFGs are stored */
config.local_dfg_dir = './dfgs/';


/////////////
// DEDOS CONFIGURATION
////////////

/** Base directory used in subsequent directories */
config.remote_basedir = '/home/' + config.user_name + '/nfs/';
/** Directory to which DFGs are uploaded */
config.remote_dfg_dir = `${config.remote_basedir}/dedos_dfgs/`;
/** Directory containing DeDOS executables */
config.dedos_dir = `${config.remote_basedir}/Dedos/`;

/** Name of global controller executable */
config.gc_exec = 'global_controller';
/** Port on which global controller broadcasts DFG */
config.gc_sock_port = 10000;
/** Port on which control commands can be sent to controller */
config.gc_ctl_port = 4321;

/**
 * Command to start global controller
 * @param json_file DFG file used to start the controller
 * @return string the command to start the controller
 */
config.controller_cmd = function(json_file) {
    return `${this.dedos_dir}/${this.gc_exec} -j ${this.remote_dfg_dir}/${json_file} ` +
           `-p ${this.gc_sock_port} -c ${this.gc_ctl_port} --init-db`;
}

/** Name of runtime executable */
config.rt_exec = 'rt';

/**
 * Command to start runtime
 * @param json_file DFG file used to start the runtime
 * @param rt_id ID of the runtime to start
 * @return string the command to start the runtime
 */
config.runtime_cmd = function(json_file, rt_id) {
    return `${this.dedos_dir}/${this.rt_exec} -j ${this.remote_dfg_dir}/${json_file} -i ${rt_id}`;
}

module.exports = config;
