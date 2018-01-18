var auxUtils = {};

var toposort = require('toposort');
var log = require('./logger');

/**
 * Perform topological sort of MSU types, resolving any cyclic dependencies
 * @param msu_types the graph for the system
 * @return array whose order of the entry represents the level of each type in the graph
 */
auxUtils.topoSort = function(msu_types){

    var nodes = [];
    var edges = [];

    for (var i = 0; i < msu_types.length; i++) {
        nodes.push(msu_types[i].id);
        if ('dst_types' in msu_types[i].meta_routing) {
            for (var j = 0; j < msu_types[i].meta_routing.dst_types.length; j++) {
                edges.push([msu_types[i].id, msu_types[i].meta_routing.dst_types[j]]);
            }
        }
    }

    // Have to loop and remove cyclic dependencies
    while (1) {
        try {
            var sorted_codes = toposort.array(nodes, edges);
            break;
        } catch (e) {
            var bad = e.message.split(':').slice(-1)[0].trim();
            //log.note(`Cyclic dependency! Removing ${bad}`);
            var removed = false;
            for (var i = 0; i < edges.length; i++) {
                if (edges[i][1] == parseInt(bad)) {
                    edges.splice(i, 1);
                    removed = true;
                    break;
                }
            }
            if (!removed) {
                log.warn("HOW!?");
                throw(e);
            }
        }
    }


    var sorted_names = new Array(sorted_codes.length);
    for(var i = 0;i<msu_types.length;i++){
        var ind = sorted_codes.indexOf(msu_types[i].id);
        sorted_names[ind] = msu_types[i].name;
    }
    var types = [];
    for(var i = 0;i<sorted_codes.length;i++){
        types.push({
            id:sorted_codes[i],
            name:sorted_names[i],
            order:i
        });
    }
    return types;
}

/**
 * Returns all of the destinations of a given route
 * @param id ID of the route to match
 * @param routes List of all routes in DFG
 */
auxUtils.routingMatch = function(id,routes){
    var arr = [];
    for(var i = 0;i<routes.length;i++)
        if(routes[i].id==id.toString())
            return routes[i].destinations;
    return arr;
}

module.exports = auxUtils;
