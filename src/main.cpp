#include <Arduino.h>
#include "OneButton.h"
#include <Wire.h>
#include <Adafruit_MAX31855.h>  //Library for the thermocouple amplifier
#include <Preferences.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <Update.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <arduino-timer.h>  
#include "secrets.h"
#include "updateEntireDisplay.h"
#include "fuzzyRules.h"
#include "webserverStyling.h"

String version = "2.2.2";   // Update the version # here

#pragma region OneWire DS18B20 Definitions
#define ONE_WIRE_BUS 17
#define TEMPERATURE_PRECISION 9
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature probes(&oneWire);
DeviceAddress probe1 = {0x28, 0x8B, 0x25, 0x80, 0xE3, 0xE1, 0x3C, 0x70};
DeviceAddress probe2 = {0x28, 0x54, 0x99, 0x80, 0xE3, 0xE1, 0x3C, 0xFE};
DeviceAddress probe3 = {0x28, 0x4C, 0xA3, 0x80, 0xE3, 0xE1, 0x3C, 0x3D};
DeviceAddress probe4 = {0x28, 0xDF, 0x0B, 0x80, 0xE3, 0xE1, 0x3C, 0xD8};
DeviceAddress probe5 = {0x28, 0x29, 0xD7, 0x80, 0xE3, 0xE1, 0x3C, 0x67};
DeviceAddress probe6 = {0x28, 0x81, 0x11, 0x80, 0xE3, 0xE1, 0x3C, 0x24};
#pragma endregion

auto timer = timer_create_default();

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
unsigned long lastTCread= 0;
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];



#pragma region  Webserver Styling

#pragma endregion

WebServer server(80);

Preferences preferences;

const int thermoDO = 19;  // GPIO19, D38 (MISO)
const int thermoCS = 5;   // GPIO5, D34 (SS)
const int thermoCLK = 18; //GPIO18, D35 (SCK)
Adafruit_MAX31855 thermocouple(thermoCLK, thermoCS, thermoDO); // Initialize the Thermocouple

// Rotary Encoder Inputs
#define CLK 39
#define DT 36
#define ENC_BUTTON_PIN 34   // USE EXTERNAL PULL-UP

#pragma region  Global Variables
int counter = 0;
int currentStateCLK;
int lastStateCLK;
volatile bool encoderChanged;
String currentDir ="";
int setTemp;
int thermoReadTemp = 0;
int thermoDisplayTemp;
int minFanPWMSetting = 942;  // Corresponds to 23% fan output
int maxFanPWMSetting = 2253; // Corresponds to 55% fan output
int minFanPercentage = 23;
int maxFanPercentage = 55;
int fuzzOutput;
int currentFanPercentMinMax;
int lastFanPercentMinMax = 0;
int currentReadTemp, prevReadTemp;
int controlMode = 0; // 0 = auto, 1 = manual
bool isConnected = false;

#pragma endregion

const int fanOutPin = 4; // GPIO 4
const int pwmChannel = 0;  // Setting the PWM Channel
const int frequency = 1000;  // PWM Frequency
const int resolution_bits = 12;   // PWM Resolution

int menuDisplayMode;

OneButton button(ENC_BUTTON_PIN, true);

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

boolean MQTTreconnect() {
  if (client.connect("SmokerController", "smoker", "smoker")) {
    // Once connected, publish an announcement...
    client.publish("smoker/online","hello world");
    // ... and resubscribe
    client.subscribe("smoker/control");
  }
  return client.connected();
}

void IRAM_ATTR checkTicks()
{
  // include all buttons here to be checked
  button.tick(); // just call tick() to check the state.
}

void IRAM_ATTR updateEncoder(){
	portENTER_CRITICAL_ISR(&mux);
  // Read the current state of CLK
	currentStateCLK = digitalRead(CLK);

	// If last and current state of CLK are different, then pulse occurred
	// React to only 1 state change to avoid double count
	if (currentStateCLK != lastStateCLK  && currentStateCLK == 1){

		// If the DT state is different than the CLK state then
		// the encoder is rotating CCW so decrement
		if (digitalRead(DT) != currentStateCLK) {
			counter --;
			currentDir ="CCW";
		} else {
			// Encoder is rotating CW so increment
			counter ++;
			currentDir ="CW";
		}
    encoderChanged=true;
	}
	// Remember last CLK state
	lastStateCLK = currentStateCLK;
  portEXIT_CRITICAL_ISR(&mux);
}

int getProbeTempF(DeviceAddress deviceAddress) {
  float tempC = probes.getTempC(deviceAddress);
  int probeTempF = (DallasTemperature::toFahrenheit(tempC));;
  return probeTempF;
}

bool readThermocouple(void *){
  double fuzzyTempBase;
  double tempThermoRead = (thermocouple.readFahrenheit()); // Read the thermocouple temperature
  if (isnan(tempThermoRead) || (tempThermoRead > 1000)) {                // If the MAX31855 chip does not read a correct temp this time through the loop
    currentReadTemp = prevReadTemp;          // Make the last temp reading the current one, disregarding the erroroneous reading
  } else {                                   // Otherwise, there was a good temp reading
    fuzzyTempBase = tempThermoRead;
    currentReadTemp = round(tempThermoRead);              // And the current temp reading will be what we use
    if (currentReadTemp != prevReadTemp){   //if the reading is different from the last
      lcd.setCursor(13,1);
      lcd.print(currentReadTemp); // update the display with the new read temp
      lcd.print("  ");
    }
    prevReadTemp = currentReadTemp;          // And write it for the next iteration
  } 
  if(controlMode==0){  // if we are in Auto mode, update the fuzzy logic controller with the new temp reading
      double tempError = fuzzyTempBase - setTemp;  // calculate the temperature error
      fuzzy->setInput(1, tempError);  // send the current temp to the fuzzy input
      fuzzy->fuzzify(); // Running the Fuzzification
      fuzzOutput = fuzzy->defuzzify(1);  // get the fuzzy output for the fan speed
      ledcWrite(pwmChannel, fuzzOutput);   // in the PID channel write the PID value (0-4096) to control the fan speed
      currentFanPercentMinMax = map(fuzzOutput, minFanPWMSetting, maxFanPWMSetting, 0, 100);  // create a percentage from the 942-2253 PID output for the fan
    }
    return true;
}

bool sendTempsJSONMQTT(void*){

  probes.requestTemperatures(); // update the meat probe sensor readings
  int probe1TempF = getProbeTempF(probe1);
  int probe2TempF = getProbeTempF(probe2);
  int probe3TempF = getProbeTempF(probe3);
  int probe4TempF = getProbeTempF(probe4);
  int probe5TempF = getProbeTempF(probe5);
  int probe6TempF = getProbeTempF(probe6);

  StaticJsonDocument<256> doc;
  String jsoncontrol;
  doc["IP"] = (WiFi.localIP());
  if(controlMode==1){
    doc["control"] = "Manual";
  }  else {
    doc["control"] = "Auto";
  }
  doc["readtemp"] = currentReadTemp;
  doc["settemp"] = setTemp;
  doc["fan"] = currentFanPercentMinMax;
  doc["ver"] = (version);
  if (probe1TempF > 0){
    doc["p1"] = (probe1TempF);
  }
  if (probe2TempF > 0){
    doc["p2"] = (probe2TempF);
  }
  if (probe3TempF > 0){
    doc["p3"] = (probe3TempF);
  }
  if (probe4TempF > 0){
    doc["p4"] = (probe4TempF);
  }
  if (probe5TempF > 0){
    doc["p5"] = (probe5TempF);
  }
  if (probe6TempF > 0){
    doc["p6"] = (probe6TempF);
  }

  char buffer [256];
  serializeJson(doc, buffer);
  client.publish("smoker/status", buffer);
  client.loop();
  return true;
}

void singleEncClick() // this function will be called when a single encoder button press happens
{  
  if (menuDisplayMode==0) {

    if(controlMode==1){   // if the control mode is manual
      preferences.putInt("controlMode", 0); // Write control mode to memory
      controlMode = 0; // Set control mode to Auto
    } 
    else if (controlMode==0) {  //otherwise, the control mode must be 0 (Auto)
      preferences.putInt("controlMode", 1); // Write control mode to memory
      controlMode=1;  // change the control mode from Auto to Manual
    }
    updateEntireDisplay(setTemp, currentReadTemp, controlMode, currentFanPercentMinMax);
  } 
  sendTempsJSONMQTT(nullptr); // send an update to HA on button press
}

void callback(char* topic, byte* payload, unsigned int length) {
  // command topic:  "smoker/control"
  StaticJsonDocument<64> INdoc;
  deserializeJson(INdoc, payload, length);
  // strlcpy(name, doc["name"] | "default", sizeof(name));
  if(INdoc["setTemp"]){   // if there is something in the setTemp JSON object 
    setTemp = INdoc["setTemp"];    // setting the set temp from the message received
    preferences.putInt("setTemp", setTemp);
  }
  if(INdoc["mode"]){      // if there is something in the mode JSON object
    if(INdoc["mode"] == "Auto"){
      controlMode = 0;
    }
    if(INdoc["mode"] == "Manual"){
      controlMode = 1;
    }
  }
  if(INdoc["fanPerc"]){      // if there is something in the mode JSON object
    if (controlMode==1){   // if we are in manual fan control mode
      currentFanPercentMinMax=INdoc["fanPerc"];    // setting the fan percentage from the message received
      if (currentFanPercentMinMax > 100){
        currentFanPercentMinMax=100;
      }
      if (currentFanPercentMinMax < 0){
        currentFanPercentMinMax=0;
      }
      updateEntireDisplay(setTemp, currentReadTemp, controlMode, currentFanPercentMinMax);
    }
  }
  timer.in(500,sendTempsJSONMQTT);  //send an update back to HA
}

void updateLCDfanPercent(int x){

  lcd.setCursor(5,3);
  lcd.print(x);
  lcd.print("%           ");
}

void updateLCDSetTemp(){

  lcd.setCursor(10, 0);
  lcd.print(setTemp,0);
  lcd.print("  ");

}

void setup() {

  WiFi.hostname(HOSTNAME);
  WiFi.begin(HOME_WIFI_SSID, HOME_WIFI_PASSWORD);

	// Serial.begin(115200); // Setup Serial Monitor

  probes.begin();

  probes.setResolution(probe1, TEMPERATURE_PRECISION);
  probes.setResolution(probe2, TEMPERATURE_PRECISION);
  probes.setResolution(probe3, TEMPERATURE_PRECISION);
  probes.setResolution(probe4, TEMPERATURE_PRECISION);
  probes.setResolution(probe5, TEMPERATURE_PRECISION);
  probes.setResolution(probe6, TEMPERATURE_PRECISION);

  attachInterrupt(digitalPinToInterrupt(ENC_BUTTON_PIN), checkTicks, CHANGE);
  button.attachClick(singleEncClick);

  encoderChanged=false;

  // PWM output configuration
  ledcSetup(pwmChannel, frequency, resolution_bits); // Wonsmart Controller PWM 0-5VDC, 2-50KHZ PWM frequency
  ledcAttachPin(fanOutPin, pwmChannel);

// Load preferences from memory
  preferences.begin("settings", false); // use "settings" namespace
  setTemp = preferences.getInt("setTemp", 220); 
  controlMode = preferences.getInt("controlMode", 0);

	// Read the initial state of CLK
	lastStateCLK = digitalRead(CLK);
	
	// Call updateEncoder() when any high/low changed seen on the rotary encoder
	attachInterrupt(39, updateEncoder, CHANGE); // (GPIO, Function to call, Mode)
	attachInterrupt(36, updateEncoder, CHANGE); // (GPIO, Function to call, Mode)
  
  Wire.begin(21, 22);

  while (lcd.begin(COLUMS, ROWS, LCD_5x8DOTS, 4, 5, 400000, 250) != 1) //colums, rows, characters size, SDA, SCL, I2C speed in Hz, I2C stretch time in usec 
  {
     delay(1000);
  }
  counter=0;
  lastFanPercentMinMax = 0;

  delay(3000);

  client.setServer(MQTT_SERVER, 1883);
  client.setCallback(callback);
  client.connect("SmokerController", MQTT_USER, MQTT_PASSWORD );


  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.begin();

  lcd.clear();
  lcd.setCursor(0, 0);      //set 1-st colum & 1-st row
  lcd.print(" Smoker  Controller ");
  lcd.setCursor(6, 1);      //set 5th colum & 2nd row
  lcd.print("v. ");
  lcd.print(version);
  lcd.setCursor(0,3);
  lcd.print("IP:");
  lcd.print(WiFi.localIP());
  delay(3000);
  lcd.clear();
  
  timer.every(300, readThermocouple); // fire the readThermocouple function every 300ms to update the temp reading and fuzzy logic output
  timer.every(5000, sendTempsJSONMQTT); // fire the sendTempsJSONMQTT function every 5 seconds to send an update to HA

  updateEntireDisplay(setTemp, currentReadTemp, controlMode, currentFanPercentMinMax); // update the LCD with the initial values from memory

  updateFuzzy(minFanPWMSetting, maxFanPWMSetting); //create the fuzzy sets and rules for the fuzzy logic controller

}

void loop() {

  timer.tick();   // tick the timer
  button.tick();  // check the encoder button for a press and add time to it
  client.loop();  // loop the MQTT client to check for messages and maintain connection

  server.handleClient();    //Web Update Server Handling
  
  if (WiFi.status() == WL_CONNECTED && !isConnected) {
    // Serial.println("Connected");
    // digitalWrite(LED_BUILTIN, HIGH);
    isConnected = true;
  }

  if (WiFi.status() != WL_CONNECTED) {
    // Serial.println(".");
    // digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    // delay(1000);
    isConnected = false;
  }

  if (!client.connected()) {    // If we are disconnected from the MQTT Server
    MQTTreconnect();
  }

  if(encoderChanged==true){   // if a change to the rotary encoder is detected
    if (controlMode==1){   // if we are in manual fan control mode
      currentFanPercentMinMax=currentFanPercentMinMax+counter;  // change the fan output by the counter value
      if (currentFanPercentMinMax > 100){
        currentFanPercentMinMax=100;
      }
      if (currentFanPercentMinMax < 0){
        currentFanPercentMinMax=0;
      }
      counter=0;  //reset the counter
      encoderChanged=false;  // Clear the Encoder Update flag
      updateLCDfanPercent(currentFanPercentMinMax);
    } 
    else if (controlMode==0){  // if we are in Auto control mode
      setTemp=setTemp+(counter*5);  //change the set temp with the counter
      preferences.putInt("setTemp", setTemp);  // Write the new setting to memory
      counter=0;  //reset the counter
      encoderChanged=false;  // Clear the Encoder Update flag
      updateLCDSetTemp(); // update the LCD with the new set temp
    } 
    
  }

  if (controlMode == 1){            // if control mode is manual
    ledcWrite(pwmChannel, map(currentFanPercentMinMax, 0, 100, minFanPWMSetting, maxFanPWMSetting)); // write the fan output percentage as a PWM value to the fan
  }
  else if (controlMode == 0) {  // if control mode is automatic
    if (currentFanPercentMinMax != lastFanPercentMinMax){   // if the fan percentage has changed
      updateLCDfanPercent(currentFanPercentMinMax); // update the fan percentage display on the LCD
      lastFanPercentMinMax = currentFanPercentMinMax; //update the last fan percentage value
    }
  }

}


