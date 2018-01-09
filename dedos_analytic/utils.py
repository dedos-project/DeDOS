#!/usr/bin/python

import os
import yaml

class utils:
    def __repr__(self):
        tools = 'Available tools: \n'
        tools += 'load_config()'


    def load_config(self):
        conf_file = os.path.join(os.path.dirname(__file__), 'config.yml')
        return yaml.load(open(conf_file))
