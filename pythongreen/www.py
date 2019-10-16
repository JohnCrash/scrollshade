# -*- coding: utf-8 -*-
import zmq
import json
from flask import Flask,escape,request,url_for,redirect,abort

context = zmq.Context()
socket = context.socket(zmq.REQ)
socket.connect("tcp://localhost:5555")

app = Flask(__name__)

def sendCommand(obj):
    socket.send(json.dumps(obj))
    return socket.recv()

@app.route('/')
def index():
    return redirect('/static/index.html')

#取当前温度以及风扇和加速器状态
@app.route('/state')
def state():
    s = sendCommand({'cmd':'state'})
    return s

#强制打开风扇和加速器一定时间
@app.route('/force/<device>')
def set(device):
    s = sendCommand({'cmd':'set','arg':[device]})
    print(s)
    return s