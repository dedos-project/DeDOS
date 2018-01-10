import yaml
import sys
import os

if __name__ == '__main__':
    if len(sys.argv) != 1:
        print "Usage: {}".format(sys.argv[0])
        exit()

    sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    from dedos_analytic.engine.daemon import daemon
    
    daemon.start()
