#!/usr/bin/python
import os
import sys
import time
import matplotlib.pyplot as plt

sys.path.append(os.path.abspath(os.getcwd() + '/../database'))
import db_api

from classifiers import *
from feature_engineering import *

#############################################################################
# Python daemon:                                                            #
# - On startup, train a new model per object type specified in conf file    #
# - Use all data from the DB, or from the specificed time range             #
# - Model for object types are also specific in conf file                   #
# - Periodically pull new data from the DB. If none available do nothing (?)#
# - else, use a model to classify the state of the object (normal/abnormal) #
#############################################################################

def pre_process(df):
    # Feature engineer
    gen_activity_ratio(df)
    gen_mem_per_state(df)

    # Scale fields between {0,1}
    scale_fields = ['EXEC_TIME', 'IDLE_TIME', 'QUEUE_LEN', 'MEMORY_ALLOCATED', 'MEM_PER_STATE']
    normalize_df(df, scale_fields)

# Load system info
db = db_api.db_api()
msus = db.get_items('msus')
#FIXME: dirty length control to work with few msus first
msus = [msus[3]]

# Features to operate on
fields = ['EXEC_TIME', 'IDLE_TIME', 'QUEUE_LEN', 'MEMORY_ALLOCATED', 'ACTIVITY_RATIO',
          'MEM_PER_STATE']

# Get all data from beginning of time and treat data
dataframes = {}
models = {}
for msu in msus:
    df = db.get_msu_full_df(msu)

    pre_process(df)

    #FIXME: simulate that we have only points up to 1.5119001064880338e+18
    dataframes[msu] = df[df.TIME <= 1.5119001064880338e+18]

    # Fit one model for each MSU
    train_set = df.loc[:, fields]
    models[msu] = do_dbscan(train_set, 0.3, 500)

last_timestamp = dataframes[msus[0]].TIME.tail(1)

while (True):
    #TODO: check for new MSUs

    # Get new data
    for msu in msus:
        print 'getting new data until:' + str(last_timestamp)
        df = db.get_msu_full_df(msu, last_timestamp)
        print df

        if df.empty:
            print 'nothing new, sleeping...'
            time.sleep(1)
            next
        else:
            pre_process(df)

            print 'processing new data...'
            X_new = df.loc[:, fields]
            '''
            for row in X_new.iterrows():
                y_hat = dbscan_predict(models[msu], row)
                if y_hat == -1:
                    print "we have an outlier"
            '''

            last_timestamp = df.TIME.tail(1)
            print 'updated timestamp:' + str(last_timestamp)
