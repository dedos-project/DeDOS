import os
import sys
import pandas as pd
from classifiers import *

sys.path.append(os.path.abspath(os.path.dirname(__file__)) + '/../database/')

from db_api import *

db = db_api()
msus = db.get_items('msus')

for msu in msus:
    print msu.msu_type.name

ts = db.get_msu_timeseries(msus[0])

pts = db.get_ts_points(ts[0])

vals = [pt.val for pt in pts]
times = [pt.ts for pt in pts]

df = pd.DataFrame({'timestamp': times,
                  'value'     :vals})

df = db.get_msu_full_df(msus[0])

X = df.loc[:, ['QUEUE_LEN', 'EXEC_TIME', 'IDLE_TIME']]

k_means_model = do_kmeans(X, 5)
