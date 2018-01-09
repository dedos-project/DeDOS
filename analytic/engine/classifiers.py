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

def do_dbscan(X, e, m):
    return DBSCAN(eps = e, min_samples = m, n_jobs = -1).fit(X)

def do_lof(k):
    return LocalOutlierFactor(n_neighbors = k)

def do_svm(X, k, g):
    return svm.OneClassSVM(kernel = k, gamma = g).fit(X)

def do_isolation_forest(X, m, r):
    return IsolationForest(max_samples = m, random_state = r).fit(X)

#https://stackoverflow.com/questions/27822752/scikit-learn-predicting-new-points-with-dbscan
def dbscan_predict(model, X_new, metric=sp.spatial.distance.cosine):
    # Result is noise by default
    y_new = np.ones(shape = len(X_new), dtype=int) * -1

    # Iterate all input samples for a label
    for j, x_new in enumerate(X_new):
        # Find a core sample closer than EPS
        for i, x_core in enumerate(model.components_):
            if metric(x_new, x_core) < model.eps:
                # Assign label of x_core to x_new
                y_new[j] = model.labels_[model.core_sample_indices_[i]]
                break

    return y_new
