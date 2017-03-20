import yaml
import json
import sys
from collections import OrderedDict

def sort_msus(msus):
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

def add_routing(msu, msus, routes):

    for route in routes:

        if isinstance(route['from'], list):
            froms = route['from']
        else:
            froms = [route['from']]

        if isinstance(route['to'], list):
            tos = route['to']
        else:
            tos = [route['to']]
        
        for route_from in froms:
            if route_from == msu['name']:
                for route_to in tos:
                    for msu_to in msus:
                        if route_to == msu_to['name']:
                            msu['scheduling']['routing'].append(msu_to['id'])

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
def make_cfg(yml_filename, pretty=False):
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

    msus = sort_msus(input['msus'])
    msus_out = []
    for msu in msus:
        msus_out.extend(make_msus_out(msu))
    
    for msu in msus_out:
        add_routing(msu, msus_out, input['routes'])

    for msu in msus_out:
        del msu['name']
    
    output['MSUs'] = msus_out[::-1]
    stringify(output)

    if pretty:
        print(json.dumps(output, indent=2))
    else:
        print(json.dumps(output))


if __name__ == '__main__':
    pretty = '-p' in sys.argv
    make_cfg('dfg.yml', pretty)
