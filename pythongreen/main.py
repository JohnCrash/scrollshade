# -*- coding: utf-8 -*-
import time
import RPi.GPIO as GPIO
import Adafruit_DHT

sensorType = 22
sensorPin = 18

isfanopen = False
isjsopen = False

def openJs(b):
    pass

def openFan(b):  
    pass

while True:
    humi,temp = 0,0
    try:
        humi,temp = Adafruit_DHT.read_retry(sensorType,sensorPin)
    except ValueError as e:
        print(e)
        continue
    except RuntimeError as e:
        print(e)
        continue
    hour = time.localtime().tm_hour
    print('hour=%d Temp=%.2f  Humidity=%.2f%%'%(hour,humi,temp))
    #当温度高于30度或者湿度大于80%打开风扇，当湿度低于55打开加湿，70停止加湿
    #每天6点和18点后打开风扇1小时
    if temp>30 or hour==6 or hour==16:
        openFan(True)
    else:
        openFan(False)
    if humi>70:
        openJs(False)
    elif humi<55 and humi>5:
        openJs(True)
    time.sleep(1)
