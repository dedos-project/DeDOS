var auxUtils = require('./auxUtils');
var log = require('./logger');

var readRuntimes = function(raw_dfg) {
    var runtimes = [];
    var raw_runtimes = raw_dfg['runtimes'];
    for (var i = 0; i < raw_runtimes.length; i++) {
        var rt = raw_runtimes[i];
        var runtime = {
           "id": rt['id'],
           'ip': rt['ip'],
           'threads': rt['n_unpinned_threads'] + rt['n_pinned_threads']
        }
        if (!('n_unpinned_threads' in rt)) {
            // Because I'm terrible, and `num` -> `n` when dfg is loaded
            runtime.threads = rt['num_unpinned_threads'] + rt['num_pinned_threads'];
        }
        if ('connected' in rt && rt['connected']) {
            runtime['status'] = 'Connected';
        } else {
            runtime['status'] = 'Idle';
        }
        runtimes.push(runtime);
    }
    return runtimes;
}

var readController = function(raw_dfg) {
    return {
        'ip': raw_dfg['global_ctl_ip']
    };
}

var readMsus = function(raw_dfg, types) {
    var msus = []
    var raw_msus = raw_dfg['MSUs'];
    for (var i = 0; i < raw_msus.length; i++) {
        msus.push(readMsu(raw_msus[i], types));
    }
    log.debug(`Read ${msus.length} MSUs`);
    return msus;
}

var readMsu = function(raw_msu, raw_types) {
    var name = typeName(raw_types, raw_msu['type']);
    if (name == 0) {
        log.warn(`Could not find name for type ${raw_msu['type']}`);
        name = '???';
    }
    if ('routes' in raw_msu.scheduling) {
        var routes = raw_msu.scheduling.routes;
    } else {
        var routes = [];
    }
    return {
        'id': raw_msu.id,
        'runtime_id': raw_msu.scheduling.runtime,
        'thread_id': raw_msu.scheduling.thread_id,
        'name': name,
        'type': raw_msu.type,
        'routing': routes
    };
}

var typeName = function(types, type_id) {
    for (var i = 0; i < types.length; i++) {
        if (type_id == types[i]['id'])
            return types[i]['name'];
    }
    return -1;
}

var readTypes = function(raw_dfg) {
    return auxUtils.topoSort(raw_dfg['MSU_types']);
}

var readRoutes = function(raw_dfg) {
    var routes = []
    var rts = raw_dfg['runtimes']
    for (var i = 0; i < rts.length; i++) {
        var rt_routes = rts[i]['routes'];
        for (var j = 0; j < rt_routes.length; j++) {
            var eps = rt_routes[j]['endpoints'];
            var route = {
                'id': rt_routes[j].id,
                'destinations': []
            }
            for (var k = 0; k < eps.length; k++) {
                route['destinations'].push(eps[k]['msu']);
            }
            routes.push(route);
        }
    }
    return routes;
}

var parseLinks = function(msus, routes) {
    var links = [];
    for (var i = 0; i < msus.length; i++) {
        var msu = msus[i];
        for (var j = 0; j < msu.routing.length; j++) {
            var dests = auxUtils.routingMatch(msu.routing[j], routes);
            for (var k = 0; k < dests.length; k++) {
                links.push({
                    source: msu.id,
                    target: dests[k]
                });
            }
        }
    }
    return links;
}

var buildMsuHierarchy = function(msus, types, n_runtimes) {
    var runtimes = [];
    for (var rt_id = 1; rt_id <= n_runtimes; rt_id++) {
        var msus_by_type = {};
        for (var j = 0; j < types.length; j++) {
            var type = types[j];
            var type_msus = [];
            for (var k = 0; k < msus.length; k++) {
                var msu = msus[k];
                if (msu['type'] == type['type'] && msu['runtime_id'] == rt_id) {
                    type_msus.push(msu);
                }
            }
            msus_by_type[type['type']] = type_msus;
        }
        runtimes.push(msus_by_type);
    }
    return runtimes;
}

/**
 * Takes the raw dfg JSON object and converts it into the DFG necessary for the front-end
 * (NOTE: Can throw errors)
 */
var readDfg = function(raw_dfg) {
    dfg = {};
    dfg.app_name = raw_dfg.application_name;
    dfg.runtimes = readRuntimes(raw_dfg);
    dfg.controller = readController(raw_dfg);
    dfg.types = readTypes(raw_dfg);
    dfg.msus = readMsus(raw_dfg, dfg.types);
    dfg.routes = readRoutes(raw_dfg);
    dfg.links = parseLinks(dfg.msus, dfg.routes);
    dfg.hierarchy = buildMsuHierarchy(dfg.msus, dfg.types, dfg.runtimes.length);
    return dfg;
}

/**
 * Parses a string containing a DFG in JSON format
 * @param dfg_content String containing DFG
 * @return Promise resolves(dfg) on success, rejects(error) on error
 */
var parseDfg = function(dfg_content) {
    return new Promise((resolve, reject) => {
        var bad = false;
        try {
            var raw_dfg = JSON.parse(dfg_content);
        } catch (e) {
            reject(e);
            return;
        }

        var rt_info = [];
        try {
            var dfg = readDfg(raw_dfg);
        } catch (e) {
            var error = `Error parsing JSON: ${e}  ${e.stack}`;
            log.error(error);
            reject(e);
            return;
        }
        resolve(dfg);
    });
}

module.exports = {
    parseDfg: parseDfg
};
