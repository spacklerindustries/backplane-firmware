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
unsigned long pollInterval = 30000; // 30 seconds

int respBytes=8; //number of bytes returned for each slot 8bytes

byte maxAddresses = 120;

void setup() {
  /* join as slave ID 1 to accept onrecieve events */
  /* i2c */
  Wire.begin();
  Wire.onReceive(receiveEvents);
  /* i2c */
  Serial.begin(9600);
}

void loop() {
  if (stringComplete) {
    // do something with the data we got
    jsonBuffer.clear(); //clear the buffer or it stops updating properly
    JsonObject& root = jsonBuffer.parseObject(inputString);
    inputString.trim();
    String buffer = "{\"message\": \"received request\",\"slots\":[], \"received\": ["+inputString+"]}";
    //output the buffer to serial for reading by backplane-controller service
    Serial.println(buffer);
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
  String buffer = "{\"message\": \"I2C"+String(i2c)+",SL"+String(slot)+",CMD"+String(cmd)+",RESP:"+String(respond[0])+String(respond[1])+"\",\"slots\":[]}";
  //output the buffer to serial for reading by backplane-controller service
  Serial.println(buffer);
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
  String buffer = "{\"message\": \"slot data\",";
  buffer += "\"slots\":[{\"i2ca\":" +String(a[0])+ ",\"i2cs\":" +String(a[6])+ ",\"ps\":" +String(a[1])+ ",\"ao\":" +String(a[4])+ ",\"ct\":" +String(a[5])+ "}]}";
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
  for(address = 8; address < maxAddresses; address++ )
  {
    Wire.beginTransmission(address);
    delay(100);
    error = Wire.endTransmission();
    delay(500);
    if (error == 0) {
      delay(500);
      
      Serial.print("{\"message\":\"I2C device found at address ");
      Serial.print(address);
      Serial.println("\", \"slots\":[]}");
      
      Wire.requestFrom((int)address, 32); //request 7 bytes from slave
      int argIndex = -1;
      while(Wire.available()) { //loop through the bytes
        if (argIndex < 32){
          argIndex++;
          a[argIndex] = Wire.read(); /* collect all the data from slave */
        }
      }
      String buffer = "{\"message\": \"slot data\", \"slots\":[";
      int i2ca=0;
      int i2cs=6;
      int ps=1;
      int ao=4;
      int ct=5;
      int numSlots=0;
      int byteCount=7;
      // calculate how many slots we got data for
      for (int retData=0; retData<32; retData++) {
        //if (retData == byteCount) {
          if (a[retData] == 254) {
            numSlots++;
            //byteCount=byteCount+7;
            //if (a[byteCount+1] == 255) {
            //break;
            //}
          }
        //}
      }
      /*
      Serial.print("{\"message\":\"number of slots returned; ");
      Serial.print(numSlots);
      Serial.println("\", \"slots\":[]}");
      */
      // parse the data into json
      for (int slot = 0; slot < numSlots; slot++) {
        if (slot != 0) {
          buffer += ","; // add comma to all other slots
        }
        buffer += "{\"i2ca\":" +String(a[i2ca])+ ",\"i2cs\":" +String(a[i2cs])+ ",\"ps\":" +String(a[ps])+ ",\"ao\":" +String(a[ao])+ ",\"ct\":" +String(a[ct]) +"}";
        i2ca=i2ca+respBytes;
        i2cs=i2cs+respBytes;
        ps=ps+respBytes;
        ao=ao+respBytes;
        ct=ct+respBytes;
      }
      //buffer += "{\"i2ca\":" +String(a[i2ca])+ ",\"i2cs\":" +String(a[i2cs])+ ",\"ps\":" +String(a[ps])+ ",\"ao\":" +String(a[ao])+ ",\"ct\":" +String(a[ct]) +"}";
      buffer += "]}";
      //output the buffer to serial for reading by backplane-controller service
      Serial.println(buffer);
      delay(500);
    }
    else if (error==4) {
      Serial.print("{\"message\":\"Unknown error at address 0x");
      if (address<16) {
        Serial.print("0");
      }
      Serial.print(address,HEX);
      Serial.println("\", \"slots\":[]}");
    }
  }
}
