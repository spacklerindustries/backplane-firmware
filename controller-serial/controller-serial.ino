/*
Caddyshack Master controller - spackler
*/

// I2C includes
#include <Wire.h>
#include <ArduinoJson.h>

StaticJsonBuffer<200> jsonBuffer;

#define BUFSIZE 16

int a[32];

String inputString = "";
boolean stringComplete = false;

void setup() {
  /* join as slave ID 1 to accept onrecieve events */
  /* i2c */
  Wire.begin(1);
  Wire.onReceive(receiveEvents);
  /* i2c */
  Serial.begin(9600);
}

void loop() {
  if (stringComplete) {
    // do something with the data we got
    JsonObject& root = jsonBuffer.parseObject(inputString);
    powerControlSlot(root["i2caddress"], root["i2slot"], root["powercon"]);
    // clear the string:
    inputString = "";
    stringComplete = false;
  }
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
  Serial.print(cmd);
  Serial.print(slot);
  Serial.println(i2c);
  int received_backplane = i2c;
  int received_slotnum = slot;
  int power_cmd = cmd;
  Wire.beginTransmission(received_backplane); // transmit to device #8
  uint8_t respond[2];
  respond[0] = power_cmd;
  respond[1] = received_slotnum;
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
    if (argIndex < 32){
      argIndex++;
      a[argIndex] = Wire.read(); /* collect all th data from slave */
    }
  }
  String buffer = "{\"i2ca\":" +String(a[0])+ ",\"i2cs\":" +String(a[6])+ ",\"ps\":" +String(a[1])+ ",\"ao\":" +String(a[4])+ ",\"ct\":" +String(a[5])+ "}";
  //output the buffer to serial for reading by backplane-controller service
  Serial.println(buffer);
}
