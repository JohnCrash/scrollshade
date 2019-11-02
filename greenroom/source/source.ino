#include <SPI.h>
#include <SD.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include "dht.h"
#include "RTClib.h"
#include "Temp.h"

#define EndStop_A0_Pin 12 //第一扇遮阳帘的收起
#define EndStop_A1_Pin 13 //第一扇遮阳帘的展开
#define EndStop_B0_Pin 8 //第二扇遮阳帘的收起   
#define EndStop_B1_Pin 9 //第二扇遮阳帘的展开
#define Open_A_Pin 4  //打开第一扇遮阳帘的按键
#define Close_A_Pin 3 //关闭第一扇遮阳帘的按键
#define Open_B_Pin 7  //打开第二扇遮阳帘的按键
#define Close_B_Pin 6 //关闭第二扇遮阳帘的按键 
#define MOTOR_A_PIN0 28 //卷帘电机A控制in0
#define MOTOR_A_PIN1 26
#define MOTOR_B_PIN0 24 
#define MOTOR_B_PIN1 22  
#define DHT22_PIN0 14
#define DHT22_PIN1 15
#define JSOPEN_PIN 30
#define OUTDOOR_LIGH_PIN A1
#define OUTDOOR_TEMP_PIN A2
#define SET_DATETIME_PIN 5 //设置当前时钟
#define SWITCH_INDOOR_OUTDOOR_PIN 2 //切换室内室外温度，坚固开灯的作用
#define CLOSECYCLE_T (6*60*100L) 	 //关闭阀门的时间
#define OPENCYCLE_T (60*100L)    //打开阀门的时间
#define OPENCYCLE_T1 (30*100L) //上排开启时间
#define OPENCYCLE_T2 (30*100L) //下排开启时间
#define OPENFAN_T	 (2*60*100L)	//打开风扇的时间
#define CLOSEFAN_T (2*60*100L)	//关闭风扇的时间
//温度湿度传感器DHT22资料
//https://playground.ardu0ino.cc/Main/DHTLib
//使用的库
//https://github.com/RobTillaart/Arduino/tree/master/libraries/DHTlib
//时钟组件DS3231 
//https://github.com/adafruit/RTClib
//注意：DHT22和DS3231公用总线I2C,链接对应SDA和SCL到arduino上即可

//光线强度传感器
//外部时间扩展
//SD卡
//LCD显示

//全局状态如果为true表示对应卷帘是处于在自动控制状态
bool A_auto_state = true;   //初始状态是开启
bool A_switching_state = false; //正在切换状态中
bool B_auto_state = true; //初始状态是开启
bool B_switching_state = false;
bool bIndoor = true; //显示室内
bool isSensorFail = false; //湿度失效
//温度传感器0
dht dht0;
byte secs = 0;
//dht dht1;

//lcd显示
LiquidCrystal_I2C	lcd(0x27,2,1,0,4,5,6,7);

//时间
RTC_DS3231 rtc;

//切换显示室内和室外
int lastSwitchIndoorOurdoor = HIGH;

int lcdlighs = 1000;

//控制周期计数,每秒增加1满周期重置为0
long forcefan = 0;
long forcejs = 0;
bool isfanopen = false;
bool isjsopen = false;
bool isreleaseA = true;
bool isreleaseB = true;
DateTime now;
bool iscoolprogram = false; //进入冷却程序
int opjsminuts = 0;
uint32_t openjsts = 0; //加湿器开始时间

void EnableLCDLigh(){
	lcd.setBacklight(HIGH);
	lcdlighs = 100*60*5; //5分钟
}

void LCDLighCyle(){
	if(lcdlighs-- == 0){
		lcd.setBacklight(LOW);
	}
}

void(* resetFunc) (void) = 0; //declare reset function @ address 0

//切换内外温度显示
void switchIndoorOutdoor(){
	int state = digitalRead(SWITCH_INDOOR_OUTDOOR_PIN);
	
	if(state==LOW && lastSwitchIndoorOurdoor==HIGH){
		lastSwitchIndoorOurdoor = LOW;
		//关闭内外设置
		//bIndoor = !bIndoor;
		EnableLCDLigh();
	}else if(state==HIGH&&lastSwitchIndoorOurdoor==LOW){
		lastSwitchIndoorOurdoor = HIGH;
	}
	if(state==LOW){
		if(digitalRead(SET_DATETIME_PIN)==LOW){
			//重启
			resetFunc();
		}
	}
}

//取低两位不足补0
String lowInt2(int v){
	if(v<0){
		return "-"+lowInt2(-v);
	}else{
		if(v<10)
			return "0"+String(v,DEC);
		else if(v>99)
			return String(v%100,DEC);
		else return String(v,DEC);
	}
}

extern int mode;

//向日志文件追加写入
void writeLogString(String s){
	String filename =  lowInt2(now.year())+lowInt2(now.month())+lowInt2(now.day())+".log";
	File f = SD.open(filename,FILE_WRITE);
	if(f){
		f.println(s);
		f.close();
	}
}

String getDateString(){
	return lowInt2(now.hour())+':'+lowInt2(now.minute())+':'+lowInt2(now.second());
}

void log(String s){
	return writeLogString(getDateString()+' '+s);
}

void logop(String s){
	log(s+" ("+String(dht0.temperature,1)+"C"+String(dht0.humidity,1)+"%)");
}

String getDHTError(int chk){
  switch (chk)
  {
    case DHTLIB_OK:  
		return "Ok";
    case DHTLIB_ERROR_CHECKSUM: 
		return "Checksum error"; 
    case DHTLIB_ERROR_TIMEOUT: 
		return "Time out error"; 
    case DHTLIB_ERROR_CONNECT:
        return "Connect error";
    case DHTLIB_ERROR_ACK_L:
        return "Ack Low error";
    case DHTLIB_ERROR_ACK_H:
        return "Ack High error";
    default: 
		return "Unknown error"; 
  }	
}

//将每分钟的温度，湿度，光照，以一小时为周期存入SD卡中
//文件名命名为日期YYMMDDHH.log
void temperature_storage_cycle(){
  int chk0 = dht0.read22(DHT22_PIN0);
  now = rtc.now();
  if(mode==0){
	//显示时钟00:00:00-
	lcd.setCursor(0,0);
	lcd.print(lowInt2(now.month())+"/"+
			lowInt2(now.day())+" "+
			lowInt2(now.hour())+":"+
			lowInt2(now.minute())+":"+
			lowInt2(now.second()));

	lcd.setCursor(0,1);
  }
  if(chk0==DHTLIB_OK){ //室内温度
	//每分钟一次
	if(now.second()==0)
		log(String(dht0.temperature,1)+"C"+String(dht0.humidity,1)+'%');
	//Serial.println(String(dht0.temperature,DEC)+","+String(dht0.humidity,DEC));
	//显示温度00C 00% 00C 00%
	if(bIndoor && mode==0){
		String se = isSensorFail?"ERROR":"     ";
		lcd.print(String(dht0.temperature,0)+"C"+String(dht0.humidity,0)+String("% ")+se);
	}		
  }else{
	  if(mode==0)lcd.print("1."+getDHTError(chk0));
	  dht0.temperature = 0;
	  dht0.humidity = 0;
  }
  //读取户外的温度与光照强度
  //int lighv = 1024-analogRead(OUTDOOR_LIGH_PIN);
  //int tempv = analogRead(OUTDOOR_TEMP_PIN);
  
  /*
  if(mode==0){
    float r = 10*tempv/(1024-tempv); //10k
	  float temp = calcTemp(r) - 15; //修正
	  hlogs[ilogs].temp1 = temp;
	  hlogs[ilogs].light = lighv;
		if(!bIndoor){
				lcd.print(String(temp,0)+"C"+String(lighv)+"H ");
		}
  }
  */
}

void SDInit(){
  if (!SD.begin(53)) {
    Serial.println("Card failed, or not present");
  }else{
	Serial.println("Card ok.");
  }
}

void setup(){
  //设置输入出入模式
  pinMode(EndStop_A0_Pin,INPUT_PULLUP);
  pinMode(EndStop_A1_Pin,INPUT_PULLUP);
  pinMode(EndStop_B0_Pin,INPUT_PULLUP);
  pinMode(EndStop_B1_Pin,INPUT_PULLUP);
  pinMode(Open_A_Pin,INPUT_PULLUP);
  pinMode(Close_A_Pin,INPUT_PULLUP);
  pinMode(Open_B_Pin,INPUT_PULLUP);
  pinMode(Close_B_Pin,INPUT_PULLUP);
  pinMode(SWITCH_INDOOR_OUTDOOR_PIN,INPUT_PULLUP);
  pinMode(SET_DATETIME_PIN,INPUT_PULLUP);
  pinMode(MOTOR_A_PIN0,OUTPUT);
  pinMode(MOTOR_A_PIN1,OUTPUT);
  pinMode(MOTOR_B_PIN0,OUTPUT);
  pinMode(MOTOR_B_PIN1,OUTPUT);
  pinMode(JSOPEN_PIN,OUTPUT);
  digitalWrite(MOTOR_A_PIN0,LOW); //关闭电机
  digitalWrite(MOTOR_A_PIN1,LOW);
  digitalWrite(MOTOR_B_PIN0,LOW);
  digitalWrite(MOTOR_B_PIN1,LOW);	

  //初始化lcd
  lcd.begin(16,2);
  lcd.setBacklightPin(3,POSITIVE);
  //EnableLCDLigh();
  //设置串口用于调试和命令传送
  Serial.begin(115200);
  //初始化SD card
  SDInit();
  //从外部芯片取出当前时间
  now = rtc.now();
  openjsts = now.unixtime();
  log('START');
}

//打开或者关闭加湿
void openjs(bool b){
	if(b){
		if(!isjsopen && !isfanopen){ //风扇打开时不能打开加速器
			if(opjsminuts!=(now.minute()+now.hour()*60)){//不能快速打开或这关闭加速器最小周期一分钟（这是一种保护措施）
				isjsopen = true;
				logop(String("JSON"));
				digitalWrite(MOTOR_B_PIN0,HIGH);
				digitalWrite(MOTOR_B_PIN1,LOW);
				opjsminuts = now.minute()+now.hour()*60;
				openjsts = now.unixtime();
			}
		}
	}else{
		//当温度大于20度的时候每次最少打开1分钟，当温度大于25度的时候每次至少打开2分钟
		if(isjsopen){
			float ot = dht0.temperature;
			uint32_t dt = now.unixtime() - openjsts;
			if( iscoolprogram||ot<20 || (ot>=20 && ot<25 && dt>60) || (ot>=25 && dt>120)){
				logop(String("JSOFF ")+dt+"S ");
				digitalWrite(MOTOR_B_PIN0,LOW);
				digitalWrite(MOTOR_B_PIN1,LOW);
				isjsopen = false;
				openjsts = now.unixtime();
			}
		}
	} 
}

//加湿多少秒
void openjs2(int s){
	if(forcejs==0){
		forcejs = s*100L;
	}
}

//打开或者关闭风扇
//打开风扇期间关闭加速器
void openfan(bool b){
	if(b){
		if(!isfanopen){
			logop(String("FANON"));
		    digitalWrite(MOTOR_A_PIN0,HIGH);
     	    digitalWrite(MOTOR_A_PIN1,LOW);
			isfanopen = true;
		}
		openjs(false);
	}else{
		if(isfanopen){
			logop(String("FANOFF"));
		    digitalWrite(MOTOR_A_PIN0,LOW);
		    digitalWrite(MOTOR_A_PIN1,LOW);
			isfanopen = false;
		}
	}  
}

//早上6点到晚上6点，湿度低于65打开加湿器，85停止加湿
//早上6点到晚上6点，温度高于28打开风扇，低于关闭风扇
void evalve(){
	float ot = dht0.temperature;
	float oh = dht0.humidity;
	int hour = now.hour();
	int minuts = now.minute();

	if(!iscoolprogram && !isSensorFail){
		if(forcejs>0){
			openjs(true);
		}else{
			if(hour>=6 && hour<=18){ 
				if(oh>85){
					openjs(false);
				}else if(oh<65 && oh>5){
					openjs(true);
				}
			}else{//夜晚不进行调节
				openjs(false);
			}
		}
	}else if(isSensorFail){
		//湿度失效控制
		if(forcejs>0){
			openjs(true);
		}else{
			if(hour>=6 && hour<=18){
				//根据温度进行控制，温度在20度下1小时加湿30s
				//温度在20-25度1小时加湿60s大于25度半小时加湿1分钟
				if(ot<20){
					if(minuts==1 && now.second()==1)
						openjs2(30);
				}else if(ot>=20&&ot<25){
					if(minuts==1 && now.second()==1)
						openjs2(60);
				}else if(ot>=25 && ot<30){
					if((minuts==1||minuts==30)&& now.second()==1)
						openjs2(60);
				}else if(ot>=30){
					if((minuts==1||minuts==15||minuts==30||minuts==45)&& now.second()==1)
						openjs2(60);
				}
			}else{//夜晚不进行调节
				openjs(false);
			}
		}		
	}
	if(forcejs>0){
		forcejs--;
		if(forcejs==0){
			openjs(false);
		}
	}else if(forcejs<0){
		forcejs = 0;
	}
	//这里判断湿度传感器失效，如果加湿超过20秒湿度值还小于70,或者停止加湿1小时湿度仍然保持在80以上
	//湿度失效后只能通过手动重启来重置湿度传感器
	if(!isSensorFail){
		uint32_t dt = now.unixtime() - openjsts;
		if( (isjsopen && dt>=20&& oh<70) || (!isjsopen && dt>=3600 && oh>80)){
			isSensorFail = true;
			log("humidity sensor failed!");
			Serial.println("humidity sensor failed!");
			openjs(false);
		}
	}
	//风扇控制
	//1降温，2通风
	if(forcefan>0){
		openfan(true);
	}else{
		if(hour>=6 && hour<=18){
			//早中晚各通风5分钟
			if((hour==6||hour==12||hour==18)&& minuts==0 && forcefan<=0){
				forcefan = 5*60*100L; //增加5分钟
				forcejs = (5+2)*60*100L; //这里在通风结束后开2分钟加湿器
			}else{
				if(ot>=32){
					//降温程序，开风扇2分钟，开加湿器1分钟，停止2分钟。周期进行直到温度达到要求
					if(!iscoolprogram)
						logop(String("COOLING"));
					iscoolprogram = true;
					
					if(minuts % 5 == 1){
						openfan(false);
						openjs(true);
					}else if(minuts % 5 == 2 || minuts % 5 == 3){
						openjs(false);
						openfan(false);
					}else{
						openjs(false);
						openfan(true);					
					}
				}else{
					if(iscoolprogram)
						logop(String("COOLING STOP"));
					iscoolprogram = false;
					openfan(false);
				}
			}
		}else{//夜晚不进行调节
			iscoolprogram = false;
			openfan(false);
		}
	}
  if(forcefan>0)forcefan--;
	//手动控制风扇
	int openPressA = digitalRead(Open_A_Pin);
	int closePressA = digitalRead(Close_A_Pin);
	//openPressA 打开风扇5分钟
	if(openPressA==HIGH && closePressA==LOW){
		if(isreleaseA)
			forcefan += 5*60*100L; //增加5分钟
		isreleaseA = false;
		EnableLCDLigh();
	}else if(openPressA==LOW && closePressA==HIGH){
		if(isreleaseA){
			forcefan -= 5*60*100L; //减少5分钟
			if(forcefan<0)forcefan = 0;
		}
		isreleaseA = false;
		EnableLCDLigh();
	}else if(openPressA==HIGH && closePressA==HIGH){ //重置
		isreleaseA = true;
	}else{
		forcefan = 0;
		isreleaseA = false;
		EnableLCDLigh();
	}
	//手动控制湿度
	int openPressB = digitalRead(Open_B_Pin);
	int closePressB = digitalRead(Close_B_Pin);
	//openPressA 打开加湿器2分钟
	if(openPressB==HIGH && closePressB==LOW){
		if(isreleaseB)
			forcejs += 2*60*100L; //增加2分钟
		isreleaseB = false;
		EnableLCDLigh();
	}else if(openPressB==LOW && closePressB==HIGH){
		if(isreleaseB){
			forcejs -= 2*60*100L; //减少2分钟
			if(forcejs<0){
        forcejs = 0;
      }
		}
		isreleaseB = false;
		EnableLCDLigh();
	}else if(openPressB==HIGH && closePressB==HIGH){ //重置
		isreleaseB = true;
	}else{
		forcejs = 0;
		isreleaseB = false;
		EnableLCDLigh();
	}
}

//在设置时间时使用第一个电机控制来增减数字,如果不控制则不会修改。
//设置模式5秒钟不动将推出修改模式
int mode = 0; //0正常模式，1设置年,2设置月,3设置天,4设置小时,5设置分钟,6设置秒
int lastDateSetState = HIGH;
int modeCooldown = 0;
int modeCurrentValue = 0;
bool modeCurrentValueChange = false;

//切换到下个字段，必要的话保存上个字段的设置值
void setDateTimeValue(){
	if(modeCurrentValueChange){
		DateTime now = rtc.now();
		switch(mode){
			case 1: //year
			rtc.adjust(DateTime(modeCurrentValue,now.month(),now.day(),now.hour(),now.minute(),now.second()));
			break;
			case 2: //month
			rtc.adjust(DateTime(now.year(),modeCurrentValue,now.day(),now.hour(),now.minute(),now.second()));
			break;
			case 3: //day
			rtc.adjust(DateTime(now.year(),now.month(),modeCurrentValue,now.hour(),now.minute(),now.second()));
			break;
			case 4: //hour
			rtc.adjust(DateTime(now.year(),now.month(),now.day(),modeCurrentValue,now.minute(),now.second()));
			break;
			case 5: //minute
			rtc.adjust(DateTime(now.year(),now.month(),now.day(),now.hour(),modeCurrentValue,now.second()));
			break;
			case 6: //second
			rtc.adjust(DateTime(now.year(),now.month(),now.day(),now.hour(),now.minute(),modeCurrentValue));
			break;
		}
		
	}
}

int lastAddState = HIGH;
int lastSubState = HIGH;
void changeCurrentValue(int addPIN,int subPIN){
	int addPress = digitalRead(addPIN);
	int subPress = digitalRead(subPIN);
	if(addPress==HIGH&&lastAddState==LOW){
		lastAddState = HIGH;
		modeCurrentValueChange = true;
		modeCurrentValue++;
		lcd.setCursor(0,1);
		lcd.print(modeCurrentValue);
		lcd.print("      ");
		modeCooldown = 0;
	}else if(addPress==LOW&&lastAddState==HIGH){
		lastAddState = LOW;
	}
	if(subPress==HIGH&&lastSubState==LOW){
		lastSubState = HIGH;
		modeCurrentValueChange = true;
		modeCurrentValue--;
		lcd.setCursor(0,1);
		lcd.print(modeCurrentValue);		
		lcd.print("      ");
		modeCooldown = 0;
	}else if(subPress==LOW&&lastSubState==HIGH){
		lastSubState = LOW;
	}	
}

void setDateTime_cycle(){
	int state = digitalRead(SET_DATETIME_PIN);
	if(state==LOW&&lastDateSetState==HIGH){
		EnableLCDLigh();
		lastDateSetState = LOW;
		modeCooldown = 0;
		DateTime now = rtc.now();
		lcd.setCursor(0,0);
		switch(mode){
			case 0:
			    digitalWrite(MOTOR_A_PIN0,LOW); //关闭电机
				digitalWrite(MOTOR_A_PIN1,LOW);
			    digitalWrite(MOTOR_B_PIN0,LOW);
				digitalWrite(MOTOR_B_PIN1,LOW);	
				mode = 1;
				lcd.clear();
				lcd.print("year:");
				lcd.setCursor(0,1);
				modeCurrentValue = now.year();
				lcd.print(modeCurrentValue);
				break;
			case 1:
				setDateTimeValue();
				mode = 2;
				lcd.print("month:   ");
				lcd.setCursor(0,1);
				modeCurrentValue = now.month();
				lcd.print(modeCurrentValue);
				break;
			case 2:
				setDateTimeValue();
				mode = 3;
				lcd.print("day:   ");		
				lcd.setCursor(0,1);
				modeCurrentValue = now.day();
				lcd.print(modeCurrentValue);					
				break;
			case 3:
				setDateTimeValue();
				mode = 4;
				lcd.print("hour:   ");
				lcd.setCursor(0,1);				
				modeCurrentValue = now.hour();
				lcd.print(modeCurrentValue);					
				break;
			case 4:
				setDateTimeValue();
				mode = 5;
				lcd.print("minute:   ");	
				lcd.setCursor(0,1);				
				modeCurrentValue = now.minute();
				lcd.print(modeCurrentValue);	
				break;
			case 5:
				setDateTimeValue();
				mode = 6;
				lcd.print("second:   ");
				lcd.setCursor(0,1);				
				modeCurrentValue = now.second();
				lcd.print(modeCurrentValue);					
				break;
			case 6:
				setDateTimeValue();
				mode = 0;
				break;
		}
		lcd.print("      ");
		modeCurrentValueChange = false;		
	}else if(state==HIGH&&lastDateSetState==LOW){
		lastDateSetState = HIGH;
	}
	if(mode){
		//监控第一个卷帘的电机控制键，用以调整时间值
		changeCurrentValue(Open_A_Pin,Close_A_Pin);
		modeCooldown++;
		if(modeCooldown>500){
			setDateTimeValue();
			mode = 0;
			Serial.println("time over ser mode = 0");
		}
	}
}

void loop(){
	//正常的显示控制模式
  if(mode==0){ 
	  evalve();
	  switchIndoorOutdoor();
  }
	//设置时间以及时间显示
  setDateTime_cycle();
  LCDLighCyle();
  if(secs==100){
	//温度数据存储
	temperature_storage_cycle();
	secs = 0;
  }else{
	  secs++;
  }
	//每个循环100ms
  delay(10);  
}
