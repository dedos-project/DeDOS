var fs = require('fs');
var ssh2Client = require('ssh2').Client;
var path = require('path');
var node_ssh = require('node-ssh');
var log = require('./logger');
var config = require('./config');

var connections = {};

/** Object wrapper for an SSH connection */
var Connection = function(host, username, key, port) {
    this.host = host;
    this.username = username;
    this.key = key;
    this.port = port;
    this.ssh = new node_ssh();
    log.debug(`Created new connection object for host ${host}`);
}

Connection.prototype.connect = function() {
    log.info(`connecting to ${this.username}@${this.host}`);
    return this.ssh.connect({
        host: this.host,
        username: this.username,
        privateKey: this.key,
        port: this.port
    });
}

/**
 * Puts the file in `src` on localhost into `dst` on this connection's node
 * @returns Promise resolves on success, rejects(err) on error
 */
Connection.prototype.put = function(src, dst) {
    var ssh = this.ssh;
    var host = this.host;

    return new Promise((resolve, reject) => {
        ssh.putFile(src, dst).then(() => {
            log.info(`Put file ${src} into ${host}:${dst}`);
            resolve();
        }, (err) => {
            log.error(`Failed to put file ${src} into ${host}:${dst} :: ${err}`);
            reject(err);
        });
    });
}

/**
 * Runs cmd on this connection's node. Does not return promise until execution finishes
 * @return Promise resolves(stdout) on success, rejects(err) on error
 */
Connection.prototype.run = function(cmd) {
    var ssh = this.ssh;
    var host = this.host;

    return new Promise((resolve, reject) => {
        this.connect().then(function() {
            ssh.execCommand(cmd).then( 
                (result) => {
                    log.debug(`Received ${result.stdout} from ${cmd}`);
                    resolve(result.stdout);
                },
                (err) => {
                    log.warn(`Command ${cmd} errored: ${err}`);
                    reject(err);
                }
            );
        }, reject);
    });
}

/**
 * Starts cmd on this node. Promise returns immediately if connection succeeded.
 * @param cmd Command to execute
 * @param on_complete Callback(stdout) at end of execution
 * @return Promise resolves() on successful start, rejects(err) on error to start
 */
Connection.prototype.start = function(cmd, on_complete) {
    var connection = this;

    var split_cmd = cmd.split(" ");

    // Have to resort to non-promise-based ssh2Client to return immediately
    var client = new ssh2Client();

    return new Promise((resolve, reject) => {
        client.on('ready', () => {
            client.exec(cmd, (err, stream) => {
                if (err) {
                    reject(err);
                } else {
                    var data = '';
                    stream.on('data', (dat) => {
                        data += dat;
                    });
                    stream.on('exit', (code) => {
                        log.info(`Command ${cmd} ended with code ${code}`);
                        if (code != 0) {
                            log.debug(`Output was: ${data}`);
                        }
                        on_complete(code);
                    });
                    resolve();
                }
            });
        }).connect({
            host: connection.host,
            username: connection.username,
            port: connection.port,
            privateKey: fs.readFileSync(connection.key)
        });
    });
}

/** Makes a directory on a remote host */
Connection.prototype.mkdir = function(path) {
    var ssh = this.ssh;
    var host = this.host;

    return new Promise((resolve, reject) => {
        this.connect().then(() => {
            log.debug(`Attempting to make directory ${path} on ${host}`);
            ssh.mkdir(path).then(resolve, reject);
        }, reject);
    });
}

var ssh_utils = {};

ssh_utils.clear_connections = function() {
    connections = [];
}

/** Adds a new connection object which can be subsequently referenced by its ID */
ssh_utils.add_connection = function(id, host) {
    log.info(`Adding connection ${id}: ${host}`);
    if (id in connections) {
        log.warn(`Overwriting connection ${id}`);
    }
    connections[id] = new Connection(host, config.user_name, config.ssh_key, config.ssh_port);
}

ssh_utils.has_connection = function(id) {
    return (id in connections);
}

/**
 * Planned to use this to check if all nodes in DFG could be connected to on DFG upload
 * Never used at the moment
 */
ssh_utils.test_connection = function(id) {
    return new Promise(function(resolve, reject) {
        log.debug(`Testing connection ${id}`);
        if (!this.has_connection(id)) {
            log.warn(`Connection ${id} does not exist`);
            reject("DNE");
            return;
        }
        connections[id].connect().then(resolve, reject);
    });
}

/**
 * Calls test_connection for all connections
 * @return Promise resolves if all connections succeeded, rejects(id) otherwise
 */
ssh_utils.test_all_connections = function() {
    var tests = [];
    for (var cid in connections) {
        if (!connections.hasOwnProperty(cid)) {
            continue;
        }
        tests.push(new Promise((resolve, reject) => {
            var p_cid = cid;
            connections[p_cid].connect().then( function() {
                log.info(`Successfully connected to ${p_cid}`);
                resolve(p_cid);
            }, function() {
                log.warn(`Failed to connect to ${p_cid}`);
                reject(p_cid);
            })
        }));
    }
    return Promise.all(tests);
}

/**
 * Wrapper utility to execute a command on a given host
 * @return Promise resolve(output) at end of command execution, rejects(err) on error
 */
ssh_utils.exec = function(host_id, cmd) {
    return new Promise((resolve, reject) => {
        log.debug(`Attempting ${cmd} on ${host_id}`);
        if (!this.has_connection(host_id)) {
            log.warn(`Connection ${host_id} does not exist`);
            reject("DNE");
            return;
        }
        connections[host_id].run(cmd).then( (output) => {
            log.debug(`Got output ${output} from ${cmd}`);
            resolve(output);
        }, (err) => {
            log.debug(`Errored output ${err} from ${cmd}`);
            reject(err);
        });
    });
}

/**
 * Wrapper utility to start a command on a host
 * @return Promise resolves immediately if command succeded, rejects(err) on error
 */
ssh_utils.start = function(host_id, cmd, on_complete) {
    return new Promise((resolve, reject) => {
        log.debug(`Attempting start ${cmd} on ${host_id}`);
        if (!this.has_connection(host_id)) {
            log.warn(`Connection ${host_id} does not exist`);
            reject("DNE");
            return;
        }
        connections[host_id].start(cmd, on_complete).then( (output) => {
            log.debug(`Got output ${output} from ${cmd}`);
            resolve(output);
        }, (err) => {
            log.warn(`${cmd} errored: ${err}`);
            reject(err);
        });
    });
}

/**
 * Wrapper utility to stop a command (using pkill -f)
 */
ssh_utils.kill = function(host_id, cmd) {
    return new Promise((resolve, reject) => {
        log.debug(`Attempting to keill ${cmd} on ${host_id}`);
        this.exec(host_id, `pkill -f ${cmd}`).then((msg) => {
            log.info(`Killed ${cmd} on ${host_id}`);
            resolve();
        }, (err) => {
            log.warn(`Error killing ${cmd} on ${host_id}:: ${err}`);
            reject(err);
        });
    });
}

/**
 * Wrapper utility to check if a command is running on a given host
 * @return Promise resolve(X), where X is true if command is running. rejects(err) on error
 */
ssh_utils.isRunning = function(host_id, cmd) {
    return new Promise((resolve, reject) => {
        this.exec(host_id, `pgrep ${cmd}`).then((msg) => {
            if (msg == "") {
                resolve(false);
            } else {
                resolve(true);
            }
        }, (err) => {
            log.warn(`Error determining status of ${cmd} on ${host_id}:: ${err}`);
            reject(err);
        });
    });
}

/**
 * Broadcasts a file from src on localhost to dst on all initialized ssh connections
 * @return Promise resolves() if all uploads succeeded, rejects(err) otherwise
 */
ssh_utils.broadcastFile = function(src, dst) {
    var promises = [];
    for (var cid in connections) {
        if (!connections.hasOwnProperty(cid)) {
            // Not a real connection, a property of the prototype
            continue;
        }
        // Try to put the file regardless of if mkdir succeeds
        promises.push(function(tmp_cid) {
            return new Promise((resolve, reject) => {
                var conn = connections[tmp_cid];
                conn.mkdir(path.dirname(dst)).then(
                    () => {conn.put(src, dst).then(resolve, reject)},
                    () => {conn.put(src, dst).then(resolve, reject)});
            });
        }(cid));
    }

    return new Promise((resolve, reject) => {
        Promise.all(promises).then( () => {
            log.info(`Successfully broadcasted ${src}`);
            resolve();
        }, (err) => {
            log.error(`Failed to broadcast ${src}: ${err}`);
            reject(err);
        });
    });
}


module.exports = ssh_utils;
