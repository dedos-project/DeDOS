var http = require('http');
var url = require('url');
var fs = require('fs');
var io = require('socket.io');

var ssh = require('./sshUtils');
var config = require('./config');
var parseUtils = require('./parseUtils');
var sysUtils = require('./sysUtils');
var log = require('./logger');
var rt = require('./runtimeUtils');
var controller = require('./controllerUtils');

var global_dfg_filename;
var global_dfg;
var dfg_initialized = false;

log.info("----[RUNNING]----");

/*
 * return response according to different request
 */
var server = http.createServer(function(request, response){
    var path = url.parse(request.url).pathname;
    if (path == '/') {
        fs.readFile(__dirname + "/index.html", function(err, data){
            sysUtils.getResponse(response,err,data,'html');
        });
    } else if (path == '/index2.html') {
        fs.readFile(__dirname + '/index2.html', function(err, data) {
            sysUtils.getResponse(response, err, data, 'html');
        })
    } else if(path.indexOf('.jpg') != -1) {
        fs.readFile(__dirname + "/src/img/" + path,function(err,data){
            sysUtils.getResponse(response,err,data,'jpg');
        });
    } else if(path.indexOf('.css') != -1) {
        fs.readFile(__dirname + "/src/css/" + path,function(err,data){
            sysUtils.getResponse(response,err,data,'css');
        });
    } else if(path.indexOf('.png') != -1) {
        fs.readFile(__dirname + "/src/img/" + path,function(err,data){
            sysUtils.getResponse(response,err,data,'png');
        });
    } else if(path.indexOf('.js') != -1) {
        fs.readFile(__dirname + "/src/js/" + path,function(err,data){
            sysUtils.getResponse(response,err,data,'js');
        });
    } else {
        response.writeHead(404);
        response.write("opps this doesn't exist - 404");
        response.end();
    }
});

/** Sends a server to a listening front-end and logs the message */
var emit_error = function(socket, msg) {
    log.error(`Emitting error: ${msg}`);
    socket.emit('error_msg', String(msg));
}

/** A list of all currently-listening front-end sockets */
var listening_sockets = []

/** Broadcasts the DFG to all listening sockets */
var broadcast_dfg = function(dfg, filename) {
    for (var i = 0; i < listening_sockets.length; i++) {
        sysUtils.sendDfg(dfg, listening_sockets[i]);
    }
    log.debug(`Broadcast DFG to ${listening_sockets.length} listeners`);
}

/** Broadcasts a message/object to all listening sockets */
var broadcast_to_sockets = function(label, obj) {
    for (var i = 0; i < listening_sockets.length; i++) {
        listening_sockets[i].emit(label, obj);
    }
    log.debug(`Broadcast ${label} to ${listening_sockets.length} listeners`);
}

/*
 * Server starts here
 */

server.listen(config.listen_port);

var listener = io.listen(server);

listener.sockets.on('connection', function(socket) {

    //operation for each connection
    log.note("New connection!");

    // If the DFG is already initialized, forward it to the
    // newly connected socket
    if (dfg_initialized) {
        log.debug("Sending new connection the instantiated DFG");
        socket.emit('rt_info', global_dfg.runtimes);
        sysUtils.sendDfg(global_dfg, socket, true);
    }

    listening_sockets.push(socket)

    /** Run on clicking 'load file' in website */
    socket.on('load-file',function(){
        log.debug(`Attempting to locate files in ${config.local_dfg_dir}`);
        fs.readdir(config.local_dfg_dir, 
            (err, files) => {
                if (err) {
                    log.error(`Error reading from directory ${config.local_dfg_dir}`);
                    emit_error(socket, `Error finding DFGs. Check config.`);
                } else {
                    log.debug(`Found files: ${files}`);
                    json = files.filter((file) => file.endsWith('.json'));
                    socket.emit('dfg_files',json);
                }
            }
        );
    });

    /** Attempt to start and connect to the global controller */
    var start_controller = function(filename) {
        log.debug("Attempting to start controller");
        controller.start(filename).then( () => {
            log.info("Controller started I think! Waiting to test connection for 2 seconds");
            setTimeout(() => {
                if (!controller.is_connected()) {
                    log.error(`No connection from controller!`);
                    emit_error(socket, "No connection from controller");
                }
            }, 2000);
        }, (err) => {
            log.error(`Error starting controller!`);
            emit_error(socket, `Error when starting controller! Check node.js console for details`);
        });
    }

    /** Notifies listening sockets that a runtime has stopped */
    var notify_stopped_runtime = function(rt_id) {
        log.note(`Runtime ${rt_id} has stopped`);
        broadcast_to_sockets('stop-finish', rt_id);
        if (controller.is_connected())
            broadcast_to_sockets('error_msg', `Runtime ${rt_id} has stopped`);
    }

    /** Notifies listening sockets that all runtimes have stopped */
    var notify_stopped_controller = function() {
        broadcast_to_sockets('stop-finish', -1);
    }

    /** Attempts to start all idle runtimes. Does nothing if already started */
    var ensure_runtimes_started = function(rts, filename) {
        var promise = rt.startIdle(rts, filename, notify_stopped_runtime).then(
            () => { /* Do nothing on success */ },
            (err) => {
                log.error(`Error starting runtime ${err}`);
            }
        );
    }

    /** When the user gives a DFG file to be parsed */
    socket.on('dfg_file', function(filename) {
        if (dfg_initialized) {
            log.warn(`DFG already initialized to ${global_dfg.app_name}. Overwriting`);
        }

        log.debug(`Selected DFG file: ${filename}`);
        var local_path = `${config.local_dfg_dir}/${filename}`;
        var dfg_content = fs.readFileSync(local_path).toString();

        if (dfg_content.length == 0) {
            var err = `Selected file ${local_path} is empty`;
            log.error(err);
            emit_error(socket, err);
            return;
        }

        parseUtils.parseDfg(dfg_content).then(
            // On the successful parsing of a DFG
            (dfg) => {
                dfg_initialized = true;
                global_dfg = dfg;
                global_dfg_filename = filename;

                broadcast_to_sockets('launching', dfg.app_name);
                broadcast_to_sockets('rt_info', dfg.runtimes);
                broadcast_dfg(dfg, filename, true)

                // Initialize the runtime objects
                // (does not start the runtimes)
                rt.initAll(dfg.runtimes);

                // Initialize the controller object
                // (doesn't start the controller, just assigns callbacks)
                controller.init(dfg.controller['ip'],
                    // On receipt of a new DFG
                    (new_dfg) => {
                        if (JSON.stringify(new_dfg) == JSON.stringify(global_dfg)) {
                            log.debug("No DFG diff")
                        } else {
                            ensure_runtimes_started(new_dfg.runtimes, filename);
                            broadcast_dfg(new_dfg, filename, false);
                            global_dfg = new_dfg;
                        }
                    },
                    // If the connection to the controller errors out
                    (err) => {
                        log.error("OH NO!");
                        emit_error(socket, err);
                    },
                    // When the controller shuts down
                    () => {
                        log.error("Controller connection closed");
                        emit_error(socket, 'Controller shut down');
                        notify_stopped_controller();
                    }
                );

                // Broadcast the DFG to all remotes mentioned in the DFG
                // (initialization of runtimes and controller adds to ssh registry)
                ssh.broadcastFile(local_path, `${config.remote_dfg_dir}/${filename}`).then(
                    () => {
                        log.debug("DFG broadcasted");
                        setTimeout( () => {
                            if (controller.is_connected()) {
                                emit_error(socket, "Connected to running DeDOS instance");
                            }
                        },1500);
                    },
                    (err) => {
                        emit_error(socket, `Error broadcasting DFG: ${err}`);
                    }
                );
            },
            // If the DFG failed to parse
            (err) => {
                log.error(`Error parsing dfg: ${err}, ${err.stack}`);
                emit_error(socket, err);
            }
        ).then(
            () => { /* Success, do nothing */ },
            (err) => {
                log.error(`Got error: ${err}, ${err.stack}`);
                emit_error(socket, err);
            }
        );
    });

    socket.on('start', () => {
        log.note("--- STARTING DeDOS ---");
        start_controller(global_dfg_filename);
    });

    socket.on('stop',function(){
        controller.kill();
    });

    socket.on('reset',function(){
        controller.kill();
    });

    socket.on('disconnect',function() {
        console.log('disconnect');

        // Remove the current socket from the list of sockets to broadcast to
        var index = listening_sockets.indexOf(socket);
        if (index > -1) {
            listening_sockets.splice(index);
        }

        socket.disconnect('unauthorized');
    });

    socket.on('clone', function(msu_id) {
        log.debug(`Got: clone ${msu_id}`);
        if (controller.is_connected()) {
            controller.clone_msu(msu_id).then( () =>
                {log.note(`Cloned ${msu_id}`);}
            );
        } else {
            broadcast_to_sockets('error_msg', 'Cannot clone: No controller connection');
        }
    });

    socket.on('unclone', function(msu_id) {
        log.debug(`Got: unclone ${msu_id}`);
        if (controller.is_connected()) {
            controller.unclone_msu(msu_id).then( () => {
                log.note(`Uncloned: ${msu_id}`);
            });
        } else {
            broadcast_to_sockets('error_msg', 'Cannot remove: No controller connection');
        }
    });

});


