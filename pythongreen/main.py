# -*- coding: utf-8 -*-
import time
import math
import RPi.GPIO as GPIO
import Adafruit_DHT
import sqlite3
from flask import Flask,escape,request
from multiprocessing import Process,Queue

cmd = Queue()
sensorType = 22 #dht22温湿度传感器
sensorPin = 18 #dht22读取pin
jspowerPin = 11 #加速器电源pin
jsswitchPin = 13 #加湿器开关
fanPin = 12 #风扇pin
isfanopen = False
isjsopen = False
jsopentime = 0
last_min = 0

GPIO.setmode(GPIO.BOARD)
GPIO.setup(jspowerPin,GPIO.OUT)
GPIO.setup(jsswitchPin,GPIO.OUT)
GPIO.setup(fanPin,GPIO.OUT)

#每分钟将数据写入到数据库中
def writeSql60(temp,humi):
    global last_min
    t = time.localtime()
    if last_min!=t.tm_min:
        try:
            print('write data')
            conn = sqlite3.connect('./db/green.db')
            conn.execute("INSERT INTO data VALUES (datetime(),?,?)",(math.floor(temp*10)/10,math.floor(humi*10)/10))
            conn.commit()
            conn.close()
        except sqlite3.Error as e:
            print(e)
    last_min = t.tm_min

def openJs(b):
    global isjsopen,jsopentime
    if b:
        if not isjsopen:
            #首先需要给加湿器模块供电
            GPIO.output(jspowerPin,GPIO.HIGH)
            #打开加湿器需要一个下降波形
            GPIO.output(jsswitchPin,GPIO.HIGH)
            time.sleep(0.2)
            GPIO.output(jsswitchPin,GPIO.LOW)
            time.sleep(0.2)
            GPIO.output(jsswitchPin,GPIO.HIGH)
            isjsopen = True
            jsopentime = time.time()
    else:
        if isjsopen:
            GPIO.output(jspowerPin,GPIO.LOW)
            isjsopen = False
    #加湿操作最长不能超过2小时
    if isjsopen and time.time()-jsopentime>2*3600:
        openJs(False)

def openFan(b):
    global isfanopen
    if b:
        if not isfanopen:
            GPIO.output(fanPin,GPIO.HIGH)
            isfanopen = True
    else:
        if isfanopen:
            GPIO.output(fanPin,GPIO.LOW)
            isfanopen = False

def MainLoop():
    global cmd
    while True:
        humi,temp = 0,0
        try:
            humi,temp = Adafruit_DHT.read_retry(sensorType,sensorPin)
            hour = time.localtime().tm_hour
            print('hour=%d Temp=%.2f  Humidity=%.2f%%'%(hour,humi,temp))
            #当温度高于30度或者湿度大于80%打开风扇，当湿度低于55打开加湿，70停止加湿
            #每天6点和18点后打开风扇1小时
            if temp>30 or hour==6 or hour==16:
                openFan(True)
            else:
                openFan(False)
            #风扇打开时停止加湿
            if humi>70 or isfanopen:
                openJs(False)
            elif humi<55 and humi>5:
                openJs(True)
            writeSql60(temp,humi)
            while not cmd.empty() :
                print(cmd.get())
            time.sleep(1)
        except ValueError as e:
            print(e)
            continue
        except RuntimeError as e:
            print(e)
            continue
        except Exception as e:
            print(e)
            continue

def B():
    global cmd
    while True:
        cmd.put('hello B')
        time.sleep(5)

Main = Process(target=MainLoop)
B = Process(target=B)
B.start()
Main.start()

app = Flask(__name__)
@app.route('/')
def hello_world():
    global cmd
    cmd.put('do it')
    return 'Hello, World!'