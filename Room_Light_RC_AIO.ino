/*
  Room Light RC AIO
  Copyright (c) 2016 Subhajit Das

  Licence Disclaimer:

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <DTMFDecoder.h>
#include <IRremote.h>        // necessary for remote data decoding
#include <USB_MP3_Player_Remote.h>  // remote controller map
#include <RemoteHelper.h>

#define BAUD_RATE 115200
/*
   ::Variables::
  OUTPin[] and onStatus[] used together to define their respective value for a single load.
  They represent different properties of same output. Their index number must be same when accessing them.
  OUTPin[] contains the output pin numbers on the microcontroller / arduino.
  onStatus[] represents their on status, true(1) for on and false(0) for off. Initially 0, as the all off.

   ::Functions::
  void setup(), does initial setup (compulsory).
  void loop(), loop through iterations.
  void setOutputs(), sets outputs HIGH or LOW depending on onStatus[].
  boolean getOUTAny(), returns true if any output is on.
  void checkRemote(long remoteValue), checks for remote inputs, takes action on onStatus[] accordingly.

    When user presses a button on the remote, the data is received by IR receiver.
    Then it is decoded by decoder library. Based on received data, output is switched.
    For 1-6 output is directly switched.
    For power button, current status stored and if any load is on they turned off, or stored value is mimicked in output.
*/

/*
    Used Pins:
    2,: 3, 4, 5, 6, 7, 8,: 9,: 13,: A0, A1, A2, A3
*/

// function decareartions
void checkDTMF();
void checkWifi();
void checkRemote();
boolean switchOutputs();
void setOutputs();
boolean getOUTAny();
void DTMFRecvISR();
void printStatus();

const byte OUTPin[] = {  // output OUT pin no
  3, 4, 5, 6, 7, 8
};

const byte noOfOutputs = sizeof(OUTPin);  // number of OUTPUTs
boolean onStatus[noOfOutputs];  // states if OUT is ON (1) or OFF (0)
boolean onStatusTemp[noOfOutputs];  // temporary onStatus

const byte INDICATOR_PIN = 13; // indicator blinks when remote data recieves

const byte RECV_PIN = 9;  // recive pin for IR

IRrecv irrecv(RECV_PIN);  // calling constructor of IRrecv
decode_results results;  // decoded input results stored in results object
DTMFDecoder DTMF(A0, A1, A2, A3); // DTMF decoder
const byte DTMFRcvIntPin = 2;

// flags to keep changes in mind
boolean stateChangedIR = false;
boolean stateChangedWifi = false;
boolean stateChangedDTMF = false;

volatile boolean DTMFReceived = false;

long IndicationEndTime = 0;

void setup()
{
  byte i;
  // serial connection from this controller to external modulde
  Serial.begin(BAUD_RATE);
  // defining all output pins as OUTPUT
  for (i = 0; i < noOfOutputs; i++) {
    pinMode(OUTPin[i], OUTPUT);
  }
  pinMode(INDICATOR_PIN, OUTPUT);
  irrecv.enableIRIn(); // Start the receiver
  // setting all OUTPUTs to off initially
  for (i = 0; i < noOfOutputs; i++) {
    onStatus[i] = false;
  }
  attachInterrupt(digitalPinToInterrupt(DTMFRcvIntPin), DTMFRecvISR, RISING);
}

void loop() {
  // turn indicator off after IndicationEndTime is crossed
  if (millis() >= IndicationEndTime) {
    digitalWrite(INDICATOR_PIN, LOW);
  }

  // checks for Remote control inputs
  checkRemote();
  // check for DTMF input
  checkDTMF();
  // check for Wifi input
  checkWifi();
  if (stateChangedIR || stateChangedDTMF || stateChangedWifi) {
    // switches the couputs accordingly
    setOutputs();
  }
  stateChangedIR = false;
  stateChangedWifi = false;
  stateChangedDTMF = false;
}

// checks remote data and sets onStatus
void checkRemote() {
  if (irrecv.decode(&results)) {
    //Serial.println("IR");
    long recv = results.value;
    irrecv.resume(); // Receive the next value
    byte key = (byte)Remote.getNumber(recv);
    stateChangedIR = switchOutputs(key);
  }
}

void checkWifi() {
  byte key;
  if (Serial.available()) {
    key = Serial.read();
    if (key == 'P' || (key >= '1' && key <= '9')) {
      //Serial.println(key);
      key -= 48; // taking charecters from serial (WIFI Module) convert to int
      stateChangedWifi = switchOutputs(key);
    } else if (key == 'S') { // 'S' means asking for status
      printStatus();
    }
  }
}

void checkDTMF() {
  byte key;
  if (DTMFReceived) {
    key = DTMF.getKey() - 48;
    stateChangedDTMF = switchOutputs(key);
    DTMFReceived = false;
  }
}

boolean switchOutputs(byte key) {
  byte i;
  boolean outChanged = true;
  switch (key) {
    case 1 :
    case 2 :
    case 3 :
    case 4 :
    case 5 :
    case 6 :
      onStatus[key - 1] = !onStatus[key - 1];
      break;
    case 10:
    case ('P'-48):
      if (getOUTAny()) {
        // setting all OUT pins to LOW
        for (i = 0; i < noOfOutputs; i++) {  // storing current onStatus to temp location
          onStatusTemp[i] = onStatus[i];
          onStatus[i] = false;
        }
      }
      else { //setting all OUT pins to HIGH
        for (i = 0; i < noOfOutputs; i++) {
          // retriving stored onStatus
          onStatus[i] = onStatusTemp[i];
        }
      }
      break;
    default:
      outChanged = false;
  }
  return outChanged;
}

// setting all OUT pins according to their respective onStatus
void setOutputs() {
  byte i;

  for (i = 0; i < noOfOutputs; i++) {
    digitalWrite(OUTPin[i], onStatus[i]);
  }
  printStatus();
}

void printStatus() {
  // blinking indicator
  digitalWrite(INDICATOR_PIN, HIGH);
  IndicationEndTime = millis() + 300;

  String OUTStatusStr = String(getOUTAny());
  for (byte i = 0; i < noOfOutputs; i++) {
    OUTStatusStr += String(onStatus[i]);
  }
  // sereialize status and send to wifi module
  Serial.println(OUTStatusStr);
}

// returns true if any output is on
boolean getOUTAny() {
  byte i;
  for (i = 0; i < noOfOutputs; i++) {
    if (onStatus[i] == true) {
      return true;
    }
  }
  return false;
}

void DTMFRecvISR() {
  DTMFReceived = true;
}

