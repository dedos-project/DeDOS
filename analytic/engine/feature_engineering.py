import os
import sys
import pandas as pd
from sklearn import preprocessing

def normalize_df(df, fields):
    min_max_scaler = preprocessing.MinMaxScaler()

    for field in fields:
        df[field] = min_max_scaler.fit_transform(df[field].values.reshape(-1,1))

def gen_activity_ratio(df):
    non_zero = df.loc[(df.EXEC_TIME > 0) & (df.IDLE_TIME > 0)]
    act_ratio = non_zero.EXEC_TIME / (non_zero.EXEC_TIME + non_zero.IDLE_TIME)
    df.loc[(df.EXEC_TIME > 0) & (df.IDLE_TIME > 0), 'ACTIVITY_RATIO'] = act_ratio
    df.ACTIVITY_RATIO = df.ACTIVITY_RATIO.fillna(0)

def gen_mem_per_state(df):
    non_zero = df.loc[(df.NUM_STATES > 0) & (df.MEMORY_ALLOCATED > 0)]
    mem_ratio = non_zero.MEMORY_ALLOCATED / non_zero.NUM_STATES
    df.loc[(df.NUM_STATES > 0) & (df.MEMORY_ALLOCATED > 0), 'MEM_PER_STATE'] = mem_ratio
    df.MEM_PER_STATE = df.MEM_PER_STATE.fillna(0)
