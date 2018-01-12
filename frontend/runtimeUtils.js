
var ssh = require('./sshUtils');
var config = require('./config');
var log = require('./logger');

var runtimes = {};

/** Object wrapper for a runtime connection */
var Runtime = function(id, host) {
    this.id = id;
    this.host = host;
    this.attempted_start = false;
    ssh.add_connection(id, host, config);
    if (id in runtimes) {
        log.warn(`Replacing runtime ${id}`);
    } else {
        log.debug(`Initialized connection to runtime ${id}`);
    }
};

Runtime.prototype.start = function(json_file, on_stop) {
    if (this.attempted_start) {
        log.warn(`Already attempted to start rt ${this.id}. Not starting again...`);
        return null;
    }
    return new Promise((resolve, reject) => {
        if (this.attempted_start) {
            log.warn(`Already attempted to start rt ${this.id}. Not starting again...`);
            // Not sure what happens if i don't resolve or reject... Imma try anyway.
            return;
        }
        log.info(`Attempting to start rt ${this.id}`);
        this.attempted_start = true;
        ssh.start(this.id, config.runtime_cmd(json_file, this.id), () => {
            log.warn(`Runtime ${this.id} stopped`);
            this.attempted_start = false;
            on_stop(this.id);
        }).then(
            (output) => {
                log.info(`Runtime ${this.id} started with line ${output}`);
                resolve();
            },
            (err) => {
                log.error(`Could not start runtime ${this.id}: ${err}`);
                reject(err);
            }
        );
    });
};

Runtime.prototype.kill = function() {
    return new Promise((resolve, reject) => {
        ssh.kill(this.id, config.rt_exec).then( () => {
            log.info(`Killed runtime ${this.id}`);
        }, (err) => {
            log.warn(`Error killing runtime ${this.id}:: ${err}`);
        });
    });
}

Runtime.prototype.isRunning = function() {
    return new Promise((resolve, rejectt) => {
        ssh.isRunning(this.id, config.rt_exec).then( (rtn) => {
            log.debug(`Rt ${this.id} is running? ${rtn}`);
            resolve(rtn);
        }, (err) => {}
        );
    });
}

/**
 * Initializes all runtimes in the DFG
 * @param rts DFG.runtimes
 */
var initRuntimes = function(rts) {
    for (var i = 0; i < rts.length; i++) {
        var rt= rts[i];
        runtimes[rt['id']] = new Runtime(rt['id'], rt['ip']);
    }
}

/**
 * Starts all runtimes that have not yet been started
 * @param rts DFG.runtimes
 * @param json_file DFG Json file to use as argument when starting runtimes
 * @param on_stop Callback to call when runtime stops
 * @return Promise resolves if all runtimes complete, rejects(id) if runtime `id` errors
 */
var startIdleRuntimes = function(rts, json_file, on_stop) {
    var promises = [];
    for (var i = 0; i < rts.length; i++) {
        var rt = rts[i];
        if (rt['status'] != 'Connected') {
            if (!(rt['id'] in runtimes)) {
                log.warn(`Initializing rt ${rt.id} on the fly`);
                runtimes[rt.id] = Runtime(rt.id, rt.ip);
            }
            var promise = runtimes[rt['id']].start(json_file, on_stop);
            if (promise != null) {
                promises.push(promise);
            }
        }
    }
    return Promise.all(promises);
}

/** Stops all initilized runtimes */
var stopAllRuntimes = function() {
    for (id in runtimes) {
        if (!runtimes.hasOwnProperty(id)) {
            continue;
        }
        runtimes[id].kill();
    }
}

module.exports = {
    initAll: initRuntimes,
    startIdle: startIdleRuntimes,
    stopAll: stopAllRuntimes
};

