var log = require('./logger');

var clientUtils = {}

var clients = [];

var Client = function(socket) {
    log.note("Creating new client");

    this.last_sent = {
        application: '',
        msu_types: '',
        runtimes: '',
        msus: '',
        links: ''
    };
    this.socket = socket;
    clients.push(this);

    socket.on('disconnect',  () => {
        log.info("Client disconnected");
        var index = clients.indexOf(this);
        if (index > -1) {
            clients.splice(index);
        }
        socket.disconnect();
    });
}

var topic_fields = {
    application: 'app_name',
    msu_types: 'types',
    runtimes: 'runtimes',
    msus: 'msus',
    links: 'links'
};

Client.prototype.sendIfNew = function(topic, dfg) {
    var serialized = JSON.stringify(dfg[topic_fields[topic]]);

    if (serialized != this.last_sent[topic]) {
        this.last_sent[topic] = serialized;
        this.socket.emit(topic, dfg[topic_fields[topic]]);
    }
}

Client.prototype.sendDfg = function(dfg, force) {
    for (var topic in topic_fields) {
        if (!force) {
            this.sendIfNew(topic, dfg);
        } else {
            this.send(topic, dfg[topic_fields[topic]]);
        }
    }
}

Client.prototype.send = function(topic, object) {
    this.socket.emit(topic, object);
}

Client.prototype.sendError = function(msg) {
    this.socket.emit('error_msg', String(msg));
}

Client.prototype.on = function(topic, fn) {
    this.socket.on(topic, (input) => { fn(this, input); });
}

var broadcastDfg = function(dfg, force) {
    for (var i = 0; i < clients.length; i++) {
        clients[i].sendDfg(dfg, force);
    }
}

var broadcast = function(topic, object) {
    for (var i = 0; i < clients.length; i++) {
        clients[i].send(topic, object);
    }
}

var broadcastError = function(msg) {
    log.error(`Broadcasting error ${msg}`);
    for (var i = 0; i < clients.length; i++) {
        clients[i].sendError(msg);
    }
}

module.exports = {
    Client: Client,
    broadcastDfg: broadcastDfg,
    broadcast: broadcast,
    broadcastError: broadcastError
}
