#!/usr/bin/python

import json
import os
import sys
from subprocess import PIPE
import subprocess
from peewee import *

sys.path.append(os.path.abspath(os.path.dirname(__file__)) + '/../')
from utils import *

metrics = {
    'MSU_QUEUE_LEN': 'int',
    'MSU_ITEMS_PROCESSED': 'int',
    'MSU_EXEC_TIME': 'int',
    'MSU_IDLE_TIME': 'int',
    'MSU_MEM_ALLOC': 'int',
    'MSU_NUM_STATES': 'int',
    'THREAD_CTX_SWITCHES': 'int',
}

class db_setup:
    def __init__(self):
        self.utils = utils()
        self.config = self.utils.load_config()

        self.json_f = self.config['json']

        fp = open(self.json_f, 'r')
        self.json = json.load(fp)
        fp.close()

        #generate schema
        self.schema = self.gen_sql_schema()

        #inject schema (create/drop DB if needed)
        self.inject_schema()

    def gen_sql_schema(self):
        tmp = os.path.dirname(__file__) + '/schema.sql'
        f = open(tmp, 'w')

        columns = '('
        for m in metrics:
            columns += m + ' ' + metrics[m] + ', '

        columns += 'TS bigint'
        columns += ')'

        msus = self.json['MSUs']
        for msu in msus:
            table     = 'CREATE TABLE '
            msu_table = 'MSU_' + str(msu['id']) + ' '
            f.write(table + msu_table + columns + ';\n')

        f.close()

        return f.name

    def inject_schema(self):
        # Drop if exists, and (re)create the database
        tmp = os.path.dirname(__file__) + '/create_db.sql'
        f = open(tmp, 'w')
        f.write('DROP DATABASE IF EXISTS `' + self.json['db_name'] + '`; ')
        f.write('CREATE DATABASE `' + self.json['db_name'] + '` character set utf8 collate utf8_general_ci;')
        f.close()
        tmp_abs_path = f.name

        for s in [tmp_abs_path, self.schema]:
            if (self.json['db_pwd']):
                cmd  = ['/usr/bin/mysql', '-u' + self.json['db_user'], '-p' + self.json['db_pwd']]
                if (s == self.schema):
                    cmd += ['--database=' + self.json['db_name']]

            else:
                cmd = ['/usr/bin/mysql', '-u', self.json['db_user']]
                if (s == self.schema):
                    cmd += self.json['db_name']

            try:
                print('Injecting DB schema...')
                p = subprocess.check_call(cmd, stdin = open(s), stdout = PIPE, stderr = PIPE)
            except subprocess.CalledProcessError as e:
                raise RuntimeError(
                    "'{}' error (code {}): {}".format(e.cmd, e.returncode, e.output)
                )

        print('done')

if __name__ == '__main__':
    init = db_setup()
