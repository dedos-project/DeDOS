#!/usr/bin/python

from sklearn import svm
from sklearn.cluster import KMeans
from sklearn.cluster import DBSCAN
from sklearn.neighbors import LocalOutlierFactor
from sklearn.ensemble import IsolationForest
import scipy as sp
import numpy as np
from  dedos_analytic.database.db_api import DbApi
import feature_engineering as dfe
import data_manipulation as ddm
import json
import os

from ..logger import *

def do_kmeans(X, k):
    return KMeans(n_clusters = k).fit(X)

def do_dbscan(X, e, m, metric = 'euclidean', n_jobs = -1):
    return DBSCAN(eps = e, min_samples = m, metric = metric, n_jobs = n_jobs).fit(X)

def do_lof(k):
    return LocalOutlierFactor(n_neighbors = k)

def do_svm(X, k, g):
    return svm.OneClassSVM(kernel = k, gamma = g).fit(X)

def do_isolation_forest(X, m, r):
    return IsolationForest(max_samples = m, random_state = r).fit(X)

#https://stackoverflow.com/questions/27822752/scikit-learn-predicting-new-points-with-dbscan
def dbscan_predict(model, X_new, metric=sp.spatial.distance.euclidean):
   # find a core sample closer that EPS
   for i, x_core in enumerate(model.components_):
       if metric(X_new[1], x_core) < model.eps:
           # Return core point label
           return model.labels_[model.core_sample_indices_[i]]

   # New datapoint is noise otherwise
   return -1



class DbScanner():

    def __init__(self, config, log_dir = None):
        self.db = DbApi(config)
        self.metrics = config['msu_metrics']
        self.params = config['dbscan_params']
        self.models = {}
        self.ckdtrees = {}
        self.scalers = {}
        self.is_trained = False
        self.last_classification = None

        self.log_dir = log_dir
        if log_dir is not None:
            self.logs = {}
        else:
            self.logs = None

    def write_classification(self, X, labels, label, epoch, core_indices=None): 
        X['epoch']  = epoch
        X['label']  = labels
        if core_indices is None:
            X['core'] = False
        else:
            mask = np.zeros_like(labels, dtype=bool)
            mask[core_indices] = True
            X['core'] = mask
        with open(self.get_log(label, X), 'a') as f:
            X.to_csv(f, index=False, header=False)

    def dbscan_predict(self,  model, tree, X, type_name, epoch):
        dists, indices = tree.query(X)

        labels = model.labels_[model.core_sample_indices_[indices]]

        labels[dists > model.eps] = -1

        if self.log_dir is not None:
            self.write_classification(X, labels, type_name, epoch)

        unique_vals, counts = np.unique(labels, return_counts = True)
        return dict(zip(unique_vals, counts))

    def get_log(self, label, X):
        if self.logs is None:
            return None
        if label not in self.logs:
            self.logs[label] = os.path.join(self.log_dir, label)
            with open(self.logs[label], 'w') as f:
                f.write(','.join(X.columns) + '\n')
        return self.logs[label]


    def train(self, start=0, end = None):

        log_info("Training classifier on time range: {}:{}".format(start, end if end is not None else "end"))

        msus = self.db.get_items('msus')

        types = self.metrics.keys()

        msus = [msu for msu in msus if msu.msu_type.name in types]

        df = self.db.get_msus_epoch_df(msus, start, end)

        rate_df = dfe.make_rate_df(df)
        type_df = ddm.average_within_type_epoch(rate_df)

        type_dfs = ddm.separate_msu_types(type_df)

        for df in type_dfs:

            msu_type = df.msu_type.iloc[0]

            if msu_type not in self.metrics:
                log_error("Could not find metrics for msu type {}".format(msu_type))
                continue
            metrics = self.metrics[msu_type]

            if msu_type not in self.params:
                log_error("Could not find params for msu type {}".format(msu_type))
                continue

            X, self.scalers[msu_type] = dfe.scale_data(df, metrics, return_scalers = True)
            params = {
                    'eps': self.params[msu_type]['eps'],
                    'min_samples': self.params[msu_type]['percent_samples'] * len(X)
            }

            self.models[msu_type] = DBSCAN(**params).fit(X)

            if self.log_dir is not None:
                self.write_classification(X, self.models[msu_type].labels_, msu_type, df.epoch, self.models[msu_type].core_sample_indices_)

            if np.all(self.models[msu_type].labels_ == -1):
                log_warn("No classes detected for msu type {}, metrics {}, params {}"
                         .format(msu_type, metrics, params))
            else:
                log_info("{}: # core samples = {}".format(msu_type, len(self.models[msu_type].components_)))
                log_debug("Computing KDTree")
                self.ckdtrees[msu_type] = sp.spatial.cKDTree(self.models[msu_type].components_)
        self.is_trained = True

    def classify(self, start, end=None):
        msus = self.db.get_items('msus')

        classification = {}

        types = self.metrics.keys()

        msus = [msu for msu in msus if msu.msu_type.name in types]

        df = self.db.get_msus_epoch_df(msus, start, end)

        rate_df = dfe.make_rate_df(df)
        type_df = ddm.average_within_type_epoch(rate_df)

        type_dfs = ddm.separate_msu_types(type_df)

        for df in type_dfs:

            msu_type = df.msu_type.iloc[0]
            if msu_type not in self.models:
                log_warn("New msu type? id: {}".format(msu_type))
                continue

            if msu_type not in self.metrics:
                log_error("Could not find metrics for msu type {}".format(msu_type))
            metrics = self.metrics[msu_type]

            if msu_type not in self.params:
                log_error("Could not find params for msu type {}".format(msu_type))
            params = self.params[msu_type]

            X = dfe.scale_data(df, metrics, self.scalers[msu_type])

            if msu_type not in self.ckdtrees:
                log_warn("No core indices for msu type {}".format(msu_type))
                continue

            classification[msu_type] = self.dbscan_predict(self.models[msu_type], self.ckdtrees[msu_type], X, msu_type, df.epoch)

        for msu_type, cl in classification.items():
            log_info('MSU type {} Classification : {}'.format(msu_type, json.dumps(cl, sort_keys=True)))

        self.last_classification = classification
        return classification
