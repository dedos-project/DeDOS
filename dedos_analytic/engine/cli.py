import yaml
from .daemon import daemon 
from .api import DaemonApi
from ..logger import *

api = None
do_quit = False

def start():
    api.start_daemon()

def train(start=None, end=None):
    if start is not None:
        start = float(start)
    if end is not None:
        end = float(end)

    api.train(start, end)

def classify(start=None, end=None):
    if start is not None:
        start = float(start)
    if end is not None:
        end = float(end)

    api.classify(start, end)

def help():
    print "Available Functions: {}".format(fn_map.keys())

def cfg(filename):
    api.set_config_file(filename)

def connect():
    api.connect_to_daemon()

def shutdown():
    api.shutdown_daemon()
    print "Shut down daemon! Exiting now."
    global do_quit
    do_quit = True

def quit():
    print "Exiting..."
    global do_quit
    do_quit = True
    daemon.is_running = False

fn_map = dict(
        start=start,
        cfg=cfg,
        connect=connect,
        train=train,
        classify=classify,
        help=help,
        quit=quit,
        shutdown=shutdown
)

def prompt():
    input = raw_input(">>> ")

    cmd = input.split()

    if len(cmd) and cmd[0] in fn_map:
        fn_map[cmd[0]](*cmd[1:])
    else:
        log_error("Command {} not recognized".format(cmd))

def start(config_file):
    global api
    config = yaml.load(open(config_file))
    api = DaemonApi(config)
    while not do_quit:
        prompt()

