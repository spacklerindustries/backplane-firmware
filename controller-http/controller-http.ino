/*
Caddyshack Master controller - spackler
*/

// I2C includes
#include <Wire.h>
// Ethernet includes
#include <Ethernet.h>
// JSON includes
#include <ArduinoJson.h>
// mdns includes
#include <ArduinoMDNS.h>

#define BUFSIZE 16

byte mac[] {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(10, 1, 1, 200);

bool usemdns = false;
//const char* greenskeeper = "greenskeeper.example.com";
IPAddress greenskeeper(10, 1, 1, 1);
//bool usemdns = true;
const char* greenskeepermdns = "greenskeeper";
int a[32];

/* mdns */
EthernetUDP udp;
MDNS mdns(udp);
/* mdns */

EthernetServer server(80);

EthernetClient client;
boolean alreadyConnected = false;
  
unsigned long checkInterval = 3000;

const int BUFFER_SIZE = JSON_OBJECT_SIZE(3);

/*
  mdns translation
*/
void nameFound(const char* name, IPAddress ipr);

void setup() {
  /* join as slave ID 1 to accept onrecieve events */
  /* i2c */
  Wire.begin(1);
  Wire.onReceive(receiveEvents);
  /* i2c */
  /* ethernet */
  Serial.begin(9600);
  delay(1500); //ethernet init delay
  //Ethernet.begin(mac, ip);
  // Initialize Ethernet libary DHCP
  if (!Ethernet.begin(mac)) {
    Serial.println(F("Failed to initialize Ethernet library"));
    return;
  }
  //client.setClientTimeout(100);
  delay(1000); //ethernet additional init delay
  server.begin();
  /* mdns */
  if (usemdns == true) {
    mdns.begin(Ethernet.localIP(), "spackler");
    mdns.setNameResolvedCallback(nameFound);
    mdns.resolveName(greenskeepermdns, 5000);
  }
  /* mdns */
  Serial.print("IP Address: ");
  Serial.println(Ethernet.localIP());
  /* ethernet */
}

void loop() {
  /* mdns */
  if (usemdns == true) {
    mdns.run();
  }
  /* mdns */
  delay(100);
  EthernetClient connectedClient = server.available();
  if (connectedClient) {
    Serial.println("client connected_server");
    connectedClient.setTimeout(500);
    process(connectedClient);
  }
}

/*
  process the request from the client,
  send it to the backplane i2c bus
*/
void process(EthernetClient connectedClient1) {
  boolean currentLineIsBlank = true;
  byte index = 0;
  char clientline[BUFSIZE];
  boolean success = false;

  while (connectedClient1.connected()) {
    while (connectedClient1.available()) {
      char c = connectedClient1.read();

      if (index < BUFSIZE) {
        clientline[index++] = c;
      }

      if (c == '\n' && currentLineIsBlank) {
        connectedClient1.flush();

        char *method = strtok(clientline, "/");
        char *i2cstring = strtok(NULL, "/");
        char *slotstring = strtok(NULL, "/");
        char *commandstring = strtok(NULL, " ");
        byte i2c = atoi(i2cstring);
        byte slot = atoi(slotstring);
        byte command = atoi(commandstring);

        Serial.print(command);
        Serial.print(": ");
        Serial.println(i2c);

        if (command != NULL && strcmp(method, "GET\n") && i2c >= 8 && i2c <= 120 && slot >= 1 && slot <= 3) {
          powerControlSlot(i2c,slot,command);
          success = true;
        }

        if (success) {
          connectedClient1.println("HTTP/1.1 200 OK");
        } else {
          connectedClient1.println("HTTP/1.1 404 Not Found");
        }
        connectedClient1.println("Content-Type: text/plain");
        connectedClient1.println("Connection: close");
        connectedClient1.println();
        connectedClient1.print("ok");

        delay(1);
        connectedClient1.stop();
        break;
      }
      if (c == '\n') {
        currentLineIsBlank = true;
      } else if (c != '\r') {
        currentLineIsBlank = false;
      }
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
  
  if (!alreadyConnected) {
    alreadyConnected = true;
  // do http client POST here
  //client.publish("bp/caddy_data", buffer);
  //if (!client.connected()) {
    
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
  //root["message_type"] = "caddy_data";
  //JsonObject& message_data = root.createNestedObject("message_data");
  //message_data["i2c"] = a[0];
  root["ps"] = a[1];
  //message_data["ls"] = a[2];
  //message_data["pi"] = a[3];
  root["ao"] = a[4];
  root["ct"] = a[5];
  //message_data["sn"] = a[6];

  char buffer[root.measureLength() + 1];
  root.printTo(buffer, sizeof(buffer));
  
  client.flush();
  client.setTimeout(500);
    if (client.connect(greenskeeper, 8080))  {
     Serial.println("connected");
     String postQuery = "POST /api/v1/caddydata/i2c/" + String(a[0]) + "/slot/" + String(a[6]) + " HTTP/1.1";
     //String hostQuery = "Host: " + IpAddress2String(greenskeeper);
     client.println(postQuery);
     client.println("Host: 10.1.1.1");
     client.println("Content-Type: application/json");
     client.print("Content-Length: ");
     //char str[] = "key=value";
     client.println(strlen(buffer));
     client.println("Connection: close\r\n");
     client.println(buffer);
     Serial.println(buffer);
     while(client.connected()) {
         while(client.available()) {
            Serial.print(client.read());
            Serial.print(" ");
         }
         
     }
     Serial.println("");
     client.stop();
     Serial.println("post complete");
     //delay(100);
     alreadyConnected = false;
   }
 }

}

String IpAddress2String(const IPAddress& ipAddress)
{
  return String(ipAddress[0]) + String(".") +\
  String(ipAddress[1]) + String(".") +\
  String(ipAddress[2]) + String(".") +\
  String(ipAddress[3])  ; 
}

/*
  mdns translation
*/
void nameFound(const char* name, IPAddress ipr)
{
  if (ipr != INADDR_NONE) {
    /*Serial.print("The IP address for '");
    Serial.print(name);
    Serial.print("' is ");*/
    Serial.println(ipr);
  } else {
    /*Serial.print("Resolving '");
    Serial.print(name);*/
    Serial.println("' timed out.");
  }
}
