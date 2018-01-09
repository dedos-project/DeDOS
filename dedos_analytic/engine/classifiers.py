#!/usr/bin/python

from sklearn import svm
from sklearn.cluster import KMeans
from sklearn.cluster import DBSCAN
from sklearn.neighbors import LocalOutlierFactor
from sklearn.ensemble import IsolationForest
import scipy as sp
import numpy as np

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
