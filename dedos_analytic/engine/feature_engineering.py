import os
import sys
import copy
import pandas as pd
import numpy as np
from ..logger import *

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
 "ERROR_COUNT",
 "MSU_USER_TIME", "MSU_KERNEL_TIME",
 "MSU_MINOR_FAULTS", "MSU_MAJOR_FAULTS",
 "MSU_VOL_CTX_SW","MSU_INVOL_CTX_SW",
)

def make_numeric(df):
    for x in df.columns:
        try:
            df[x] = pd.to_numeric(df[x])
        except ValueError:
            pass

def make_rate_df(df, types=AGGREGATE_STAT_TYPES):
    log_debug('Converting dataframe to numeric form')
    make_numeric(df)
    log_debug('Converting to increase rate')
    df_cp = copy.copy(df)
    for t in types:
        df_cp[t] = df[t].diff() / df.TIME.diff()
        df = df[~np.isnan(df_cp[t])]
        df_cp = df_cp[~np.isnan(df_cp[t])]
    return df_cp


def scale_data(X, columns_to_keep=None, scalers=None, return_scalers=False):
    init_cols = X.columns

    if columns_to_keep is None:
        x1 = X[[c for c in init_cols if not c.startswith('traffic') and not c == 'msu_type']]
    else:
        x1 = X[[c for c in init_cols if any(c.startswith(c2) for c2 in columns_to_keep)]]

    scaler = preprocessing.StandardScaler()

    if scalers is None:
        scalers = {}

    for field in x1.columns:
        if field not in scalers:
            scalers[field] = preprocessing.StandardScaler()
            scalers[field].fit(x1[field].values.reshape(-1,1))
        x1 = x1.assign(**{field: scalers[field].transform(x1[field].values.reshape(-1,1))})

    if return_scalers:
        return x1, scalers

    return x1
