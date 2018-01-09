import json
import os
import pandas as pd

def read_event_log(event_log_file, columns, event_types):
    event_log = json.load(open(event_log_file))
    events = pd.DataFrame(event_log, columns=columns)
    events = events.rename(columns={c: c.strip("_") for c in columns})
    events = events[events.name.isin(event_types)]
    events.time *= 1e9
    return events

def get_traffic_times(events):
    start_events = events[events.status == 'start'].sort_values(by='name').reset_index()
    end_events = events[events.status == 'end'].sort_values(by='name').reset_index()

    timeframes = pd.concat((start_events.name,
                            start_events.time.rename("start"),
                            end_events.time.rename('end')),
                           axis=1).sort_values(by='start')
    return timeframes

def read_traffic_events(config, columns=('time_', 'status_', 'name_')):

    event_log_file = os.path.join(config['event_log_dir'], config['dataset'], 'event_log.json')

    events = read_event_log(event_log_file, columns, config['event_types'])

    return get_traffic_times(events)

