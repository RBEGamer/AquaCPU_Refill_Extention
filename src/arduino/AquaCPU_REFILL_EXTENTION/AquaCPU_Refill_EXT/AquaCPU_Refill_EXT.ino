/*AquaCPU - REFILL UNIT

   https://github.com/fdebrabander/Arduino-LiquidCrystal-I2C-library
   ACS712 CODE http://blog.thesen.eu
*/

#define VERSION "1.0a"
#define EXTENTION_ID_BYTE 1



#if EXTENTION_ID_BYTE == 0
#error "BLA"
#endif
#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <mcp_can.h>
#include <SPI.h>

union long_conv {
	char myByte[4];
	long mylong;
};
union float_conv
{
	char myByte[3];
	float myfloat;
};



union int_conv
{
	char myByte[2];
	int myInt;
};


MCP_CAN CAN0(10);     // Set CS to pin 10
bool CAN_SETUP_SUCC = false;
#define CAN_SPEED CAN_100KBPS
#define MCP_CRYSTAL_SPEED MCP_16MHZ
#define CAN0_INT 2
long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];
char msgString[128];
#define CAN_BUS_ID_GENERAL_ERR 0x142
#define CAN_BUS_ID_GENERAL_STOP 0x143
#define CAN_BUS_ID_GENERAL_REGISTER_ID 0x144

#define CAN_BUS_ID_REFILL_STARTED 0x100
#define CAN_BUS_ID_REFILL_STOP 0x101
#define CAN_BUS_ID_MANUAL_REFILL 0x102
#define CAN_BUS_ID_REFILL_FINISH 0x103
#define CAN_BUS_TANK_STATE 0x104
#define CAN_BUS_ID_REFILL_UPDATE_TIME_REMEANING 0x105 
#define CAN_BUS_ID_AUTO_OFF_TIME_SEC 0x106
#define CAN_BUS_ID_TANK_SIZE 0x107
#define CAN_BUS_ID_REFILL_FLOW_RATE 0x108
#define CAN_BUS_ID_CURRENT 0x109

//TANK SETTINGS
float WATER_FLOW_RATE_ML_S = 2.0f; //IN ML PER SECOND
float WATER_REFILL_TANK_SIZE_L = 4.5f; // MLiter
float WATER_REFILL_TANK_SIZE_MULTIPLIER = 1.05f;
//PIN SETTINGS
#define LEVEL_SENSOR_BOTTOM_PIN 6
#define LEVEL_SENSOR_TOP_PIN 7
#define ERROR_RESET_PIN 8
#define PART_FILLED_PIN 4
#define VALVE_PIN 3
#define NOT_OFF_PIN 5
#define CONFIG_MODE_PIN A3
//PIN INVERT SETTINGS
#define INVERT_LEVEL_SENSOR_BOTTOM
#define INVERT_LEVEL_SENSOR_TOP
//#define INVERT_VALVE_STATE
//VALVE SETTINGS
float AUTO_OFF_CURRENT_MIDDLE = 10.0f; //WATT Alles als Stromverbrauch unter 10 Watt wird als error ausgeben
//SENSOR SETTINGS
#define ACS712_30A //#define ACS712_5A //#define ACS712_20A //please set this to you ACS model (5,20,30A)
//DISPLAY SETTINGS
#define ENABLE_I2C_DSIPLAY //if you have connect a 16x2 I2C Display
#define I2C_DISPLAY_ADDR 0x27 //set the I2C Adress of the display (eg use a i2c scanner)
//ERROR SETTINGS
#define ENABLE_ERROR_MANUAL_RESET //if you enable this you have to reset the system with the FIX ERROR BUTTON


bool NOT_FIXABLE_ERROR = false;
//SENSOR VARS
bool state_water_level_bottom_sensor = false;
bool state_water_level_top_sensor = false;
//DISPLAY VARS
#ifdef ENABLE_I2C_DSIPLAY
LiquidCrystal_I2C lcd(I2C_DISPLAY_ADDR, 16, 2);
uint8_t i2c_display_cross[8] = { 0x0, 0x1b, 0xe, 0x4, 0xe, 0x1b, 0x0 };
#endif


//VALVE VARS
#define AUTO_OFF_CURRENT_MIDDLE_COUNT 3
unsigned long AUTO_OFF_TIME_IN_SEC = ((WATER_REFILL_TANK_SIZE_L*WATER_REFILL_TANK_SIZE_MULTIPLIER * 1000) / WATER_FLOW_RATE_ML_S);
bool valve_state = false;
float last_values_middle[AUTO_OFF_CURRENT_MIDDLE_COUNT] = { AUTO_OFF_CURRENT_MIDDLE };
int middle_counter = 0;
int middle_value_current = AUTO_OFF_CURRENT_MIDDLE * AUTO_OFF_CURRENT_MIDDLE_COUNT;
long on_time = 0;
unsigned long AUTO_OFF_TIMER_INTERVALL = AUTO_OFF_TIME_IN_SEC *1000; //
//ACS VARS
float gfLineVoltage = 235.0f;
// typical effective Voltage in Germany
#ifdef ACS712_20A
const float gfACS712_Factor = 50.0f;
#endif
#ifdef ACS712_30A
const float gfACS712_Factor = 75.76f;
#endif
#ifdef ACS712_5A
const float gfACS712_Factor = 27.03f;
#endif
unsigned long gulSamplePeriod_us = 100000;  // 100ms is 5 cycles at 50Hz and 6 cycles at 60Hz
int giADCOffset = 512;                      // initial digital zero of the arduino input from ACS712

static unsigned long lastmillis;
static unsigned long lastmillis_disp_refresh;
static unsigned long lastmillis_disp_light;
#define DISP_DISPLAY_REFRESH (4*1000)
#define DISP_DISPLAY_LIGHT (10*1000)
//#define _DEBUG_EEPROM_RESET


void send_can_message(int _id, float _val) {
	if (CAN_SETUP_SUCC) {
		return;
	}

	float_conv conv;
	conv.myfloat = _val;
	byte sndStat = CAN0.sendMsgBuf(_id, 0, 3, conv.myByte);
	if (sndStat == CAN_OK) {
		//   Serial.println("Message Sent Successfully!");
	}
	else {
		 Serial.println("Error Sending Message... float");
	}
}

void send_can_message(int _id, bool _val) {
	if (CAN_SETUP_SUCC) {
		return;
	}
	byte sndStat = 0;
	if (_val) {
		sndStat = CAN0.sendMsgBuf(_id, 0, 1, 0x01);
	}
	else {
		sndStat = CAN0.sendMsgBuf(_id, 0, 1, 0x00);
	}
	if (sndStat == CAN_OK) {
		//   Serial.println("Message Sent Successfully!");
	}
	else {
		Serial.println("Error Sending Message... bool");
	}
}

void send_can_message(int _id, int _val) {
	if (CAN_SETUP_SUCC) {
		return;
	}
	int_conv i;
	i.myInt = _val;
	byte sndStat = CAN0.sendMsgBuf(_id, 0, 2, i.myByte);
	if (sndStat == CAN_OK) {
		//   Serial.println("Message Sent Successfully!");
	}
	else {
		 Serial.println("Error Sending Message...int");
	}
}

void send_can_message(int _id, long _val) {
	if (CAN_SETUP_SUCC) {
		return;
	}
	long_conv dba;
	dba.mylong = _val;
	byte sndStat = CAN0.sendMsgBuf(_id, 0, 4, dba.myByte);
	if (sndStat == CAN_OK) {
		//  Serial.println("Message Sent Successfully!");
	}
	else {
		 Serial.println("Error Sending Message...long");
	}
}


//FUNC DESC 
void read_level_sensors();

void show_config_menu() {

	//ENTER CONFIG MODE
	if (digitalRead(CONFIG_MODE_PIN) == LOW) {
		Serial.println("ENTER_CONFIG_MODE = 1");
#ifdef ENABLE_I2C_DSIPLAY
		lcd.backlight();
		lastmillis_disp_light = millis();
		lcd.clear();
		lcd.home();
		lcd.print("----- INFO -----");
		lcd.setCursor(0, 1);
		lcd.print("ENTER CONFIG MODE");
#endif

		delay(1000);

		volatile bool in_conf_mode = true;
		volatile int men_pos = 0;
		while (in_conf_mode) {
			if (digitalRead(CONFIG_MODE_PIN) == LOW) {
				men_pos++;
			}
			if (men_pos == 5) {
				in_conf_mode = false;
				break;
			}



			if (men_pos == 0 && digitalRead(PART_FILLED_PIN) == HIGH && digitalRead(NOT_OFF_PIN) == LOW) {
				WATER_FLOW_RATE_ML_S += 0.1f;
			}
			else if (men_pos == 0 && digitalRead(PART_FILLED_PIN) == LOW && digitalRead(NOT_OFF_PIN) == HIGH) {
				WATER_FLOW_RATE_ML_S -= 0.1f;
			}
			if (men_pos == 1 && digitalRead(PART_FILLED_PIN) == HIGH && digitalRead(NOT_OFF_PIN) == LOW) {
				WATER_REFILL_TANK_SIZE_L += 0.1f;
			}
			else if (men_pos == 1 && digitalRead(PART_FILLED_PIN) == LOW && digitalRead(NOT_OFF_PIN) == HIGH) {
				WATER_REFILL_TANK_SIZE_L -= 0.1f;
			}

			if (men_pos == 2 && digitalRead(PART_FILLED_PIN) == HIGH && digitalRead(NOT_OFF_PIN) == LOW) {
				WATER_REFILL_TANK_SIZE_MULTIPLIER += 0.01f;
			}
			else if (men_pos == 2 && digitalRead(PART_FILLED_PIN) == LOW && digitalRead(NOT_OFF_PIN) == HIGH) {
				WATER_REFILL_TANK_SIZE_MULTIPLIER -= 0.01f;
			}


			if (men_pos == 3 && digitalRead(PART_FILLED_PIN) == HIGH && digitalRead(NOT_OFF_PIN) == LOW) {
				AUTO_OFF_CURRENT_MIDDLE += 1.0f;
			}
			else if (men_pos == 3 && digitalRead(PART_FILLED_PIN) == LOW && digitalRead(NOT_OFF_PIN) == HIGH) {
				AUTO_OFF_CURRENT_MIDDLE -= 1.0f;
			}

			if (WATER_FLOW_RATE_ML_S < 1.0f) {
				WATER_FLOW_RATE_ML_S = 1.0f;
			}
			if (WATER_REFILL_TANK_SIZE_L < 0.1f) {
				WATER_REFILL_TANK_SIZE_L = 0.1f;
			}
			if (WATER_REFILL_TANK_SIZE_MULTIPLIER < 0.8f) {
				WATER_REFILL_TANK_SIZE_MULTIPLIER = 0.8f;
			}
			if (AUTO_OFF_CURRENT_MIDDLE < 5.0f) {
				AUTO_OFF_CURRENT_MIDDLE = 5.0f;
			}


			if (men_pos == 0) {
#ifdef ENABLE_I2C_DSIPLAY
				lcd.backlight();
				lastmillis_disp_light = millis();
				lcd.clear();
				lcd.home();
				lcd.print("REFILL FLOW RATE");
				lcd.setCursor(0, 1);
				lcd.print("" + String(WATER_FLOW_RATE_ML_S) + " ML/S");
#endif
			}
			if (men_pos == 1) {
#ifdef ENABLE_I2C_DSIPLAY
				lcd.backlight();
				lastmillis_disp_light = millis();
				lcd.clear();
				lcd.home();
				lcd.print("REFILL TANK SIZE");
				lcd.setCursor(0, 1);
				lcd.print("" + String(WATER_REFILL_TANK_SIZE_L) + " L");
#endif
			}

			if (men_pos == 2) {
#ifdef ENABLE_I2C_DSIPLAY
				lcd.backlight();
				lastmillis_disp_light = millis();
				lcd.clear();
				lcd.home();
				lcd.print("TIME MULTIPLIER");
				lcd.setCursor(0, 1);
				lcd.print("" + String(WATER_REFILL_TANK_SIZE_MULTIPLIER) + " %");
#endif
			}

			if (men_pos == 3) {
#ifdef ENABLE_I2C_DSIPLAY
				lcd.backlight();
				lastmillis_disp_light = millis();
				lcd.clear();
				lcd.home();
				lcd.print("VALVE POWER CONS");
				lcd.setCursor(0, 1);
				lcd.print("" + String(AUTO_OFF_CURRENT_MIDDLE) + " Watt");
#endif
			}


			if (men_pos == 4) {
				read_level_sensors();
#ifdef ENABLE_I2C_DSIPLAY
				lcd.backlight();
				lastmillis_disp_light = millis();
				lcd.clear();
				lcd.home();
				lcd.print("SENSOR STATES");
				lcd.setCursor(0, 1);
				lcd.print("BOTTOM:" + String((byte)state_water_level_bottom_sensor) + " TOP" + String((byte)state_water_level_top_sensor));
#endif
			}


			delay(100);
		}
		//SAFE TO EEPROM
		int addr_put = 8;
		EEPROM.put(addr_put, WATER_FLOW_RATE_ML_S);
		addr_put += sizeof(WATER_FLOW_RATE_ML_S);
		EEPROM.put(addr_put, WATER_REFILL_TANK_SIZE_L);
		addr_put += sizeof(float);
		EEPROM.put(addr_put, WATER_REFILL_TANK_SIZE_MULTIPLIER);
		addr_put += sizeof(float);
		EEPROM.put(addr_put, AUTO_OFF_CURRENT_MIDDLE);
		addr_put += sizeof(float);
	}
}

void setup()
{
#ifdef ENABLE_I2C_DSIPLAY
	lcd.begin();
	lcd.backlight();
	lcd.clear();
	lcd.home();
#endif

#ifdef ENABLE_I2C_DSIPLAY
	lcd.backlight();
	lcd.createChar(0, i2c_display_cross);
	lcd.clear();
	lcd.home();
	lcd.print("AquaCPU-Refill");
	lcd.setCursor(0, 1);
	lcd.print("VERSION: " + String(VERSION));
	delay(1000);
#endif


	

	Serial.begin(115200);

	delay(100);

	//FAIL RESET SCHALTUNG
#ifdef ENABLE_ERROR_MANUAL_RESET
	EEPROM.begin();
#ifdef _DEBUG_EEPROM_RESET
	EEPROM.write(0, 0); //ERROR FLAG
#endif
	int ret = EEPROM.read(0);
	if (ret == 1) {
		Serial.println("ERR:" + String(ret));
		NOT_FIXABLE_ERROR = true;
	}
	else {
		NOT_FIXABLE_ERROR = false;
	}
	pinMode(ERROR_RESET_PIN, INPUT);
	digitalWrite(ERROR_RESET_PIN, HIGH);
	if (digitalRead(ERROR_RESET_PIN) == LOW && NOT_FIXABLE_ERROR) {
		NOT_FIXABLE_ERROR = false;
		EEPROM.write(0, 0);
	}
#else
	NOT_FIXABLE_ERROR = false;
#endif
#ifdef ENABLE_I2C_DSIPLAY
	if (NOT_FIXABLE_ERROR) {
		lcd.backlight();
		lcd.clear();
		lcd.home();
		lcd.println("ERR PLEASE PRESS");
#ifdef ENABLE_ERROR_MANUAL_RESET
		lcd.setCursor(0, 1);
		lcd.print("RESET + ERR_RES");
#endif
	}
#endif

	while (NOT_FIXABLE_ERROR) {
		delay(500);
		continue;
	}

	//SEND DEBUG OVER CAN
	//PIN SETUP
	pinMode(VALVE_PIN, OUTPUT);
	digitalWrite(VALVE_PIN, LOW);
	valve_state = false;
	pinMode(LEVEL_SENSOR_BOTTOM_PIN, INPUT);
	pinMode(LEVEL_SENSOR_TOP_PIN, INPUT);
	digitalWrite(LEVEL_SENSOR_TOP_PIN, HIGH);
	digitalWrite(LEVEL_SENSOR_BOTTOM_PIN, HIGH);
	bool state_water_level_bottom_sensor = false;
	bool state_water_level_top_sensor = false;

	pinMode(PART_FILLED_PIN, INPUT);
	digitalWrite(PART_FILLED_PIN, HIGH);
	pinMode(NOT_OFF_PIN, INPUT);
	digitalWrite(NOT_OFF_PIN, HIGH);
	pinMode(CONFIG_MODE_PIN, INPUT);
	digitalWrite(CONFIG_MODE_PIN, HIGH);



	//LOAD VALUES
	int addr_get = 8;
	EEPROM.get(addr_get, WATER_FLOW_RATE_ML_S);
	addr_get += sizeof(WATER_FLOW_RATE_ML_S);
	EEPROM.get(addr_get, WATER_REFILL_TANK_SIZE_L);
	addr_get += sizeof(WATER_REFILL_TANK_SIZE_L);
	EEPROM.get(addr_get, WATER_REFILL_TANK_SIZE_MULTIPLIER);
	addr_get += sizeof(WATER_REFILL_TANK_SIZE_MULTIPLIER);
	EEPROM.get(addr_get, AUTO_OFF_CURRENT_MIDDLE);
	addr_get += sizeof(AUTO_OFF_CURRENT_MIDDLE);

	show_config_menu();

	//CALC IMPORTANT VALUES AFTER LOAD
	AUTO_OFF_TIME_IN_SEC = ((WATER_REFILL_TANK_SIZE_L*WATER_REFILL_TANK_SIZE_MULTIPLIER * 1000) / WATER_FLOW_RATE_ML_S);
	AUTO_OFF_TIMER_INTERVALL = AUTO_OFF_TIME_IN_SEC *1000;
	middle_value_current = AUTO_OFF_CURRENT_MIDDLE * AUTO_OFF_CURRENT_MIDDLE_COUNT;



	//SETUP CAN
	bool CAN_SETUP_SUCC = false;
	pinMode(CAN0_INT, INPUT);

	if (CAN0.begin(MCP_ANY, CAN_SPEED, MCP_CRYSTAL_SPEED) == CAN_OK) {
		CAN_SETUP_SUCC = true;
		CAN0.setMode(MCP_NORMAL);

#ifdef ENABLE_I2C_DSIPLAY
		lcd.begin();
		lcd.backlight();
		lcd.createChar(0, i2c_display_cross);
		lcd.clear();
		lcd.home();
		lcd.print("CAN-BUS ENABLED");
		lcd.setCursor(0, 1);
		lcd.print("");
		delay(500);
#endif

	}
	else {
		CAN_SETUP_SUCC = false;
	}

	//SEND CAN SETUP MESSAGES
	send_can_message(CAN_BUS_ID_GENERAL_REGISTER_ID, (char)EXTENTION_ID_BYTE);

	//SEND REGISTER VALUES
	send_can_message(CAN_BUS_ID_AUTO_OFF_TIME_SEC, (long)AUTO_OFF_TIME_IN_SEC);
	send_can_message(CAN_BUS_ID_TANK_SIZE, WATER_REFILL_TANK_SIZE_L);
	send_can_message(CAN_BUS_ID_REFILL_FLOW_RATE, WATER_FLOW_RATE_ML_S);
	send_can_message(CAN_BUS_ID_CURRENT, AUTO_OFF_CURRENT_MIDDLE);



	Serial.println("-- AquaCPU - REFILL UNIT --");
	Serial.println("-- Version : " + String(VERSION) + " --");
	Serial.println("AUTO_OFF_TIMER = " + String(AUTO_OFF_TIME_IN_SEC));
	Serial.println("REFILL_TANK_SIZE = " + String(WATER_REFILL_TANK_SIZE_L));
	Serial.println("VALVE_MIN_WATT = " + String(AUTO_OFF_CURRENT_MIDDLE));
	Serial.println("WATER_FLOW_RATE = " + String(WATER_FLOW_RATE_ML_S));
	if (CAN_SETUP_SUCC) {
		Serial.println("CAN_BUS_INIT_ERROR = false");
	}
	else {
		Serial.println("CAN_BUS_INIT_ERROR = true");
	}
	//Read init sensor states
	read_level_sensors();
	delay(500);
	Serial.println("SETUP DONE");
}

void read_level_sensors() {
#ifdef INVERT_LEVEL_SENSOR_BOTTOM
	state_water_level_bottom_sensor = digitalRead(LEVEL_SENSOR_BOTTOM_PIN);
#else
	state_water_level_bottom_sensor = !digitalRead(LEVEL_SENSOR_BOTTOM_PIN);
#endif

#ifdef INVERT_LEVEL_SENSOR_TOP
	state_water_level_top_sensor = digitalRead(LEVEL_SENSOR_TOP_PIN);
#else
	state_water_level_top_sensor = !digitalRead(LEVEL_SENSOR_TOP_PIN);
#endif
}

void set_valve_state(bool _en) {
	valve_state = _en;
#ifdef INVERT_VALVE_STATE
	digitalWrite(VALVE_PIN, !_en);
#else
	digitalWrite(VALVE_PIN, _en);
#endif
}

void enable_auto_refill() {

#ifdef ENABLE_I2C_DSIPLAY
	lcd.backlight();
	lastmillis_disp_light = millis();
	lcd.clear();
	lcd.home();
	lcd.print("   RUN REFILL");
	lcd.setCursor(0, 1);
	lcd.print("REM TIME : ");
#endif


	for (int i = 0; i < AUTO_OFF_CURRENT_MIDDLE_COUNT; i++) {
		last_values_middle[i] = AUTO_OFF_CURRENT_MIDDLE;
	}
	send_can_message(CAN_BUS_ID_REFILL_STARTED, true);
	on_time = 0;
	middle_value_current = AUTO_OFF_CURRENT_MIDDLE;
	set_valve_state(true); //open valve
	lastmillis = millis();
}

void set_not_fixable_error(int _c = -1) {
	set_valve_state(false);
	NOT_FIXABLE_ERROR = true;
#ifdef ENABLE_ERROR_MANUAL_RESET
	EEPROM.write(0, 1);
#endif
	send_can_message(CAN_BUS_ID_GENERAL_ERR, true);

#ifdef ENABLE_I2C_DSIPLAY
	if (NOT_FIXABLE_ERROR) {
		lcd.backlight();
		lastmillis_disp_refresh = millis();
		lcd.clear();
		lcd.home();
		lcd.write(0);
		switch (_c) {
		case -1: lcd.print("UNKNOWN ERR"); break;
		case 0: lcd.print("SENSOR MISMATCH"); break;
		case 1: lcd.print("TIME OUT"); break;
		case 2: lcd.print("NO LOAD"); break;
		default: lcd.print("UNKNOWN ERR"); break;
		}
#ifdef ENABLE_ERROR_MANUAL_RESET
		lcd.setCursor(0, 1);
		lcd.print("RESET+ERR BTN");
#endif
	}
#endif
	send_can_message(CAN_BUS_ID_REFILL_STOP, true);
}

void loop()
{


	read_level_sensors();

	if (NOT_FIXABLE_ERROR) {
		Serial.println("ERR");
		set_valve_state(false);
		delay(500);
		return;
	}

	//STOP BUTTON
	if (digitalRead(NOT_OFF_PIN) == LOW) {
		if (valve_state) {
#ifdef ENABLE_I2C_DSIPLAY
			lcd.backlight();
			lastmillis_disp_light = millis();
			lcd.clear();
			lcd.home();
			lcd.print("----- INFO -----");
			lcd.setCursor(0, 1);
			lcd.print("CANCEL REFILL");
#endif
			send_can_message(CAN_BUS_ID_REFILL_STOP, true);
			send_can_message(CAN_BUS_ID_GENERAL_STOP, true);
		}
		set_valve_state(false);

	}

	//INVALID SENSOR STATE CHECK
	if (state_water_level_top_sensor == 0 && state_water_level_bottom_sensor == 1) {
		Serial.println("ERROR - INVALID SENSOR STATE - VALVE OFF");
		set_not_fixable_error(0);
	}

	//ENABLE BACKLIGHT ONLY
	if (digitalRead(CONFIG_MODE_PIN) == LOW || digitalRead(PART_FILLED_PIN) == LOW) {
		lcd.backlight();
		lastmillis_disp_light = millis();
	}

	//PARTLY REFILL BUTTON
	if (digitalRead(PART_FILLED_PIN) == LOW && !valve_state && state_water_level_top_sensor == 1 && state_water_level_bottom_sensor == 0) {
#ifdef ENABLE_I2C_DSIPLAY
		lcd.backlight();
		lastmillis_disp_light = millis();
		lcd.clear();
		lcd.home();
		lcd.print("----- INFO -----");
		lcd.setCursor(0, 1);
		lcd.print("MANUAL FILL");
#endif
		send_can_message(CAN_BUS_ID_MANUAL_REFILL, true);
		enable_auto_refill();
	}




	//IF REFILL IN PROGRESS
	if (valve_state) {
		lastmillis_disp_light = millis();
		//TANK FULL -> STOP REFIL CLOSE VALVE
		if (state_water_level_top_sensor == 0) {
			Serial.println("INFO - WATER TANK FULL - VALVE OFF");
#ifdef ENABLE_I2C_DSIPLAY
			lcd.backlight();
			lastmillis_disp_light = millis();
			lcd.clear();
			lcd.home();
			lcd.print("----- INFO -----");
			lcd.setCursor(0, 1);
			lcd.print("TANK COMPLETE FILLED");
#endif
			set_valve_state(false);
			send_can_message(CAN_BUS_ID_REFILL_FINISH, true);
		}

		//CHECK FOR FLOWING WATT
		last_values_middle[middle_counter++] = acs_measure_loop();
		middle_value_current = 0.0f;
		for (int i = 0; i < AUTO_OFF_CURRENT_MIDDLE_COUNT; i++) {
			middle_value_current += last_values_middle[i];
		}
		if (middle_counter >= AUTO_OFF_CURRENT_MIDDLE_COUNT) {
			middle_counter = 0;
		}
		//NO CHECK IF HIGHER THAN THE MIN AMP
		if (middle_value_current < AUTO_OFF_CURRENT_MIDDLE * AUTO_OFF_CURRENT_MIDDLE_COUNT || middle_value_current < 0.0f) {
			valve_state = false;
			digitalWrite(VALVE_PIN, valve_state);
			Serial.println("ERROR - VALVE UNDERCURRENT SENSE - VALVE OFF");
			set_not_fixable_error(2);
		}

		//CHECK TIME
		if (millis() - lastmillis > AUTO_OFF_TIMER_INTERVALL) {
			valve_state = false;
			digitalWrite(VALVE_PIN, valve_state);
			Serial.println("ERROR - VALVE TIMER OVERFLOW - VALVE OFF");
			set_not_fixable_error(1);
		}


		
		unsigned long tr = (AUTO_OFF_TIMER_INTERVALL - (millis() - lastmillis));
		send_can_message(CAN_BUS_ID_REFILL_UPDATE_TIME_REMEANING, (long)tr);
		//SHOW LEFT REFILL TIME (ESTIMATED)
#ifdef ENABLE_I2C_DSIPLAY
		if (!NOT_FIXABLE_ERROR) { 
		lcd.backlight();
		lastmillis_disp_light = millis();

		lcd.setCursor(9, 1);
		lcd.print(String((tr / 6000.0f)));
		delay(100);
		}
#endif
		delay(100);
	}
	else {
		//START REFILL PROCESS AT EMPTY SENSOR STATE
		if (state_water_level_top_sensor == 1 && state_water_level_bottom_sensor == 1) {
			Serial.println("INFO - WATER TANK EMPTY - VALVE ON");
#ifdef ENABLE_I2C_DSIPLAY
			if (!NOT_FIXABLE_ERROR) {
				lcd.backlight();
				lastmillis_disp_light = millis();
				lcd.clear();
				lcd.home();
				lcd.print("  TANK REFILL  ");
				lcd.setCursor(0, 1);
				lcd.print("STARTED");
				delay(2000);
			}
#endif
			if (!NOT_FIXABLE_ERROR) {
				enable_auto_refill();
			}
		}


		//SHOW TANK FILL STATE AT IDLE, NO FILL JUST FOR INFO
#ifdef ENABLE_I2C_DSIPLAY
		if (millis() - lastmillis_disp_refresh > DISP_DISPLAY_REFRESH) {
			lastmillis_disp_refresh = millis();
			lcd.clear();
			lcd.home();
			lcd.print("-- TANK STATE --");
			lcd.setCursor(0, 1);
			if (state_water_level_top_sensor == 0 && state_water_level_bottom_sensor == 0) {
				lcd.print("COMPLETE FILLED");
				send_can_message(CAN_BUS_TANK_STATE, (long)1);
			}
			else  if (state_water_level_top_sensor == 1 && state_water_level_bottom_sensor == 0) {
				lcd.print("PARTLY FILLED");
				send_can_message(CAN_BUS_TANK_STATE, (long)2);
			}
			else if (state_water_level_top_sensor == 1 && state_water_level_bottom_sensor == 1) {
				lcd.print("EMPTY");
				send_can_message(CAN_BUS_TANK_STATE, (long)3);
			}
			else {
				lcd.print("UNKNOWN STATE");
				send_can_message(CAN_BUS_TANK_STATE, (long)0);
			}
		}
#endif

	}
	//BACKLIGHT OFF TIMER
#ifdef ENABLE_I2C_DSIPLAY
	if (millis() - lastmillis_disp_light > DISP_DISPLAY_LIGHT) {
		lcd.noBacklight();
	}
#endif







}











float acs_measure_loop() {

	long lNoSamples = 0;
	long lCurrentSumSQ = 0;
	long lCurrentSum = 0;

	// set-up ADC
	ADCSRA = 0x87;  // turn on adc, adc-freq = 1/128 of CPU ; keep in min: adc converseion takes 13 ADC clock cycles
	ADMUX = 0x40;   // internal 5V reference

	// 1st sample is slower due to datasheet - so we spoil it
	ADCSRA |= (1 << ADSC);
	while (!(ADCSRA & 0x10));

	// sample loop - with inital parameters, we will get approx 800 values in 100ms
	unsigned long ulEndMicros = micros() + gulSamplePeriod_us;
	while (micros() < ulEndMicros)
	{
		// start sampling and wait for result
		ADCSRA |= (1 << ADSC);
		while (!(ADCSRA & 0x10));

		// make sure that we read ADCL 1st
		long lValue = ADCL;
		lValue += (ADCH << 8);
		lValue -= giADCOffset;

		lCurrentSum += lValue;
		lCurrentSumSQ += lValue * lValue;
		lNoSamples++;
	}

	// stop sampling
	ADCSRA = 0x00;

	if (lNoSamples > 0) // if no samples, micros did run over
	{
		// correct quadradic current sum for offset: Sum((i(t)+o)^2) = Sum(i(t)^2) + 2*o*Sum(i(t)) + o^2*NoSamples
		// sum should be zero as we have sampled 5 cycles at 50Hz (or 6 at 60Hz)
		float fOffset = (float)lCurrentSum / lNoSamples;
		lCurrentSumSQ -= 2 * fOffset * lCurrentSum + fOffset * fOffset * lNoSamples;
		if (lCurrentSumSQ < 0) {
			lCurrentSumSQ = 0; // avoid NaN due to round-off effects
		}

		float fCurrentRMS = sqrtf((float)lCurrentSumSQ / (float)lNoSamples) * gfACS712_Factor * gfLineVoltage / 1024;


		// correct offset for next round
		giADCOffset = (int)(giADCOffset + fOffset + 0.5f);
		return fCurrentRMS;
	}
}

