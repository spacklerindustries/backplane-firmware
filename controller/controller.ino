/*

Caddyshack Master controller - spackler

*/

/* includes */
#include <Wire.h>
/* Ethernet --> */
#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

byte mac[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDA, 0x02 };
IPAddress ip(10,1,1,200); //<<< ENTER YOUR IP ADDRESS HERE!!!
//IPAddress mqtt_server(10, 1, 1, 201);
const char* mqtt_server = "mqtt.local";

EthernetClient ethClient;
PubSubClient client(ethClient);

const int BUFFER_SIZE = JSON_OBJECT_SIZE(10);

String midString(String str, String start, String finish){
  int locStart = str.indexOf(start);
  if (locStart==-1) return "";
  locStart += start.length();
  int locFinish = str.indexOf(finish, locStart);
  if (locFinish==-1) return "";
  return str.substring(locStart, locFinish);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  char message[length + 1];
  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  Serial.println("");

  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(message);
  if (!root.success()) {
    Serial.println("parseObject() failed");
    return;
  }
  int power_cmd=0;
  int received_backplane=0;
  int received_slotnum=1; // fall back to slot 1 all the time
  if (root.containsKey("message_data")) {
    power_cmd=root["message_data"]["power_status"];
    received_backplane=root["message_data"]["i2c"];
    if (root["message_data"]["slot_num"]) {
      received_slotnum=root["message_data"]["slot_num"];
    }
  }
  Wire.beginTransmission(received_backplane); // transmit to device #8
  uint8_t respond[2];
  respond[0] = power_cmd;
  respond[1] = received_slotnum;
  //Wire.write(power_cmd);              // sends one byte
  //Serial.println(power_cmd);
  //Wire.write(received_slotnum);              // sends one byte
  //Serial.println(received_slotnum);
  Wire.write(respond, 2);
  Wire.endTransmission();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("spackler")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;
      JsonObject& root = jsonBuffer.createObject();
      root["message_type"] = "service";
      JsonObject& message_data = root.createNestedObject("message_data");
      message_data["service_name"] = "spackler";
      message_data["service_status"] = "started";
      char buffer[root.measureLength() + 1];
      root.printTo(buffer, sizeof(buffer));
      client.publish("service/status",buffer);
      // ... and resubscribe
      client.subscribe("bp/power");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

/* <-- Ethernet */

void setup()
{
  /* join as slave ID 1 to accept onrecieve events */
  Wire.begin(1);
  Serial.begin(115200);
  Wire.onReceive(receiveEvents);
  /* Ethernet --> */
  Serial.println("Init DHCP...");
  if(Ethernet.begin(mac) == 0) {
    Serial.println(F("Ethernet configuration using DHCP failed"));
    for(;;);
  }
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  /* <-- Ethernet */
}

int a[32];

void recieveFromSlave(int slaveNumber)
{
  Wire.requestFrom(8, 7);
  recieveFrom();
}

void recieveFrom()
{
  int argIndex = -1;
  while(Wire.available()) {
    if (argIndex < 32){
      argIndex++;
      a[argIndex] = Wire.read(); /* collect all th data from slave */
      /*
      if (argIndex != 0) {
        Serial.print(",");
      }
      Serial.print(a[argIndex]);
      */
    }
  }
  
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["message_type"] = "caddy_data";
  JsonObject& message_data = root.createNestedObject("message_data");
  message_data["i2c"] = a[0];
  message_data["ps"] = a[1];
  message_data["ls"] = a[2];
  message_data["pi"] = a[3];
  message_data["ao"] = a[4];
  message_data["ct"] = a[5];
  message_data["sn"] = a[6];

  char buffer[root.measureLength() + 1];
  root.printTo(buffer, sizeof(buffer));

  client.publish("bp/caddy_data", buffer);
}

void receiveEvents(int howMany)
{
  recieveFrom();
}

void loop()
{
  delay(100);

  /* Ethernet --> */
  if (!client.connected()) {
    reconnect();
  }
  //client.loop();
  if (!client.loop()) {
    Serial.print("Client disconnected...");
    if (client.connect("spackler")) {
      Serial.println("reconnected.");
    } else {
      Serial.println("failed.");
    }
  }
  /* <-- Ethernet */
}
