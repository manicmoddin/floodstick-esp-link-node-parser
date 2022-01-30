 
#include "JeeLib.h"
#include <ELClient.h>
#include <ELClientCmd.h>
#include <ELClientMqtt.h>
#include <Wire.h>
#include <Adafruit_INA219.h>

//////////////////
// Power Sensor //
//////////////////

Adafruit_INA219 mainPower;
Adafruit_INA219 charger(0x41);



/////////////
// ESPLINK //
/////////////

// Initialize a connection to esp-link using the normal hardware serial port both for
// SLIP and for debug messages.
ELClient esp(&Serial, &Serial);

// Initialize CMD client (for GetTime)
ELClientCmd cmd(&esp);

// Initialize the MQTT client
ELClientMqtt mqtt(&esp);


//--------------------------------------------------------------------------------------------
// RFM12B Settings
//--------------------------------------------------------------------------------------------
#define MYNODE 20            // Should be unique on network, node ID 30 reserved for base station
#define freq RF12_433MHZ     // frequency - match to same frequency as RFM12B module (change to 868Mhz or 915Mhz if appropriate)
#define group 210            // network group, must be same as emonTx and emonBase

//---------------------------------------------------
// Data structures for transfering data between units

typedef struct { 
  int brightness; 
} 
PayloadTX;         // neat way of packaging data for RF comms
PayloadTX emontx;

typedef struct {
  int state, counter, battery, brightness;
}
floodStickTX;
floodStickTX floodStick;

typedef struct { 
  int seconds, mins, hours; 
} 
PayloadBase;
PayloadBase emonBaseStation;


unsigned long previousMillis = 0;

int newBrightness = 150;
//int oldBrightness = 0;


//---------------------------------- 
// LED Setup
//--------------------------------------------------------------------------------------------


////////////////////
// SLIP Functions //
////////////////////

bool connected; //Used for MQTT
#define MSG_BUFFER_SIZE  (50)
char mqttmsg[MSG_BUFFER_SIZE];




void loop(void) {

  esp.Process();
  
  if (rf12_recvDone())
  {
    if (rf12_crc == 0 && (rf12_hdr & RF12_HDR_CTL) == 0)  // and no rf errors
    {
      int node_id = (rf12_hdr & 0x1F);

      if (node_id < 4 ) {

        floodStick = *(floodStickTX*) rf12_data; 
        // do we need to update the brightness?
        int oldBrightness = floodStick.brightness;
        if (oldBrightness != newBrightness) {
          sendBrightness(node_id, newBrightness);
        }
        
        Serial.print(F("FloodStick: "));
        Serial.println(node_id);
        
        char mqttTopic[30] = "house/floodStick/";
        char mqttTopicNode[2];
        itoa(node_id, mqttTopicNode, 10);
        char mqttTopicEnder[] = "/status";
        //char mqttTopic[30];
        //strcat(mqttTopic, mqttTopicStart);
        strcat(mqttTopic, mqttTopicNode);
        strcat(mqttTopic, mqttTopicEnder);

        int counter = floodStick.counter;
        int switchLevel = floodStick.state;
        int battery = floodStick.battery;
        char floodStickInfo[50] = "{\"state\":";
        char floodStickBuff[6];
        strcat(floodStickInfo, itoa(switchLevel,floodStickBuff,10));
        strcat(floodStickInfo, ",\"Counter\":");
        strcat(floodStickInfo, itoa(counter,floodStickBuff,10));
        strcat(floodStickInfo, ",\"Battery\":");
        strcat(floodStickInfo, itoa(battery,floodStickBuff,10));
        strcat(floodStickInfo, "}");
        
        //Serial.println(mqttTopic);
        //Serial.println(floodStickInfo);
        mqtt.publish(mqttTopic, floodStickInfo);
        mqttTopic[0] = "0";
        floodStickInfo[0] = "0";

        
      }  
    }
  }

  // Every 60 seconds, get a reading of the power requirements
  unsigned long newMillis = millis();
  if (newMillis - previousMillis > 60000) {
    previousMillis = newMillis;
    Serial.println(F("Time to update power details"));
    //char powerbuf[6];
    //long power = readVcc();
    //ltoa(power, powerbuf, 10);
    //Serial.println(powerbuf);
    char mainPowerInfo[200] = "{\"shuntVoltage\":";
    char mainPowerBuff[6];
    float shuntVoltage = mainPower.getShuntVoltage_mV();
    float busVoltage   = mainPower.getBusVoltage_V();
    float current_mA   = mainPower.getCurrent_mA();
    float loadVoltage  = busVoltage + (shuntVoltage / 1000);
    float power_mW     = mainPower.getPower_mW();

   strcat(mainPowerInfo, dtostrf(shuntVoltage,1,2,mainPowerBuff));
   strcat(mainPowerInfo, ",\"busVoltage\":");
   strcat(mainPowerInfo, dtostrf(busVoltage,1,2,mainPowerBuff));
   strcat(mainPowerInfo, ",\"loadVoltage\":");
   strcat(mainPowerInfo, dtostrf(loadVoltage,1,2,mainPowerBuff));
   strcat(mainPowerInfo, ",\"current\":");
   strcat(mainPowerInfo, dtostrf(current_mA,1,2,mainPowerBuff));
   strcat(mainPowerInfo, ",\"power\":");
   strcat(mainPowerInfo, dtostrf(power_mW,1,2,mainPowerBuff));
   strcat(mainPowerInfo, "}");
    
    
   mqtt.publish("house/gardenRepeater/Battery", mainPowerInfo);

   char chargerPowerInfo[200] = "{\"shuntVoltage\":";
   char chargerPowerBuff[6];
   float cshuntVoltage = charger.getShuntVoltage_mV();
   float cbusVoltage   = charger.getBusVoltage_V();
   float ccurrent_mA   = charger.getCurrent_mA();
   float cloadVoltage  = busVoltage + (shuntVoltage / 1000);
   float cpower_mW     = charger.getPower_mW();
   //Serial.println(cshuntVoltage);
   //Serial.println(cbusVoltage);

   strcat(chargerPowerInfo, dtostrf(cshuntVoltage,1,2,chargerPowerBuff));
   strcat(chargerPowerInfo, ",\"busVoltage\":");
   strcat(chargerPowerInfo, dtostrf(cbusVoltage,1,2,chargerPowerBuff));
   strcat(chargerPowerInfo, ",\"loadVoltage\":");
   strcat(chargerPowerInfo, dtostrf(cloadVoltage,1,2,chargerPowerBuff));
   strcat(chargerPowerInfo, ",\"current\":");
   strcat(chargerPowerInfo, dtostrf(ccurrent_mA,1,2,chargerPowerBuff));
   strcat(chargerPowerInfo, ",\"power\":");
   strcat(chargerPowerInfo, dtostrf(cpower_mW,1,2,chargerPowerBuff));
   strcat(chargerPowerInfo, "}");
    
    
   mqtt.publish("house/gardenRepeater/charger", chargerPowerInfo);
   
  }

}
