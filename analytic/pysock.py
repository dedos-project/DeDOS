import zmq
context = zmq.Context()

socket = context.socket(zmq.REQ)
socket.connect("ipc:///tmp/dedosipc")

while True:
    x = input("cmd:")
    socket.send(bytes(x, 'utf-8'))
    message = socket.recv()
    print("Received %s" % message)
