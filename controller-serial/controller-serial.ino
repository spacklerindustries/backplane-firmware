/*
Caddyshack Master controller - spackler
*/

// I2C includes
#include <Wire.h>
#include <ArduinoJson.h>

StaticJsonBuffer<200> jsonBuffer;

int a[32];

String inputString = "";
boolean stringComplete = false;

unsigned long currentMillisPoll;
unsigned long previousMillisPoll;
unsigned long pollInterval = 60000; // 60 seconds

void setup() {
  /* join as slave ID 1 to accept onrecieve events */
  /* i2c */
  Wire.begin(8);
  Wire.onReceive(receiveEvents);
  /* i2c */
  Serial.begin(9600);
}

void loop() {
  if (stringComplete) {
    // do something with the data we got
    jsonBuffer.clear(); //clear the buffer or it stops updating properly
    JsonObject& root = jsonBuffer.parseObject(inputString);
    powerControlSlot(root["i2caddress"], root["i2cslot"], root["powercon"]);
    // clear the string:
    inputString = "";
    stringComplete = false;
  }
  runPollInterval();
}

void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    inputString += inChar;
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
}

/*
  actually send the command to the backplanes
*/
void powerControlSlot(byte i2c, byte slot, byte cmd) {
  /* testing output */
  //String buffer = "\"i2ca\":" +String(i2c)+ ",\"i2cs\":" +String(slot)+ ",\"ps\":0,\"ao\":0,\"ct\":8";
  //Serial.println(buffer);
  int received_backplane = i2c;
  int received_slotnum = slot;
  int power_cmd = cmd;
  Wire.beginTransmission(received_backplane); // transmit to device #8
  uint8_t respond[2];
  respond[0] = power_cmd;
  respond[1] = received_slotnum;
  //Serial.println(String(received_backplane) + ":" + String(received_slotnum) + ":" + String(power_cmd));
  Wire.write(respond, 2);
  Wire.endTransmission();
}

/*
  when we get an event from a backplane, we want to do something,
  like inform the greenskeeper that something is happening
*/
void receiveEvents(int howMany)
{
  int argIndex = -1;
  while(Wire.available()) {
    if (argIndex < 32) {
      argIndex++;
      a[argIndex] = Wire.read(); /* collect all th data from slave */
    }
  }
  String buffer = "{\"i2ca\":" +String(a[0])+ ",\"i2cs\":" +String(a[6])+ ",\"ps\":" +String(a[1])+ ",\"ao\":" +String(a[4])+ ",\"ct\":" +String(a[5])+ "}";
  //output the buffer to serial for reading by backplane-controller service
  Serial.println(buffer);
}

void runPollInterval() {
  currentMillisPoll = millis();
  if ((currentMillisPoll - previousMillisPoll) >= pollInterval) { // enough time passed yet?
    requestStatus();
    previousMillisPoll = currentMillisPoll; // sets the time we wait "from"
  }
}

void requestStatus() {
  byte error, address;
  for(address = 9; address < 120; address++ )
  {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("{\"message\":\"I2C device found at address 0x");
      if (address<16) {
        Serial.print("0");
      }
      Serial.print(address,HEX);
      Serial.println("\"}");
      Wire.requestFrom((int)address, 7); //request 7 bytes from slave
      int argIndex = -1;
      while(Wire.available()) { //loop through the bytes
        if (argIndex < 32){
          argIndex++;
          a[argIndex] = Wire.read(); /* collect all th data from slave */
        }
      }
      String buffer = "{\"i2ca\":" +String(a[0])+ ",\"i2cs\":" +String(a[6])+ ",\"ps\":" +String(a[1])+ ",\"ao\":" +String(a[4])+ ",\"ct\":" +String(a[5])+ "}";
      //output the buffer to serial for reading by backplane-controller service
      Serial.println(buffer);
    }
    else if (error==4) {
      Serial.print("{\"message\":\"Unknown error at address 0x");
      if (address<16) {
        Serial.print("0");
      }
      Serial.print(address,HEX);
      Serial.println("\"}");
    }
  }
}
