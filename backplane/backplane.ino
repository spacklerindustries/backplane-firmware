/*

Caddyshack Backplane Firmware
Version: 0.1

HowTo:
* Modify i2cAddress to be unique on the bus
* Write down the slave address in the white silk screen area on the backplane controller, then install it into the shack
* Log in to greenskeeper and associate the slave address to the correct slot in the shack

Move to 3 slots per backplane board:
  * use 74hc165 shiftin, 7 pins used per slot
    * physical (software)
    * pins 1-4 (0-3) are used as binary inputs for determining slot type
    * pins 5 (4) are used as the slots button input
    * pins 6 (5) are used as the always on detection
    * pins 7 (6) are used as the pi power detection
    * pins 8 (7) are used as the mosfet power status (also connected to pin 1 of the associated SHIFTOUT register)
  * use 74hc595 shiftout, 3 used per slot
    * physical (software)
    * pins 1 (0) used for mosfet (also connected to pin 8 of the associated SHIFTIN register)
    * pins 2 (1) used for led
    * pins 3 (2) used for pi shutdown signal
*/

/* Includes */
#include <Wire.h>

#include <Shifty.h> /* https://github.com/johnnyb/Shifty */
/* Set up I/O */
/* Output Shift register pins */
Shifty shiftout;
int dataOutPin = 4;
int clockOutPin = 6;
int latchOutPin = 5;
/* Input Shift register pins */
int ploadPin        = 8;  // Connects to Parallel load pin the 165
int clockEnablePin  = 9;  // Connects to Clock Enable pin the 165
int dataPin         = 11; // Connects to the Q7 pin the 165
int clockPin        = 12; // Connects to the Clock pin the 165
/* End I/O */

/* Setup --> */
int enableSerialPrintout=1;
/* Define the number of slots that this backplane unit will support */
const int numBackplaneSlots=3;
/* set up the shift in and out registers */
/* Define the number of slots that this backplane unit will support */
#define NUMBER_OF_SHIFT_CHIPS   3
#define DATA_WIDTH   NUMBER_OF_SHIFT_CHIPS * 8
#define PULSE_WIDTH_USEC   5
#define POLL_DELAY_MSEC   1
#define BYTES_VAL_T unsigned long
/* Define the number of slots that this backplane unit will support */
BYTES_VAL_T pinValues;
BYTES_VAL_T oldPinValues;

/* I2C Address range use 8 to 119 */
int i2cAddress=11;

int buttondowncount[numBackplaneSlots];
int buttonupcount[numBackplaneSlots];
int powerstatus[numBackplaneSlots];
int checkOffCount[numBackplaneSlots];
int waitingOffTimeoutCount[numBackplaneSlots];


bool blinking[numBackplaneSlots];
bool fastBlinking[numBackplaneSlots];
bool checkPiIsOff[numBackplaneSlots];
int flash[numBackplaneSlots];
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

int buttonstate[numBackplaneSlots];
int buttonstatelast[numBackplaneSlots];
unsigned long downTime[numBackplaneSlots];
unsigned long upTime[numBackplaneSlots];
/* <-- Setup */

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

/* basic setup */
void setup() {
  if (enableSerialPrintout == 1) {
    Serial.begin(115200);
    Serial.println("Set Up");
    Serial.println("Intelligent Controller");
  }
  /* Set up shiftregsiter pins */
  pinMode(ploadPin, OUTPUT);
  pinMode(clockEnablePin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, INPUT);
  shiftout.setBitCount(DATA_WIDTH);
  shiftout.setPins(dataOutPin, clockOutPin, latchOutPin);
  //pinMode(latchOutPin, OUTPUT);
  //pinMode(clockOutPin, OUTPUT);
  //pinMode(dataOutPin, OUTPUT);
  //shiftout.begin(dataOutPin, clockOutPin, latchOutPin);
  /* when a backplane unit is reset, it should set everything to low */
  //shiftout.setAllLow();
  //shiftout.write();
  /* I2C --> */
  Wire.begin(i2cAddress);
  Wire.onRequest(requestEvents);
  Wire.onReceive(receiveEvents);
  /* <-- I2C */
  if (enableSerialPrintout == 1) {
    Serial.println("Set Up2");
  }

  /* set up inital button states */
  for (int i = 1; i <= numBackplaneSlots; i++) {
    buttondowncount[numBackplaneSlots] = 0;
    buttonupcount[numBackplaneSlots] = 0;
    powerstatus[numBackplaneSlots] = 0;
    checkOffCount[numBackplaneSlots] = 0;
    waitingOffTimeoutCount[numBackplaneSlots] = 0;
    blinking[numBackplaneSlots] = false;
    fastBlinking[numBackplaneSlots] = false;
    flash[numBackplaneSlots] = 0;
    buttonstate[numBackplaneSlots] = 0;
    buttonstatelast[numBackplaneSlots] = 0;
    checkPiIsOff[numBackplaneSlots] = false;
  }
  pinValues = read_shift_regs();
  display_pin_values();
  oldPinValues = pinValues;
  Serial.print("Values: ");
  Serial.println(pinValues);
}

int readShiftInPin(int slotNum, int pinNumber) {
  int pinOffsetVal = pinNumber;
  for(int i = 1; i <= numBackplaneSlots; i ++) {
    if (i == slotNum) {
      break;
    }
    pinOffsetVal=pinOffsetVal+8;
  }
  //Serial.print(pinOffsetVal); //Serial.print(slotNum); //return shiftin.state(pinOffsetVal); //Serial.println(get_pin_value(pinOffsetVal));
  return get_pin_value(pinOffsetVal);
}


void writeShiftOutPin(int slotNum, int pinNumber, int highOrLow) {
  int pinOffsetVal = pinNumber;
  for(int i = 0; i < numBackplaneSlots; i ++) {
    if (i == slotNum) {
      break;
    }
    pinOffsetVal=pinOffsetVal+8;
  }
  //Serial.print("Slot:");
  //Serial.print(slotNum);
  //Serial.print(pinNumber);
  //Serial.println(pinOffsetVal);
  if (highOrLow == HIGH) {
    //shiftout.setHigh(pinOffsetVal);
    shiftout.writeBit(pinOffsetVal, HIGH);
  } else {
    //shiftout.setLow(pinOffsetVal);
    shiftout.writeBit(pinOffsetVal, LOW);
  }
  //shiftout.write();
}

int getType(int slotNum) {
  //Serial.print("SlotNum: ");
  //Serial.println(slotNum);
  int pinOffsetVal = 0;
  for(int i = 1; i <= numBackplaneSlots; i ++) {
    if (i == slotNum) {
      break;
    }
    pinOffsetVal=pinOffsetVal+8;
  }
  int ret = 0;  /* Initialize the variable to return */
  int bitValue[] = {1,2,4,8};  /* bit position decimal equivalents */
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

/* I2C --> */
/*
 * request events
 */
void requestEvents()
{
  /* poll through input shift register data */
  for(int i = 1; i <= numBackplaneSlots; i ++) {
    i2cRespond(i);
  }
}

int invertLogic(int logic)
{
  if (logic == 1) {
    return 0;
  } else {
    return 1;
  }
}

/*
 * reply to send to master
 */
void i2cRespond(int slotNum)
{
  uint8_t respond[7];
  /* Slavenumber */
  respond[0]=i2cAddress;
  /* Power Status */
  int powState = readShiftInPin(slotNum, 7);
  respond[1]=invertLogic(powState);
  /* LED Status */
  respond[2]=invertLogic(powState);
  /* Pi Detection */
  int piState = readShiftInPin(slotNum, 6);
  respond[3]=invertLogic(piState);
  /* AlwaysOn */
  int alwaysOn = readShiftInPin(slotNum, 5);
  respond[4]=invertLogic(alwaysOn);
  /* CaddyType */
  //caddyType = parallelToByte();
  respond[5]=getType(slotNum);
  /* slot number on the backplane */
  respond[6]=slotNum;
  Wire.write(respond, 7);
  if (enableSerialPrintout == 1) {
    Serial.print("Respond:");
    Serial.println(slotNum);
    Serial.print("power: ");         // print the character
    Serial.print(invertLogic(powState));         // print the character
    Serial.print("pidetect: ");         // print the character
    Serial.print(invertLogic(piState));         // print the character
    Serial.print("always on: ");         // print the character
    Serial.print(invertLogic(alwaysOn));         // print the character
    Serial.print("button: ");         // print the character
    Serial.print(readShiftInPin(slotNum, 4));         // print the character
    Serial.print("type: ");         // print the character
    //Serial.println(readShiftInPin(slotNum, 5));         // print the character
    Serial.println(getType(slotNum));         // print the character
  }
}

/*
 * receive event to control state
 */
void receiveEvents(int howMany)
{
  Serial.println("Received Message");
  int c[2];
  for(int b=0; b<2; b++) {
    c[b]=0;
  }
  int i = 0;
  while (Wire.available()) { // loop through all but the last
    c[i] = Wire.read(); // receive byte as a character
    if (enableSerialPrintout == 1) {
      Serial.print(c[i]);         // print the character
    }
    i=i+1;
  }
  /* Power Toggle Mode
   * 0 Power off
   * 1 Powering On (not used for now)
   * 2 Powered on
   * 3 Powering off
   * 4 Waiting for Safe Shutdown to finish
   * 5 Hard Shutdown
   * 9 Update Master with data
   */
  int slotNum;
  slotNum = c[1]-1;
  powerstatus[slotNum] = c[0];
}

/*
 * update master
 */
void sendToMaster(int slotNum)
{
  /* We send to 1 to send to the master */
  /* begin transmission to master */
  Wire.beginTransmission(1);
  /* Run the sequence to send to the master */
  i2cRespond(slotNum);
  /* end transmission */
  Wire.endTransmission();
}
/* <-- I2C */

/*
 * Main loop
 */
void loop()
{
  /* Check if any pins have updated on the shiftregisters */
  pinValues = read_shift_regs();
  if (pinValues != oldPinValues) {
    /* Update master */
    int count=0;
    int slotnum=0;
    int changed[3];
    for(int b=0; b<3; b++) {
      changed[b]=0;
    }
    for(int i = 0; i < DATA_WIDTH; i++)
    {
      int test1 = (pinValues >> i) & 1;
      int test2 = (oldPinValues >> i) & 1;
      if(test1 != test2) {
        changed[slotnum] = 1;
      }
      if (count == 7) { //we have 8 pins in the shift register per slot, if we hit count 7 that is the last pin for that slot, do the function
        if (changed[slotnum] == 1) {
          int sendslot = slotnum+1; //we can't use 0, bump it by 1 to send to master
          sendToMaster(sendslot);
        }
        count = 0; // reset
        changed[slotnum] = 0; // reset
        slotnum++; // increment slotnum for next slot
      }
      count++;
    }
    oldPinValues = pinValues; // revert
  }
  if(powerstatus[0]==9) {
    /* Polled by master */
    if (enableSerialPrintout == 1) {
      Serial.println("Polled by Master");
    }
    /* Update master */
    for(int i = 1; i <= numBackplaneSlots; i ++) {
      sendToMaster(i);
    }
    powerstatus[0] = 0;
  }
  /* do button check stuff here, physical pin 5 is pinoffset value of 4 on the shift register*/
  int pinOffsetVal = 4;
  for(int i = 1; i <= numBackplaneSlots; i ++) {
    int slotNum = i-1;
    int reading = readShiftInPin(i, 4);
    buttonstate[slotNum] = reading;
    int slotAlwaysOn = invertLogic(readShiftInPin(i, 5));
    if (getType(i) == 0) {
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
        Serial.println("down");
      } else if (buttonstate[slotNum] == 1 && buttonstatelast[slotNum] == 0 && (millis() - downTime[slotNum]) > longPress) {
        upTime[slotNum] = millis();
        Serial.println("up longpress");
        powerstatus[slotNum] = 5;
      } else if (buttonstate[slotNum] == 0 && (millis() - downTime[slotNum]) > longPress) {
        fastBlinking[slotNum] = true;
      } else if (buttonstate[slotNum] == 1 && buttonstatelast[slotNum] == 0 && (millis() - downTime[slotNum]) > debounce) {
        upTime[slotNum] = millis();
        Serial.println("up");
        Serial.println(readShiftInPin(slotNum+1, 7));
        if (readShiftInPin(slotNum+1, 7) == 1) {
          Serial.println("power is off");
          powerstatus[slotNum] = 2;
        } else {
          Serial.println("power is on");
          powerstatus[slotNum] = 3;
        }
      }
      buttonstatelast[slotNum] = buttonstate[slotNum];
      /* end button state check */
      /* If powerstatus is 2 (ON) and the MOSFET is not already HIGH then power on once only */
      if(powerstatus[slotNum]==2 && readShiftInPin(slotNum+1, 7) == 1) {
        /* Initiate shutdown on raspberry pi GPIO */
        //digitalWrite(SHUTDOWN_PIN, LOW);
        writeShiftOutPin(slotNum, 2, LOW);
        /* Turn power on */
        if (getType(i) != 0 ) { // if the caddy is empty, don't bother turning it on
          powerOn(slotNum);
        } else {
          powerstatus[slotNum]=0;
        }
        /* Update master */
        sendToMaster(slotNum+1);
      } else if(powerstatus[slotNum]==3) {
        if (enableSerialPrintout == 1) {
          if (enableSerialPrintout == 1) {
            Serial.println("Send shutdown signal");
          }
          /* Start Shutdown Sequence */
          writeShiftOutPin(slotNum, 2, HIGH);
          delay(800);
          writeShiftOutPin(slotNum, 2, LOW);
          if (enableSerialPrintout == 1) {
            Serial.println("Start shutdown sequence");
          }
          powerstatus[slotNum]=4;
          checkOffCount[slotNum] = 0;
          waitingOffTimeoutCount[slotNum] = 0;
        }
      } else if(powerstatus[slotNum]==4) {
        blinking[slotNum] = true;
        checkPiIsOff[slotNum] = true;

        /* Check if it's off 5 times before shutting down */
        if(checkOffCount[slotNum]>=5) {
          if (enableSerialPrintout == 1) {
            Serial.println("Power Off");
          }
          blinking[slotNum] = false;
          fastBlinking[slotNum] = false;
          checkPiIsOff[slotNum] = false;
          /* It's really off */
          powerOff(slotNum);
          /* Update master */
          sendToMaster(slotNum+1);
          powerstatus[slotNum] = 0;
          checkOffCount[slotNum] = 0;
          waitingOffTimeoutCount[slotNum] = 0;
        } else if(waitingOffTimeoutCount[slotNum]>=40) {
          if (enableSerialPrintout == 1) {
            Serial.println("Timed out, powering off");
          }
          blinking[slotNum] = false;
          fastBlinking[slotNum] = false;
          checkPiIsOff[slotNum] = false;
          /* Time out shutdown anyway */
          powerOff(slotNum);
          /* Update master */
          sendToMaster(slotNum+1);
          powerstatus[slotNum] = 0;
          checkOffCount[slotNum] = 0;
          waitingOffTimeoutCount[slotNum] = 0;
        }
      } else if(powerstatus[slotNum]==5) {
        /* Hard shutdown */
        if (enableSerialPrintout == 1) {
          Serial.println("Hard Shutdown");
        }
        blinking[slotNum] = false;
        fastBlinking[slotNum] = false;
        checkPiIsOff[slotNum] = false;
        /* Turn power off */
        powerOff(slotNum);
        /* Update master */
        sendToMaster(slotNum+1);
        powerstatus[slotNum] = 0;
        checkOffCount[slotNum] = 0;
      }/* end powerstatus check */
    /* end always on check */
    } else {
      /* if always on is enabled, and the slot is not powered on, power it on */
      if (readShiftInPin(slotNum+1, 7) == 1) {
        if (enableSerialPrintout == 1) {
            Serial.println(slotNum+1);
        }
        powerOn(slotNum);
      }
    }
  }
  /* set a short delay */
  delay(50);
}

/* start check functions */
void blinkCheck(int slotNum) {
  if (blinking[slotNum]) {
    currentMillis[slotNum] = millis();
    if ((unsigned long)(currentMillis[slotNum] - previousMillis[slotNum]) >= blinkInterval) { // enough time passed yet?
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
    if ((unsigned long)(currentMillisFast[slotNum] - previousMillisFast[slotNum]) >= fastBlinkInterval) { // enough time passed yet?
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
    if ((unsigned long)(currentMillisPiCheck[slotNum] - previousMillisPiCheck[slotNum]) >= piCheckInterval) { // enough time passed yet?
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
   int sensorValue = readShiftInPin(slotNum+1, 6) ; //digitalRead(PIDETECTION);
   //if (enableSerialPrintout == 1) {
   //  Serial.println("Detection state: "+String(sensorValue));
   //}
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
  Serial.println("PowerOff");
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
  //Serial.println("PowerOn");
}

int pinOffsetNumber(int slotNum, int pinNum) {
  int pinOffsetVal = pinNum;
  for(int i = 1; i <= numBackplaneSlots; i ++) {
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
