#!/usr/bin/python
import os
import sys
import time
import matplotlib.pyplot as plt

sys.path.append(os.path.dirname(os.path.dirname(__file__)))
import dedos_analytic.database.db_api as db_api

from dedos_analytic.engine.classifiers import *
from dedos_analytic.engine.feature_engineering import *

#############################################################################
# Python daemon:                                                            #
# - On startup, train a new model per object type specified in conf file    #
# - Use all data from the DB, or from the specificed time range             #
# - Model for object types are also specific in conf file                   #
# - Periodically pull new data from the DB. If none available do nothing (?)#
# - else, use a model to classify the state of the object (normal/abnormal) #
#############################################################################

def pre_process(df):
    make_numeric(df)

    # Normalize fields
    scale_fields = []
    df = normalize_df(df, scale_fields, 'minmax')

def main():
    # Load system info
    db = db_api.DbApi()
    msus = db.get_items('msus')
    #FIXME: dirty length control to work with few msus first
    msus = [msus[1]]

    # Features to operate on
    fields = ['MSU_USER_TIME', 'QUEUE_LEN']

    # Get all data from beginning of time and generate model
    dataframes = {}
    models = {}
    for msu in msus:
        df = db.get_msu_full_df(msu)

        pre_process(df)

        #dataframes[msu] = df[df.TIME <= 1.5119001064880338e+18]
        dataframes[msu] = df.loc[:, fields]

        # Fit one model for each object
        models[msu] = do_dbscan(dataframes[msu], 2, 20)

    last_timestamp = dataframes[msus[0]].tail(1).index.values[0]

    while (True):
        #TODO: check for new MSUs

        # Get new data
        for msu in msus:
            print 'getting new data since: ' + str(last_timestamp)
            df = db.get_msu_full_df(msu, last_timestamp)

            if df.empty:
                print 'nothing new, sleeping...'
                time.sleep(1)
                next
            else:
                pre_process(df)

                print 'processing new data...'
                X_new = df.loc[:, fields]

                for row in X_new.iterrows():
                    y_hat = dbscan_predict(models[msu], row)
                    if y_hat == -1:
                        print "we have an outlier"


                last_timestamp = df.tail(1).index.values[0]
                print 'updated timestamp:' + str(last_timestamp)


if __name__ == '__main__':
    main()
