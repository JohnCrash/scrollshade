import zmq
from flask import Flask,escape,request

context = zmq.Context()
socket = context.socket(zmq.REQ)
socket.connect("tcp://localhost:5555")

app = Flask(__name__)

def sendCommand(s):
    socket.send(s)
    message = socket.recv()

@app.route('/')
def hello_world():
    sendCommand('fanOFF')
    return 'Hello, World!'