var http = require('http');
var url = require('url');
var fs = require('fs');
var io = require('socket.io');

var ssh = require('./sshUtils');
var config = require('./config');
var parseUtils = require('./parseUtils');
var clientUtils = require('./clientUtils');
var log = require('./logger');
var rt = require('./runtimeUtils');
var controller = require('./controllerUtils');


log.info("----[RUNNING]----");

var options = {
    key: fs.readFileSync('mycert.pem'),
    cert: fs.readFileSync('mycert.pem')
};

/**
 * append type of the file as response to and then respond the request
 * @param response: an object which wraps the response's information
 * @param err potential error
 * @param data the data to be respond
 * @param type the type of the response file, could be html or specific format of the image
 * @return null
 */
var getResponse = function(response,err,data,type){
    if (err) {
        response.writeHead(404);
        response.write("opps this doesn't exist - 404");
        response.end();
    } else {
        if (type == "js" || type == "css" || type == "html") {
            response.writeHead(200, {"Content-Type": "text/"+type});
        }
        else if(type=="jpg"){
            response.writeHead(200, {"Content-Type": "image/jpeg"});
        }
        else if(type=="png"){
            response.writeHead(200, {"Content-Type": "image/png"});
        }
        response.write(data, "utf8");
        response.end();
    }
}

/*
 * return response according to different request
 */
var server = http.createServer(function(request, response){
    var path = url.parse(request.url).pathname;
    if (path == '/') {
        fs.readFile(__dirname + "/index.html", function(err, data){
            getResponse(response,err,data,'html');
        });
    } else if (path == '/index2.html') {
        fs.readFile(__dirname + '/index2.html', function(err, data) {
            getResponse(response, err, data, 'html');
        })
    } else if(path.indexOf('.jpg') != -1) {
        fs.readFile(__dirname + "/src/img/" + path,function(err,data){
            getResponse(response,err,data,'jpg');
        });
    } else if(path.indexOf('.css') != -1) {
        fs.readFile(__dirname + "/src/css/" + path,function(err,data){
            getResponse(response,err,data,'css');
        });
    } else if(path.indexOf('.png') != -1) {
        fs.readFile(__dirname + "/src/img/" + path,function(err,data){
            getResponse(response,err,data,'png');
        });
    } else if(path.indexOf('.js') != -1) {
        fs.readFile(__dirname + "/src/js/" + path,function(err,data){
            getResponse(response,err,data,'js');
        });
    } else {
        response.writeHead(404);
        response.write("opps this doesn't exist - 404");
        response.end();
    }
});

var global_dfg_filename;
var global_dfg;
var dfg_initialized = false;

server.listen(config.listen_port);

var listener = io.listen(server);

function send_dfgs(client) {
    log.debug(`Attempting to locate files in ${config.local_dfg_dir}`);
    fs.readdir(config.local_dfg_dir, 
        (err, files) => {
            if (err) {
                log.error(`Error reading from directory ${config.local_dfg_dir}`);
                client.sendError(`Error finding DFGs. Check config.`);
            } else {
                log.debug(`Found files: ${files}`);
                json = files.filter((file) => file.endsWith('.json'));
                client.send('dfg_files',json);
            }
        }
    );
}

function start_controller(client, filename) {
    log.debug("Attempting to start controller");
    controller.start(filename, ()=> { clientUtils.broadcast('stopped', '')}).then( () => {
        log.info("I think I started the controller...");

        setTimeout( () => {
            if (!controller.is_connected()) {
                log.error('No connection from controller');
                log.info(client);
                client.sendError("Could not connect to controller");
                clientUtils.broadcast('stopped', ' ');
            } else {
                clientUtils.broadcast('started', ' ');
            }
        }, 2000);
    }, (err) => {
        log.error("Error starting controller!");
        client.sendError("Error starting controller! Check node output for details");
    });
}

/** Notifies listening sockets that a runtime has stopped */
function notify_runtime_stopped(runtime_id) {
    log.note(`Runtime ${runtime_id} has stopped`);
    if (controller.is_connected) {
        clientUtils.broadcastError(`Runtime ${runtime_id} has stopped`);
    }
}

/** Notifies listening sockets that controller has stopped */
function notify_stopped_controller() {
   clientUtils.broadcast('stopped', '');
} 

/** Attempts to start all idle runtimes. Does nothing if already started */
function start_idle_runtimes(rts, filename) {
    rt.startIdle(rts, filename, notify_runtime_stopped).then( 
        () => { /* Do nothing on success */ },
        (err) => {
            clientUtils.broadcastError(`Error starting runtime: ${err}`);
        }
    );
}

function init_dfg(client, filename) {
    if (dfg_initialized) {
        log.warn(`DFG already initialized to app ${global_dfg.app_name}. Overwriting`);
    }

    log.info(`Starting dedos with file ${filename}`);

    var local_path = `${config.local_dfg_dir}/${filename}`;
    var dfg_text = fs.readFileSync(local_path).toString();

    if (dfg_text.length == 0) {
        var err = `Selected file ${local_path} is empty`;
        log.error(err);
        client.sendError(err);
        return;
    }

    parseUtils.parseDfg(dfg_text).then(
        // On successful parsing
        (dfg) => {
            dfg_initialized = true;
            global_dfg = dfg;
            global_dfg_filename = filename;

            clientUtils.broadcastDfg(dfg, true);
            
            // Remove previous SSH connections to nodes
            ssh.clear_connections();

            // Initialize the runtime objects
            // (does not start the runtimes)
            rt.initAll(dfg.runtimes);

            // Initialize the controller object
            // (doesn't start the controller, just assigns callbacks)
            controller.init(dfg.controller['ip'],
                // On receipt of a new DFG
                (dfg) => {
                    start_idle_runtimes(dfg.runtimes, global_dfg_filename);
                    clientUtils.broadcastDfg(dfg);
                },
                // If the connection to the controller errors out
                (err) => {
                    log.error("OH NO!");
                    clientUtils.broadcastError(err);
                },
                // When the controller shuts down
                () => {
                    log.info("Controller connection closed");
                    clientUtils.broadcastError('Controller shut down');
                    notify_stopped_controller();
                }
            );

            var remote_path = `${config.remote_dfg_dir}/${filename}`
            // Broadcast the DFG to all remotes mentioned in the DFG
            // (initialization of runtimes and controller adds to ssh registry)
            ssh.broadcastFile(local_path, remote_path).then(
                () => {
                    log.debug("DFG broadcasted");
                    setTimeout( () => {
                        if (controller.is_connected()) {
                            clientUtils.broadcastError("Connected to running DeDOS instance");
                            clientUtils.broadcast('started', ' ');
                        }
                    },1000);
                },
                (err) => {
                    client.sendError(`Error broadcasting DFG: ${err}`);
                }
            );
        },
        // If the DFG failed to parse
        (err) => {
            log.error(`Error parsing dfg: ${err}, ${err.stack}`);
            client.sendError(err);
        }
    ).then( /** Just to catch errors in clause above */
        () => { /* Success, do nothing */ },
        (err) => {
            log.error(`Got error: ${err}, ${err.stack}`);
            clientUtils.broadcastError(socket, err);
        }
    );
}

function start_dedos(client) {
    log.note('--- STARTING DEDOS ---');
    start_controller(client, global_dfg_filename);
}

function stop_dedos() {
    log.note('--- KILLING DEDOS ---');
    controller.kill();
}

function reset() {
    global_dfg = {};
    global_dfg_filename = '';
    dfg_initialized = false;
}

function clone(client, msu_id) {
    log.debug(`Trying to clone ${msu_id}`);
    if (controller.is_connected()) {
        controller.clone_msu(msu_id).then( () => {
            log.note(`Cloned ${msu_id}`);
        }, (err) => {
            log.error(err);
            client.sendError(`Failed to clone: ${err}`);
        });
    } else {
        client.sendError('Cannot clone: no controller connection');
    }
}

function unclone(client, msu_id) {
    log.debug(`Trying to unclone ${msu_id}`);
    if (controller.is_connected()) {
        controller.unclone_msu(msu_id).then(() => {
            log.note(`Uncloned: ${msu_id}`);
        }, (err) => {
            log.error(err);
            client.sendError(`Failed to clone: ${err}`);
        });
    } else {
        client.sendError('Cannot remove: no controller connection');
    }
}

listener.sockets.on('connection', function(socket) {

    var client = new clientUtils.Client(socket);

    // If the DFG is already initialized, forward it to the
    // newly connected socket
    if (dfg_initialized) {
        client.sendDfg(global_dfg, true);
    }

    /** Run on clicking 'load file' in website */
    client.on('get_dfgs', send_dfgs);

    /** When the user gives a DFG file to be parsed */
    client.on('selected_dfg', init_dfg);

    /** When user hits the start button */

    client.on('start', start_dedos);

    client.on('stop', stop_dedos);
    client.on('reset', stop_dedos);

    client.on('clone', clone);
    client.on('unclone', unclone);

});


