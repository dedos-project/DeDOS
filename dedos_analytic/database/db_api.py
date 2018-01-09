import sys
import os
import peewee
import json
from models import *
import pandas as pd
import itertools as it
from dedos_analytic.engine.data_manipulation import resolution_round

from utils import *

class DbApi:
    def __init__(self, config=None):
        self.utils = utils()

        if config is None:
            self.config = self.utils.load_config()
        else:
            self.config = config

        self.json_f = self.config['dedos_dfg']
        fp = open(self.json_f, 'r')
        self.json = json.load(fp)
        fp.close()

        print("Setting database to ip: %s, port %d" % (self.json['db_ip'], self.json['db_port']))
        self.db = MySQLDatabase(self.json['db_name'],
                                host = self.json['db_ip'],
                                port = self.json['db_port'],
                                user = self.json['db_user'],
                                passwd = self.json['db_pwd'])

    def db_connect(self):
        try:
            self.db.connect()
            #print("Connected to database")
        except Exception as e:
            print(e)
            raise

    def get_items(self, item):
        self.db_connect()

        items = []
        if (item == 'stats'):
            for stat in Statistics.select():
                items.append(stat)
        elif (item == 'msus'):
            for msu in Msus.select():
                items.append(msu)
        elif (item == 'msu_types'):
            for msu_type in Msutypes.select():
                items.append(msu_type)
        elif (item == 'runtimes'):
            for rt in Runtimes.select():
                items.append(rt)
        elif (item == 'threads'):
            for thread in Threads.select():
                items.append(thread)


        self.db.close()

        return items

    def get_msu_timeseries(self, msu):
        self.db_connect()

        timeseries = []
        for timeserie in Timeseries.select().where(Timeseries.msu_pk == msu.pk):
            timeseries.append(timeserie)

        self.db.close()

        return timeseries

    def get_ts_points(self, timeserie, timestamp = 0):
        self.db_connect()

        points = []
        for point in Points.select().where((Points.timeseries_pk == timeserie.pk) & (Points.ts > timestamp)):
            points.append(point)

        self.db.close()

        return points

    def get_msu_full_df(self, msu, timestamp = 0):
        cols = {}
        cols['TIME'] = []
        timeseries = self.get_msu_timeseries(msu)

        self.db_connect()

        query = None
        aliases = [Points.alias() for _ in timeseries]
        for i, (ts, alias) in enumerate(zip(timeseries, aliases)):
            stat_name = ts.statistic.name
            if stat_name not in cols:
               cols[stat_name] = []

            subquery = alias.select().where(
                (alias.timeseries_pk == ts.pk) & (alias.ts > timestamp)).alias(stat_name
            )
            if query is not None:
                query = query.join(subquery, on=SQL(stat_name + '.ts') == aliases[0].ts)
            else:
                query = subquery

        query = query.select(
            aliases[0].ts, aliases[0].val, *[SQL(ts.statistic.name + '.val') for ts in timeseries[1:]]
        ).tuples()

        self.db.close()

        for row in query:
            for i, cell in enumerate(row):
                if (i == 0):
                    cols['TIME'].append(cell)
                else:
                    cols[timeseries[i - 1].statistic.name].append(cell)

        return pd.DataFrame(cols).set_index('TIME')


    def get_msu_epoch_df(self,msu):
        print "Getting dataframe for msu {msu.id} ({msu.msu_type.name})".format(msu=msu)
        df = self.get_msu_full_df(msu)
        df = df.assign(TIME = df.index)
        trange = (max(df.TIME) - min(df.TIME)) * 1e-9
        spp = round(trange / len(df), 2)
        print "\n # Points: {}\n Time range: {} seconds\n Points / second: ~{}".format(
            len(df), trange, 1/spp
        )
        rounded_time = resolution_round(df.TIME, spp * 1e9)

        epoch = ((rounded_time - min(rounded_time)) / (spp * 1e9)).astype(int)
        df = df.assign(msu_id = msu.id)
        df = df.assign(msu_type = msu.msu_type.name)
        df = df.assign(epoch = epoch)
        df = df.set_index('TIME')
        df = df.assign(TIME = df.index)
        return df

    def get_msus_epoch_df(self, msus):
        return pd.concat([self.get_msu_epoch_df(msu) for msu in msus])
