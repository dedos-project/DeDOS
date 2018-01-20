import yaml
import sys
import os

if __name__ == '__main__':
    if len(sys.argv) not in (2,3):
        print "Usage: {} <cfg_file> [log_dir]".format(sys.argv[0])
        exit()

    if len(sys.argv) == 3:
        logdir = sys.argv[2]
    else:
        logdir = None

    sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    from dedos_analytic.engine.cli import start as start_cli

    start_cli(sys.argv[1], logdir)
