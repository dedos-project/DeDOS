import sys
import json
import copy
import yaml
from collections import OrderedDict

MSU_ID_START = 10
route_id = 1000

""" Ordered dictionaries containing simple mappings from fields in json to fields in yaml. """

ROOT_MAPPING = OrderedDict([
    ('application_name', ['application', 'name']),
    ('global_ctl_ip', ['global_ctl', 'ip']),
    ('global_ctl_port', ['global_ctl', 'port']),
    ('db_ip', ['database', 'ip']),
    ('db_port', ['database', 'port']),
    ('db_user', ['database', 'user']),
    ('db_pwd', ['database', 'password']),
    ('db_name', ['database', 'name'])
])

MSU_MAPPING = OrderedDict([
    ('vertex_type', 'vertex_type'),
    ('init_data', 'init_data'),
    ('type', 'type'),  # Directly copied but properly resolved later, easier to get correct ordering for gc parser
    ('blocking_mode', 'blocking_mode'),
    ('scheduling', OrderedDict([
        ('thread_id', 'thread'),
        ('runtime', 'runtime'),
    ])),
    ('name', 'name')  # Needed for routes, but not output
])

MSU_TYPE_MAPPING = OrderedDict([
    ('id', 'id'),
    ('meta_routing', 'meta_routing'),
    ('cloneable', 'cloneable'),
])

DEPENDENCY_MAPPING = OrderedDict([
    ('type', 'type'),
    ('locality', 'locality'),
])

RUNTIME_MAPPING = OrderedDict([
    ('ip', 'ip'),
    ('port', 'port'),
    ('num_cores', 'n_cores'),
    ('num_unpinned_threads', 'n_unpinned_threads'),
    ('num_pinned_threads', 'n_pinned_threads')
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


def dict_from_mapping(a_dict, mapping, output=None):
    """
    Transform a given dictionary into a new dictionary using the mapping
    dictionary given. The mapping dictionary may contain a mapping from
    output dictionary key to a list of nested keys, a single key, or
    a nested mapping dictionary.

    :param a_dict: dictionary to copy values from
    :param mapping: dictionary mapping output keys to input keys
    :return: the new dictionary transformed from the input dictionary
    """
    if output is None:
        output = OrderedDict()
    for key in mapping:
        value = mapping[key]
        if isinstance(value, dict):
            output[key] = dict_from_mapping(a_dict, value)
        elif isinstance(value, list):
            output[key] = nested_dict_getter(a_dict, value)
        else:
            if value in a_dict:
                output[key] = a_dict[value]
    return output


def add_routing(froms, tos, routes):
    """
    Generates new routes to the MSUs given in tos, and adds them to all the MSUs in
    froms as well as to the full list of routes.

    :param froms: the set of msus to add the new routes to
    :param tos: the set of msus that the routes will be generated from
    :param routes: the current set of all routes for the runtime
    :return: None
    """
    global route_id

    # Create the new routes and add each destination
    new_routes = {}
    for dest in tos:
        if dest['type'] not in new_routes:
            new_routes[dest['type']] = OrderedDict([('id', route_id), ('type', dest['type']), ('endpoints', [])])
            route_id += 1
        endpoints = new_routes[dest['type']]['endpoints']
        endpoints.append(OrderedDict([('key', len(endpoints) + 1), ('msu', dest['id'])]))

    # Now add these new routes to runtime and their ids to the msus in froms
    routes.extend(new_routes.values())
    for msu in froms:
        if 'routes' not in msu['scheduling']:
            msu['scheduling']['routes'] = []
        msu['scheduling']['routes'].extend([new_routes[key]['id'] for key in new_routes])


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

    for i, route in enumerate(yaml_routes):
        tos = route['to'] if isinstance(route['to'], list) else [route['to']]
        froms = route['from'] if isinstance(route['from'], list) else [route['from']]

        from_msus = [msu for msu in msus if msu['name'] in froms and
                     msu['scheduling']['runtime'] == rt_id]

        if len(from_msus) == 0:
            continue

        thread_match = route.get('thread-match', False)

        if not thread_match:
            to_msus = [msu for msu in msus if msu['name'] in tos]
            add_routing(from_msus, to_msus, json_routes)
        else:
            threads = set(msu['scheduling']['thread_id'] for msu in from_msus)
            for thread in threads:
                thread_froms = [msu for msu in from_msus
                                if msu['scheduling']['thread_id'] == thread]
                thread_tos = [msu for msu in msus if msu['name'] in tos and
                              msu['scheduling']['thread_id'] == thread]
                add_routing(thread_froms, thread_tos, json_routes)

    return json_routes


def count_endpoints(msu, dfg):
    """
    Count the total number of endpoints for any route with the given msu
    as a source.

    :param msu: the source msu of the routes to consider
    :param dfg: the entire dfg
    :return: the total number of endpoints
    """
    if 'routes' not in msu['scheduling']:
        return 1

    msu_route_ids = set(msu['scheduling']['routes'])

    # Find the runtime for the given msu
    runtime = next(rt for rt in dfg['runtimes'] if rt['id'] == msu['scheduling']['runtime'])
    if runtime is None:
        print "Failed to find runtime {0} for msu {1}".format(msu['scheduling']['runtime'], msu['id'])

    # Return the sum of the endpoints for each route referenced in the MSU
    return sum([len(route['endpoints']) for route in runtime['routes'] if route['id'] in msu_route_ids])


def fix_route_keys(dfg):
    """
    For each route in each runtime, set the key for each MSU to the number
    of endpoints in the MSU's routes.

    :param dfg: the dfg to update the route keys in
    :return: None, the dfg is directly modified
    """
    msus = {msu['id']: msu for msu in dfg['MSUs']}

    for runtime in dfg['runtimes']:
        for route in runtime['routes']:
            min_key = 0
            for (i, endpoint) in enumerate(route['endpoints']):
                msu = msus[endpoint['msu']]
                endpoints = count_endpoints(msu, dfg)
                min_key += endpoints if endpoints > 0 else 1
                route['endpoints'][i]['key'] = min_key


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

    next_msu_id = MSU_ID_START  # To ensure each MSU has a unique id

    def __init__(self, msu, type_name_to_id):
        """
        Initialize some of the generator values and generate the base msu from
        the given msu yaml dictionary.

        :param msu: the dictionary parsed from the yaml msu input
        """
        list.__init__(self)
        self.reps = msu['reps'] if 'reps' in msu else 1
        self.runtimes = msu['runtime'] if isinstance(msu['runtime'], list) else [msu['runtime']]
        self.starting_id = self.next_msu_id
        self.generated = 0

        # Pre-allocating IDs
        self.base_msu = OrderedDict([('id', MSUGenerator.next_msu_id)])
        MSUGenerator.next_msu_id += (self.reps * len(self.runtimes))

        # Add simple mappings to base_msu now
        self.base_msu = dict_from_mapping(msu, MSU_MAPPING, self.base_msu)
        self.base_msu['type'] = type_name_to_id[msu['type']]  # Overwrite with the id
        if 'init_data' in msu:
            self.base_msu['init_data'] = msu['init_data']

    def __iter__(self):
        """
        Generates the next MSU up to self.reps MSUs

        :return: the next MSU
        """
        while self.generated < (self.reps * len(self.runtimes)):
            self.generated += 1
            yield self.generate_msu(self.generated - 1)
        raise StopIteration()

    def generate_msu(self, i):
        """
        Generate the next rep of this MSU and override its id and thread_id.

        :param i: the rep value of this MSU
        :return: a new deep copy of the base MSU updated for the next rep
        """
        next_msu = copy.deepcopy(self.base_msu)
        next_msu['scheduling']['thread_id'] += i % self.reps
        next_msu['scheduling']['runtime'] = self.runtimes[i / self.reps]
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

        msu_type_name_to_id = {}
        dfg['MSU_types'] = []
        for (name, msu_type) in cfg_yaml['msu_types'].items():
            dfg_msu_type = dict_from_mapping(msu_type, MSU_TYPE_MAPPING)
            dfg_msu_type['name'] = name
            dfg_msu_type['colocation_group'] = msu_type['colocation_group'] if 'colocation_group' in msu_type else 0
            if 'dependencies' in msu_type:
                dfg_msu_type['dependencies'] = [dict_from_mapping(dep, DEPENDENCY_MAPPING)
                                                for dep in msu_type['dependencies']]
            dfg['MSU_types'].append(dfg_msu_type)
            msu_type_name_to_id[name] = dfg_msu_type['id']

        # Fix types in MSU dependencies and meta_routing->dst_types from name to id
        for msu_type in dfg['MSU_types']:
            if 'dependencies' in msu_type:
                for dep in msu_type['dependencies']:
                    if not isinstance(dep['type'], int):
                        dep['type'] = msu_type_name_to_id[dep['type']]
            for key in ['source_types', 'dst_types']:
                if 'meta_routing' in msu_type and key in msu_type['meta_routing']:
                    for (i, t) in enumerate(msu_type['meta_routing'][key]):
                        if not isinstance(t, int):
                            msu_type['meta_routing'][key][i] = msu_type_name_to_id[t]

        dfg['MSUs'] = [msu for base_msu in cfg_yaml['msus']
                       for msu in MSUGenerator(base_msu, msu_type_name_to_id)]

        dfg['runtimes'] = []
        for (rt_id, rt) in cfg_yaml['runtimes'].items():
            dfg_rt = OrderedDict([('id', rt_id)])
            dfg_rt = dict_from_mapping(rt, RUNTIME_MAPPING, dfg_rt)
            dfg['runtimes'].append(dfg_rt)

        for rt in dfg['runtimes']:
            rt['routes'] = runtime_routes(rt['id'], dfg['MSUs'], cfg_yaml['routes'])

        for msu in dfg['MSUs']:
            del(msu['name'])  # Name needed for routing but not output

        fix_route_keys(dfg)
        #stringify(dfg)

        return dfg


if __name__ == '__main__':
    cfg = sys.argv[-1]
    dfg = make_dfg(cfg)
    print(json.dumps(dfg, indent=2))
