#!/usr/bin/python

from sklearn.cluster import KMeans

def do_kmeans(X, k):
    k_means = KMeans(n_clusters = k)
    k_means.fit(X)

    return k_means
