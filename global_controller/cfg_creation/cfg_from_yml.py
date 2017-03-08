from __future__ import print_function
import sys
import yaml
from collections import defaultdict, OrderedDict

SYNTAX = dict(
    thread='create_pinned_thread {rt_socket}',
    msu='addmsu {rt_socket} {type_id} {id} {block_type} {thread.id}',
    route='add route {m1.rt_socket} {m1.id} {m1.type_id} {m2.id} {m2.type_id} {remote}',
)

class Thread:
    def __init__(self, runtime_socket, id):
        self.rt_socket = runtime_socket
        self.id = id

    def to_cfg(self):
        return SYNTAX['thread'].format(**vars(self))
    
class MSU:

    USED_IDS = []
    def __init__(self, threads, rep, name, rt_socket, type_id, block_type, 
            thread, ip, id=None, reps=None):
        self.name, self.rt_socket, self.type_id, self.block_type, self.ip= \
                name, rt_socket, type_id, block_type, ip, 
        self.rep_num = rep

        if id is None:
            self.id = self.gen_id()
        else:
            self.id = id
        
        if thread+rep not in threads[rt_socket]:
            threads[rt_socket][thread+rep] =  Thread(rt_socket, thread+rep)
        self.thread = threads[rt_socket][thread+rep]
            
    def gen_id(self):
        i=1
        while i in self.USED_IDS:
            i+=1
        self.USED_IDS.append(i)
        return i

    def to_cfg(self, used_threads):
        msu_output = SYNTAX['msu'].format(**vars(self))
        output = ""
        while self.thread.id > used_threads[self.rt_socket]:
            used_threads[self.rt_socket]+=1
            output += self.thread.to_cfg() + '\n'
        return output + msu_output

class Route:

    def __init__(self, local_ip, m1, m2):
        self.local_ip = local_ip
        self.m1 = m1
        self.m2 = m2
        self.remote = self.calculate_remote()
    
    def calculate_remote(self):
        if self.m1.ip != self.m2.ip:
            return '2 {m2.ip}'.format(**vars(self))
        else:
            return '1'
    
    def to_cfg(self):
        output = []
        #for msu in (self.m1, self.m2):
        #    if msu not in used_msus:
        #        used_msus.append(msu)
        #        output.append(msu.to_cfg(used_threads))
        return SYNTAX['route'].format(**vars(self))

def add_routes(gc_ip, routes, msus, froms, tos, thread_match):
    if isinstance(froms, str) and isinstance(tos, str):
        for msu_from in msus[froms]:
            matched_one = False
            for msu_to in msus[tos]:
                if (not thread_match) or (msu_from.thread.id == msu_to.thread.id):
                    routes.append(Route(gc_ip, msu_from, msu_to))   
                    matched_one = True
            if not matched_one:
                print('WARNING! NO THREAD MATCH FOR ROUTE: %s thread %d -> %s'%(froms, msu_from.thread.id, tos), file=sys.stderr)
        return
    if isinstance(froms, list):
        for from_ in froms:
            add_routes(gc_ip, routes, msus, from_, tos, thread_match)
    if isinstance(tos, list):
        for to in tos:
            add_routes(gc_ip, routes, msus, froms, to, thread_match)


def create_cfg(yml_filename):
    input = yaml.load(open(yml_filename))
    gc_ip = input['global_controller_ip']

    threads = defaultdict(dict)
        
    msus = OrderedDict()
    for raw_msu in input['msus']:
        msus[raw_msu['name']] = []
        for i in range(raw_msu['reps']):
            msus[raw_msu['name']].append(MSU(threads, i, **raw_msu))

    routes = []
    for route in input['routes']:
        add_routes(gc_ip, routes, msus, route['from'], route['to'], route['thread-match'])
    
    n_used_threads = defaultdict(int)
    used_msus = []
    output = []
    for msu_group in msus.values():
        for msu in msu_group:
            output.append(msu.to_cfg(n_used_threads))

    for route in routes:
        output.append(route.to_cfg())

    print('\n'.join(output))

if __name__ == '__main__':
    if len(sys.argv)>1:
        create_cfg(sys.argv[1])
    else:
        create_cfg('cfg.yml')
