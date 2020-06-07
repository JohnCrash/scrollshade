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

#define STOP_PIN	38
#define EN_PIN		36
#define STEP_PIN	34
#define DIR_PIN		32
#define LIGHT_Pin 11 //开灯
#define RAINING_PIN EndStop_A1_Pin
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
bool isSensorFail = true; //湿度失效
bool isRoomEnabled = false; 
bool switch_done;
int shadeState = -1;

//温度传感器0
dht dht0;
int secs = 0;
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
bool islightopen = false;
bool isreleaseA = true;
bool isreleaseB = true;
DateTime now;
bool iscoolprogram = false; //进入冷却程序
int opjsminuts = 0;
uint32_t openjsts = 0; //加湿器开始时间

#define STEPDELAY1 5
#define STEPDELAY2 5
bool isGreenHouse = true;

void turnSteper(bool dir,int n){
	digitalWrite(EN_PIN,LOW); //打开步进电机
	digitalWrite(DIR_PIN,dir?HIGH:LOW);
	for(int i=0;i<n;i++){
		digitalWrite(STEP_PIN,HIGH);
		delay(STEPDELAY1);
		digitalWrite(STEP_PIN,LOW);
		delay(STEPDELAY2);
	}
	digitalWrite(EN_PIN,HIGH);
}

/**
 * 初始化步进电机在指定的角度停下
 */
void initSteper(){
	do{
		digitalWrite(EN_PIN,LOW); //打开步进电机
		digitalWrite(DIR_PIN,LOW);
		for(int i = 0;i < 200;i++){
			if(digitalRead(STOP_PIN)==LOW){
				turnSteper(true,8);
				isGreenHouse = true;
				return;
			}
			digitalWrite(STEP_PIN,HIGH);
			delay(STEPDELAY1);
			digitalWrite(STEP_PIN,LOW);
			delay(STEPDELAY2);
		}
		delay(1000);
	}while(1);
}

//b = true加湿温室
void switchJS(bool b){
	if(b && !isGreenHouse){
		turnSteper(false,67);
		isGreenHouse = true;
	}else if(!b && isGreenHouse){
		turnSteper(true,67);
		isGreenHouse = false;
	}
}

void EnableLCDLigh(){
	lcd.setBacklight(HIGH);
	lcdlighs = 10*60*5; //5分钟
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
		isRoomEnabled = !isRoomEnabled;
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
	while(1){
		int chk0 = dht0.read11(DHT22_PIN0);
		if(chk0==DHTLIB_OK){ //室内温度
			//每分钟一次
			log(String(dht0.temperature,1)+"C"+String(dht0.humidity,1)+'%');
			return;
		}
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
  pinMode(RAINING_PIN,INPUT_PULLUP);
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
  pinMode(LIGHT_Pin,OUTPUT);
  pinMode(STOP_PIN,INPUT_PULLUP);
  pinMode(EN_PIN,OUTPUT);
  pinMode(STEP_PIN,OUTPUT);
  pinMode(DIR_PIN,OUTPUT);

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
  //temperature_storage_cycle();
  //initSteper();
  if(isShadeEndStopPassed()){
	  shadeState = 0;
  }else{
	  shadeState = 2;
  }

  log('START');
}

//打开灯
void openLight(bool b){
	if(b){
		if(!islightopen){
			digitalWrite(LIGHT_Pin,HIGH);
			islightopen = true;
		}
	}else{
		if(islightopen){
			digitalWrite(LIGHT_Pin,LOW);
			islightopen = false;
		}
	}
}

//打开或者关闭加湿
void openjs(bool b){
	if(b){
		if(!isjsopen && !isfanopen){ //风扇打开时不能打开加速器
			if(opjsminuts!=(now.minute()+now.hour()*60)){//不能快速打开或这关闭加速器最小周期一分钟（这是一种保护措施）
				isjsopen = true;
				Serial.println("JS ON");
				logop(String("JSON"));
				digitalWrite(MOTOR_B_PIN0,HIGH);
				digitalWrite(MOTOR_B_PIN1,LOW);
				opjsminuts = now.minute()+now.hour()*60;
				openjsts = now.unixtime();
			}
		}
	}else{
		if(isjsopen){
			Serial.println("JS OFF");
			logop(String("JSOFF "));
			digitalWrite(MOTOR_B_PIN0,LOW);
			digitalWrite(MOTOR_B_PIN1,LOW);
			isjsopen = false;
			openjsts = now.unixtime();
		}
	} 
}

//加湿多少秒
void openjs2(int s){
	if(forcejs==0){
		forcejs = s*10L;
	}
}

void openfan2(int s){
	if(forcefan==0){
		forcefan = s*10L;
	}
}
//打开或者关闭风扇
//打开风扇期间关闭加速器
void openfan(bool b){
	if(b){
		if(!isfanopen){
			Serial.println("FAN ON");
			logop(String("FANON"));
		    digitalWrite(MOTOR_A_PIN0,HIGH);
     	    digitalWrite(MOTOR_A_PIN1,LOW);
			isfanopen = true;
		}
		openjs(false);
	}else{
		if(isfanopen){
			Serial.println("FAN OFF");
			logop(String("FANOFF"));
		    digitalWrite(MOTOR_A_PIN0,LOW);
		    digitalWrite(MOTOR_A_PIN1,LOW);
			isfanopen = false;
		}
	}  
}

//shadeState = 0 , ShadeEndStopPassed True, 遮阳状态
//shadeState = 1 , ShadeEndStopPassed False, 敞开过程中
//shadeState = 2 , ShadeEndStopPassed False, 敞开状态
//shadeState = 3 , ShadeEndStopPassed False, 关闭状态中
//shadeState = -1 未初始化状态
//判断遮阳棚是否已经打开
bool isShadeEndStopPassed(){
	return digitalRead(EndStop_A0_Pin)==LOW;
}
#define OPEN_DELAY 1000*18
#define CLOSE_MAX_DELAY 1000*25
unsigned long op_stop = 0;
//操作时间到返回true
bool opTimeOver(){
	unsigned long c = millis();
	bool b = c>=op_stop;
	return b;
}
void shadeLoop(){
	//int val;
	//val = analogRead(A2);
	//Serial.println(String(val));
	if(shadeState==3){//关闭状态中
		if(isShadeEndStopPassed() || opTimeOver()){ //保护性措施，如果endstop失灵不至于一只卷动
			Serial.println("SHADE CLOSE DONE");
		    digitalWrite(MOTOR_A_PIN0,LOW);
     	    digitalWrite(MOTOR_A_PIN1,LOW);
			shadeState = 0;
		}
	}else if(shadeState==1){//敞开过程中
		if(opTimeOver()){
			Serial.println("SHADE OPEN DONE");
		    digitalWrite(MOTOR_A_PIN0,LOW);
     	    digitalWrite(MOTOR_A_PIN1,LOW);
			shadeState = 2;
		}
	}
}
void openShade(bool b){
	if(b){
		//b=true 关闭遮阳棚
		if(shadeState==0){
			return;
		}else if(shadeState==1){
			return; //操作被忽略，敞开操作必须完整做完，才能进一步操作
		}else if(shadeState==2){
			shadeState=3;
			Serial.println("SHADE CLOSE");
			op_stop = millis()+CLOSE_MAX_DELAY;
		    digitalWrite(MOTOR_A_PIN0,HIGH);
     	    digitalWrite(MOTOR_A_PIN1,LOW);
		}else if(shadeState==3){
			return;
		}
	}else{
		//b=false 打开遮阳棚
		if(shadeState==0){
			shadeState=1;
			Serial.println("SHADE OPEN");
			op_stop = millis()+OPEN_DELAY;
		    digitalWrite(MOTOR_A_PIN0,LOW);
     	    digitalWrite(MOTOR_A_PIN1,HIGH);
		}else if(shadeState==1){
			return;
		}else if(shadeState==2){
			return;
		}else if(shadeState==3){
			return;
		}		
	}  
}

bool israiningImp(){
	return digitalRead(RAINING_PIN)==LOW;
}
unsigned long raing_ms = 0;
//正在下雨
//每半个小时给出一次预报
bool israining(){
	unsigned long t = millis();
	if(raing_ms<t && israiningImp()){
		raing_ms = t+1000l*60l*30l;
		return true;
	}else if(raing_ms>t){
		return true;
	}else{
		raing_ms = 0;
		return false;
	}
}
//早上10点关，下午4点开
//如果下雨关
void evalve(){
	//float ot = dht0.temperature;
	//float oh = dht0.humidity;
	int hour = now.hour();
	int minuts = now.minute();
	int sec = now.second();

	if(israining()){
		if(shadeState==2){
			openShade(true); //关闭遮阳棚
		}
	}else{
		//确保每天固定时间打开一次，关闭一次
		if(hour==10 && minuts==0){
			if(shadeState==2){
				openShade(true); //关闭遮阳棚
			}
		}else if(hour==16 && minuts==0){
			if(shadeState==0){
				openShade(false); //打开遮阳棚
			}
		}
	}
	shadeLoop();
	//手动控制遮阳棚
	int openPressA = digitalRead(Open_A_Pin);
	int closePressA = digitalRead(Close_A_Pin);
	//openPressA 打开风扇5分钟
	if(openPressA==HIGH && closePressA==LOW){
		if(isreleaseA){
			openShade(true); //关闭遮阳棚
		}
		isreleaseA = false;
		EnableLCDLigh();
	}else if(openPressA==LOW && closePressA==HIGH){
		if(isreleaseA){
			openShade(false); //打开遮阳棚
		}
		isreleaseA = false;
		EnableLCDLigh();
	}else if(openPressA==HIGH && closePressA==HIGH){ //重置
		isreleaseA = true;
	}else{
		isreleaseA = false;
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
void timeDisplay(){
	if(mode==0){
		//显示时钟00:00:00-
		lcd.setCursor(0,0);
		lcd.print(lowInt2(now.month())+"/"+
				lowInt2(now.day())+" "+
				lowInt2(now.hour())+":"+
				lowInt2(now.minute())+":"+
				lowInt2(now.second()));

		lcd.setCursor(0,1);
		//Serial.println(String(dht0.temperature,DEC)+","+String(dht0.humidity,DEC));
		//显示温度00C 00% 00C 00%
		String se = isSensorFail?"E":"";
		if(isRoomEnabled){
			se+=" ROOM";
		}else{
			se+="     ";
		}
		lcd.print(String(dht0.temperature,0)+"C"+String(dht0.humidity,0)+String("% ")+se);	
  }
}

int accdt = 0;
void loop(){
  //正常的显示控制模式
  unsigned long t = millis();
  now = rtc.now();

  if(mode==0){
	  timeDisplay();
	  evalve();
	  //switchIndoorOutdoor();
  }

  //设置时间以及时间显示
  setDateTime_cycle();
  LCDLighCyle();
  
  //if(secs==60*10){
	//温度数据存储
	//temperature_storage_cycle();
	//secs = 0;
  //}else{
  //	  secs++;
  //}
  //每秒循环10次
  accdt +=(100-(millis()-t));

  if(accdt>0){
	delay(accdt);
	accdt=0;
  }	
}
