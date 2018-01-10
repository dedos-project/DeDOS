import yaml
import sys
import os

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print "Usage: {} <cfg_file>".format(sys.argv[0])
        exit()

    sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    from dedos_analytic.engine.cli import start as start_cli

    start_cli(sys.argv[1])
