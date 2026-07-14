/*
Written by Benjamin Jack Cullen.

GPIO Port Expander - IIC I/O device.

  - Sends pin reads to master.

  - Sets pins from master.

  - PWM.

  For perfomance reasons it is recommnended for single use as either input
  or output device but can be used as both.
*/

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <Wire.h>
#include "UnidentifiedStudios_I2C.h"
#include "UnidentifiedStudios_GPIOPortExpander.h"

// ------------------------------------------------------------------------------------------------------------------
//                                                                                                              SETUP
// ------------------------------------------------------------------------------------------------------------------

void setup() {
  // ------------------------------------------------------------
  // ADC prescaler
  // ------------------------------------------------------------
  // Arduino core default is /128 (125kHz ADC clock, ~104us/conversion) -
  // the datasheet's recommended range for full 10-bit accuracy. /16
  // (1MHz ADC clock) cuts that to ~13us/conversion at the cost of some
  // resolution/noise margin - validate readings against known references
  // if analog precision matters for this build.
  ADCSRA = (ADCSRA & ~0x07) | 0x04; // ADPS2:0 = 100 -> /16
  // ------------------------------------------------------------
  // Serial
  // ------------------------------------------------------------
  Serial.setTimeout(50); // ensure this is set before begin()
  Serial.begin(115200);  while(!Serial);

  
  setAllPinMode();

  // ------------------------------------------------------------
  // I2C
  // ------------------------------------------------------------
  GPIOPortExpander_SLAVE.wire.begin(GPIOPortExpander_SLAVE.address);
  Serial.println("[IIC] Starting IIC as slave address: " + String(GPIOPortExpander_SLAVE.address));
  // ------------------------------------------------------------
  // Function to run when data requested from master
  // ------------------------------------------------------------
  GPIOPortExpander_SLAVE.wire.onRequest(requestEventBus0Bin);
  // ------------------------------------------------------------
  // Function to run when data received from master
  // ------------------------------------------------------------
  GPIOPortExpander_SLAVE.wire.onReceive(receiveEventBus0Bin);

  Serial.println("[READY] (waiting for initialization)");
}

// ------------------------------------------------------------------------------------------------------------------
//                                                                                                          MAIN LOOP
// ------------------------------------------------------------------------------------------------------------------

void loop() {
  #ifdef GPIOPE_WRITE_MODE
  GPIOPE_Output_Modulator(); // for output: uncomment if required
  #endif
}