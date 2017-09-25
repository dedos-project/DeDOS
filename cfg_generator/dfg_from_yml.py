import sys
import json
import copy
import yaml
from collections import OrderedDict, defaultdict

ROOT_MAPPING = OrderedDict([
    ('application_name', ['application', 'name']),
    ('global_ctl_ip', ['global_ctl', 'ip']),
    ('global_ctl_port', ['global_ctl', 'port']),
])

MSU_MAPPING = OrderedDict([
    ('vertex_type', 'vertex_type'),
    ('meta_routing', 'meta_routing'),
    ('working_mode', 'working_mode'),
    ('type', 'type'),
    ('name', 'name'),
    ('scheduling', OrderedDict([
        ('thread_id', 'thread'),
        ('runtime', ['scheduling', 'runtime_id']),
    ]))
])

RUNTIME_MAPPING = OrderedDict([
    ('id', 'runtime_id'),
    ('ip', 'ip'),
    ('port', 'port'),
    ('num_cores', 'num_cores'),
    ('num_threads', 'num_threads'),
    ('num_pinned_threads', 'num_pinned_threads')
])


def nested_dict_getter(a_dict, keys):
    """
    Retrieve the value from the given dictionary containing other nested
    dictionaries using the list of nested keys. Should also work for lists.

    :param a_dict: a dictionary or list containing nested dictionaries or
                   lists
    :param keys: a list of ordered keys to find the value in a_dict
    :return: the value retrieved from a_dict[key_0][key_1]...[key_n-1]
    """
    for k in keys:
        a_dict = a_dict[k]
    return a_dict


def dict_from_mapping(a_dict, mapping):
    """
    Transform a given dictionary into a new dictionary using the mapping
    dictionary given. The mapping dictionary may contain a mapping from
    output dictionary key to a list of nested keys, a single key, or
    a nested mapping dictionary.

    :param a_dict: dictionary to copy values from
    :param mapping: dictionary mapping output keys to input keys
    :return: the new dictionary transformed from the input dictionary
    """
    output = OrderedDict()
    for key in mapping:
        value = mapping[key]
        if isinstance(value, dict):
            output[key] = dict_from_mapping(a_dict, mapping)
        elif isinstance(value, list):
            output[key] = nested_dict_getter(a_dict, value)
        else:
            output[key] = a_dict[key]
    return output


def route_name(runtime_id, type, i):
    return '%03d%01d' % (type, i)


def add_routing(rt_id, froms, tos, routes, route_indexes):
    """
    Generates new routes to the MSUs given in tos, and adds them to the MSUs in
    froms, as well as to the full list of routes in routes.

    :param rt_id: the id of the runtime the routes will be added to
    :param froms: the set of msus to add the new routes to
    :param tos: the set of msus that the routes will be generated from
    :param routes: the current set of all routes for the runtime with id rt_id
    :param route_indexes: the defaultdict(int) needed to generate unique ids for
                          each route that is generated
    :return: None
    """
    type_counts = defaultdict(int)
    for msu in tos:
        type_counts[msu['type']] += 1

    new_routes = {}

    # Initialize new routes for each type of route being added
    for t in type_counts:
        name = route_name(rt_id, type, route_indexes[t])
        route_indexes[t] += 1
        new_routes[t] = {'id': name, 'destinations': {}, 'type': t}

    # For each type, sequentially initialize each destination's routing key
    type_iters = defaultdict(int)  # Maintaining iter value for each type
    for msu in tos:
        t = msu['type']
        new_routes[t]['destinations'][msu['id']] = type_iters[t] + 1
        type_iters[t] += 1

    # Add these new routes to each msu in froms
    for msu in froms:
        msu['scheduling']['routing'].extend(new_routes)

    # Now add the new routes to the main list
    routes.append(new_routes.items())


def runtime_routes(rt_id, msus, yaml_routes):
    """
    Generates all the routes for the json output given the routes defined in
    the yaml config, the list of msus, and the id of the runtime for which to
    consider the routes for.

    :param rt_id: the runtime id
    :param msus: the list of msus
    :param yaml_routes: all of the routes as defined in the yaml config
    :return: the list of generated rotues
    """
    json_routes = []
    route_indexes = defaultdict(int)

    for i, route in enumerate(yaml_routes):
        tos = list(route['to'])
        froms = list(route['from'])

        from_msus = [msu for msu in msus if msu['name'] in froms and
                     msu['scheduling']['runtime_id'] == rt_id]

        if len(from_msus) == 0:
            continue

        thread_match = route.get('thread-match', False)

        if not thread_match:
            to_msus = [msu for msu in msus if msu['name'] in tos]
            add_routing(rt_id, from_msus, to_msus, json_routes, route_indexes)
        else:
            threads = set(msu['scheduling']['thread_id'] for msu in from_msus)
            for thread in threads:
                thread_froms = [msu for msu in from_msus
                                if msu['scheduling']['thread_id'] == thread]
                thread_tos = [msu for msu in msus if msu['name'] in tos and
                              msu['scheduling']['thread_id'] == thread]
                add_routing(rt_id, thread_froms, thread_tos, json_routes,
                            route_indexes)

    return json_routes


def count_downstream(msu, dfg, found_already=None):
    """
    Recursively count the number of downstream destinations from the current msu.

    :param msu: the msu to search the routing from
    :param dfg: the entire dfg
    :param found_already: the set of MSU ids that have been seen already
    :return: the number of downstream destinations
    """
    found_already = {msu['id'] if found_already is None else found_already}

    if 'routing' not in msu['scheduling']:
        return 1

    # Get runtime of the given msu
    runtime = next(rt for rt in dfg['runtimes']
                   if rt['id'] == msu['scheduling']['runtime_id'])

    # Get the unique MSUs that exist downstream from this MSU
    for route in msu['scheduling']['routing']:
        dfg_route = next(r for r in runtime['routes'] if r['id'] == route)

        for dst in dfg_route['destinations']:
            if dst not in found_already:
                dst_msu = next(m for m in dfg['MSUs'] if m['id'] == dst)
                if dst_msu is None:
                    print "Cannot find MSU {0}".format(dst)
                found_already.add(dst_msu['id'])
                count_downstream(dst_msu, dfg, found_already)

    return len(found_already)


def fix_route_keys(dfg):
    """
    For each route in each runtime, set the value of each route's destination to
    min_key.

    :param dfg: the dfg to update the route keys in
    :return: None, the dfg is directly modified
    """
    msus = {msu['id']: msu for msu in dfg['MSUs']}

    for runtime in dfg['runtimes']:
        for route in runtime['routes']:
            min_key = 0
            for destination in route['destinations']:
                msu = msus[destination]
                min_key += count_downstream(msu, dfg)
                route['destinations'][destination] = min_key


def stringify(output):
    """
    Modifies the given output by converting anything not a list or a dict
    into a string (eg: ints, floats, etc)

    :param output: the output to stringify
    :return: None, the given output is directly modified
    """

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


class MSUGenerator(list):
    """
    This class makes copies of a base MSU as defined by the config in yaml for
    up to the number of reps specified.
    """

    next_msu_id = 1  # To ensure each MSU has a unique id

    def __init__(self, msu):
        """
        Initialize some of the generator values and generate the base msu from
        the given msu yaml dictionary.

        :param msu: the dictionary parsed from the yaml msu input
        """
        list.__init__(self)
        self.reps = msu['reps'] if 'reps' in msu else 1
        self.starting_id = self.next_msu_id
        self.generated = 0

        # Create the base MSU
        self.base_msu = dict_from_mapping(msu, MSU_MAPPING)
        if 'dependencies' in msu and len(msu['dependencies']) > 0:
            self.base_msu['dependencies'] = []
            for dep in msu['dependencies']:
                self.base_msu['dependencies'].append({
                    'msu_type': dep['msu_type'],
                    'locality': dep['locality']
                })
        if 'init_data' in msu:
            self.base_msu['init_data'] = msu['init_data']

        # Pre-allocating IDs
        self.base_msu['id'] = self.next_msu_id
        self.next_msu_id += self.reps

    def __iter__(self):
        """
        Generates the next MSU up to self.reps MSUs

        :return: the next MSU
        """
        if self.generated < self.reps:
            self.generated += 1
            return self.generate_msu(self.generated - 1)
        raise StopIteration()

    def generate_msu(self, i):
        """
        Generate the next rep of this MSU and override its id and thread_id.

        :param i: the rep value of this MSU
        :return: a new deep copy of the base MSU updated for the next rep
        """
        next_msu = copy.deepcopy(self.base_msu)
        next_msu['scheduling']['thread_id'] += i
        next_msu['id'] += i
        return next_msu

    def __len__(self):
        return self.reps


def make_dfg(cfg_fpath):
    """
    Generate a dfg from the given yaml cfg file as a dictionary to be dumped
    into the json format.

    :param cfg_fpath: path to the cfg file
    :return: the dfg generated from the cfg file
    """
    with open(cfg_fpath, 'r') as cfg_file:
        cfg_yaml = yaml.load(cfg_file)
        dfg = dict_from_mapping(cfg_yaml, ROOT_MAPPING)

        dfg['runtimes'] = [dict_from_mapping(rt, RUNTIME_MAPPING)
                           for rt in cfg_yaml['runtimes'].values()]

        msus = [msu for base_msu in cfg_yaml['msus']
                for msu in MSUGenerator(base_msu)]

        for rt in dfg['runtimes']:
            rt['routes'] = runtime_routes(rt['id'], msus, input['routes'])

        dfg['MSUs'] = msus
        fix_route_keys(dfg)
        stringify(dfg)

        return dfg


if __name__ == '__main__':
    cfg = sys.argv[-1]
    dfg = make_dfg(cfg)
    print(json.dumps(dfg, indent=2))
