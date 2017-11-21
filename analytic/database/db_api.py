import sys
import os
from peewee import *
from models import *
import pandas as pd

sys.path.append(os.path.abspath(os.path.dirname(__file__)) + '/../')
from utils import *

class db_api:
    def __init__(self):
        self.utils = utils()
        self.config = self.utils.load_config()

        self.json_f = self.config['json']
        fp = open(self.json_f, 'r')
        self.json = json.load(fp)
        fp.close()

        self.db = MySQLDatabase(self.json['db_name'],
                                host = self.json['db_ip'],
                                port = self.json['db_port'],
                                user = self.json['db_user'],
                                passwd = self.json['db_pwd'])

    def db_connect(self):
        try:
            self.db.connect()
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

    def get_ts_points(self, timeserie):
        self.db_connect()

        points = []
        for point in Points.select().where(Points.timeseries_pk == timeserie.pk):
            points.append(point)

        self.db.close()

        return points

    def get_msu_full_df(self, msu):
        cols = {}
        timeseries = self.get_msu_timeseries(msu)

        #we assume that times are equal across metrics
        cols['TIME'] = [datapoint.ts for datapoint in self.get_ts_points(timeseries[0])]

        for ts in timeseries:
            datapoints = self.get_ts_points(ts)
            cols[ts.statistic.name] = [datapoint.val for datapoint in datapoints]

        return pd.DataFrame(cols)
