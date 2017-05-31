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
    #return '%03d%02d%01d' % (type, runtime_id, i)

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


        to_msus = [msu for msu in msus if msu['name'] in tos]
        types = [msu['type'] for msu in to_msus]

        these_routes = {}

        for type in set(types):
            for i in range(99):
                name = route_name(rt_id, type, i)
                if name not in routes_out:
                    routes_out[name] = {'destinations':{}, 'type':type, 'link': route, 'key-max': 0}
                    these_routes[type] = name
                    break

        for route_to in tos:
            route_msus = [msu for msu in msus if msu['name'] == route_to]
            if len(msu) == 0:
                print('No MSU with name %s! Stopping!'%route_to)
            for msu in route_msus:
                route_out = routes_out[these_routes[msu['type']]]
                last_max = route_out['key-max'];
                route_out['destinations'][msu['id']] = last_max + 1;
                route_out['key-max']+=1;

        for from_msu in from_msus:
            for type in types:
                if these_routes[type] not in from_msu['scheduling']['routing']:
                    from_msu['scheduling']['routing'].append(these_routes[type])

    for id, route_out in routes_out.items():
        if 'key-max' in route_out['link']:
            ratio = route_out['link']['key-max'] / route_out['key-max']
            for msu_id in route_out['destinations']:
                route_out['destinations'][msu_id] *= ratio
        del route_out['key-max']
        del route_out['link']

    routes_final = [ dict(id=k, **v) for k, v in routes_out.items()]

    return routes_final

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
        if msu_out['vertex_type'] != 'exit':
            msu_out['scheduling']['routing'] = []
        msu_out['type'] = msu['type']
        msu_out['id'] = max_id
        msu_out['name'] = msu['name']
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
            io_network_bw = rt['io_network_bw']
        ))
    output['runtimes'] = rts

    msus = input['msus'] #sort_msus(input['msus'])
    msus_out = []
    for msu in msus:
        msus_out.extend(make_msus_out(msu))

    for rt in output['runtimes']:
        rt['routes'] = runtime_routes(rt['id'], msus_out, input['routes'])

    output['MSUs'] = msus_out
    stringify(output)

    if pretty:
        print(json.dumps(output, indent=2))
    else:
        print(json.dumps(output))


if __name__ == '__main__':
    pretty = True
    cfg = sys.argv[-1]
    make_dfg(cfg, pretty)
