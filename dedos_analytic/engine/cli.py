import yaml
from .daemon import daemon 
from .api import DaemonApi
from ..logger import *

api = None
do_quit = False

def start():
    ''' Starts the Daemon running in the same process as the CLI'''
    api.start_daemon()

def train(start=None, end=None):
    ''' Tells the daemon to train the classifier.
    If arguments are omitted, trains on all available data.
    Times are provided in seconds since the first recorded data point.
    Arguments:
        start: Beginning of time period on which to train
        end: End of time period on which to train'''
    if start is not None:
        start = float(start)
    if end is not None:
        end = float(end)

    api.train(start, end)

def classify(start=None, end=None):
    ''' Tells the daemon to classify a portion of data
    If arguments are omitted, trains on all available data.
    Times are provided in seconds since the first recorded data point.
    Arguments:
        start: Beginning of time period to classify
        end: End of time period to classify'''
    if start is not None:
        start = float(start)
    if end is not None:
        end = float(end)

    api.classify(start, end)

def help_(name=None):
    ''' Prints this message '''

    if name is not None:
        if name not in fn_map:
            print "Invalid function {}".format(name)
            return
        fn = fn_map[name]
        print "{0} {1}: {2.__doc__}".format(name, " ".join(fn.__code__.co_varnames), fn)
        return

    for name in fn_map:
        help_(name)
        print ""

def cfg(filename):
    ''' Sets the configuration file for CLI and Daemon
    Arguments:
        filename: Path to configuration file'''
    api.set_config_file(filename)

def connect():
    ''' Connects to an already-running instance of the daemon'''
    api.connect_to_daemon()

def shutdown():
    ''' Shuts down a running daeomn'''
    api.shutdown_daemon()
    print "Shut down daemon! Exiting now."
    global do_quit
    do_quit = True

def quit():
    ''' Quits the CLI, and shuts down the daemon if it was started by the CLI '''
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
        help=help_,
        quit=quit,
        shutdown=shutdown
)

def prompt():
    input = raw_input(">>> ")

    cmd = input.split()

    if len(cmd) and cmd[0] in fn_map:
        try:
            fn_map[cmd[0]](*cmd[1:])
        except Exception as e:
            print "Error: {}".format(e)
    else:
        log_error("Command {} not recognized".format(cmd))

def start(config_file):
    global api
    config = yaml.load(open(config_file))
    api = DaemonApi(config)
    while not do_quit:
        prompt()

