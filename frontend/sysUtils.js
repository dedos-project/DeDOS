var log = require('./logger');

var sysUtils = {}


/**
 * append type of the file as response to and then respond the request
 * @param response: an object which wraps the response's information
 * @param err potential error
 * @param data the data to be respond
 * @param type the type of the response file, could be html or specific format of the image
 * @return null
 */
sysUtils.getResponse = function(response,err,data,type){
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

/**
 * Formats the DFG in the proper way to send to the javascript frontend
 * @param dfg DFG object to send
 * @param socket Socket to which to send the DFG
 * @param is_init If this is the initial DFG parsing, not received from gc
 * @return Nothing.
 */
sysUtils.sendDfg = function(dfg, socket, is_init) {
    n_threads = [];
    for (var i=0; i < dfg.runtimes.length; i++) {
        n_threads.push(dfg.runtimes[i]['threads']);
    }
    if (is_init) {
        mode = 'init';
    } else {
        mode = 'running-refresh';
    }
    socket.emit('dfg', {
        filename: dfg.app_name,
        nodes: dfg.msus,
        links: dfg.links,
        classified_data: dfg.hierarchy,
        types: dfg.types,
        mode: mode,
        cores: n_threads,
    });
}

module.exports = sysUtils;
