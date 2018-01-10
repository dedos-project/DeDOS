import copy
import pandas as pd
import numpy as np
from ..logger import *


AGGREGATE_STAT_TYPES = (
 "ERROR_COUNT",
 "MSU_USER_TIME", "MSU_KERNEL_TIME",
 "MSU_MINOR_FAULTS", "MSU_MAJOR_FAULTS",
 "MSU_VOL_CTX_SW","MSU_INVOL_CTX_SW",
)

def sample_msus_by_type(msus):
    types = set([msu.msu_type.name for msu in msus])
    return [[m for m in msus if m.msu_type.name == t][0] for t in types]

def resolution_round(x, resolution):
    return ((x / resolution).round() * resolution).astype(x.dtype)

def average_within_epoch(df):
    return df.groupby('epoch', as_index=False).mean().drop('msu_id',1)

def average_within_type_epoch(df):
    return df.groupby(['msu_type', 'epoch'], as_index=False).mean().drop('msu_id', 1)

def label_with_traffic(df, traffic):
    mask = (df.TIME > min(traffic.start)) & (df.TIME < max(traffic.end))

    log_debug("Filtering out {} pre/post-traffic points".format(np.sum(~mask)))
    log_debug("{} points remaining".format(np.sum(mask)))
    df = df[mask]
    log_debug("Range of time: {} seconds ".format((max(df.TIME) - min(df.TIME))*1e-9))

    ordered = traffic.sort_values(by='start')
    df = df.assign(traffic='')
    for i, attack in ordered.iterrows():
        mask = (df.TIME > attack.start) & (df.TIME < attack.end)
        log_debug("Marking %d points as type %s" % (mask.sum(), attack['name']))
        df.loc[(df.TIME > attack.start) & (df.TIME < attack.end), 'traffic'] = attack['name']
    return df


def separate_traffic_types(df):
    traffic_types = np.unique(df.traffic)
    separated = []
    for traffic in traffic_types:
        if traffic == 'client':
            continue
        traffic_df = df[df.traffic == traffic]
        traffic_start_time = min(traffic_df.TIME)
        traffic_end_time = max(traffic_df.TIME)
        last_traffic = df[(df.TIME < traffic_start_time) & (df.traffic != 'client')]
        if len(last_traffic) == 0:
            last_end = min(df.TIME)
        else:
            last_end = max(last_traffic.TIME)

        separated.append(df[(df.TIME > last_end) & (df.TIME < traffic_end_time)])
    return separated

def separate_msu_types(df):
    separated = []
    for msu_type in np.unique(df.msu_type):
        separated.append(df[df.msu_type == msu_type])
    return separated

def traffic_type(df):
    return df.iloc[-1].traffic

def separate_msu_type_per_traffic_type(df, traffic_msus):
    traffic_dfs = separate_traffic_types(df)

    msu_dfs = []
    for tdf in traffic_dfs:
        msu_dfs.append(tdf[tdf.msu_type == traffic_msus[traffic_type(tdf)]])

    return msu_dfs

def flatten_msus(df, metrics=None):
    msu_ids = np.unique(df.msu_id)
    cols = df.columns
    if metrics is None:
        stats = cols[(cols != 'TIME') & (cols != 'msu_id') & (cols != 'epoch')]
    else:
        stats = list(metrics) + ['traffic']

    tuples = [(mid, st) for mid in msu_ids for st in stats]
    new_cols = ['%s_%d' % (st, mid) for mid, st in tuples]
    new_df = pd.DataFrame(index=df.epoch.unique(), columns=new_cols)

    for (mid, st), col in zip(tuples, new_cols):
        msu_vals = df[df.msu_id == mid]
        vals = msu_vals.set_index('epoch')[[st]]
        new_df[[col]] = vals

    return new_df[~new_df.isnull().any(1)]


def unflatten_msus(df):
    cols = df.columns
    msu_ids = set(c.split('_')[-1] for c in cols)
    stats = set('_'.join(c.split('_')[:-1]) for c in cols)
    new_cols = list(stats)
    new_cols.append('msu_id')
    new_df = pd.DataFrame(columns = new_cols)

    for mid in msu_ids:
        msu_df = pd.DataFrame(columns = new_cols)
        for stat in stats:
            orig_name = '%s_%s' % (stat, mid)
            stat_msu_df = df[[orig_name]]
            stat_msu_df = stat_msu_df.rename(columns={orig_name: stat})
            stat_msu_df = stat_msu_df.assign(msu_id = mid)
            msu_df[[stat, 'msu_id']] = stat_msu_df

        new_df = pd.concat([new_df, msu_df])
    return new_df


