/*
 * For Lolin32 WEMOS
 * 
 */

// Include the libraries
#include <Arduino.h>
#include <FS.h>                 // Access non volatile memory
#include <OneWire.h>            // OneWire for one wire digital communication with Temp sensors
#include <DallasTemperature.h>  // DallasTemperature for temp sensor interpretation
#include <WiFi.h>               // WiFi connection lib
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h> 
#include <BlynkSimpleEsp32.h>   // Blynk App library for MQTT communication
#include "time.h"               // Handles time
//#include <string>

#define BLYNK_PRINT Serial      // Permits Blynk to write to Serial port
#define ONE_WIRE_BUS 15         // One Wire Data wire is connected to GPIO15

// Global variables

//Temperatures
float Temp1;                    // Measured Sensor 1 temperature
float MaxTemp1;                 // Records Maximum Temperature from sensor 1
float last_MaxTemp1 = -200;     // Records previous MaxTemp1
float TempLimit = 25;           // Maximum temperature allowed, above this limit triggers fault2, update available on App

//Messages
bool hi_temp_msg = false;       // True if temp above TempLimit
String faults;                  // string with fault messages
String faults2 = " ";           // string for fault if temp is above limit for more than TempLimit

//Timers
unsigned long chrono_zero;      // starts a timer when temperature gets hotter than TempLimit
unsigned long max_time_above_temp = 2 * 60 * 1000;   // 30 minutes, update available on App

//pins
int pinV9data;                  // Blynk virtual pin, receives from app a reset input (virtual momentary button)
int pin_button = 32;            // GPIO 32 connected to reset button

// Constants for time update
const char* ntpServer = "pool.ntp.org";       // ntp server where time update is taken from
const long  gmtOffset_sec = -3 * 3600;        // GMT offset in Brazil = -3 h. 1 h conversion = 60min * 60sec = 3600sec
const int   daylightOffset_sec = 0;           // DST, in seconds
struct tm timeinfo;                           // variable for actual time
char MaxTemp1_time[14];                       // time at which MaxTemp1 was reached
char AboveTemp1_time[14];

//Blynk
char auth[] = "ZjNdygu-mNGGDI_OTrBA9TPW2nNLot4v";         // authorization token from Blynk App
BlynkTimer timer;                                         // timer object

//Sensors adresss
DeviceAddress sensor1 = { 0x28, 0x55, 0xA5, 0x79, 0xA2, 0x0, 0x3, 0x5A };   //28 DA 6F 79 A2 0 3 B
//DeviceAddress sensor2 = { 0x28, 0xDA, 0x6F, 0x79, 0xA2, 0x0, 0x3, 0xB };  //28 55 A5 79 A2 0 3 5A

//Sensor init
OneWire oneWire(ONE_WIRE_BUS);                            // Setup a oneWire instance to communicate with a OneWire device
DallasTemperature sensors(&oneWire);                      // Pass our oneWire reference to Dallas Temperature sensor 

//WiFi Manager init
WiFiManager wifiManager;

void get_temps( void){
  Serial.print("Requesting temperatures...");
  sensors.requestTemperatures(); // Send the command to get temperatures
  Serial.println("DONE");
  
  Temp1 = sensors.getTempC(sensor1);
  Serial.print("Sensor 1(*C): ");
  Serial.println(Temp1); 
}

void get_high_temps(void){
  Serial.print("updating time...");
  getLocalTime(&timeinfo);
  Serial.println(&timeinfo);

  if (MaxTemp1 == NULL){           // If MaxTemp1 is empty, adopts MaxTemp1 as being current Temp1
    MaxTemp1 = Temp1;
    strftime(MaxTemp1_time,sizeof(MaxTemp1_time), "%d/%b %H:%M", &timeinfo);
  }

//  if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0 && timeinfo.tm_sec < 10){   //Resets Max temps at midnight.
//    MaxTemp1 = Temp1;
//    strftime(MaxTemp1_time,sizeof(MaxTemp1_time), "%d/%b %H:%M", &timeinfo);
//  }

  if (Temp1 > MaxTemp1){          // Monitors Highest Temp1
    MaxTemp1 = Temp1;
    strftime(MaxTemp1_time,sizeof(MaxTemp1_time), "%d/%b %H:%M", &timeinfo);
    Serial.print("Max temp 1 recorded");
    Serial.println(MaxTemp1);
  }
}

void check_faults(void){
  Serial.println("Checking failures");
  faults = "";
  /* Checks high temperature */
  if (Temp1 > TempLimit){
    faults += "HiTemp ";
    Serial.println("High Temperature!");
    
    if (hi_temp_msg == false){        //change of hi_temp_msg status into True
      faults2 = " ";
      hi_temp_msg = true;
      getLocalTime(&timeinfo);    // stores initial time
      strftime(AboveTemp1_time,sizeof(AboveTemp1_time), "%d/%b %H:%M", &timeinfo);
      chrono_zero = millis();     // starts a timer
    }
    if (chrono_zero + max_time_above_temp < millis()){    //if above temperature for longer than max_time_above_temp
      Serial.print("Hot for too long, starting at ");
      Serial.println(AboveTemp1_time);
      faults2 = String(max_time_above_temp/1000/60) + "' acima de " + String(TempLimit) + "graus";
    }
  }
  else if (hi_temp_msg == false){
    faults2 = " ";
  }

  /* Checks Battery voltage */ 

  Serial.print("ADC getting battery voltage.");
  unsigned int reading = analogRead(35);
  //Serial.println(reading);
  //Serial.print("DCT ");
  //Serial.println(float(reading)/4095*3.3*3);
  Serial.print("Poli1 ");
  float BattVolts = -0.000000000000016 * pow(reading,4) + 0.000000000118171 * pow(reading,3)- 0.000000301211691 * pow(reading,2)+ 0.001109019271794 * reading + 0.034143524634089;
  //float BattVolts = -0.000000000009824 * pow(reading,3) + 0.000000016557283 * pow(reading,2) + 0.000854596860691 * reading + 0.065440348345433;
  //Serial.print("Poli2 ");
  //Serial.println (BattVolts);
  BattVolts = BattVolts * 3;          // Accounts for wiring correction, 1 200K resistor and a 100K resistor
  //Serial.print("corrected Bat Voltage");
  Serial.println(BattVolts);
  
  //if (BattVolts >= 4.3){
  //  faults += "BattHiVolt ";
  //  Serial.println("High battery voltage");
  //}
  if (BattVolts < 3.1){
    faults+= "BattLoVolt ";
    Serial.println("Low battery voltage");
  }
  
}

void send_to_blynk_highfreq(void){
  Serial.print("WiFi status: ");
  Serial.println(WiFi.status());
  if (WiFi.status() == 3){
    Serial.println("Sending temperature to Blynk");
    Blynk.virtualWrite(V4, Temp1);
  }
  else if (WiFi.status() == 0){
    Serial.println("Internet conection lost, trying to reconnect");
    WiFi.reconnect();
  }
}

void send_to_blynk_lowfreq(void){
  if (WiFi.status() == 3){
    Serial.println("Sending low frequency data to Blynk");
    Serial.println(faults);   //if 'faults' is empty, this method is not executed...
    if (faults == ""){
      faults = " ";
    }
    Blynk.virtualWrite(V6,faults);
    Blynk.virtualWrite(V5,faults2);
    Blynk.virtualWrite(V7,MaxTemp1);
    Blynk.virtualWrite(V8,MaxTemp1_time);
    Serial.print("Max temp 1 recorded: ");
    Serial.print(MaxTemp1);
    Serial.print(" at ");
    Serial.println(MaxTemp1_time);
  }
}

BLYNK_WRITE(V9){
  pinV9data = param.asInt();
  }

BLYNK_WRITE(V1){
  TempLimit = param.asFloat();
}

BLYNK_WRITE(V2){
  max_time_above_temp = param.asInt() * 1000 * 60;
}

void setup(void){
  Serial.begin(115200);  
  sensors.begin();

  //adcAttachPin(12);
  //analogSetPinAttenuation(12,ADC_11db);

  pinMode(pin_button, INPUT);

  //WiFi setup via WiFi Manager
  //WiFiManager wifiManager;
  //exit after config instead of connecting
  wifiManager.setBreakAfterConfig(true);
  //reset settings - for testing
  //wifiManager.resetSettings();
  
  if (!wifiManager.autoConnect("Termometro_ESP", "password")) {
    Serial.println("failed to connect, we should reset as see if it connects");
    delay(3000);
    ESP.restart();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected to WiFi.");


  Serial.println("local ip");
  Serial.println(WiFi.localIP());

  Blynk.config(auth, "blynk-cloud.com", 8080);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  
  
  timer.setInterval(10000, send_to_blynk_highfreq);
  timer.setInterval(60000, send_to_blynk_lowfreq);
  timer.setInterval(1000, get_temps);
  timer.setInterval(1000, get_high_temps);
  timer.setInterval(5000, check_faults);
}

void loop(void){
  Blynk.run();
  timer.run();
  if (pinV9data == 1){
    Serial.println("button V9 pressed on app");
    Serial.println(TempLimit);
    Serial.println(max_time_above_temp);
    MaxTemp1 = NULL;
    get_high_temps();
    delay(10);
    hi_temp_msg = false;
    check_faults();
    delay(10);
    send_to_blynk_lowfreq();
  }

  if (digitalRead(pin_button) == HIGH){
    Serial.println("***Button pressed ***");
    delay(2000);
    if (digitalRead(pin_button) == HIGH){
      Serial.println("***Button pressed long ***");
      delay(2000);
      wifiManager.resetSettings();
    }
    delay(1000);
    ESP. restart();
    delay(200);
  }
  delay(50);
}
