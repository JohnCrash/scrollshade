#include <SPI.h>
#include <SD.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include "dht.h"
#include "RTClib.h"

#define EndStop_A0_Pin 12 //第一扇遮阳帘的收起
#define EndStop_A1_Pin 13 //第一扇遮阳帘的展开
#define EndStop_B0_Pin 8 //第二扇遮阳帘的收起   
#define EndStop_B1_Pin 9 //第二扇遮阳帘的展开
#define Open_A_Pin 3  //打开第一扇遮阳帘的按键
#define Close_A_Pin 4 //关闭第一扇遮阳帘的按键
#define Open_B_Pin 7  //打开第二扇遮阳帘的按键
#define Close_B_Pin 6 //关闭第二扇遮阳帘的按键 
#define MOTOR_A_PIN0 28 //卷帘电机A控制in0
#define MOTOR_A_PIN1 26
#define MOTOR_B_PIN0 24 
#define MOTOR_B_PIN1 22  
#define DHT22_PIN0 15
#define DHT22_PIN1 11
#define SET_DATETIME_PIN 5 //设置当前时钟
#define SWITCH_INDOOR_OUTDOOR_PIN 2 //切换室内室外温度，坚固开灯的作用
//温度湿度传感器DHT22资料
//https://playground.arduino.cc/Main/DHTLib
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
//温度传感器0
dht dht0;
dht dht1;

//lcd显示
LiquidCrystal_I2C	lcd(0x27,2,1,0,4,5,6,7);
//时间
RTC_DS3231 rtc;
byte lastHour = 255;
byte lastMinute = 255;
byte secs = 0;

struct hourlog{
	float temp0;
	float humi0;
	float temp1;
	float humi1;
};
char ilogs = 0;
hourlog hlogs[60];
//切换显示室内和室外
int lastSwitchIndoorOurdoor = HIGH;

int lcdlighs = 1000;

void EnableLCDLigh(){
	lcd.setBacklight(HIGH);
	lcdlighs = 1000;
}

void LCDLighCyle(){
	if(lcdlighs-- == 0){
		lcd.setBacklight(LOW);
	}
}

void switchIndoorOutdoor(){
	int state = digitalRead(SWITCH_INDOOR_OUTDOOR_PIN);
	if(state==LOW && lastSwitchIndoorOurdoor==HIGH){
		lastSwitchIndoorOurdoor = LOW;
		bIndoor = !bIndoor;
		EnableLCDLigh();
	}else if(state==HIGH&&lastSwitchIndoorOurdoor==LOW){
		lastSwitchIndoorOurdoor = HIGH;
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
//将一个小时的温度与湿度数据保存的sd
void writeHourlog(){
	DateTime now = rtc.now();
	if(now.minute()!=lastMinute){
		lastMinute = now.minute();
		
		if(now.hour()!=lastHour && ilogs){
			String filename = lowInt2(now.year())+lowInt2(now.month())+
								lowInt2(now.day())+
								lowInt2(now.hour())+".log";
			File f = SD.open(filename,FILE_WRITE);
			if(f){
				for(int i=0;i<ilogs;i++){
					f.println(String(hlogs[i].temp0,1)+"C"+
								String(hlogs[i].humi0,1)+"%"+
								String(hlogs[i].temp1,1)+"C"+
								String(hlogs[i].humi1,1)+"%");
				}
				f.close();
			}
			lastHour = now.hour();
			ilogs = 0;
		}else{
			ilogs++;
		}
	}
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
  int chk1 = dht1.read22(DHT22_PIN1);
  if(mode==0){
	  DateTime now = rtc.now();
	  //显示时钟00:00:00-
	  lcd.setCursor(0,0);
	  lcd.print(lowInt2(now.month())+"/"+
				lowInt2(now.day())+" "+
				lowInt2(now.hour())+":"+
				lowInt2(now.minute())+":"+
				lowInt2(now.second())+"   ");
	  
	  lcd.setCursor(0,1);
  }
  if(chk0==DHTLIB_OK){ //室内温度
	  hlogs[ilogs].temp0 = dht0.temperature;
	  hlogs[ilogs].humi0 = dht0.humidity;
	  //显示温度00C 00% 00C 00%
	  if(bIndoor && mode==0)
		lcd.print("id "+String(dht0.temperature,0)+"C "+String(dht0.humidity,0)+"% "+(A_auto_state?"1A":" ")+(B_auto_state?"2A":" ")+"    ");
  }else{
	  hlogs[ilogs].temp0 = 0;
	  hlogs[ilogs].humi0 = 0;
	  if(mode==0)lcd.print("1."+getDHTError(chk0));
  }
  if(chk1==DHTLIB_OK){ //室外温度
	  hlogs[ilogs].temp1 = dht1.temperature;
	  hlogs[ilogs].humi1 = dht1.humidity;
	  if(!bIndoor && mode==0)
		lcd.print("od "+String(dht1.temperature,0)+"C "+String(dht1.humidity,0)+"% "+(A_auto_state?"1A":" ")+(B_auto_state?"2A":" ")+"    ");
  }else{
	  hlogs[ilogs].temp1 = 0;
	  hlogs[ilogs].humi1 = 0;
	  if(!bIndoor &&mode==0)lcd.print("2."+getDHTError(chk1));	  
  }
  //每小时存入一次温度数据到sd卡中
  writeHourlog();
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
  digitalWrite(MOTOR_A_PIN0,LOW); //关闭电机
  digitalWrite(MOTOR_A_PIN1,LOW);
  digitalWrite(MOTOR_B_PIN0,LOW);
  digitalWrite(MOTOR_B_PIN1,LOW);	

  //初始化lcd
  lcd.begin(16,2);
  lcd.setBacklightPin(3,POSITIVE);
  EnableLCDLigh();
//  lcd.print("lcd init ok");
  //设置串口用于调试和命令传送
  Serial.begin(115200);
//  Serial.println("Serial ok");
//  lcd.setCursor(0,1);
//  lcd.setBacklight(LOW);
//  lcd.print("init sd card...");
//  lcd.setCursor(0,0);
//  lcd.print("<===>.....<===>.?@#$");
  //初始化SD card
  SDInit();
  //从外部芯片取出当前时间
  DateTime now = rtc.now();
  lastHour = now.day();
  lastMinute = now.minute();  
}

//
void manual_control_scroll(int esPin0,int esPin1,
            int openPin,int closePin,
            int motorPin0,int motorPin1,
            bool* pstate,bool* pswitching){
  int openPress = digitalRead(openPin);
  int closePress = digitalRead(closePin);
  int motorCmd = 0; //停止转动,1打开,2关闭
  if(openPress==LOW && closePress==LOW){ //都按下切换控制状态
    *pstate = true;
    *pswitching = true;
	EnableLCDLigh();
  }else if(openPress==HIGH && closePress==HIGH){ //没有命令
    *pswitching = false;
    if(*pstate){
      //自动控制,以一小时为周期做控制

    }
  }else if(openPress==LOW){ //命令打开
    if(!*pswitching){ //单独按键，没有进入切换状态
      *pstate = false;
      //open scroll
      if(digitalRead(esPin1)==HIGH){ //还未完全打开
        //继续打开
        motorCmd = 1;
      }
    }
	EnableLCDLigh();
  }else{ //命令关闭 
    if(!*pswitching){ //单独按键，没有进入切换状态
      *pstate = false;
      //close scroll
      if(digitalRead(esPin0)==HIGH){ //还未完全关闭
        //继续关闭
        motorCmd = 2;
      }
    } 
	EnableLCDLigh();
  }
  switch(motorCmd){
    case 0://停止
      digitalWrite(motorPin0,LOW);
      digitalWrite(motorPin1,LOW);
      break;
    case 1://打开
      digitalWrite(motorPin0,HIGH);
      digitalWrite(motorPin1,LOW);
      break;
    case 2://关闭
      digitalWrite(motorPin0,LOW);
      digitalWrite(motorPin1,HIGH);
      break;
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
	  manual_control_scroll(  EndStop_A0_Pin,EndStop_A1_Pin,
				  Open_A_Pin,Close_A_Pin,
				  MOTOR_A_PIN0,MOTOR_A_PIN1,
				  &A_auto_state,&A_switching_state);

	  manual_control_scroll(  EndStop_B0_Pin,EndStop_B1_Pin,
				  Open_B_Pin,Close_B_Pin,
				  MOTOR_B_PIN0,MOTOR_B_PIN1,
				  &B_auto_state,&B_switching_state);
				  
	  switchIndoorOutdoor();
  }
  setDateTime_cycle();
  LCDLighCyle();
  if(secs==100){
	temperature_storage_cycle();
	secs = 0;
  }else{
	  secs++;
  }
  delay(10);  
}
