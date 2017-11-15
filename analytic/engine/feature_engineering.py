import os
import sys
import pandas as pd

sys.path.append(os.path.abspath(os.path.dirname(__file__)) + '/../database/')

from db_api import *

db = db_api()
msus = db.get_items('msus')

ts = db.get_msu_timeseries(msus[0])

pts = db.get_ts_points(ts[0])

vals = [pt.val for pt in pts]
times = [pt.ts for pt in pts]

df = pd.DataFrame({'timestamp': times,
                  'value'     :vals})
print df
