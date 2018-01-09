import os
import sys
import copy
import pandas as pd
from sklearn import preprocessing

def make_numeric(df):
    for x in df.columns:
        try:
            df[x] = pd.to_numeric(df[x])
        except:
            pass

    return df

def normalize_df(df, fields, mode):
    if (mode == 'minmax'):
        scaler = preprocessing.MinMaxScaler()
    elif (mode == "zscore"):
        scaler = preprocessing.StandardScaler()
    else:
        print "invalid normalization mode"
        return df

    if not fields:
        for col in df.columns:
            df[col] = scaler.fit_transform(df[col].values.reshape(-1,1))
    else:
        for field in fields:
            df[field] = scaler.fit_transform(df[field].values.reshape(-1,1))

    return df

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

AGGREGATE_STAT_TYPES = (
 "QUEUE_LEN", "ERROR_COUNT",
 "MSU_USER_TIME", "MSU_KERNEL_TIME",
 "MSU_MINOR_FAULTS", "MSU_MAJOR_FAULTS",
 "MSU_VOL_CTX_SW","MSU_INVOL_CTX_SW",
)

def gen_stat_change_rate(df):
    for type in AGGREGATE_STAT_TYPES:
        df[type] = pd.to_numeric(df[type].diff()) / pd.to_numeric(df['TIME'].diff())
