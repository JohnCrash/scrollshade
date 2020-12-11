//索引0对应-40度
#include <avr/pgmspace.h>

const float temp100k[] PROGMEM = {
	3225, //-40
	3026,
	2840,
	2665,
	2501,
	2348,
	2204,
	2070,
	1945,
	1828,
	1718, //-30
	1614,
	1518,
	1428,
	1344,
	1267,
	1194,
	1127,
	1063,
	1004,
	948.8, //-20
	896.3,
	847.1,
	801.0,
	757.8,
	717.2,
	679.0,
	643.2,
	609.6,
	577.9,
	548.1, //-10
	519.7,
	492.8,
	467.5,
	443.5,
	420.9,
	399.5,
	379.3,
	360.2,
	342.2,
	325.1, //0
	309.4,
	294.5,
	280.4,
	267.0,
	254.3,
	242.2,
	230.7,
	219.9,
	209.5,
	199.7, //10
	190.4,
	181.6,
	173.2,
	165.2,
	157.7,
	150.5,
	143.7,
	137.2,
	131.0,
	125.2, //20
	119.6,
	114.3,
	109.3,
	104.5,
	100.0, //25
	95.68,
	91.57,
	87.66,
	83.93,
	80.39, //30
	77.01, //1
	73.79, //2
	70.73, //3
	67.81, //4
	65.03, //5
	62.38, //6
	59.84, //7
	57.43, //8
	55.13, //9
	52.93, //40
	50.83,
	48.83,
	46.92,
	45.09,
	43.34,
	41.67,
	40.08,
	38.55,
	37.09,
	35.70, //50
	34.36,
	33.08,
	31.86,
	30.68,
	29.56,
	28.48,
	27.45,
	26.46,
	25.51,
	24.60  //60
};

//通过电阻值求温度,电阻单位kom
float calcTemp(float r){
	for(int i = 1;i < sizeof(temp100k)/sizeof(float);i++){
		float v1 = pgm_read_float(temp100k+i);
		if( v1 <= r ){
			float v0 = pgm_read_float(temp100k+i-1);
			return (float)i-(r-v1)/(v0-v1)-40.0;
		}
	}
	if(r>=3225)return -40;
	return (100.0 - r*5.0/3.0);
}