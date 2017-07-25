import yaml
import json
import sys
from collections import OrderedDict

def sort_msus(msus):
    if len(msus) == 0:
        return []
    msus_out = []
    start_msus = [msu for msu in msus if msu['vertex_type'] == 'entry']
    msus_out.extend(start_msus)

    last_types = start_msus[-1]['meta_routing']['dst_types']
    while any(msu['type'] in last_types for msu in msus):
        next_types = set()
        rid = []
        for i, msu in enumerate(msus[:]):
            if (msu['type'] in last_types):
                if 'dst_types' in msu['meta_routing']:
                    for type in msu['meta_routing']['dst_types']:
                        next_types.add(type)
                msus_out.append(msu)
                rid.append(msu)
        msus = [msu for msu in msus if msu not in rid]
        last_types = next_types
    return msus_out

max_id = 1

def route_name(runtime_id, type, i):
    return '%03d%01d' % (type, i)
    #return '%03d%01d%01d' % (type, runtime_id, i)

def add_pseudo_routing(rt_id, froms, tos, routes, json_route):

    types = [msu['type'] for msu in tos]

    new_routes = {}

    for type in set(types):
        for i in range(99):
            name = route_name(rt_id, type, i)
            if name not in routes:
                routes[name] = {'destinations':{}, 'type': type, 'link': json_route, 'key-max': 0}
                new_routes[type] = name
                break

    for msu in tos:
        route_out = routes[new_routes[msu['type']]]
        last_max = route_out['key-max']
        route_out['destinations'][msu['id']] = last_max + 1
        route_out['key-max'] += 1

    for msu in froms:
        for type in types:
            if new_routes[type] not in msu['scheduling']['routing']:
                msu['scheduling']['routing'].append(new_routes[type])

def runtime_routes(rt_id, msus, routes):

    routes_out = {}

    for i, route in enumerate(routes):
        if isinstance(route['to'], list):
            tos = route['to']
        else:
            tos = [route['to']]

        if isinstance(route['from'], list):
            froms = route['from']
        else:
            froms = [route['from']]

        from_msus = [msu for msu in msus if msu['name'] in froms and msu['scheduling']['runtime_id'] == rt_id]

        if len(from_msus) == 0:
            continue

        thread_match = route.get('thread-match', False)

        if not thread_match:
            to_msus = [msu for msu in msus if msu['name'] in tos ]
            add_pseudo_routing(rt_id, from_msus, to_msus, routes_out, route)
        else:
            threads = set(msu['scheduling']['thread_id'] for msu in from_msus)
            for thread in threads:
                thread_froms = [msu for msu in msus if msu['scheduling']['thread_id'] == thread
                                and msu['scheduling']['runtime_id'] == rt_id
                                and msu['name'] in froms]
                thread_tos = [msu for msu in msus if msu['name'] in tos and msu['scheduling']['thread_id'] == thread]
                add_pseudo_routing(rt_id, thread_froms, thread_tos, routes_out, route)

    for id, route_out in routes_out.items():
        if 'key-max' in route_out['link']:
            ratio = route_out['link']['key-max'] / route_out['key-max']
            for msu_id in route_out['destinations']:
                route_out['destinations'][msu_id] *= ratio
        del route_out['key-max']
        del route_out['link']

    routes_final = [ dict(id=k, **v) for k, v in routes_out.items()]

    return routes_final

def count_downstream(msu, dfg, found_already=None):

    these_found = [msu['id']]
    found_already = [] if found_already is None else found_already

    if 'routing' not in msu['scheduling']:
        found_already.extend(these_found)
        return 1

    for route in msu['scheduling']['routing']:
        runtime = [rt for rt in dfg['runtimes'] if rt['id'] == msu['scheduling']['runtime_id']][0]
        dfg_route = [r for r in runtime['routes'] if r['id'] == route][0]

        for dst in dfg_route['destinations']:
            if not dst in found_already:
                dst_msus = [m for m in dfg['MSUs'] if m['id'] == dst]
                if len(dst_msus) == 0:
                    print "MSU %s can't find" % dst
                dst_msu = dst_msus[0]
                count_downstream(dst_msu, dfg, these_found)

    found_already.extend(these_found)
    return len(these_found)

def fix_route_keys(dfg):

    msus = {msu['id']:msu for msu in dfg['MSUs']}

    for runtime in dfg['runtimes']:
        for route in runtime['routes']:
            min_key = 0
            for destination in route['destinations']:
                msu = msus[destination]
                min_key += count_downstream(msu, dfg)
                route['destinations'][destination] = min_key

def make_msus_out(msu):
    global max_id
    reps = msu['reps'] if 'reps' in msu else 1
    msus = []
    for i in range(reps):
        msu_out = OrderedDict()
        msu_out['vertex_type'] = msu['vertex_type']
        msu_out['profiling'] = msu['profiling']
        msu_out['meta_routing'] = msu['meta_routing']
        msu_out['working_mode'] = msu['working_mode']
        msu_out['scheduling'] = OrderedDict()
        msu_out['scheduling']['thread_id'] = msu['thread']+i
        msu_out['scheduling']['deadline'] = msu['deadline']
        msu_out['scheduling']['runtime_id'] = msu['scheduling']['runtime_id']
        msu_out['scheduling']['cloneable'] = msu['cloneable']
        msu_out['scheduling']['colocation_group'] = msu['colocation_group'] if 'colocation_group' in msu else 0
        if msu_out['vertex_type'] != 'exit':
            msu_out['scheduling']['routing'] = []
        if 'depedencies' in msu:
            msu_out['dependencies'] = []
            for dependency in msu['dependencies']:
                msu_out['dependencies'].append(dependency)
        msu_out['type'] = msu['type']
        msu_out['id'] = max_id
        msu_out['name'] = msu['name']
        if ('dependencies' in msu and len(msu['dependencies']) > 0):
            msu_out['dependencies'] = []
            for dep in msu['dependencies']:
                msu_out['dependencies'].append(dict(
                    msu_type = dep['msu_type'],
                    locality = dep['locality']))
        if 'init_data' in msu:
            msu_out['init_data'] = msu['init_data']
        max_id += 1
        msus.append(msu_out)
    return msus

def stringify(output):

    if isinstance(output, dict) or isinstance(output, OrderedDict):
        for k, v in output.items():
            if isinstance(v, dict) or isinstance(v, OrderedDict):
                stringify(v)
            elif isinstance(v, list):
                stringify(v)
            else:
                output[k] = str(v)
    else:
        for i, v in enumerate(output):
            if isinstance(v, dict) or isinstance(v, OrderedDict):
                stringify(v)
            elif isinstance(v, list):
                stringify(v)
            else:
                output[i] = str(v)
def make_dfg(yml_filename, pretty=False):
    input = yaml.load(open(yml_filename))
    output = OrderedDict()

    output['application_name'] = input['application']['name']
    output['application_deadline'] = input['application']['deadline']
    output['global_ctl_ip'] = input['global_ctl']['ip']
    output['global_ctl_port'] = input['global_ctl']['port']
    output['load_mode'] = input['application']['load_mode']

    rts = []
    for rt in input['runtimes'].values():
        rts.append(dict(
            id = rt['runtime_id'],
            ip = rt['ip'],
            port = rt['port'],
            num_cores = rt['num_cores'],
            dram = rt['dram'],
            io_network_bw = rt['io_network_bw'],
            num_threads = rt['num_threads'],
            num_pinned_threads = rt['num_pinned_threads'],
        ))
    output['runtimes'] = rts

    msus = input['msus'] #sort_msus(input['msus'])
    msus_out = []
    for msu in msus:
        msus_out.extend(make_msus_out(msu))

    for rt in output['runtimes']:
        rt['routes'] = runtime_routes(rt['id'], msus_out, input['routes'])

    output['MSUs'] = msus_out
    fix_route_keys(output)
    stringify(output)

    return output

if __name__ == '__main__':
    pretty = True
    cfg = sys.argv[-1]
    output = make_dfg(cfg, pretty)
    if pretty:
        print(json.dumps(output, indent=2))
    else:
        print(json.dumps(output))
