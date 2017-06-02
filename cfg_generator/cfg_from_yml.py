import sys
import yaml
from dfg_from_yml import make_dfg, make_msus_out, runtime_routes

SYNTAX = dict(
        thread='create_pinned_thread {socket}',
        msu='addmsu {socket} {type} {id} {block} {thread}',
        endpoint='add endpoint {socket} {route} {dest} {key}',
        route='add route {socket} {route} {msu}'
)

MINSOCK=4

def make_cfg(yml_filename):
    input = yaml.load(open(yml_filename))

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

    msus = input['msus'] #sort_msus(input['msus'])
    msus_out = []
    for msu in msus:
        msus_out.extend(make_msus_out(msu))

    output = []

    for i, rt in enumerate(rts):
        rt_routes = runtime_routes(rt['id'], msus_out, input['routes'])
        rt_msus = [msu for msu in msus_out if
                   msu['scheduling']['runtime_id'] == rt['id']]

        if len(rt_msus) == 0:
            continue

        rt_threads = set(msu['scheduling']['thread_id'] for msu in rt_msus)

        for thread in range(max(rt_threads)):
            output.append(SYNTAX['thread'].format(socket=i+MINSOCK))

        for msu in rt_msus:
            output.append(SYNTAX['msu'].format(socket=i+MINSOCK,
                                               type=msu['type'],
                                               id=msu['id'],
                                               block=msu['working_mode'],
                                               thread=msu['scheduling']['thread_id']))

    for i, rt in enumerate(rts):
        rt_routes = runtime_routes(rt['id'], msus_out, input['routes'])
        rt_msus = [msu for msu in msus_out if
                   msu['scheduling']['runtime_id'] == rt['id']]

        if len(rt_msus) == 0:
            continue

        rt_threads = set(msu['scheduling']['thread_id'] for msu in rt_msus)


        for route in rt_routes:
            for k, v in route['destinations'].items():
                output.append(SYNTAX['endpoint'].format(
                        socket=i+MINSOCK,
                        route=route['id'],
                        dest=k,
                        key=v
                ))

        for msu in rt_msus:
            if 'routing' in msu['scheduling']:
                for route in msu['scheduling']['routing']:
                    output.append(SYNTAX['route'].format(socket=i+MINSOCK,
                                                         route=route,
                                                         msu=msu['id']))

    print('\n'.join(output))

if __name__ == '__main__':
    make_cfg(sys.argv[1])
