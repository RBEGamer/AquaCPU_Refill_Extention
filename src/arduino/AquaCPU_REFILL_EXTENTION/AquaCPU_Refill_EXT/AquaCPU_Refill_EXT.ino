/*AquaCPU - REFILL UNIT
 * 
 * https://github.com/fdebrabander/Arduino-LiquidCrystal-I2C-library
 * ACS712 CODE http://blog.thesen.eu 
 */
#define VERSION "0.3b"

#include <EEPROM.h>
#define ENABLE_ERROR_MANUAL_RESET //if you enable this you habe to reset the system with the 

#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

//TANK SETTINGS
#define WATER_FLOW_RATE_ML_S 2.2f //IN ML PER SECOND
#define WATER_REFILL_TANK_SIZE_L 4500 // MLiter
#define WATER_REFILL_TANK_SIZE_MULTIPLIER 1.05f
//PIN SETTINGS
#define LEVEL_SENSOR_BOTTOM_PIN 6
#define LEVEL_SENSOR_TOP_PIN 7
#define ERROR_RESET_PIN 8
#define PART_FILLED_PIN 4
#define VALVE_PIN 3
//PIN INVERT SETTINGS
#define INVERT_LEVEL_SENSOR_BOTTOM
#define INVERT_LEVEL_SENSOR_TOP
//#define INVERT_VALVE_STATE

//VALVE SETTINGS
#define AUTO_OFF_CURRENT_MIDDLE 10.0f //WATT Alles als Stromverbrauch unter 10 Watt wird als error ausgeben
//SENSOR SETTINGS
#define ACS712_30A //#define ACS712_5A //#define ACS712_20A //please set this to you ACS model (5,20,30A)



bool NOT_FIXABLE_ERROR = false;
//SENSOR VARS
bool state_water_level_bottom_sensor = false;
bool state_water_level_top_sensor = false;
//DISPLAY VARS
#define ENABLE_I2C_DSIPLAY

#ifdef ENABLE_I2C_DSIPLAY
#define I2C_DISPLAY_ADDR 0x27
LiquidCrystal_I2C lcd(I2C_DISPLAY_ADDR, 16, 2);
uint8_t i2c_display_cross[8] = {0x0, 0x1b, 0xe, 0x4, 0xe, 0x1b, 0x0};
#endif


//VALVE VARS
#define AUTO_OFF_CURRENT_MIDDLE_COUNT 3
#define AUTO_OFF_TIME_IN_SEC ((WATER_REFILL_TANK_SIZE_L*WATER_REFILL_TANK_SIZE_MULTIPLIER)/ WATER_FLOW_RATE_ML_S)
bool valve_state = false;
float last_values_middle[AUTO_OFF_CURRENT_MIDDLE_COUNT] = { AUTO_OFF_CURRENT_MIDDLE };
int middle_counter = 0;
int middle_value_current = AUTO_OFF_CURRENT_MIDDLE*AUTO_OFF_CURRENT_MIDDLE_COUNT;
long on_time = 0;
const unsigned long AUTO_OFF_TIMER_INTERVALL = AUTO_OFF_TIME_IN_SEC*1000UL; //
//ACS VARS
float gfLineVoltage = 235.0f;
// typical effective Voltage in Germany
#ifdef ACS712_20A
const float gfACS712_Factor =  50.0f;
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

 //TODO SEND CAN MESSAGES
//TODO AUTO REFILL AFTER TIME POSSIBLE
 
void setup()
{
Serial.begin(115200);

//FAIL RESET SCHALTUNG
#ifdef ENABLE_ERROR_MANUAL_RESET
EEPROM.begin();
#ifdef _DEBUG_EEPROM_RESET
EEPROM.write(0,0); //ERROR FLAG

#endif
int ret =EEPROM.read(0);
if(ret ==1 ){
  Serial.println("ERR:" + String(ret));
NOT_FIXABLE_ERROR = true;
}else{
NOT_FIXABLE_ERROR = false;
}
pinMode(ERROR_RESET_PIN, INPUT);
digitalWrite(ERROR_RESET_PIN, HIGH);
if(digitalRead(ERROR_RESET_PIN) == LOW && NOT_FIXABLE_ERROR){
NOT_FIXABLE_ERROR = false;
EEPROM.write(0,0);
} 
#else
NOT_FIXABLE_ERROR = false;
#endif



#ifdef ENABLE_I2C_DSIPLAY
lcd.begin();
lcd.backlight();
lcd.createChar(0, i2c_display_cross);
lcd.clear();
lcd.home();
lcd.print("AquaCPU-Refill");
lcd.setCursor(0, 1);
lcd.print("VERSION: " + String(VERSION));
delay(100);
#endif


 #ifdef ENABLE_I2C_DSIPLAY
if(NOT_FIXABLE_ERROR){
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



 //SEND DEBUG OVER CAN
 //PIN SETUP
 pinMode(VALVE_PIN,OUTPUT);
 digitalWrite(VALVE_PIN,LOW);
 valve_state = false;
pinMode(LEVEL_SENSOR_BOTTOM_PIN, INPUT);
pinMode(LEVEL_SENSOR_TOP_PIN,INPUT);
digitalWrite(LEVEL_SENSOR_TOP_PIN, HIGH);
digitalWrite(LEVEL_SENSOR_BOTTOM_PIN,HIGH);
bool state_water_level_bottom_sensor = false;
bool state_water_level_top_sensor = false;

pinMode(PART_FILLED_PIN, INPUT);
digitalWrite(PART_FILLED_PIN, HIGH);

Serial.println("-- AquaCPU - REFILL UNIT --");
Serial.println("-- Version : " + String(VERSION) + " --");
Serial.println("AUTO_OFF_TIMER = " + String(AUTO_OFF_TIME_IN_SEC));
Serial.println("REFILL_TANK_SIZE = " + String(WATER_REFILL_TANK_SIZE_L));
Serial.println("VALVE_MIN_WATT = " + String(AUTO_OFF_CURRENT_MIDDLE));
Serial.println("WATER_FLOW_RATE = " + String(WATER_FLOW_RATE_ML_S));
//Read init sensor states
read_level_sensors();
delay(500);
Serial.println("SETUP DONE");
}

void read_level_sensors(){
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



void set_valve_state(bool _en){
   valve_state=_en;
   #ifdef INVERT_VALVE_STATE
 digitalWrite(VALVE_PIN,!_en);
 #else
  digitalWrite(VALVE_PIN,_en);
 #endif
  }



void enable_auto_refill(){
   for(int i = 0; i < AUTO_OFF_CURRENT_MIDDLE_COUNT; i++){
  last_values_middle[i] = AUTO_OFF_CURRENT_MIDDLE;
  }
 on_time = 0;
  middle_value_current = AUTO_OFF_CURRENT_MIDDLE;
set_valve_state(true); //open valve
lastmillis = millis();
  }

void set_not_fixable_error(int _c = -1){
set_valve_state(false);
 NOT_FIXABLE_ERROR = true;
  #ifdef ENABLE_ERROR_MANUAL_RESET
 EEPROM.write(0,1);
 #endif

 #ifdef ENABLE_I2C_DSIPLAY
if(NOT_FIXABLE_ERROR){
  lcd.backlight();
  lastmillis_disp_refresh = millis();
  lcd.clear();
  lcd.home();
  lcd.write(0);
  switch(_c){
 case -1: lcd.print("UNKNOWN ERR");break;
 case 0: lcd.print("SENSOR MISMATCH");break;
 case 1: lcd.print("TIME OUT");break;
 case 2: lcd.print("NO LOAD");break;
 default: lcd.print("UNKNOWN ERR");break;    
    }
    #ifdef ENABLE_ERROR_MANUAL_RESET
  lcd.setCursor(0, 1);
  lcd.print("RESET+ERR BTN");
  #endif
  }
#endif








}


void loop()
{


read_level_sensors();

if(NOT_FIXABLE_ERROR){Serial.println("ERR");set_valve_state(false); delay(500);return;}




//INVALID SENSOR STATE CHECK
if(state_water_level_top_sensor == 0 && state_water_level_bottom_sensor == 1){
   Serial.println("ERROR - INVALID SENSOR STATE - VALVE OFF"); 
  set_not_fixable_error(0); 
}




//PARTLY REFILL BUTTON
if(digitalRead(PART_FILLED_PIN) == LOW && !valve_state && state_water_level_top_sensor == 1 && state_water_level_bottom_sensor == 0){
  #ifdef ENABLE_I2C_DSIPLAY
 lcd.backlight();
 lastmillis_disp_light = millis();
  lcd.clear();
  lcd.home();
  lcd.print("----- INFO -----");
   lcd.setCursor(0, 1);
   lcd.print("MANUAL FILL");
 #endif
  enable_auto_refill();
  }




  
  if(valve_state){
lastmillis_disp_light = millis();
if(state_water_level_top_sensor == 0 ){
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
}
    
last_values_middle[middle_counter++] = acs_measure_loop();
middle_value_current = 0.0f;
 for(int i = 0; i < AUTO_OFF_CURRENT_MIDDLE_COUNT; i++){
  middle_value_current += last_values_middle[i];
  }
if(middle_counter >= AUTO_OFF_CURRENT_MIDDLE_COUNT){
 middle_counter = 0;
  }
 // Serial.println(middle_value_current);
  //NO CHECK IF HIGHER
  if(middle_value_current < AUTO_OFF_CURRENT_MIDDLE*AUTO_OFF_CURRENT_MIDDLE_COUNT || middle_value_current < 0.0f){
    valve_state=false;
 digitalWrite(VALVE_PIN,valve_state);
 Serial.println("ERROR - VALVE UNDERCURRENT SENSE - VALVE OFF"); 
 set_not_fixable_error(2);
  }
  //CHECK TIME

  if ( millis() - lastmillis > AUTO_OFF_TIMER_INTERVALL )
  {
 valve_state=false;
  valve_state=false;
 digitalWrite(VALVE_PIN,valve_state);
 Serial.println("ERROR - VALVE TIMER OVERFLOW - VALVE OFF"); 
 set_not_fixable_error(1);
  }


delay(50);


}else{
  
  if(state_water_level_top_sensor == 1 && state_water_level_bottom_sensor == 1){
   Serial.println("INFO - WATER TANK EMPTY - VALVE ON"); 
    #ifdef ENABLE_I2C_DSIPLAY
 lcd.backlight();
 lastmillis_disp_light = millis();
  lcd.clear();
  lcd.home();
  lcd.print("----- INFO -----");
   lcd.setCursor(0, 1);
   lcd.print("TANK REFILL STATED");
 #endif
enable_auto_refill();
  }



 #ifdef ENABLE_I2C_DSIPLAY
 if(millis() - lastmillis_disp_refresh > DISP_DISPLAY_REFRESH){
  lastmillis_disp_refresh = millis();
  lcd.clear();
  lcd.home();
  lcd.print("-- TANK STATE --");
  lcd.setCursor(0, 1);
   if(state_water_level_top_sensor == 0 && state_water_level_bottom_sensor == 0){
    lcd.print("COMPLETE FILLED");
   }else  if(state_water_level_top_sensor == 1 && state_water_level_bottom_sensor == 0){
    lcd.print("PARTLY FILLED");
   }else if(state_water_level_top_sensor == 1 && state_water_level_bottom_sensor == 1){
    lcd.print("EMPTY");
    }else{
    lcd.print("UNKNOWN STATE");
    }
 }

#endif

}

#ifdef ENABLE_I2C_DSIPLAY
if(millis() - lastmillis_disp_light > DISP_DISPLAY_LIGHT){lcd.noBacklight();}
#endif

  





}











float acs_measure_loop(){

 long lNoSamples=0;
 long lCurrentSumSQ = 0;
 long lCurrentSum=0;

  // set-up ADC
  ADCSRA = 0x87;  // turn on adc, adc-freq = 1/128 of CPU ; keep in min: adc converseion takes 13 ADC clock cycles
  ADMUX = 0x40;   // internal 5V reference

  // 1st sample is slower due to datasheet - so we spoil it
  ADCSRA |= (1 << ADSC);
  while (!(ADCSRA & 0x10));
  
  // sample loop - with inital parameters, we will get approx 800 values in 100ms
  unsigned long ulEndMicros = micros()+gulSamplePeriod_us;
  while(micros()<ulEndMicros)
  {
    // start sampling and wait for result
    ADCSRA |= (1 << ADSC);
    while (!(ADCSRA & 0x10));
    
    // make sure that we read ADCL 1st
    long lValue = ADCL; 
    lValue += (ADCH << 8);
    lValue -= giADCOffset;

    lCurrentSum += lValue;
    lCurrentSumSQ += lValue*lValue;
    lNoSamples++;
  }
  
  // stop sampling
  ADCSRA = 0x00;

  if (lNoSamples>0)  // if no samples, micros did run over
  {  
    // correct quadradic current sum for offset: Sum((i(t)+o)^2) = Sum(i(t)^2) + 2*o*Sum(i(t)) + o^2*NoSamples
    // sum should be zero as we have sampled 5 cycles at 50Hz (or 6 at 60Hz)
    float fOffset = (float)lCurrentSum/lNoSamples;
    lCurrentSumSQ -= 2*fOffset*lCurrentSum + fOffset*fOffset*lNoSamples;
    if (lCurrentSumSQ<0) {lCurrentSumSQ=0;} // avoid NaN due to round-off effects
    
    float fCurrentRMS = sqrtf((float)lCurrentSumSQ/(float)lNoSamples) * gfACS712_Factor * gfLineVoltage / 1024;

  
    // correct offset for next round
    giADCOffset=(int)(giADCOffset+fOffset+0.5f);
        return fCurrentRMS;
  }
}

