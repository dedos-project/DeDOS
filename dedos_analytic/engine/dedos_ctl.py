from ..logger import *
import zmq
import json

zctx = zmq.Context.instance()

class DedosCtl():

    def __init__(self, config):
        self.init_dfg = json.load(open(config['dedos_dfg']))
        self.config = config

        self.socket = zctx.socket(zmq.REQ)
        sock_loc = "tcp://{dfg[global_ctl_ip]}:{cfg[dedos_control_port]}" \
                   .format(cfg=self.config, dfg=self.init_dfg)
        self.socket.connect(sock_loc)

        log_info("Connection to dedos at {} initialized".format(sock_loc))

    def send_message(self, message):
        self.socket.send(str(message))
        log_info("Sent {} to DeDOS".format(message))
        rtn = self.socket.recv()
        log_info("Received {} from DeDOS".format(rtn))
        return rtn
