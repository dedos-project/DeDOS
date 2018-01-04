
var net = require('net');

var parseUtils = require('./parseUtils');
var log = require('./logger');
var config = require('./config');
var ssh = require(`./sshUtils`);
var zmq = require('zmq');

/** Whether the connection to the controller has been established */
var connected = false;
/** Whether the controller object has been initialized */
var controller_init = false;
/** The socket connection to the controller to receive DFG */
var dfg_socket;
/** The socket connection to the controller to send control commands */
var ctl_socket;



/** IP address of controller */
var controller_ip;

/** Callback for errors */
var error_cb;
/** Callback for closing connection */
var close_cb;

/** resets initialization flags */
var uninit = function() {
    controller_init = false;
    connected = false;
}

/** Object to hold exported functions */
var controller = {};

/**
 * Initializes controller SSH object and callbacks
 * @param ip IP address of controller to connect to
 * @param on_dfg(dfg) function to call when DFG is received from controller
 * @param on_error function to call if global controller errors out
 * @param on_close function to call when connection to gc is closed
 */
controller.init = function(ip, on_dfg, on_error, on_close) {
    if (controller_init) {
        log.warn(`Overwriting initialized Controller at ${controller_ip}`);
    }
    controller_ip = ip;

    ssh.add_connection("ctl", ip);

    var receive_buffer = '';
    var error_cnt = 0;
    dfg_socket = new net.Socket();

    // On receipt of data from the controller
    dfg_socket.on('data', function(data) {
        // Add to the received buffer, and attempt to parse
        receive_buffer += data;
        parseUtils.parseDfg(receive_buffer).then(
            // If parsing works, call the dfg callback
            (dfg) => {
                log.debug(`Received dfg of length ${data.length}`);
                receive_buffer = '';
                on_dfg(dfg);
                error_cnt = 0;
            },
            // Otherwise add to the received buffer
            (err) => {
                error_cnt+=1;
                if (error_cnt > 5) {
                    log.error("Can't recover from bad JSON");
                    receive_buffer = '';
                    error_cnt = 0;
                }
            }
        );
    });

    ctl_socket = zmq.socket('req');

    ctl_socket.on('data', function(data) {
        log.note(`Received ${data} from control socket`);
    });

    error_cb = on_error;
    close_cb = on_close;
}

/** When the connection to the controller closes */
var when_closed = function() {
    log.warn(`Connection to controller at ${controller_ip}:${config.gc_sock_port} was closed`);
    uninit();
    close_cb();
}

/** When the connection to the controller encounters an error */
var when_errored = function(err) {
    log.error(`Connection to controller errored:: ${err}`);
    uninit();
    error_cb(err);
}

/**
 * Connect to the global controller
 * @return Promise resolves on successful connection, rejects if cannot connect
 */
controller.connect = function() {
    // Try to connect to the control socket, but no worries if it can't happen
    ctl_socket.on('error', (e) => {
        log.warn(`Error on controller ctl socket: ${e}`);
    });
    try {
        var conn_str =`tcp://${controller_ip}:${config.gc_ctl_port}`;
        ctl_socket.connect(conn_str);
        log.info(`Attempted connect to ${conn_str}`);
    } catch(e) {
        log.warn(`Failed to connect to controller ctl socket ${e}`);
    }

    return new Promise( (resolve, reject) => {
        // Have to set the callback for errors to catch a failed connection, then overwrite later
        dfg_socket.on('error', (err) => {
            log.info(`Connection to controller failed: ${err}`);
            reject(err);
        });
        log.debug(`Attempting to connect to controller at ${controller_ip}`);
        dfg_socket.connect(config.gc_sock_port, controller_ip, () => {
            log.info(`Connected to controller at ${controller_ip}:${config.gc_sock_port}`);
            connected = true;
            dfg_socket.on('error', when_errored);
            dfg_socket.on('close', when_closed);
            resolve();
        });
    });
}

/** Closes the conncetion to the global controller */
controller.close_connection = function() {
    if (!controller_init) {
        log.warn("Controller not initialized. Cannot close");
        return;
    }
    dfg_socket.destroy();
    uninit();
}

controller.is_initialized = function() {
    return controller_init;
}

controller.is_connected = function() {
    return connected;
}

/**
 * Starts an initialized global controller
 * @param json_file DFG file to initialize controller with
 * @return Promise Resolves if attempt to start controller succeeded, otherwise rejects
 */
controller.start = function(json_file) {
    return new Promise( (resolve, reject) => {
        ssh.start("ctl", config.controller_cmd(json_file), () => {
                log.warn("Controller stopped");
                uninit();
            }).then(
            (output) => {
                log.info(`Controller started with line: ${output}`);
                resolve();
            },
            (err) => {
                log.error(`Could not start controller: ${err}`);
                reject(err);
            }
        );
    });
}

/**
 * Stops the global controller
 * @return Promise Resolves if command to kill the controller succeeded
 */
controller.kill = function() {
    return new Promise((resolve, reject) => {
        ssh.kill("ctl", config.gc_exec).then( () => {
            log.info("Killed controller");
            uninit();
        }, (err) => {
            log.warn(`Error killing controller: ${err}`);
        });
    });
}

controller.send_cmd = function(cmd) {
    return new Promise((resolve, reject) => {
        ctl_socket.send(`${cmd}`, null, () => {
            log.debug(`Wrote: '${cmd}'`);
            resolve(cmd);
        });
    });
}

controller.clone_msu = function(msu_id) {
    return new Promise((resolve, reject) => {
        this.send_cmd(`clone msu ${msu_id}`).then(
            () => {resolve(msu_id);},
            () => {reject(msu_id);});
    });
}

controller.unclone_msu = function(msu_id) {
    return new Promise((resolve, reject) => {
        this.send_cmd(`unclone msu ${msu_id}`).then(
            () => {resolve(msu_id);},
            () => {reject(msu_id)});
    });
}

module.exports = controller;
