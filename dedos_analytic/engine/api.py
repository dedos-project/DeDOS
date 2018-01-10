import yaml

from .daemon import daemon
from .daemon import DaemonMessage
from dedos_analytic.database.db_api import DbApi

class DaemonApi():

    last_end = None

    def __init__(self, config):
        self.db = DbApi(config)
        self.config = config

    def set_config_file(self, config_file):
        self.config = yaml.load(open(config_file))
        self.db = DbApi(self.config)
        msg = DaemonMessage.configure_message(self.config)
        daemon.send_message(msg)

    def start_daemon(self):
        daemon.init_dbscanner(self.config)
        daemon.set_config(self.config)
        daemon.run()

    def connect_to_daemon(self):
        daemon.connect()
        msg = DaemonMessage.configure_message(self.config)
        daemon.send_message(msg)

    def shutdown_daemon(self):
        msg = DaemonMessage.shutdown_message()
        daemon.send_message(msg)

    def train(self, start=0, end=None, name='_MAIN'):

        if start is None:
            start = 0

        if start > 0:
            start = self.db.get_start_time() + start * 1e9

        if end is not None:
            end = self.db.get_start_time() + end * 1e9

        msg = DaemonMessage.train_message(start, end, name)
        daemon.send_message(msg)

        if end is not None:
            self.last_end = end
        else:
            self.last_end = self.db.get_end_time()

    def classify(self, start=None, end=None, name='_MAIN'):

        if start is not None:
            start = self.db.get_start_time() + start * 1e9
        else:
            start = self.last_end

        if end is not None:
            end = self.db.get_start_time() + end * 1e9

        msg = DaemonMessage.classify_message(start, end, name)
        daemon.send_message(msg)

        if end is not None:
            self.last_end = end
        else:
            self.last_end = self.db.get_end_time()
