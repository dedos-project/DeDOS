from ..logger import *
import zmq
import json
from threading import Thread, Lock

zctx = zmq.Context.instance()

class DedosCtl():

    def __init__(self, config):
        self.init_dfg = json.load(open(config['dedos_dfg']))
        self.config = config

        self.ctl_socket = zctx.socket(zmq.REQ)
        sock_loc = "tcp://{dfg[global_ctl_ip]}:{cfg[dedos_control_port]}" \
                   .format(cfg=self.config, dfg=self.init_dfg)
        self.ctl_socket.connect(sock_loc)

        log_info("Connection to dedos ctl at {} initialized".format(sock_loc))

        self.dfg_socket = zctx.socket(zmq.SUB)
        sock_loc = "tcp://{dfg[global_ctl_ip]}:{cfg[dfg_broadcast_port]}" \
                    .format(cfg=self.config, dfg=self.init_dfg)
        self.dfg_socket.connect(sock_loc)
        self.dfg_socket.subscribe('DFG')

        self.dfg_lock = Lock()
        self.dfg = self.init_dfg
        self.is_running = True

    def _monitor_dfg(self):
        while self.is_running:
            topic, msg = self.dfg_socket.recv_multipart()

            dfg = json.loads(str(msg))
            self.dfg_lock.acquire()
            self.dfg = dfg
            self.dfg_lock.release()

    def start_dfg_monitor(self):
        self.is_running = True
        Thread(target=self._monitor_dfg).start()

    def stop_dfg_monitor(self):
        self.is_running = False

    def send_message(self, message):
        self.ctl_socket.send(str(message))
        log_info("Sent {} to DeDOS".format(message))
        rtn = self.ctl_socket.recv()
        log_info("Received {} from DeDOS".format(rtn))
        return rtn

    def n_msus(self, dfg, type_id):
        return len([msu for msu in dfg['MSUs'] if msu['type'] == type_id]) + \
               self.msu_diff[type_id]

    @classmethod
    def get_type_id(cls, dfg, name):
        for type in dfg['MSU_types']:
            if type['name'] == name:
                return type['id']

    @classmethod
    def most_recent_msu(cls, dfg, name):
        type_id = cls.get_type_id(dfg, name)
        msus = [msu for msu in dfg['MSUs'] if msu['type'] == type_id]
        msus = sorted(msus, key=lambda m: m['id'])
        return msus[-1]

    def action(self, msu_type, groups):
        noise = groups[-1] if -1 in groups else 0
        grouped = sum(x for g, x in groups.items() if g != -1)
        if float(noise) / (noise + grouped) > self.config['clone_threshold']:
            if msu_type in self.config['target_msu_types']:
                msu_type = self.config['target_msu_types'][msu_type]
            self.dfg_lock.acquire()
            msu = self.most_recent_msu(self.dfg, msu_type)
            self.dfg_lock.release()
            self.msu_diff
            return 'clone msu {0[id]}'.format(msu)

        if float(noise) / (noise + grouped) < self.config['remove_threshold']:
            if msu_type in self.config['target_msu_types']:
                msu_type = self.config['target_msu_types'][msu_type]
            self.dfg_lock.acquire()
            type_id = self.get_type_id(self.dfg, msu_type)
            if self.n_msus(self.dfg, type_id) > self.n_msus(self.init_dfg, type_id):
                msu = self.most_recent_msu(self.dfg, msu_type)
                cmd = 'unclone msu {0[id]}'.format(msu)
            else:
                log_info("Would remove, but not enough MSUs left")
                cmd = None
            self.dfg_lock.release()
            return cmd


    def act(self, classification):
        actions = 0
        for msu_type, groups in classification.items():
            action = self.action(msu_type, groups)
            if action is not None:
                actions += 1 
                self.send_message(action)
        return actions
