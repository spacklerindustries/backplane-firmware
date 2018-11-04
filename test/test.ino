/*********
  Rui Santos
  Complete project details at http://randomnerdtutorials.com
  Arduino IDE example: Examples > Arduino OTA > BasicOTA.ino
*********/

// esp8266 and ota
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
// json
#include <ArduinoJson.h>
// mqtt
#include <PubSubClient.h>
// shift register
#include <Shifty.h> /* https://github.com/johnnyb/Shifty */

/* `config.h` This file contains all the configuration for the backplane unit
 * Able to configure:
 * - Number of slots
 * - Unit ID
 * - WIFI SSID and key
 * - MQTT server and auth
 * - OTA auth
*/
#include "config.h"

/* WEB
#include <ESP8266WebServer.h>
ESP8266WebServer server(80);
const String HTTP_HEAD           = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/><title>"+String(mqtt_client_name)+" "+String(backplane_number)+"</title>";
const String HTTP_STYLE  = "<style>.c{text-align: center;} div,input{padding:5px;font-size:1em;}  input{width:90%;}"
    " body{text-align: center;font-family:verdana;}"
    " button{border:0;border-radius:0.6rem;background-color:#1fb3ec;color:#fdd;line-height:2.4rem;font-size:1.2rem;width:100%;}"
    " .q{float: right;width: 64px;text-align: right;} .button2 {background-color: #008CBA;} .button3 {background-color: #f44336;}"
    " .button4 {background-color: #e7e7e7; color: black;} .button5 {background-color: #555555;} .button6 {background-color: #4CAF50;} </style>";
const String HTTP_SCRIPT        = "<script>function c(l){document.getElementById('s').value=l.innerText||l.textContent;document.getElementById('p').focus();}</script>";
const String HTTP_HEAD_END      = "</head><body><div style='text-align:left;display:inline-block;min-width:260px;'>";
const String HOMEPAGE = "<form action=\"/cmd1\" method=\"get\"><button class=\"button3\">Slot1</button></form><br/>"
    "<form action=\"/cmd2\" method=\"get\"><button class=\"button6\">Slot2</button></form><br/>"
    "<form action=\"/cmd3\" method=\"get\"><button class=\"button2\">Slot3</button></form><br/>";
/* WEB */

/* set up the shift in and out registers */
/* Shift OUT register pins (74HC595) */
int latchOutPin = D2;
int clockOutPin = D3;
int dataOutPin = D1;
/* Shift IN register pins (74HC165) */
int ploadPin        = D5;  // Connects to Parallel load pin the 165
int clockEnablePin  = D8;  // Connects to Clock Enable pin the 165
int dataPin         = D7; // Connects to the Q7 pin the 165
int clockPin        = D6; // Connects to the Clock pin the 165
/* Define the number of slots that this backplane unit will support */
#define NUMBER_OF_SHIFT_CHIPS   numBackplaneSlots
#define DATA_WIDTH   NUMBER_OF_SHIFT_CHIPS * 8
#define PULSE_WIDTH_USEC   5
#define POLL_DELAY_MSEC   1
#define BYTES_VAL_T unsigned long
BYTES_VAL_T pinValues;
BYTES_VAL_T oldPinValues;

/* set up button state variables and timers */
int buttondowncount[numBackplaneSlots];
int buttonupcount[numBackplaneSlots];
int powerstatus[numBackplaneSlots];
int checkOffCount[numBackplaneSlots];
int waitingOffTimeoutCount[numBackplaneSlots];
bool blinking[numBackplaneSlots];
bool fastBlinking[numBackplaneSlots];
bool checkPiIsOff[numBackplaneSlots];
int flash[numBackplaneSlots];
unsigned long pollInterval = 60000;
unsigned long blinkInterval = 250;
unsigned long piCheckInterval = 500;
unsigned long fastBlinkInterval = 100;
unsigned long debounce = 50;
unsigned long longPress = 2000;
unsigned long currentMillis[numBackplaneSlots];
unsigned long previousMillis[numBackplaneSlots];
unsigned long currentMillisPiCheck[numBackplaneSlots];
unsigned long previousMillisPiCheck[numBackplaneSlots];
unsigned long currentMillisFast[numBackplaneSlots];
unsigned long previousMillisFast[numBackplaneSlots];
unsigned long currentMillisPoll;
unsigned long previousMillisPoll;
int buttonstate[numBackplaneSlots];
int buttonstatelast[numBackplaneSlots];
unsigned long downTime[numBackplaneSlots];
unsigned long upTime[numBackplaneSlots];

/* bit position decimal equivalents */
int bitValue[] = {1,2,4,8};

/* create shifty object */
Shifty shiftout;

/* create WIFI client and PubSub objects */
#ifdef mqtt_tls
 #if (mqtt_tls == true)
   WiFiClientSecure espClient;
   int mqtt_used_port = mqtt_tls_port;
 #else
   WiFiClient espClient;
   int mqtt_used_port = mqtt_port;
 #endif
#endif
PubSubClient client(espClient);

/* Initial setup of backplane unit */
void setup() {
  /* set up shift registers */
  pinMode(ploadPin, OUTPUT);
  pinMode(clockEnablePin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, INPUT);
  shiftout.setBitCount(DATA_WIDTH);
  shiftout.setPins(dataOutPin, clockOutPin, latchOutPin);

  /* set up serial output */
  Serial.begin(115200);
  Serial.println("Booting");
  /* set up WiFi */
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.hostname(mqtt_client_name);
  /* try connect 4 times, then skip and continue to function */
  for (int i=1; i<4; i++) {
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.println("Attepmt "+String(i)+"Connection Failed! Trying again...");
      delay(1500);
      // no restart, we want backplane to still function even without wifi
      //ESP.restart();
    }
  }
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    // no restart, we want backplane to still function even without wifi
    Serial.println("Connection failed, entering non blocking state...");
  }
  /* set up OTA */
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(mqtt_client_name);
  if (ota_auth == true ) {
    ArduinoOTA.setPassword(ota_password);
  }
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  /* ready */
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  /* initial pin state */
  pinValues = read_shift_regs();
  //display_pin_values();
  oldPinValues = pinValues;

  /* connect MQTT */
  client.setServer(mqtt_server, mqtt_used_port);
  client.setCallback(callback);

  /* WEB
  server.on("/", handleRoot);
  server.on("/cmd1", cmd1);
  server.on("/cmd2", cmd2);
  server.on("/cmd3", cmd3);
  server.begin();
  Serial.println("HTTP server started");
  /* WEB */
  Serial.println("Setup Complete");
}

/* ----
MQTT Callback function
---- */
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  // get message into array to pass to arduinojson to decode
  char message[length + 1];
  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  Serial.println("");
  StaticJsonBuffer<300> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(message);
  if (!root.success()) {
    Serial.println("parseObject() failed");
    return;
  }
  int power_cmd=0;
  int received_backplane=0;
  int received_slotnum=1; // fall back to slot 1 all the time
  if (root.containsKey("message_data")) {
    power_cmd=root["message_data"]["p_status"];
    received_backplane=root["message_data"]["bp_num"];
    if (root["message_data"]["s_num"]) {
      received_slotnum=root["message_data"]["s_num"];
    }
  }
  if (received_backplane == backplane_number) {
    //pass power_cmd and received_slotnum to action function
    Serial.print("slot ");
    Serial.print(received_slotnum);
    Serial.print(" power cmd ");
    Serial.println(power_cmd);
    powerstatus[received_slotnum] = power_cmd;
  }
}

void reconnect() {
  /* Loop until we're reconnected */
  if (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    /* if mqtt auth is configured, use it otherwise proceed with no auth */
    if (mqtt_auth == true) {
      if (client.connect(mqtt_client_name, mqtt_user, mqtt_password )) {
        Serial.println("connected");
        client.subscribe(mqtt_caddy_topic);
      } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        // Wait 5 seconds before retrying
        delay(5000);
      }
    } else {
      if (client.connect(mqtt_client_name)) {
        Serial.println("connected");
        client.subscribe(mqtt_caddy_topic);
      } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        // Wait 5 seconds before retrying
        delay(5000);
      }
    }
  }
}

void loop() {
  /* Serial output for no WiFi */
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Disconnected from WIFI, will continue to try again.");
  }
  /* OTA and WiFi handlers */
  ArduinoOTA.handle();
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  /* WEB
  server.handleClient();
  /* WEB */

  /* Do backplane functions here */
  /* get pin values */
  pinValues = read_shift_regs();
  /* process pin values to check if they changed */
  pinValueCheck();

  /* do button check stuff here, physical pin 5 is pinoffset value of 4 on the shift register*/
  int pinOffsetVal = 4;
  for(int i = 0; i < numBackplaneSlots; i ++) {
    int slotNum = i;
    int reading = readShiftInPin(slotNum, 4);
    buttonstate[slotNum] = reading;
    int slotAlwaysOn = invertLogic(readShiftInPin(slotNum, 5));
    if (getType(i) == 0 && readShiftInPin(slotNum, 7) != 1) {
      Serial.print("Empty Slot ");
      Serial.print(slotNum);
      Serial.print(" Type ");
      Serial.print(getType(i));
      Serial.print(" PowerStatus ");
      Serial.println(readShiftInPin(slotNum, 7));
      /* if a slot is empty, turn it off :) */
      powerOff(slotNum);
    }
    /* always on check */
    if (slotAlwaysOn == 0) {
      /* blinking check */
      blinkCheck(slotNum);
      /* pi detection check */
      piDetectCheck(slotNum);
      /* faster blinking check */
      fastBlinkCheck(slotNum);
      /* end blinking checks */
      /* button state check */
      if (buttonstate[slotNum] == 0 && buttonstatelast[slotNum] == 1 && (millis() - upTime[slotNum]) > debounce) {
        downTime[slotNum] = millis();
        Serial.print("Slot ");
        Serial.print(slotNum);
        Serial.println(" down");
      } else if (buttonstate[slotNum] == 1 && buttonstatelast[slotNum] == 0 && (millis() - downTime[slotNum]) > longPress) {
        upTime[slotNum] = millis();
        Serial.print("Slot ");
        Serial.print(slotNum);
        Serial.println(" up longpress");
        powerstatus[slotNum] = 5;
      } else if (buttonstate[slotNum] == 0 && (millis() - downTime[slotNum]) > longPress) {
        fastBlinking[slotNum] = true;
      } else if (buttonstate[slotNum] == 1 && buttonstatelast[slotNum] == 0 && (millis() - downTime[slotNum]) > debounce) {
        upTime[slotNum] = millis();
        Serial.print("Slot ");
        Serial.print(slotNum);
        Serial.println(" up");
        delay(100); //delay short time to check if slot comes online
        Serial.print("Slot ");
        Serial.print(slotNum);
        Serial.print(" power status ");
        Serial.println(readShiftInPin(slotNum, 7));
        if (readShiftInPin(slotNum, 7) == 1) {
          Serial.print("Slot ");
          Serial.print(slotNum);
          Serial.println(" power is off");
          powerstatus[slotNum] = 2;
        } else {
          Serial.print("Slot ");
          Serial.print(slotNum);
          Serial.println(" power is on");
          powerstatus[slotNum] = 3;
        }
      }
      buttonstatelast[slotNum] = buttonstate[slotNum];
      /* end button state check */
      /* If powerstatus is 2 (ON) and the MOSFET is not already HIGH then power on once only */
      if(powerstatus[slotNum]==2 && readShiftInPin(slotNum, 7) == 1) {
        /* Initiate shutdown on raspberry pi GPIO */
        writeShiftOutPin(slotNum, 2, LOW);
        /* Turn power on */
        if (getType(i) != 0 ) { // if the caddy is empty, don't bother turning it on
          powerOn(slotNum);
        } else {
          powerstatus[slotNum]=0;
        }
        /* Update master */
        //sendToMaster(slotNum);
      } else if(powerstatus[slotNum]==3) {
        Serial.print("Slot ");
        Serial.print(slotNum);
        Serial.println(" Send shutdown signal");
        /* Start Shutdown Sequence */
        writeShiftOutPin(slotNum, 2, HIGH);
        delay(800);
        writeShiftOutPin(slotNum, 2, LOW);
        Serial.print("Slot ");
        Serial.print(slotNum);
        Serial.println(" Start shutdown sequence");
        powerstatus[slotNum]=4;
        checkOffCount[slotNum] = 0;
        waitingOffTimeoutCount[slotNum] = 0;
      } else if(powerstatus[slotNum]==4) {
        blinking[slotNum] = true;
        checkPiIsOff[slotNum] = true;

        /* Check if it's off 5 times before shutting down */
        if(checkOffCount[slotNum]>=5) {
          Serial.print("Slot ");
          Serial.print(slotNum);
          Serial.println(" Power Off");
          blinking[slotNum] = false;
          fastBlinking[slotNum] = false;
          checkPiIsOff[slotNum] = false;
          /* It's really off */
          powerOff(slotNum);
          /* Update master */
          //sendToMaster(slotNum);
          powerstatus[slotNum] = 0;
          checkOffCount[slotNum] = 0;
          waitingOffTimeoutCount[slotNum] = 0;
        } else if(waitingOffTimeoutCount[slotNum]>=40) {
          Serial.print("Slot ");
          Serial.print(slotNum);
          Serial.println(" Timed out, powering off");
          blinking[slotNum] = false;
          fastBlinking[slotNum] = false;
          checkPiIsOff[slotNum] = false;
          /* Time out shutdown anyway */
          powerOff(slotNum);
          /* Update master */
          //sendToMaster(slotNum);
          powerstatus[slotNum] = 0;
          checkOffCount[slotNum] = 0;
          waitingOffTimeoutCount[slotNum] = 0;
        }
      } else if(powerstatus[slotNum]==5) {
        /* Hard shutdown */
        Serial.print("Slot ");
        Serial.print(slotNum);
        Serial.println(" Hard Shutdown");
        blinking[slotNum] = false;
        fastBlinking[slotNum] = false;
        checkPiIsOff[slotNum] = false;
        /* Turn power off */
        powerOff(slotNum);
        /* Update master */
        //sendToMaster(slotNum);
        powerstatus[slotNum] = 0;
        checkOffCount[slotNum] = 0;
      }/* end powerstatus check */
    /* end always on check */
    } else {
      /* if always on is enabled, and the slot is not powered on, power it on */
      if (readShiftInPin(slotNum, 7) == 1) {
        Serial.println(slotNum);
        powerOn(slotNum);
      }
    }
  }
  /* set a short delay */
  delay(50);
}


void createResponse(int slotNum) {
  StaticJsonBuffer<300> respJsonBuffer;
  JsonObject& responseRoot = respJsonBuffer.createObject();
  responseRoot["message_type"] = "caddy_data";
  JsonObject& message_data = responseRoot.createNestedObject("message_data");
  message_data["bp_num"] = backplane_number;
  message_data["s_num"] = slotNum;
  message_data["c_type"] = getType(slotNum);
  int powState = readShiftInPin(slotNum, 7);
  message_data["p_status"] = invertLogic(powState);
  int alwaysOn = readShiftInPin(slotNum, 5);
  message_data["a_on"] = invertLogic(alwaysOn);
  char buffer[300];
  responseRoot.printTo(buffer, sizeof(buffer));
  Serial.print("Response Msg: ");
  Serial.println(buffer);
  client.publish("bp/caddy_data", buffer);
}


void writeShiftOutPin(int slotNum, int pinNumber, int highOrLow) {
  int pinOffsetVal = pinNumber;
  for(int i = 0; i < numBackplaneSlots; i ++) {
    if (i == slotNum) {
      break;
    }
    pinOffsetVal=pinOffsetVal+8;
  }
  if (highOrLow == HIGH) {
    shiftout.writeBit(pinOffsetVal, HIGH);
  } else {
    shiftout.writeBit(pinOffsetVal, LOW);
  }
}

int readShiftInPin(int slotNum, int pinNumber) {
  int pinOffsetVal = pinNumber;
  for(int i = 0; i < numBackplaneSlots; i ++) {
    if (i == slotNum) {
      break;
    }
    pinOffsetVal=pinOffsetVal+8;
  }
  return get_pin_value(pinOffsetVal);
}

int get_pin_value(int pinNumber)
{
    //Serial.print("Pin States:\r\n");
    pinValues = read_shift_regs();
    for(int i = 0; i < DATA_WIDTH; i++)
    {
      if (pinNumber == i) {
        if((pinValues >> i) & 1) {
            //Serial.println("HIGH");
            return 1;
        } else {
            //Serial.println("LOW");
            return 0;
        }
      }
    }
}

void display_pin_values()
{
    Serial.print("Pin States:\r\n");
    for(int i = 0; i < DATA_WIDTH; i++)
    {
        Serial.print("  Pin-");
        Serial.print(i);
        Serial.print(": ");
        if((pinValues >> i) & 1)
            Serial.print("HIGH");
        else
            Serial.print("LOW");
        Serial.print("\r\n");
    }
    Serial.print("\r\n");
}

BYTES_VAL_T read_shift_regs()
{
    long bitVal;
    BYTES_VAL_T bytesVal = 0;
    /* Trigger a parallel Load to latch the state of the data lines, */
    digitalWrite(clockEnablePin, HIGH);
    digitalWrite(ploadPin, LOW);
    delayMicroseconds(PULSE_WIDTH_USEC);
    digitalWrite(ploadPin, HIGH);
    digitalWrite(clockEnablePin, LOW);
    /* Loop to read each bit value from the serial out line of the SN74HC165N. */
    for(int i = 0; i < DATA_WIDTH; i++)
    {
        bitVal = digitalRead(dataPin);
        /* Set the corresponding bit in bytesVal. */
        bytesVal |= (bitVal << ((DATA_WIDTH-1) - i));
        /* Pulse the Clock (rising edge shifts the next bit). */
        digitalWrite(clockPin, HIGH);
        delayMicroseconds(PULSE_WIDTH_USEC);
        digitalWrite(clockPin, LOW);
    }
    return(bytesVal);
}

void pinValueCheck() {
  if (pinValues != oldPinValues) {
    /* Update master */
    int count=0;
    int slotnum=0;
    int changed[numBackplaneSlots];
    for(int b=0; b<numBackplaneSlots; b++) {
      changed[b]=0;
    }
    for(int i = 0; i < DATA_WIDTH; i++)
    {
      int test1 = (pinValues >> i) & 1; // get value from current pin values
      int test2 = (oldPinValues >> i) & 1; // get value from previous pin values
      if(test1 != test2) {
        changed[slotnum] = 1;
      }
      if (count == 7) { //we have 8 pins in the shift register per slot, if we hit count 7 that is the last pin for that slot, do the function
        if (changed[slotnum] == 1) {
          // publish mqtt to topic with pin values or json string
          createResponse(slotnum);
          delay(5);
        }
        count = 0; // reset
        changed[slotnum] = 0; // reset
        slotnum++; // increment slotnum for next slot
      } else {
        // increment else
        count++;
      }
    }
    oldPinValues = pinValues; // pins changed, set old pin to current pin and exit
  }
}

int getType(int slotNum) {
  int pinOffsetVal = 0;
  for(int i = 0; i < numBackplaneSlots; i ++) {
    if (i == slotNum) {
      break;
    }
    pinOffsetVal=pinOffsetVal+8;
  }
  int ret = 0;  /* Initialize the variable to return */
  for(int i = 0; i < 4; i++)  /* cycle through all the pins */
  {
    int pinStateAndOffset = i+pinOffsetVal;
    //Serial.print("PinOffset: ");
    //Serial.println(pinStateAndOffset);
    if(get_pin_value(pinStateAndOffset) == 1)
    {
      /* adding the bit position decimal equivalent flips that bit position */
      ret = ret + bitValue[i];
    }
  }
  return ret;
}

int invertLogic(int logic)
{
  if (logic == 1) {
    return 0;
  } else {
    return 1;
  }
}

void blinkCheck(int slotNum) {
  if (blinking[slotNum]) {
    currentMillis[slotNum] = millis();
    if ((currentMillis[slotNum] - previousMillis[slotNum]) >= blinkInterval) { // enough time passed yet?
      if (flash[slotNum] == 0) {
        writeShiftOutPin(slotNum, 1, LOW);
        flash[slotNum] = 1;
      } else {
        writeShiftOutPin(slotNum, 1, HIGH);
        flash[slotNum] = 0;
      }
      previousMillis[slotNum] = currentMillis[slotNum]; // sets the time we wait "from"
    }
  }
}
void fastBlinkCheck(int slotNum) {
  if (fastBlinking[slotNum]) {
    currentMillisFast[slotNum] = millis();
    if ((currentMillisFast[slotNum] - previousMillisFast[slotNum]) >= fastBlinkInterval) { // enough time passed yet?
      if (flash[slotNum] == 0) {
        writeShiftOutPin(slotNum, 1, LOW);
        flash[slotNum] = 1;
      } else {
        writeShiftOutPin(slotNum, 1, HIGH);
        flash[slotNum] = 0;
      }
      previousMillisFast[slotNum] = currentMillisFast[slotNum]; // sets the time we wait "from"
    }
  }
}
void piDetectCheck(int slotNum) {
  if (checkPiIsOff[slotNum]) {
    currentMillisPiCheck[slotNum] = millis();
    if ((currentMillisPiCheck[slotNum] - previousMillisPiCheck[slotNum]) >= piCheckInterval) { // enough time passed yet?
      if (checkPiOff(slotNum) == 0) {
        checkOffCount[slotNum] = checkOffCount[slotNum] + 1;
      } else {
        checkOffCount[slotNum]=0;
        waitingOffTimeoutCount[slotNum] = waitingOffTimeoutCount[slotNum] + 1;
      }
      previousMillisPiCheck[slotNum] = currentMillisPiCheck[slotNum]; // sets the time we wait "from"
    }
  }
}
/* end check functions */

/*
 * pi detection sequence
 */
bool checkPiOff(int slotNum)
{
   int sensorValue = readShiftInPin(slotNum, 6) ; //digitalRead(PIDETECTION);
   Serial.println("Detection state: "+String(sensorValue));
   return sensorValue;
}

/*
 * power off sequence
 */
void powerOff(int slotNum)
{
  //digitalWrite(LED_PIN, LOW);
  //digitalWrite(MOSFET_PIN, LOW);
  writeShiftOutPin(slotNum, 0, LOW);
  writeShiftOutPin(slotNum, 1, LOW);
  Serial.print("Slot ");
  Serial.print(slotNum);
  Serial.println(" PowerOff");
}

/*
 * power on sequence
 */
void powerOn(int slotNum)
{
  //digitalWrite(LED_PIN, HIGH);
  //digitalWrite(MOSFET_PIN, HIGH);
  writeShiftOutPin(slotNum, 0, HIGH);
  writeShiftOutPin(slotNum, 1, HIGH);
  Serial.print("Slot ");
  Serial.print(slotNum);
  Serial.println(" PowerOn");
}

int pinOffsetNumber(int slotNum, int pinNum) {
  int pinOffsetVal = pinNum;
  for(int i = 0; i < numBackplaneSlots; i ++) {
    if (i == slotNum) {
      break;
    }
    pinOffsetVal=pinOffsetVal+8;
  }
  return pinOffsetVal;
}

/*
 * Just a flashing sequence
 */
void longPressFlash(int slotNum)
{
  for(int i=0; i< 3; i++)
  {
    //digitalWrite(LED_PIN, LOW);
    writeShiftOutPin(slotNum, 1, LOW);
    delay(50);
    //digitalWrite(LED_PIN, HIGH);
    writeShiftOutPin(slotNum, 1, HIGH);
    delay(50);
  }
}




/* WEB

void handleRoot() {
 String s =HTTP_HEAD;
       s += HTTP_STYLE;
      s += HTTP_SCRIPT;
      s += HTTP_HEAD_END;
      s += "<H3>www.dcmote.duino.lk</H3>";
      s+=HOMEPAGE;
  server.send(200, "text/html", s);
}

void cmd1() {
 String s =HTTP_HEAD;
       s += HTTP_STYLE;
      s += HTTP_SCRIPT;
      s += HTTP_HEAD_END;
      s += "<H3>www.dcmote.duino.lk</H3>";
      s+=HOMEPAGE;
  server.send(200, "text/html", s);
  powerOn(0);
}
void cmd2() {
 String s =HTTP_HEAD;
       s += HTTP_STYLE;
      s += HTTP_SCRIPT;
      s += HTTP_HEAD_END;
      s += "<H3>www.dcmote.duino.lk</H3>";
      s+=HOMEPAGE;
  server.send(200, "text/html", s);
  powerOn(1);
}
void cmd3() {
 String s =HTTP_HEAD;
       s += HTTP_STYLE;
      s += HTTP_SCRIPT;
      s += HTTP_HEAD_END;
      s += "<H3>www.dcmote.duino.lk</H3>";
      s+=HOMEPAGE;
  server.send(200, "text/html", s);
  powerOn(2);
}

/* WEB */
