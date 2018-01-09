#!/usr/bin/python

import os
import json

class utils:
    def __repr__(self):
        tools = 'Available tools: \n'
        tools += 'load_config()'

    """
    Load configuration from local config.json file
    @return: config, dictionnary containing configuration
    """
    def load_config(self):
        config    = {}
        conf_file = os.path.abspath(os.path.dirname(__file__)) + '/config.json'
        try:
            conf_desc = open(conf_file, 'r')
        except OSError as e:
            print(e)
            raise

        for line in conf_desc.readlines():
            j = json.loads(line)
            config.update(j)

        conf_desc.close()
        return config
