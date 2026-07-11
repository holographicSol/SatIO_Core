/**
 * GPIO Port Expander - Written by benjamin Jack Cullen.
 */

#include <string.h>
#include "UnidentifiedStudios_I2C.h"
#include "UnidentifiedStudios_GPIOPortExpander.h"

// ------------------------------------------------------------
/**
 * @brief Default Instance: ATMEGA2560 Slave GPIOPE.
 *        
 */
#ifdef GPIOPE_SLAVE_ATMEGA2560
GPIOPortExpander GPIOPortExpander_SLAVE = {
  "GPIOPortExpander_ATMEGA2560", // name (positional: avr-g++ 7.3's designated-initializer support
                                 // can't handle a string literal assigned to a char[] via ".name = ...")
  .wire              = iic_0,
  .i2cLink           = I2CLinkBus0,
  .address           = 9,
  .current_pin       = 0,
  .pin_min           = -1,
  .pin_max           = 69,
  .max_pins          = 70,
  .num_analog_pins   = 16,
  .num_digital_pins  = 54,
  .max_input_values  = GPIOPE_MAX_SIZE,
  .max_output_values = GPIOPE_MAX_SIZE,
  .query_cursor      = 0,

  .analog_pins       = {54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69},
  .digital_pins      = {
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, /* 0-9 */
      10,11,12,13,14,15,16,17,18,19, /* 10-19 */
      20,21,22,23,24,25,26,27,28,29, /* 20-29 */
      30,31,32,33,34,35,36,37,38,39, /* 30-39 */
      40,41,42,43,44,45,46,47,48,49, /* 40-49 */
      50,51,52,53 /* 50-53 */
  },
  .modulation_time   = {},
  .input_value       = {},
  .output_value      = {},
  .port_map          = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
  .switch_state      = {
      false, false, false, false, false, false, false, false, false, false, /* 0-9 */
      false, false, false, false, false, false, false, false, false, false, /* 10-19 */
      false, false, false, false, false, false, false, false, false, false, /* 20-29 */
      false, false, false, false, false, false, false, false, false, false, /* 30-39 */
      false, false, false, false, false, false, false, false, false, false, /* 40-49 */
      false, false, false, false, false, false, false, false, false, false, /* 50-59 */
      false, false, false, false, false, false, false, false, false, false  /* 60-69 */
  },
  .enabled           = {
      true, true, true, true, true, true, true, true, true, true, /* 0-9 */
      true, true, true, true, true, true, true, true, true, true, /* 10-19 */
      true, true, true, true, true, true, true, true, true, true, /* 20-29 */
      true, true, true, true, true, true, true, true, true, true, /* 30-39 */
      true, true, true, true, true, true, true, true, true, true, /* 40-49 */
      true, true, true, true, true, true, true, true, true, true, /* 50-59 */
      true, true, true, true, true, true, true, true, true, true  /* 60-69 */
  },
  .chan_freq_uS      = {
      1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, /* 0-9 */
      1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, /* 10-19 */
      1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, /* 20-29 */
      1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, /* 30-39 */
      1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, /* 40-49 */
      1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, /* 50-59 */
      1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000  /* 60-69 */
  }
};
#endif

#ifdef GPIOPE_SLAVE_MODE

constexpr int8_t PIN_KIND_UNKNOWN = 0;
constexpr int8_t PIN_KIND_ANALOG  = 1;
constexpr int8_t PIN_KIND_DIGITAL = 2;
static bool pin_kind_lookup_built = false;
static int8_t pin_kind_lookup[GPIOPE_MAX_SLAVE_PINS];

// --------------------------------------------------------------------
// O(1) pin-kind lookup, keyed directly by physical pin number - built once
// from analog_pins/digital_pins so readPin() doesn't re-scan up to 70
// entries (isAnalogPin + isDigitalPin) on every single I2C request.
// --------------------------------------------------------------------
inline void buildPinKindLookup() {
  memset(pin_kind_lookup, PIN_KIND_UNKNOWN, sizeof(pin_kind_lookup));
  for (int i=0; i<GPIOPortExpander_SLAVE.num_analog_pins; i++) {
    uint8_t pin = (uint8_t)GPIOPortExpander_SLAVE.analog_pins[i];
    if (pin < GPIOPortExpander_SLAVE.max_pins) {pin_kind_lookup[pin] = PIN_KIND_ANALOG;}
  }
  for (int i=0; i<GPIOPortExpander_SLAVE.num_digital_pins; i++) {
    uint8_t pin = (uint8_t)GPIOPortExpander_SLAVE.digital_pins[i];
    if (pin < GPIOPortExpander_SLAVE.max_pins) {pin_kind_lookup[pin] = PIN_KIND_DIGITAL;}
  }
  pin_kind_lookup_built = true;
}

inline void readPortmapPin(int8_t idx) {
  if (!pin_kind_lookup_built) {buildPinKindLookup();}
  int mapped_pin_r = GPIOPortExpander_SLAVE.port_map[idx];
  if (mapped_pin_r < 0 || mapped_pin_r >= GPIOPortExpander_SLAVE.max_pins) {return;}
  switch (pin_kind_lookup[mapped_pin_r]) {
    case PIN_KIND_ANALOG:
      GPIOPortExpander_SLAVE.input_value[GPIOPortExpander_SLAVE.current_pin] = analogRead((uint8_t)mapped_pin_r);
      break;
    case PIN_KIND_DIGITAL:
      GPIOPortExpander_SLAVE.input_value[GPIOPortExpander_SLAVE.current_pin] = digitalRead((uint8_t)mapped_pin_r);
      break;
  }
}

inline void writedPortmapPin(int8_t idx) {
  if (!pin_kind_lookup_built) {buildPinKindLookup();}
  int mapped_pin_w = GPIOPortExpander_SLAVE.port_map[idx];
  if (mapped_pin_w < 0 || mapped_pin_w >= GPIOPortExpander_SLAVE.max_pins) {return;}
  switch (pin_kind_lookup[mapped_pin_w]) {
    case PIN_KIND_ANALOG:
      analogWrite((uint8_t)mapped_pin_w, (uint8_t)GPIOPortExpander_SLAVE.output_value[idx]);
      break;
    case PIN_KIND_DIGITAL:
      digitalWrite((uint8_t)mapped_pin_w, (uint8_t)GPIOPortExpander_SLAVE.output_value[idx]);
      break;
  }
}

inline uint8_t readPin(int8_t pin) {
  uint8_t value = 0;
  int mapped_pin_r = pin;
  if (mapped_pin_r < 0 || mapped_pin_r >= GPIOPortExpander_SLAVE.max_pins) {return;}
  switch (pin_kind_lookup[mapped_pin_r]) {
    case PIN_KIND_ANALOG:
      value = analogRead((uint8_t)mapped_pin_r);
      break;
    case PIN_KIND_DIGITAL:
      value = digitalRead((uint8_t)mapped_pin_r);
      break;
  }
  return value;
}

// --------------------------------------------------------------------
// modulation only walks this list. modulated_pin_pos
// gives O(1) swap-remove (position -1 = not in the list).
// --------------------------------------------------------------------
static int8_t modulated_pin_list[GPIOPE_MAX_SLAVE_PINS];
static int8_t modulated_pin_pos[GPIOPE_MAX_SLAVE_PINS];
static int8_t modulated_pin_count = 0;
static bool modulated_pin_pos_init = false;

inline void ensureModulatedPinPosInit() {
  if (!modulated_pin_pos_init) {
    memset(modulated_pin_pos, -1, sizeof(modulated_pin_pos)); // -1 (0xFF) per int8_t slot = "not active"
    modulated_pin_pos_init = true;
  }
}

void activateModulatedPin(int8_t idx) {
  if (idx < 0 || idx >= GPIOPortExpander_SLAVE.max_pins || idx >= GPIOPortExpander_SLAVE.max_pins) {return;}
  ensureModulatedPinPosInit();
  if (modulated_pin_pos[idx] != -1) {return;} // already active
  modulated_pin_pos[idx] = modulated_pin_count;
  modulated_pin_list[modulated_pin_count] = idx;
  modulated_pin_count++;
  // Start the cycle clean: switch_state/last-toggle time may be stale from
  // this pin's previous activation (e.g. a manual off never touches them),
  // which would make modulator() think it's already mid-cycle and needs to
  // turn off instead of on. Force "currently low, off-timer elapsed" so it
  // turns on right away.
  GPIOPortExpander_SLAVE.switch_state[idx] = false;
  GPIOPortExpander_SLAVE.modulation_time[idx][2] = 0;
}

void deactivateModulatedPin(int8_t idx) {
  if (idx < 0 || idx >= GPIOPortExpander_SLAVE.max_pins || idx >= GPIOPortExpander_SLAVE.max_pins) {return;}
  ensureModulatedPinPosInit();
  int8_t pos = modulated_pin_pos[idx];
  if (pos == -1) return; // not active
  int8_t last = --modulated_pin_count;
  int8_t last_idx = modulated_pin_list[last];
  modulated_pin_list[pos] = last_idx;
  modulated_pin_pos[last_idx] = pos;
  modulated_pin_pos[idx] = -1;
}

void resetModulatedPinList() {
  ensureModulatedPinPosInit();
  for (int8_t i = 0; i < modulated_pin_count; i++) {
    modulated_pin_pos[modulated_pin_list[i]] = -1;
  }
  modulated_pin_count = 0;
}

const int8_t *modulatedPinIndices(int8_t *out_count) {
  ensureModulatedPinPosInit();
  *out_count = modulated_pin_count;
  return modulated_pin_list;
}

inline void setAllPinMode() {
  #ifdef GPIOPE_READ_MODE
  for (int i=0; i < GPIOPortExpander_SLAVE.num_analog_pins; i++) {
    pinMode(GPIOPortExpander_SLAVE.port_map[i], INPUT);
  }
  for (int i=0; i < GPIOPortExpander_SLAVE.num_digital_pins; i++) {
    pinMode(GPIOPortExpander_SLAVE.port_map[i], INPUT);
  }
  #endif

  #ifdef GPIOPE_WRITE_MODE
  for (int i=0; i < GPIOPortExpander_SLAVE.num_analog_pins; i++) {
    pinMode(GPIOPortExpander_SLAVE.port_map[i], OUTPUT);
  }
  for (int i=0; i < GPIOPortExpander_SLAVE.num_digital_pins; i++) {
    pinMode(GPIOPortExpander_SLAVE.port_map[i], OUTPUT);
  }
  #endif
}

void GPIOPE_Output_Modulator() {
  // ------------------------------------------------------------
  // Logic modulator
  // - Modulate output only if a switch state is already true.
  // - Modulator values: time high, time low.
  // ------------------------------------------------------------
  const unsigned long now = micros();
  int8_t count;
  const int8_t *active = modulatedPinIndices(&count);
  // Iterate backward: deactivateModulatedPin() below swap-removes the
  // *current* index, moving the list's tail into this slot. Walking high-to-low
  // guarantees that tail element (always at a position we've already visited,
  // or this same one) is never revisited - a forward loop would skip it.
  for (int8_t k=count-1; k>=0; k--) {
    const int8_t i = active[k];
    const int16_t pin = GPIOPortExpander_SLAVE.port_map[i];
    const int32_t out_val = GPIOPortExpander_SLAVE.output_value[i];
    // cache this pin's row instead of re-indexing modulation_time[i]
    uint32_t *mt = GPIOPortExpander_SLAVE.modulation_time[i];
    // Serial.println("active_i=" + String(i) + "  pin=" + String(pin) + "  val=" + String(out_val));
    // ------------------------------------------------------
    // handle currently low
    // ------------------------------------------------------
    if (GPIOPortExpander_SLAVE.switch_state[i]==false) {
      // ----------------------------------
      // modulate on
      // ----------------------------------
      if ((now - mt[2]) >= mt[0]) {
        // Serial.println("on");
        if (pin<54) {digitalWrite(pin, HIGH);}
        else {analogWrite(pin, out_val);}
        mt[2]=now;
        GPIOPortExpander_SLAVE.switch_state[i]=true;
      }
    }
    // -------------------------------------------------------
    // handle currently high
    // -------------------------------------------------------
    else {
      // ----------------------------------
      // remain off
      // ----------------------------------
      if (mt[1]==0) {
        if ((now - mt[2]) >= mt[0]) {
          // Serial.println("rem off");
          if (pin<54) {digitalWrite(pin, LOW);}
          else {analogWrite(pin, 0);}
          mt[2]=now;
          GPIOPortExpander_SLAVE.switch_state[i]=false;
          // change parent state off
          GPIOPortExpander_SLAVE.output_value[i]=0;
          // pin is done modulating for good (output_value<=0 now) - stop
          // scanning it. see the backward-iteration note above.
          deactivateModulatedPin(i);
        }
      }
      // ----------------------------------
      // modulate off
      // ----------------------------------
      else {
        if ((now - mt[2]) >= mt[1]) {
          // Serial.println("off");
          if (pin<54) {digitalWrite(pin, LOW);}
          else {analogWrite(pin, 0);}
          mt[2]=now;
          GPIOPortExpander_SLAVE.switch_state[i]=false;
        }
      }
    }
  }
}

void requestEventBus0Bin() {

  #ifdef GPIO_GPIOE_DEBUG_1
  Serial.println("[requestEventBus0Bin] " + String(GPIOPortExpander_SLAVE.i2cLink.REQUEST_ID));
  #endif

  switch (GPIOPortExpander_SLAVE.i2cLink.REQUEST_ID) {

    case GPIOPE_CMD_GET_INFO: {
        // #ifdef GPIO_GPIOE_DEBUG_2
        // Serial.println(
        //   "[GPIOPE_CMD_GET_INFO]"
        //   "  pin_min=" + String((int8_t)GPIOPortExpander_SLAVE.pin_min) +
        //   "  pin_max=" + String((int8_t)GPIOPortExpander_SLAVE.pin_max) +
        //   "  max_pins=" + String((int8_t)GPIOPortExpander_SLAVE.max_pins) +
        //   "  num_analog_pins=" + String((int8_t)GPIOPortExpander_SLAVE.num_analog_pins) +
        //   "  num_digital_pins=" + String((int8_t)GPIOPortExpander_SLAVE.num_digital_pins) +
        //   "  max_input_values=" + String((int32_t)GPIOPortExpander_SLAVE.max_input_values) +
        //   "  max_output_values=" + String((int32_t)GPIOPortExpander_SLAVE.max_output_values)
        // );
        // #endif
        clearI2CLinkOutputPacket(GPIOPortExpander_SLAVE.i2cLink);
        write_int8_ToPacket(GPIOPortExpander_SLAVE.i2cLink.OUTPUT_PACKET, GPIOPortExpander_SLAVE.i2cLink.current_bytes, (int8_t)GPIOPortExpander_SLAVE.pin_min);
        write_int8_ToPacket(GPIOPortExpander_SLAVE.i2cLink.OUTPUT_PACKET, GPIOPortExpander_SLAVE.i2cLink.current_bytes, (int8_t)GPIOPortExpander_SLAVE.pin_max);
        write_int8_ToPacket(GPIOPortExpander_SLAVE.i2cLink.OUTPUT_PACKET, GPIOPortExpander_SLAVE.i2cLink.current_bytes, (int8_t)GPIOPortExpander_SLAVE.max_pins);
        write_int8_ToPacket(GPIOPortExpander_SLAVE.i2cLink.OUTPUT_PACKET, GPIOPortExpander_SLAVE.i2cLink.current_bytes, (int8_t)GPIOPortExpander_SLAVE.num_analog_pins);
        write_int8_ToPacket(GPIOPortExpander_SLAVE.i2cLink.OUTPUT_PACKET, GPIOPortExpander_SLAVE.i2cLink.current_bytes, (int8_t)GPIOPortExpander_SLAVE.num_digital_pins);
        write_int32_ToPacket(GPIOPortExpander_SLAVE.i2cLink.OUTPUT_PACKET, GPIOPortExpander_SLAVE.i2cLink.current_bytes, (int32_t)GPIOPortExpander_SLAVE.max_input_values);
        write_int32_ToPacket(GPIOPortExpander_SLAVE.i2cLink.OUTPUT_PACKET, GPIOPortExpander_SLAVE.i2cLink.current_bytes, (int32_t)GPIOPortExpander_SLAVE.max_output_values);
        writeI2CToMasterBin(Wire, GPIOPortExpander_SLAVE.i2cLink, GPIOPortExpander_SLAVE.i2cLink.current_bytes, 0);
        break;
      }

    case GPIOPE_CMD_GET_PINS: {
        clearI2CLinkOutputPacket(GPIOPortExpander_SLAVE.i2cLink);
        if (GPIOPortExpander_SLAVE.query_cursor < GPIOPortExpander_SLAVE.num_analog_pins) {
          // #ifdef GPIO_GPIOE_DEBUG_2
          // Serial.println("get analog pin=" + String(GPIOPortExpander_SLAVE.analog_pins[GPIOPortExpander_SLAVE.query_cursor]));
          // #endif
          write_uint8_ToPacket(GPIOPortExpander_SLAVE.i2cLink.OUTPUT_PACKET, GPIOPortExpander_SLAVE.i2cLink.current_bytes, 1);
          write_int8_ToPacket(GPIOPortExpander_SLAVE.i2cLink.OUTPUT_PACKET, GPIOPortExpander_SLAVE.i2cLink.current_bytes, GPIOPortExpander_SLAVE.analog_pins[GPIOPortExpander_SLAVE.query_cursor]);
        } else {
          // #ifdef GPIO_GPIOE_DEBUG_2
          // Serial.println("get digital pin=" + String(GPIOPortExpander_SLAVE.digital_pins[GPIOPortExpander_SLAVE.query_cursor - GPIOPortExpander_SLAVE.num_analog_pins]));
          // #endif
          write_uint8_ToPacket(GPIOPortExpander_SLAVE.i2cLink.OUTPUT_PACKET, GPIOPortExpander_SLAVE.i2cLink.current_bytes, 0);
          write_int8_ToPacket(GPIOPortExpander_SLAVE.i2cLink.OUTPUT_PACKET, GPIOPortExpander_SLAVE.i2cLink.current_bytes, GPIOPortExpander_SLAVE.digital_pins[GPIOPortExpander_SLAVE.query_cursor - GPIOPortExpander_SLAVE.num_analog_pins]);
        }
        // #ifdef GPIO_GPIOE_DEBUG_2
        // Serial.println("get portmap pin=" + String(GPIOPortExpander_SLAVE.digital_pins[GPIOPortExpander_SLAVE.query_cursor - GPIOPortExpander_SLAVE.num_analog_pins]));
        // #endif
        write_int8_ToPacket(GPIOPortExpander_SLAVE.i2cLink.OUTPUT_PACKET, GPIOPortExpander_SLAVE.i2cLink.current_bytes, GPIOPortExpander_SLAVE.port_map[GPIOPortExpander_SLAVE.query_cursor]);
        writeI2CToMasterBin(Wire, GPIOPortExpander_SLAVE.i2cLink, GPIOPortExpander_SLAVE.i2cLink.current_bytes, 0);
        if (++GPIOPortExpander_SLAVE.query_cursor >= GPIOPortExpander_SLAVE.num_analog_pins + GPIOPortExpander_SLAVE.num_digital_pins) {GPIOPortExpander_SLAVE.query_cursor = 0;}
        break;
      }

    case GPIOPE_CMD_GET_PWM: {
        clearI2CLinkOutputPacket(GPIOPortExpander_SLAVE.i2cLink);
        write_uint32_ToPacket(GPIOPortExpander_SLAVE.i2cLink.OUTPUT_PACKET, GPIOPortExpander_SLAVE.i2cLink.current_bytes, (uint32_t)GPIOPortExpander_SLAVE.modulation_time[GPIOPortExpander_SLAVE.query_cursor][0]);
        write_uint32_ToPacket(GPIOPortExpander_SLAVE.i2cLink.OUTPUT_PACKET, GPIOPortExpander_SLAVE.i2cLink.current_bytes, (uint32_t)GPIOPortExpander_SLAVE.modulation_time[GPIOPortExpander_SLAVE.query_cursor][1]);
        writeI2CToMasterBin(Wire, GPIOPortExpander_SLAVE.i2cLink, GPIOPortExpander_SLAVE.i2cLink.current_bytes, 0);
        if (++GPIOPortExpander_SLAVE.query_cursor >= GPIOPortExpander_SLAVE.max_pins) {GPIOPortExpander_SLAVE.query_cursor = 0;}
        break;
      }

    case GPIOPE_CMD_READ_PIN: {
      uint8_t value = readPin(GPIOPortExpander_SLAVE.current_pin);
      clearI2CLinkOutputPacket(GPIOPortExpander_SLAVE.i2cLink);
      write_int8_ToPacket(GPIOPortExpander_SLAVE.i2cLink.OUTPUT_PACKET, GPIOPortExpander_SLAVE.i2cLink.current_bytes, GPIOPortExpander_SLAVE.current_pin);
      write_uint8_ToPacket(GPIOPortExpander_SLAVE.i2cLink.OUTPUT_PACKET, GPIOPortExpander_SLAVE.i2cLink.current_bytes, value);
      writeI2CToMasterBin(Wire, GPIOPortExpander_SLAVE.i2cLink, GPIOPortExpander_SLAVE.i2cLink.current_bytes, 0);
      break;
    }

    // Default: flush
    default: {
        #ifdef GPIO_GPIOE_DEBUG_0
        Serial.println("[requestEventBus0Bin] event id is not defined: " + String(GPIOPortExpander_SLAVE.i2cLink.REQUEST_ID));
        #endif
        while (Wire.available()) {Wire.read();} // drain
        break;
    }
  }
}

void receiveEventBus0Bin(int n_bytes_received) {

  // unsigned long t0 = micros();

  if (n_bytes_received < 1) return;

  // Expects uint8 command byte!
  uint8_t cmd = GPIOPortExpander_SLAVE.wire.read();
  #ifdef GPIO_GPIOE_DEBUG_1
  Serial.println("[receiveEventBus0Bin] " + String(cmd) + " (" + String(n_bytes_received) + " bytes)");
  #endif

  switch (cmd) {

    // ------------------------------------------------------------------------------------------
    // SET portmap entry as pin n
    // ------------------------------------------------------------------------------------------
    case GPIOPE_CMD_SET_PORTMAP_PIN: {
      if (n_bytes_received != 3) {
        while (GPIOPortExpander_SLAVE.wire.available()) GPIOPortExpander_SLAVE.wire.read();
        #ifdef GPIO_GPIOE_DEBUG_0
        Serial.println("[SET_PIN_VALUE] packet must be 3 bytes!");
        #endif
        return;
      }
      uint8_t idx;
      read_uint8_FromWire(GPIOPortExpander_SLAVE.wire, idx);
      int8_t pin;
      read_int8_FromWire(GPIOPortExpander_SLAVE.wire, pin);

      while (GPIOPortExpander_SLAVE.wire.available()) {GPIOPortExpander_SLAVE.wire.read();}

      #ifdef GPIO_GPIOE_DEBUG_2
      Serial.println("setpin"
        " idx=" + String(idx) +
        " pin=" + String(pin)
      );
      #endif

      if (idx >= GPIOPortExpander_SLAVE.max_pins) {return;}

      GPIOPortExpander_SLAVE.port_map[idx]       = (int8_t)pin;
      
      // Serial.println("T0 " + String(micros()-t0)); // 100uS
      break;
    }

    // ------------------------------------------------------------------------------------------
    // SET output value by portmap index
    // ------------------------------------------------------------------------------------------
    case GPIOPE_CMD_SET_PORTMAP_VALUE: {
      // if (n_bytes_received != 3) {
      //   while (GPIOPortExpander_SLAVE.wire.available()) GPIOPortExpander_SLAVE.wire.read();
      //   #ifdef GPIO_GPIOE_DEBUG_0
      //   Serial.println("[SET_PIN_VALUE] packet must be 3 bytes!");
      //   #endif
      //   return;
      // }
      uint8_t idx;
      read_uint8_FromWire(GPIOPortExpander_SLAVE.wire, idx);
      int32_t value;
      read_int32_FromWire(GPIOPortExpander_SLAVE.wire, value);

      while (GPIOPortExpander_SLAVE.wire.available()) {GPIOPortExpander_SLAVE.wire.read();}

      #ifdef GPIO_GPIOE_DEBUG_2
      Serial.println("set"
        " idx=" + String(idx) +
        " (pin=" + String(GPIOPortExpander_SLAVE.port_map[idx]) + ")"
        " value=" + String(value)
      );
      #endif

      if (idx >= GPIOPortExpander_SLAVE.max_pins) {return;}

      GPIOPortExpander_SLAVE.output_value[idx]       = (int32_t)value;

      if (value > 0 && (GPIOPortExpander_SLAVE.modulation_time[idx][0] != 0 || GPIOPortExpander_SLAVE.modulation_time[idx][1] != 0)) {
        // modulator() alone owns this pin's physical state from here -
        // writing directly here would fight its on/off timing.
        activateModulatedPin(idx);
      } else {
        deactivateModulatedPin(idx);
        writedPortmapPin(idx);
      }

      // Serial.println("T0 " + String(micros()-t0)); // 100uS
      break;
    }

    case GPIOPE_CMD_READ_PIN: {
      int8_t pin;
      read_int8_FromWire(GPIOPortExpander_SLAVE.wire, pin);
      GPIOPortExpander_SLAVE.query_cursor = pin;
      GPIOPortExpander_SLAVE.i2cLink.REQUEST_ID = GPIOPE_CMD_READ_PIN;
      break;
    }

    // ------------------------------------------------------------------------------------------
    // SET pwm by portmap index
    // ------------------------------------------------------------------------------------------
    case GPIOPE_CMD_SET_PORTMAP_PWM: {
      uint8_t idx;
      read_uint8_FromWire(GPIOPortExpander_SLAVE.wire, idx);
      uint32_t pwm0;
      read_uint32_FromWire(GPIOPortExpander_SLAVE.wire, pwm0);
      uint32_t pwm1;
      read_uint32_FromWire(GPIOPortExpander_SLAVE.wire, pwm1);
      while (GPIOPortExpander_SLAVE.wire.available()) {GPIOPortExpander_SLAVE.wire.read();}
      GPIOPortExpander_SLAVE.modulation_time[idx][0] = (uint32_t)pwm0;
      GPIOPortExpander_SLAVE.modulation_time[idx][1] = (uint32_t)pwm1;
      break;
    }

    case GPIOPE_CMD_GET_INFO: {
      #ifdef GPIO_GPIOE_DEBUG_2
      Serial.println("[GPIOPE_CMD_GET_INFO]");
      #endif
      GPIOPortExpander_SLAVE.query_cursor = 0;
      GPIOPortExpander_SLAVE.i2cLink.REQUEST_ID = GPIOPE_CMD_GET_INFO;
      while (GPIOPortExpander_SLAVE.wire.available()) {GPIOPortExpander_SLAVE.wire.read();}
      break;
    }

    case GPIOPE_CMD_GET_PINS: {
      #ifdef GPIO_GPIOE_DEBUG_2
      Serial.println("[GPIOPE_CMD_GET_PINS]");
      #endif
      GPIOPortExpander_SLAVE.query_cursor = 0;
      GPIOPortExpander_SLAVE.i2cLink.REQUEST_ID = GPIOPE_CMD_GET_PINS;
      while (GPIOPortExpander_SLAVE.wire.available()) {GPIOPortExpander_SLAVE.wire.read();}
      break;
    }

    case GPIOPE_CMD_GET_PWM: {
      #ifdef GPIO_GPIOE_DEBUG_2
      Serial.println("[GPIOPE_CMD_GET_PWM]");
      #endif
      GPIOPortExpander_SLAVE.query_cursor = 0;
      GPIOPortExpander_SLAVE.i2cLink.REQUEST_ID = GPIOPE_CMD_GET_PWM;
      while (GPIOPortExpander_SLAVE.wire.available()) {GPIOPortExpander_SLAVE.wire.read();}
      break;
    }

    case GPIOPE_CMD_SET_DEFAULT: {
      #ifdef GPIO_GPIOE_DEBUG_2
      Serial.println("[GPIOPE_CMD_SET_DEFAULT]");
      #endif
      for (int i = 0; i < GPIOPortExpander_SLAVE.max_pins; i++) {
        GPIOPortExpander_SLAVE.port_map[i] = -1;
        GPIOPortExpander_SLAVE.output_value[i] = 0;
        GPIOPortExpander_SLAVE.modulation_time[i][0] = 0;
        GPIOPortExpander_SLAVE.modulation_time[i][1] = 0;
        GPIOPortExpander_SLAVE.modulation_time[i][2] = 0;
        GPIOPortExpander_SLAVE.switch_state[i] = false;
      }
      resetModulatedPinList();
      GPIOPortExpander_SLAVE.current_pin = 0;
      while (GPIOPortExpander_SLAVE.wire.available()) GPIOPortExpander_SLAVE.wire.read();  // flush
      break;
    }

    // Default: flush
    default: {
      while (GPIOPortExpander_SLAVE.wire.available()) {GPIOPortExpander_SLAVE.wire.read();}
      break;
    }
  }
}
#endif

#ifdef GPIOPE_MASTER_MODE
// ------------------------------------------------------------------------------------------------------------------
// MASTER MDOE
// ------------------------------------------------------------------------------------------------------------------

// A blank data struct for GPIOPE input devices. Can be populated manually and or automatically via GPIOPE_QueryDevice.
#define GPIOPE_DEFINE_INPUT(N) \
  IICLink IICLinkGPIOE_##N; \
  GPIOPortExpander GPIOPE_INPUT_##N = { \
    .name              = "GPIOPE_INPUT_" #N, \
    .wire              = GPIOPE_INPUT_I2C_BUS, \
    .i2cLink           = IICLinkGPIOE_##N, \
    .address           = I2C_ADDR_##N, \
    .current_pin       = 0, \
    .pin_min           = 0, \
    .pin_max           = 0, \
    .max_pins          = 100, \
    .num_analog_pins   = 0, \
    .num_digital_pins  = 0, \
    .max_input_values  = GPIOPE_MAX_SIZE, \
    .max_output_values = GPIOPE_MAX_SIZE, \
    .query_cursor      = 0, \
    .analog_pins       = {}, \
    .digital_pins      = {}, \
    .modulation_time   = {}, \
    .input_value       = {}, \
    .output_value      = {}, \
    .port_map          = {}, \
    .switch_state      = {}, \
    .enabled           = {true}, \
    .chan_freq_uS      = {1000000}, \
  };

// A blank data struct for GPIOPE input devices. Can be populated manually and or automatically via GPIOPE_QueryDevice.
#define GPIOPE_DEFINE_OUTPUT(N) \
  IICLink IICLinkPCO_##N; \
  GPIOPortExpander GPIOPE_OUTPUT_##N = { \
    .name              = "GPIOPE_OUTPUT_" #N, \
    .wire              = GPIOPE_OUTPUT_I2C_BUS, \
    .i2cLink           = IICLinkPCO_##N, \
    .address           = I2C_ADDR_##N, \
    .current_pin       = 0, \
    .pin_min           = 0, \
    .pin_max           = 0, \
    .max_pins          = 100, \
    .num_analog_pins   = 0, \
    .num_digital_pins  = 0, \
    .max_input_values  = GPIOPE_MAX_SIZE, \
    .max_output_values = GPIOPE_MAX_SIZE, \
    .query_cursor      = 0, \
    .analog_pins       = {}, \
    .digital_pins      = {}, \
    .modulation_time   = {}, \
    .input_value       = {}, \
    .output_value      = {}, \
    .port_map          = {}, \
    .switch_state      = {}, \
    .enabled           = {true}, \
    .chan_freq_uS      = {1000000}, \
  };

void GPIOPE_Set_Device_Default(GPIOPortExpander gpio_expander) {
  clearI2CLinkOutputPacket(gpio_expander.i2cLink);
  write_uint8_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, gpio_expander.i2cLink.current_bytes, GPIOPE_CMD_SET_DEFAULT);
  writeI2CToSlaveBin(gpio_expander.wire, gpio_expander.i2cLink, gpio_expander.address, gpio_expander.i2cLink.current_bytes, 0, "GPIOPE_Set_Device_Default");
}

bool GPIOPE_Set_Portmap_Index_As_Pin(GPIOPortExpander &gpio_expander, uint8_t index, int8_t pin) {
  printf("%s  address=%d  (index=%d -> pin=%d)\n", gpio_expander.name, gpio_expander.address, index, pin);
  clearI2CLinkOutputPacket(gpio_expander.i2cLink);
  write_uint8_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, gpio_expander.i2cLink.current_bytes, GPIOPE_CMD_SET_PORTMAP_PIN);
  write_uint8_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, gpio_expander.i2cLink.current_bytes, index);
  write_int8_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, gpio_expander.i2cLink.current_bytes, pin);
  writeI2CToSlaveBin(gpio_expander.wire, gpio_expander.i2cLink, gpio_expander.address, gpio_expander.i2cLink.current_bytes, gpio_expander.i2cLink.current_bytes, gpio_expander.name);

  return true;
}

bool GPIOPE_Set_Portmap_Index_As_PWM(GPIOPortExpander &gpio_expander, uint8_t index, uint32_t off_time, uint32_t on_time) {
  printf("%s  address=%d  (index=%d -> pwm0=%ld  pwm1=%ld)\n", gpio_expander.name, gpio_expander.address, index, off_time, on_time);
  clearI2CLinkOutputPacket(gpio_expander.i2cLink);
  write_uint8_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, gpio_expander.i2cLink.current_bytes, GPIOPE_CMD_SET_PORTMAP_PWM);
  write_uint8_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, gpio_expander.i2cLink.current_bytes, index);
  write_uint32_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, gpio_expander.i2cLink.current_bytes, off_time);
  write_uint32_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, gpio_expander.i2cLink.current_bytes, on_time);
  writeI2CToSlaveBin(gpio_expander.wire, gpio_expander.i2cLink, gpio_expander.address, gpio_expander.i2cLink.current_bytes, gpio_expander.i2cLink.current_bytes, gpio_expander.name);

  return true;
}

bool GPIOPE_Set_All_Portmap_Index_Pin(GPIOPortExpander &gpio_expander) {
  if (gpio_expander.max_pins == 0) {printf("[GPIOPE_Set_All_Portmap_Index_Pin] max pins 0. exiting."); return false;}
  for (int i=0; i<gpio_expander.max_pins; i++) {
    GPIOPE_Set_Portmap_Index_As_Pin(gpio_expander, i, gpio_expander.port_map[i]);
  }
  return true;
}

bool GPIOPE_Set_All_Portmap_Index_PWM(GPIOPortExpander &gpio_expander) {
  if (gpio_expander.max_pins == 0) {printf("[GPIOPE_Set_All_Portmap_Index_PWM] max pins 0. exiting."); return false;}
  bool ok = true;
  for (int i=0; i<gpio_expander.max_pins; i++) {
    GPIOPE_Set_Portmap_Index_As_PWM(gpio_expander, i, gpio_expander.modulation_time[i][0], gpio_expander.modulation_time[i][1]);
  }
  return ok;
}

bool GPIOPE_Read_Pin(GPIOPortExpander gpio_expander, uint8_t pin) {
  if (pin >= (uint8_t)gpio_expander.max_pins) {return false;}
  clearI2CLinkOutputPacket(gpio_expander.i2cLink);
  write_uint8_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, gpio_expander.i2cLink.current_bytes, GPIOPE_CMD_READ_PIN);
  write_int8_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, gpio_expander.i2cLink.current_bytes, pin);
  writeI2CToSlaveBin(gpio_expander.wire, gpio_expander.i2cLink, gpio_expander.address, gpio_expander.i2cLink.current_bytes, 0, "GPIOPE_Read_Pin");

  if (!requestFromSlaveBinNoID(gpio_expander.wire, gpio_expander.i2cLink, gpio_expander.address, 4, 0, "GPIOPE_Read_Pin")) {
    return false;
  }
  uint8_t value;
  read_uint8_FromWire(gpio_expander.wire, value);
  gpio_expander.input_value[pin] = (uint32_t)value;
  return true;
}

void GPIOPE_Set_Channel_Enabled(GPIOPortExpander &gpio_expander, uint8_t channel,  bool enabled) {
  if (channel < gpio_expander.max_pins) {
    gpio_expander.enabled[channel] = enabled;
    if (enabled == false) {
      gpio_expander.input_value[channel] = 0;
      gpio_expander.output_value[channel] = 0;
    }
  }
}

void GPIOPE_Set_Channel_Frequency(GPIOPortExpander &gpio_expander, uint8_t pin, uint64_t freq_uS) {
  if (pin < (uint8_t)gpio_expander.max_pins) {
    gpio_expander.chan_freq_uS[pin] = (freq_uS > (uint64_t)INT64_MAX) ? (uint64_t)INT64_MAX : freq_uS;
  }
}

bool GPIOPE_QueryDevice(GPIOPortExpander &gpio_expander, int8_t address) {
  // ------------------------------------------------------------
  // GPIOPE_CMD_GET_INFO
  // ------------------------------------------------------------
  clearI2CLinkOutputPacket(gpio_expander.i2cLink);
  write_uint8_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, gpio_expander.i2cLink.current_bytes, GPIOPE_CMD_GET_INFO);
  writeI2CToSlaveBin(gpio_expander.wire, gpio_expander.i2cLink, address, gpio_expander.i2cLink.current_bytes, 0, "GPIOPE_QueryDevice");
  if (!requestFromSlaveBinNoID(gpio_expander.wire, gpio_expander.i2cLink, address, 7, 0, "GPIOPE_QueryDevice")) {
    return false;
  }
  int8_t  pin_min, pin_max, max_pins, num_analog_pins, num_digital_pins;
  int32_t max_input_values, max_output_values;
  read_int8_FromWire(gpio_expander.wire, pin_min);
  read_int8_FromWire(gpio_expander.wire, pin_max);
  read_int8_FromWire(gpio_expander.wire, max_pins);
  read_int8_FromWire(gpio_expander.wire, num_analog_pins);
  read_int8_FromWire(gpio_expander.wire, num_digital_pins);
  read_int32_FromWire(gpio_expander.wire, max_input_values);
  read_int32_FromWire(gpio_expander.wire, max_output_values);
  gpio_expander.pin_min          = (int8_t)pin_min;
  gpio_expander.pin_max          = (int8_t)pin_max;
  gpio_expander.max_pins         = (int8_t)max_pins;
  gpio_expander.num_analog_pins  = (int8_t)num_analog_pins;
  gpio_expander.num_digital_pins = (int8_t)num_digital_pins;
  gpio_expander.max_input_values  = (int32_t)max_input_values;
  gpio_expander.max_output_values = (int32_t)max_output_values;
  // ------------------------------------------------------------
  // GPIOPE_CMD_GET_PINS
  // ------------------------------------------------------------
  clearI2CLinkOutputPacket(gpio_expander.i2cLink);
  write_uint8_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, gpio_expander.i2cLink.current_bytes, GPIOPE_CMD_GET_PINS);
  writeI2CToSlaveBin(gpio_expander.wire, gpio_expander.i2cLink, gpio_expander.address, gpio_expander.i2cLink.current_bytes, 0, "GPIOPE_QueryDevice");
  int analog_i = 0, digital_i = 0;
  for (int i = 0; i < gpio_expander.pin_max; i++) {
    if (!requestFromSlaveBinNoID(gpio_expander.wire, gpio_expander.i2cLink, gpio_expander.address, 3, 0, "GPIOPE_QueryDevice")) {
      return false;
    }
    uint8_t is_analog;
    int8_t pin;
    int8_t mapped_pin;
    read_uint8_FromWire(gpio_expander.wire, is_analog);
    read_int8_FromWire(gpio_expander.wire, pin);
    read_int8_FromWire(gpio_expander.wire, mapped_pin);
    if (is_analog) {
      if (analog_i < gpio_expander.num_analog_pins) gpio_expander.analog_pins[analog_i++] = pin;
    } else {
      if (digital_i < gpio_expander.num_digital_pins) gpio_expander.digital_pins[digital_i++] = pin;
    }
    if (i < gpio_expander.pin_max) gpio_expander.port_map[i] = mapped_pin;
  }
  // ------------------------------------------------------------
  // GPIOPE_CMD_GET_PWM
  // ------------------------------------------------------------
  clearI2CLinkOutputPacket(gpio_expander.i2cLink);
  write_uint8_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, gpio_expander.i2cLink.current_bytes, GPIOPE_CMD_GET_PWM);
  writeI2CToSlaveBin(gpio_expander.wire, gpio_expander.i2cLink, gpio_expander.address, gpio_expander.i2cLink.current_bytes, 0, "GPIOPE_QueryDevice");
  for (int i = 0; i < gpio_expander.max_pins; i++) {
    if (!requestFromSlaveBinNoID(gpio_expander.wire, gpio_expander.i2cLink, gpio_expander.address, 8, 0, "GPIOPE_QueryDevice")) {
      return false;
    }
    uint32_t off_time, on_time;
    read_uint32_FromWire(gpio_expander.wire, off_time);
    read_uint32_FromWire(gpio_expander.wire, on_time);
    gpio_expander.modulation_time[i][0] = (unsigned long)off_time;
    gpio_expander.modulation_time[i][1] = (unsigned long)on_time;
  }
  // ------------------------------------------------------------
  // Debug
  // ------------------------------------------------------------
  printf("%s  address=%d  pin_min=%d  pin_max=%d  max_pins=%d  num_analog_pins=%d  num_digital_pins=%d  max_input_values=%ld  max_output_values=%ld\n",
    gpio_expander.name, address, gpio_expander.pin_min, gpio_expander.pin_max, gpio_expander.max_pins,
    gpio_expander.num_analog_pins, gpio_expander.num_digital_pins,
    (long)gpio_expander.max_input_values, (long)gpio_expander.max_output_values
  );
  printf("  analog_pins: [");
  for (int i = 0; i < gpio_expander.num_analog_pins; i++) {
    printf("%d%s", gpio_expander.analog_pins[i], (i < gpio_expander.num_analog_pins - 1) ? ", " : "");
  }
  printf("]\n  digital_pins: [");
  for (int i = 0; i < gpio_expander.num_digital_pins; i++) {
    printf("%d%s", gpio_expander.digital_pins[i], (i < gpio_expander.num_digital_pins - 1) ? ", " : "");
  }
  printf("]\n  port_map: [");
  for (int i = 0; i < gpio_expander.pin_max; i++) {
    printf("%d%s", gpio_expander.port_map[i], (i < gpio_expander.pin_max) ? ", " : "");
  }
  printf("]\n  pwm (off,on): [");
  for (int i = 0; i < gpio_expander.max_pins; i++) {
    printf("(%lu,%lu)%s", gpio_expander.modulation_time[i][0], gpio_expander.modulation_time[i][1], (i < gpio_expander.max_pins - 1) ? ", " : "");
  }
  printf("]\n");

  return true;
}

GPIOPortExpander* isGPIOPE(uint8_t address) {

  GPIOPortExpander *gpiope = nullptr;

  switch (address) {

    #ifdef GPIOPE_USE_OUTPUT_0
    case I2C_ADDR_0: {
      gpiope = &GPIOPE_OUTPUT_0;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_1
    case I2C_ADDR_1: {
      gpiope = &GPIOPE_OUTPUT_1;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_2
    case I2C_ADDR_2: {
      gpiope = &GPIOPE_OUTPUT_2;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_3
    case I2C_ADDR_3: {
      gpiope = &GPIOPE_OUTPUT_3;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_4
    case I2C_ADDR_4: {
      gpiope = &GPIOPE_OUTPUT_4;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_5
    case I2C_ADDR_5: {
      gpiope = &GPIOPE_OUTPUT_5;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_6
    case I2C_ADDR_6: {
      gpiope = &GPIOPE_OUTPUT_6;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_7
    case I2C_ADDR_7: {
      gpiope = &GPIOPE_OUTPUT_7;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_8
    case I2C_ADDR_8: {
      gpiope = &GPIOPE_OUTPUT_8;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_9
    case I2C_ADDR_9: {
      gpiope = &GPIOPE_OUTPUT_9;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_10
    case I2C_ADDR_10: {
      gpiope = &GPIOPE_OUTPUT_10;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_11
    case I2C_ADDR_11: {
      gpiope = &GPIOPE_OUTPUT_11;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_12
    case I2C_ADDR_12: {
      gpiope = &GPIOPE_OUTPUT_12;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_13
    case I2C_ADDR_13: {
      gpiope = &GPIOPE_OUTPUT_13;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_14
    case I2C_ADDR_14: {
      gpiope = &GPIOPE_OUTPUT_14;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_15
    case I2C_ADDR_15: {
      gpiope = &GPIOPE_OUTPUT_15;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_16
    case I2C_ADDR_16: {
      gpiope = &GPIOPE_OUTPUT_16;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_17
    case I2C_ADDR_17: {
      gpiope = &GPIOPE_OUTPUT_17;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_18
    case I2C_ADDR_18: {
      gpiope = &GPIOPE_OUTPUT_18;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_19
    case I2C_ADDR_19: {
      gpiope = &GPIOPE_OUTPUT_19;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_20
    case I2C_ADDR_20: {
      gpiope = &GPIOPE_OUTPUT_20;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_21
    case I2C_ADDR_21: {
      gpiope = &GPIOPE_OUTPUT_21;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_22
    case I2C_ADDR_22: {
      gpiope = &GPIOPE_OUTPUT_22;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_23
    case I2C_ADDR_23: {
      gpiope = &GPIOPE_OUTPUT_23;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_24
    case I2C_ADDR_24: {
      gpiope = &GPIOPE_OUTPUT_24;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_25
    case I2C_ADDR_25: {
      gpiope = &GPIOPE_OUTPUT_25;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_26
    case I2C_ADDR_26: {
      gpiope = &GPIOPE_OUTPUT_26;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_27
    case I2C_ADDR_27: {
      gpiope = &GPIOPE_OUTPUT_27;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_28
    case I2C_ADDR_28: {
      gpiope = &GPIOPE_OUTPUT_28;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_29
    case I2C_ADDR_29: {
      gpiope = &GPIOPE_OUTPUT_29;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_30
    case I2C_ADDR_30: {
      gpiope = &GPIOPE_OUTPUT_30;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_31
    case I2C_ADDR_31: {
      gpiope = &GPIOPE_OUTPUT_31;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_32
    case I2C_ADDR_32: {
      gpiope = &GPIOPE_OUTPUT_32;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_33
    case I2C_ADDR_33: {
      gpiope = &GPIOPE_OUTPUT_33;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_34
    case I2C_ADDR_34: {
      gpiope = &GPIOPE_OUTPUT_34;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_35
    case I2C_ADDR_35: {
      gpiope = &GPIOPE_OUTPUT_35;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_36
    case I2C_ADDR_36: {
      gpiope = &GPIOPE_OUTPUT_36;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_37
    case I2C_ADDR_37: {
      gpiope = &GPIOPE_OUTPUT_37;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_38
    case I2C_ADDR_38: {
      gpiope = &GPIOPE_OUTPUT_38;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_39
    case I2C_ADDR_39: {
      gpiope = &GPIOPE_OUTPUT_39;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_40
    case I2C_ADDR_40: {
      gpiope = &GPIOPE_OUTPUT_40;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_41
    case I2C_ADDR_41: {
      gpiope = &GPIOPE_OUTPUT_41;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_42
    case I2C_ADDR_42: {
      gpiope = &GPIOPE_OUTPUT_42;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_43
    case I2C_ADDR_43: {
      gpiope = &GPIOPE_OUTPUT_43;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_44
    case I2C_ADDR_44: {
      gpiope = &GPIOPE_OUTPUT_44;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_45
    case I2C_ADDR_45: {
      gpiope = &GPIOPE_OUTPUT_45;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_46
    case I2C_ADDR_46: {
      gpiope = &GPIOPE_OUTPUT_46;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_47
    case I2C_ADDR_47: {
      gpiope = &GPIOPE_OUTPUT_47;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_48
    case I2C_ADDR_48: {
      gpiope = &GPIOPE_OUTPUT_48;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_49
    case I2C_ADDR_49: {
      gpiope = &GPIOPE_OUTPUT_49;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_50
    case I2C_ADDR_50: {
      gpiope = &GPIOPE_OUTPUT_50;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_51
    case I2C_ADDR_51: {
      gpiope = &GPIOPE_OUTPUT_51;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_52
    case I2C_ADDR_52: {
      gpiope = &GPIOPE_OUTPUT_52;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_53
    case I2C_ADDR_53: {
      gpiope = &GPIOPE_OUTPUT_53;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_54
    case I2C_ADDR_54: {
      gpiope = &GPIOPE_OUTPUT_54;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_55
    case I2C_ADDR_55: {
      gpiope = &GPIOPE_OUTPUT_55;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_56
    case I2C_ADDR_56: {
      gpiope = &GPIOPE_OUTPUT_56;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_57
    case I2C_ADDR_57: {
      gpiope = &GPIOPE_OUTPUT_57;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_58
    case I2C_ADDR_58: {
      gpiope = &GPIOPE_OUTPUT_58;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_59
    case I2C_ADDR_59: {
      gpiope = &GPIOPE_OUTPUT_59;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_60
    case I2C_ADDR_60: {
      gpiope = &GPIOPE_OUTPUT_60;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_61
    case I2C_ADDR_61: {
      gpiope = &GPIOPE_OUTPUT_61;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_62
    case I2C_ADDR_62: {
      gpiope = &GPIOPE_OUTPUT_62;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_63
    case I2C_ADDR_63: {
      gpiope = &GPIOPE_OUTPUT_63;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_64
    case I2C_ADDR_64: {
      gpiope = &GPIOPE_OUTPUT_64;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_65
    case I2C_ADDR_65: {
      gpiope = &GPIOPE_OUTPUT_65;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_66
    case I2C_ADDR_66: {
      gpiope = &GPIOPE_OUTPUT_66;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_67
    case I2C_ADDR_67: {
      gpiope = &GPIOPE_OUTPUT_67;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_68
    case I2C_ADDR_68: {
      gpiope = &GPIOPE_OUTPUT_68;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_69
    case I2C_ADDR_69: {
      gpiope = &GPIOPE_OUTPUT_69;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_70
    case I2C_ADDR_70: {
      gpiope = &GPIOPE_OUTPUT_70;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_71
    case I2C_ADDR_71: {
      gpiope = &GPIOPE_OUTPUT_71;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_72
    case I2C_ADDR_72: {
      gpiope = &GPIOPE_OUTPUT_72;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_73
    case I2C_ADDR_73: {
      gpiope = &GPIOPE_OUTPUT_73;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_74
    case I2C_ADDR_74: {
      gpiope = &GPIOPE_OUTPUT_74;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_75
    case I2C_ADDR_75: {
      gpiope = &GPIOPE_OUTPUT_75;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_76
    case I2C_ADDR_76: {
      gpiope = &GPIOPE_OUTPUT_76;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_77
    case I2C_ADDR_77: {
      gpiope = &GPIOPE_OUTPUT_77;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_78
    case I2C_ADDR_78: {
      gpiope = &GPIOPE_OUTPUT_78;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_79
    case I2C_ADDR_79: {
      gpiope = &GPIOPE_OUTPUT_79;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_80
    case I2C_ADDR_80: {
      gpiope = &GPIOPE_OUTPUT_80;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_81
    case I2C_ADDR_81: {
      gpiope = &GPIOPE_OUTPUT_81;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_82
    case I2C_ADDR_82: {
      gpiope = &GPIOPE_OUTPUT_82;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_83
    case I2C_ADDR_83: {
      gpiope = &GPIOPE_OUTPUT_83;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_84
    case I2C_ADDR_84: {
      gpiope = &GPIOPE_OUTPUT_84;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_85
    case I2C_ADDR_85: {
      gpiope = &GPIOPE_OUTPUT_85;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_86
    case I2C_ADDR_86: {
      gpiope = &GPIOPE_OUTPUT_86;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_87
    case I2C_ADDR_87: {
      gpiope = &GPIOPE_OUTPUT_87;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_88
    case I2C_ADDR_88: {
      gpiope = &GPIOPE_OUTPUT_88;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_89
    case I2C_ADDR_89: {
      gpiope = &GPIOPE_OUTPUT_89;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_90
    case I2C_ADDR_90: {
      gpiope = &GPIOPE_OUTPUT_90;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_91
    case I2C_ADDR_91: {
      gpiope = &GPIOPE_OUTPUT_91;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_92
    case I2C_ADDR_92: {
      gpiope = &GPIOPE_OUTPUT_92;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_93
    case I2C_ADDR_93: {
      gpiope = &GPIOPE_OUTPUT_93;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_94
    case I2C_ADDR_94: {
      gpiope = &GPIOPE_OUTPUT_94;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_95
    case I2C_ADDR_95: {
      gpiope = &GPIOPE_OUTPUT_95;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_96
    case I2C_ADDR_96: {
      gpiope = &GPIOPE_OUTPUT_96;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_97
    case I2C_ADDR_97: {
      gpiope = &GPIOPE_OUTPUT_97;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_98
    case I2C_ADDR_98: {
      gpiope = &GPIOPE_OUTPUT_98;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_99
    case I2C_ADDR_99: {
      gpiope = &GPIOPE_OUTPUT_99;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_100
    case I2C_ADDR_100: {
      gpiope = &GPIOPE_OUTPUT_100;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_101
    case I2C_ADDR_101: {
      gpiope = &GPIOPE_OUTPUT_101;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_102
    case I2C_ADDR_102: {
      gpiope = &GPIOPE_OUTPUT_102;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_103
    case I2C_ADDR_103: {
      gpiope = &GPIOPE_OUTPUT_103;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_104
    case I2C_ADDR_104: {
      gpiope = &GPIOPE_OUTPUT_104;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_105
    case I2C_ADDR_105: {
      gpiope = &GPIOPE_OUTPUT_105;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_106
    case I2C_ADDR_106: {
      gpiope = &GPIOPE_OUTPUT_106;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_107
    case I2C_ADDR_107: {
      gpiope = &GPIOPE_OUTPUT_107;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_108
    case I2C_ADDR_108: {
      gpiope = &GPIOPE_OUTPUT_108;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_109
    case I2C_ADDR_109: {
      gpiope = &GPIOPE_OUTPUT_109;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_110
    case I2C_ADDR_110: {
      gpiope = &GPIOPE_OUTPUT_110;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_111
    case I2C_ADDR_111: {
      gpiope = &GPIOPE_OUTPUT_111;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_112
    case I2C_ADDR_112: {
      gpiope = &GPIOPE_OUTPUT_112;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_113
    case I2C_ADDR_113: {
      gpiope = &GPIOPE_OUTPUT_113;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_114
    case I2C_ADDR_114: {
      gpiope = &GPIOPE_OUTPUT_114;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_115
    case I2C_ADDR_115: {
      gpiope = &GPIOPE_OUTPUT_115;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_116
    case I2C_ADDR_116: {
      gpiope = &GPIOPE_OUTPUT_116;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_117
    case I2C_ADDR_117: {
      gpiope = &GPIOPE_OUTPUT_117;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_118
    case I2C_ADDR_118: {
      gpiope = &GPIOPE_OUTPUT_118;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_119
    case I2C_ADDR_119: {
      gpiope = &GPIOPE_OUTPUT_119;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_120
    case I2C_ADDR_120: {
      gpiope = &GPIOPE_OUTPUT_120;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_121
    case I2C_ADDR_121: {
      gpiope = &GPIOPE_OUTPUT_121;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_122
    case I2C_ADDR_122: {
      gpiope = &GPIOPE_OUTPUT_122;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_123
    case I2C_ADDR_123: {
      gpiope = &GPIOPE_OUTPUT_123;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_124
    case I2C_ADDR_124: {
      gpiope = &GPIOPE_OUTPUT_124;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_125
    case I2C_ADDR_125: {
      gpiope = &GPIOPE_OUTPUT_125;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_126
    case I2C_ADDR_126: {
      gpiope = &GPIOPE_OUTPUT_126;
      break;
    }
    #endif

    #ifdef GPIOPE_USE_OUTPUT_127
    case I2C_ADDR_127: {
      gpiope = &GPIOPE_OUTPUT_127;
      break;
    }
    #endif

    default: {
      // printf("warning: no gpiope device found for I2C address=%d\n", address);
      break;
    }
  }

  return gpiope;
}

// ------------------------------------------------------------
// Input instance creation
// ------------------------------------------------------------
#ifdef GPIOPE_USE_INPUT_0
GPIOPE_DEFINE_INPUT(0)
#endif // GPIOPE_USE_INPUT_0

#ifdef GPIOPE_USE_INPUT_1
GPIOPE_DEFINE_INPUT(1)
#endif // GPIOPE_USE_INPUT_1

#ifdef GPIOPE_USE_INPUT_2
GPIOPE_DEFINE_INPUT(2)
#endif // GPIOPE_USE_INPUT_2

#ifdef GPIOPE_USE_INPUT_3
GPIOPE_DEFINE_INPUT(3)
#endif // GPIOPE_USE_INPUT_3

#ifdef GPIOPE_USE_INPUT_4
GPIOPE_DEFINE_INPUT(4)
#endif // GPIOPE_USE_INPUT_4

#ifdef GPIOPE_USE_INPUT_5
GPIOPE_DEFINE_INPUT(5)
#endif // GPIOPE_USE_INPUT_5

#ifdef GPIOPE_USE_INPUT_6
GPIOPE_DEFINE_INPUT(6)
#endif // GPIOPE_USE_INPUT_6

#ifdef GPIOPE_USE_INPUT_7
GPIOPE_DEFINE_INPUT(7)
#endif // GPIOPE_USE_INPUT_7

#ifdef GPIOPE_USE_INPUT_8
GPIOPE_DEFINE_INPUT(8)
#endif // GPIOPE_USE_INPUT_8

#ifdef GPIOPE_USE_INPUT_9
GPIOPE_DEFINE_INPUT(9)
#endif // GPIOPE_USE_INPUT_9

#ifdef GPIOPE_USE_INPUT_10
GPIOPE_DEFINE_INPUT(10)
#endif // GPIOPE_USE_INPUT_10

#ifdef GPIOPE_USE_INPUT_11
GPIOPE_DEFINE_INPUT(11)
#endif // GPIOPE_USE_INPUT_11

#ifdef GPIOPE_USE_INPUT_12
GPIOPE_DEFINE_INPUT(12)
#endif // GPIOPE_USE_INPUT_12

#ifdef GPIOPE_USE_INPUT_13
GPIOPE_DEFINE_INPUT(13)
#endif // GPIOPE_USE_INPUT_13

#ifdef GPIOPE_USE_INPUT_14
GPIOPE_DEFINE_INPUT(14)
#endif // GPIOPE_USE_INPUT_14

#ifdef GPIOPE_USE_INPUT_15
GPIOPE_DEFINE_INPUT(15)
#endif // GPIOPE_USE_INPUT_15

#ifdef GPIOPE_USE_INPUT_16
GPIOPE_DEFINE_INPUT(16)
#endif // GPIOPE_USE_INPUT_16

#ifdef GPIOPE_USE_INPUT_17
GPIOPE_DEFINE_INPUT(17)
#endif // GPIOPE_USE_INPUT_17

#ifdef GPIOPE_USE_INPUT_18
GPIOPE_DEFINE_INPUT(18)
#endif // GPIOPE_USE_INPUT_18

#ifdef GPIOPE_USE_INPUT_19
GPIOPE_DEFINE_INPUT(19)
#endif // GPIOPE_USE_INPUT_19

#ifdef GPIOPE_USE_INPUT_20
GPIOPE_DEFINE_INPUT(20)
#endif // GPIOPE_USE_INPUT_20

#ifdef GPIOPE_USE_INPUT_21
GPIOPE_DEFINE_INPUT(21)
#endif // GPIOPE_USE_INPUT_21

#ifdef GPIOPE_USE_INPUT_22
GPIOPE_DEFINE_INPUT(22)
#endif // GPIOPE_USE_INPUT_22

#ifdef GPIOPE_USE_INPUT_23
GPIOPE_DEFINE_INPUT(23)
#endif // GPIOPE_USE_INPUT_23

#ifdef GPIOPE_USE_INPUT_24
GPIOPE_DEFINE_INPUT(24)
#endif // GPIOPE_USE_INPUT_24

#ifdef GPIOPE_USE_INPUT_25
GPIOPE_DEFINE_INPUT(25)
#endif // GPIOPE_USE_INPUT_25

#ifdef GPIOPE_USE_INPUT_26
GPIOPE_DEFINE_INPUT(26)
#endif // GPIOPE_USE_INPUT_26

#ifdef GPIOPE_USE_INPUT_27
GPIOPE_DEFINE_INPUT(27)
#endif // GPIOPE_USE_INPUT_27

#ifdef GPIOPE_USE_INPUT_28
GPIOPE_DEFINE_INPUT(28)
#endif // GPIOPE_USE_INPUT_28

#ifdef GPIOPE_USE_INPUT_29
GPIOPE_DEFINE_INPUT(29)
#endif // GPIOPE_USE_INPUT_29

#ifdef GPIOPE_USE_INPUT_30
GPIOPE_DEFINE_INPUT(30)
#endif // GPIOPE_USE_INPUT_30

#ifdef GPIOPE_USE_INPUT_31
GPIOPE_DEFINE_INPUT(31)
#endif // GPIOPE_USE_INPUT_31

#ifdef GPIOPE_USE_INPUT_32
GPIOPE_DEFINE_INPUT(32)
#endif // GPIOPE_USE_INPUT_32

#ifdef GPIOPE_USE_INPUT_33
GPIOPE_DEFINE_INPUT(33)
#endif // GPIOPE_USE_INPUT_33

#ifdef GPIOPE_USE_INPUT_34
GPIOPE_DEFINE_INPUT(34)
#endif // GPIOPE_USE_INPUT_34

#ifdef GPIOPE_USE_INPUT_35
GPIOPE_DEFINE_INPUT(35)
#endif // GPIOPE_USE_INPUT_35

#ifdef GPIOPE_USE_INPUT_36
GPIOPE_DEFINE_INPUT(36)
#endif // GPIOPE_USE_INPUT_36

#ifdef GPIOPE_USE_INPUT_37
GPIOPE_DEFINE_INPUT(37)
#endif // GPIOPE_USE_INPUT_37

#ifdef GPIOPE_USE_INPUT_38
GPIOPE_DEFINE_INPUT(38)
#endif // GPIOPE_USE_INPUT_38

#ifdef GPIOPE_USE_INPUT_39
GPIOPE_DEFINE_INPUT(39)
#endif // GPIOPE_USE_INPUT_39

#ifdef GPIOPE_USE_INPUT_40
GPIOPE_DEFINE_INPUT(40)
#endif // GPIOPE_USE_INPUT_40

#ifdef GPIOPE_USE_INPUT_41
GPIOPE_DEFINE_INPUT(41)
#endif // GPIOPE_USE_INPUT_41

#ifdef GPIOPE_USE_INPUT_42
GPIOPE_DEFINE_INPUT(42)
#endif // GPIOPE_USE_INPUT_42

#ifdef GPIOPE_USE_INPUT_43
GPIOPE_DEFINE_INPUT(43)
#endif // GPIOPE_USE_INPUT_43

#ifdef GPIOPE_USE_INPUT_44
GPIOPE_DEFINE_INPUT(44)
#endif // GPIOPE_USE_INPUT_44

#ifdef GPIOPE_USE_INPUT_45
GPIOPE_DEFINE_INPUT(45)
#endif // GPIOPE_USE_INPUT_45

#ifdef GPIOPE_USE_INPUT_46
GPIOPE_DEFINE_INPUT(46)
#endif // GPIOPE_USE_INPUT_46

#ifdef GPIOPE_USE_INPUT_47
GPIOPE_DEFINE_INPUT(47)
#endif // GPIOPE_USE_INPUT_47

#ifdef GPIOPE_USE_INPUT_48
GPIOPE_DEFINE_INPUT(48)
#endif // GPIOPE_USE_INPUT_48

#ifdef GPIOPE_USE_INPUT_49
GPIOPE_DEFINE_INPUT(49)
#endif // GPIOPE_USE_INPUT_49

#ifdef GPIOPE_USE_INPUT_50
GPIOPE_DEFINE_INPUT(50)
#endif // GPIOPE_USE_INPUT_50

#ifdef GPIOPE_USE_INPUT_51
GPIOPE_DEFINE_INPUT(51)
#endif // GPIOPE_USE_INPUT_51

#ifdef GPIOPE_USE_INPUT_52
GPIOPE_DEFINE_INPUT(52)
#endif // GPIOPE_USE_INPUT_52

#ifdef GPIOPE_USE_INPUT_53
GPIOPE_DEFINE_INPUT(53)
#endif // GPIOPE_USE_INPUT_53

#ifdef GPIOPE_USE_INPUT_54
GPIOPE_DEFINE_INPUT(54)
#endif // GPIOPE_USE_INPUT_54

#ifdef GPIOPE_USE_INPUT_55
GPIOPE_DEFINE_INPUT(55)
#endif // GPIOPE_USE_INPUT_55

#ifdef GPIOPE_USE_INPUT_56
GPIOPE_DEFINE_INPUT(56)
#endif // GPIOPE_USE_INPUT_56

#ifdef GPIOPE_USE_INPUT_57
GPIOPE_DEFINE_INPUT(57)
#endif // GPIOPE_USE_INPUT_57

#ifdef GPIOPE_USE_INPUT_58
GPIOPE_DEFINE_INPUT(58)
#endif // GPIOPE_USE_INPUT_58

#ifdef GPIOPE_USE_INPUT_59
GPIOPE_DEFINE_INPUT(59)
#endif // GPIOPE_USE_INPUT_59

#ifdef GPIOPE_USE_INPUT_60
GPIOPE_DEFINE_INPUT(60)
#endif // GPIOPE_USE_INPUT_60

#ifdef GPIOPE_USE_INPUT_61
GPIOPE_DEFINE_INPUT(61)
#endif // GPIOPE_USE_INPUT_61

#ifdef GPIOPE_USE_INPUT_62
GPIOPE_DEFINE_INPUT(62)
#endif // GPIOPE_USE_INPUT_62

#ifdef GPIOPE_USE_INPUT_63
GPIOPE_DEFINE_INPUT(63)
#endif // GPIOPE_USE_INPUT_63

#ifdef GPIOPE_USE_INPUT_64
GPIOPE_DEFINE_INPUT(64)
#endif // GPIOPE_USE_INPUT_64

#ifdef GPIOPE_USE_INPUT_65
GPIOPE_DEFINE_INPUT(65)
#endif // GPIOPE_USE_INPUT_65

#ifdef GPIOPE_USE_INPUT_66
GPIOPE_DEFINE_INPUT(66)
#endif // GPIOPE_USE_INPUT_66

#ifdef GPIOPE_USE_INPUT_67
GPIOPE_DEFINE_INPUT(67)
#endif // GPIOPE_USE_INPUT_67

#ifdef GPIOPE_USE_INPUT_68
GPIOPE_DEFINE_INPUT(68)
#endif // GPIOPE_USE_INPUT_68

#ifdef GPIOPE_USE_INPUT_69
GPIOPE_DEFINE_INPUT(69)
#endif // GPIOPE_USE_INPUT_69

#ifdef GPIOPE_USE_INPUT_70
GPIOPE_DEFINE_INPUT(70)
#endif // GPIOPE_USE_INPUT_70

#ifdef GPIOPE_USE_INPUT_71
GPIOPE_DEFINE_INPUT(71)
#endif // GPIOPE_USE_INPUT_71

#ifdef GPIOPE_USE_INPUT_72
GPIOPE_DEFINE_INPUT(72)
#endif // GPIOPE_USE_INPUT_72

#ifdef GPIOPE_USE_INPUT_73
GPIOPE_DEFINE_INPUT(73)
#endif // GPIOPE_USE_INPUT_73

#ifdef GPIOPE_USE_INPUT_74
GPIOPE_DEFINE_INPUT(74)
#endif // GPIOPE_USE_INPUT_74

#ifdef GPIOPE_USE_INPUT_75
GPIOPE_DEFINE_INPUT(75)
#endif // GPIOPE_USE_INPUT_75

#ifdef GPIOPE_USE_INPUT_76
GPIOPE_DEFINE_INPUT(76)
#endif // GPIOPE_USE_INPUT_76

#ifdef GPIOPE_USE_INPUT_77
GPIOPE_DEFINE_INPUT(77)
#endif // GPIOPE_USE_INPUT_77

#ifdef GPIOPE_USE_INPUT_78
GPIOPE_DEFINE_INPUT(78)
#endif // GPIOPE_USE_INPUT_78

#ifdef GPIOPE_USE_INPUT_79
GPIOPE_DEFINE_INPUT(79)
#endif // GPIOPE_USE_INPUT_79

#ifdef GPIOPE_USE_INPUT_80
GPIOPE_DEFINE_INPUT(80)
#endif // GPIOPE_USE_INPUT_80

#ifdef GPIOPE_USE_INPUT_81
GPIOPE_DEFINE_INPUT(81)
#endif // GPIOPE_USE_INPUT_81

#ifdef GPIOPE_USE_INPUT_82
GPIOPE_DEFINE_INPUT(82)
#endif // GPIOPE_USE_INPUT_82

#ifdef GPIOPE_USE_INPUT_83
GPIOPE_DEFINE_INPUT(83)
#endif // GPIOPE_USE_INPUT_83

#ifdef GPIOPE_USE_INPUT_84
GPIOPE_DEFINE_INPUT(84)
#endif // GPIOPE_USE_INPUT_84

#ifdef GPIOPE_USE_INPUT_85
GPIOPE_DEFINE_INPUT(85)
#endif // GPIOPE_USE_INPUT_85

#ifdef GPIOPE_USE_INPUT_86
GPIOPE_DEFINE_INPUT(86)
#endif // GPIOPE_USE_INPUT_86

#ifdef GPIOPE_USE_INPUT_87
GPIOPE_DEFINE_INPUT(87)
#endif // GPIOPE_USE_INPUT_87

#ifdef GPIOPE_USE_INPUT_88
GPIOPE_DEFINE_INPUT(88)
#endif // GPIOPE_USE_INPUT_88

#ifdef GPIOPE_USE_INPUT_89
GPIOPE_DEFINE_INPUT(89)
#endif // GPIOPE_USE_INPUT_89

#ifdef GPIOPE_USE_INPUT_90
GPIOPE_DEFINE_INPUT(90)
#endif // GPIOPE_USE_INPUT_90

#ifdef GPIOPE_USE_INPUT_91
GPIOPE_DEFINE_INPUT(91)
#endif // GPIOPE_USE_INPUT_91

#ifdef GPIOPE_USE_INPUT_92
GPIOPE_DEFINE_INPUT(92)
#endif // GPIOPE_USE_INPUT_92

#ifdef GPIOPE_USE_INPUT_93
GPIOPE_DEFINE_INPUT(93)
#endif // GPIOPE_USE_INPUT_93

#ifdef GPIOPE_USE_INPUT_94
GPIOPE_DEFINE_INPUT(94)
#endif // GPIOPE_USE_INPUT_94

#ifdef GPIOPE_USE_INPUT_95
GPIOPE_DEFINE_INPUT(95)
#endif // GPIOPE_USE_INPUT_95

#ifdef GPIOPE_USE_INPUT_96
GPIOPE_DEFINE_INPUT(96)
#endif // GPIOPE_USE_INPUT_96

#ifdef GPIOPE_USE_INPUT_97
GPIOPE_DEFINE_INPUT(97)
#endif // GPIOPE_USE_INPUT_97

#ifdef GPIOPE_USE_INPUT_98
GPIOPE_DEFINE_INPUT(98)
#endif // GPIOPE_USE_INPUT_98

#ifdef GPIOPE_USE_INPUT_99
GPIOPE_DEFINE_INPUT(99)
#endif // GPIOPE_USE_INPUT_99

#ifdef GPIOPE_USE_INPUT_100
GPIOPE_DEFINE_INPUT(100)
#endif // GPIOPE_USE_INPUT_100

#ifdef GPIOPE_USE_INPUT_101
GPIOPE_DEFINE_INPUT(101)
#endif // GPIOPE_USE_INPUT_101

#ifdef GPIOPE_USE_INPUT_102
GPIOPE_DEFINE_INPUT(102)
#endif // GPIOPE_USE_INPUT_102

#ifdef GPIOPE_USE_INPUT_103
GPIOPE_DEFINE_INPUT(103)
#endif // GPIOPE_USE_INPUT_103

#ifdef GPIOPE_USE_INPUT_104
GPIOPE_DEFINE_INPUT(104)
#endif // GPIOPE_USE_INPUT_104

#ifdef GPIOPE_USE_INPUT_105
GPIOPE_DEFINE_INPUT(105)
#endif // GPIOPE_USE_INPUT_105

#ifdef GPIOPE_USE_INPUT_106
GPIOPE_DEFINE_INPUT(106)
#endif // GPIOPE_USE_INPUT_106

#ifdef GPIOPE_USE_INPUT_107
GPIOPE_DEFINE_INPUT(107)
#endif // GPIOPE_USE_INPUT_107

#ifdef GPIOPE_USE_INPUT_108
GPIOPE_DEFINE_INPUT(108)
#endif // GPIOPE_USE_INPUT_108

#ifdef GPIOPE_USE_INPUT_109
GPIOPE_DEFINE_INPUT(109)
#endif // GPIOPE_USE_INPUT_109

#ifdef GPIOPE_USE_INPUT_110
GPIOPE_DEFINE_INPUT(110)
#endif // GPIOPE_USE_INPUT_110

#ifdef GPIOPE_USE_INPUT_111
GPIOPE_DEFINE_INPUT(111)
#endif // GPIOPE_USE_INPUT_111

#ifdef GPIOPE_USE_INPUT_112
GPIOPE_DEFINE_INPUT(112)
#endif // GPIOPE_USE_INPUT_112

#ifdef GPIOPE_USE_INPUT_113
GPIOPE_DEFINE_INPUT(113)
#endif // GPIOPE_USE_INPUT_113

#ifdef GPIOPE_USE_INPUT_114
GPIOPE_DEFINE_INPUT(114)
#endif // GPIOPE_USE_INPUT_114

#ifdef GPIOPE_USE_INPUT_115
GPIOPE_DEFINE_INPUT(115)
#endif // GPIOPE_USE_INPUT_115

#ifdef GPIOPE_USE_INPUT_116
GPIOPE_DEFINE_INPUT(116)
#endif // GPIOPE_USE_INPUT_116

#ifdef GPIOPE_USE_INPUT_117
GPIOPE_DEFINE_INPUT(117)
#endif // GPIOPE_USE_INPUT_117

#ifdef GPIOPE_USE_INPUT_118
GPIOPE_DEFINE_INPUT(118)
#endif // GPIOPE_USE_INPUT_118

#ifdef GPIOPE_USE_INPUT_119
GPIOPE_DEFINE_INPUT(119)
#endif // GPIOPE_USE_INPUT_119

#ifdef GPIOPE_USE_INPUT_120
GPIOPE_DEFINE_INPUT(120)
#endif // GPIOPE_USE_INPUT_120

#ifdef GPIOPE_USE_INPUT_121
GPIOPE_DEFINE_INPUT(121)
#endif // GPIOPE_USE_INPUT_121

#ifdef GPIOPE_USE_INPUT_122
GPIOPE_DEFINE_INPUT(122)
#endif // GPIOPE_USE_INPUT_122

#ifdef GPIOPE_USE_INPUT_123
GPIOPE_DEFINE_INPUT(123)
#endif // GPIOPE_USE_INPUT_123

#ifdef GPIOPE_USE_INPUT_124
GPIOPE_DEFINE_INPUT(124)
#endif // GPIOPE_USE_INPUT_124

#ifdef GPIOPE_USE_INPUT_125
GPIOPE_DEFINE_INPUT(125)
#endif // GPIOPE_USE_INPUT_125

#ifdef GPIOPE_USE_INPUT_126
GPIOPE_DEFINE_INPUT(126)
#endif // GPIOPE_USE_INPUT_126

#ifdef GPIOPE_USE_INPUT_127
GPIOPE_DEFINE_INPUT(127)
#endif // GPIOPE_USE_INPUT_127

// ------------------------------------------------------------
// Output instance creation
// ------------------------------------------------------------
#ifdef GPIOPE_USE_OUTPUT_0
GPIOPE_DEFINE_OUTPUT(0)
#endif // GPIOPE_USE_OUTPUT_0

#ifdef GPIOPE_USE_OUTPUT_1
GPIOPE_DEFINE_OUTPUT(1)
#endif // GPIOPE_USE_OUTPUT_1

#ifdef GPIOPE_USE_OUTPUT_2
GPIOPE_DEFINE_OUTPUT(2)
#endif // GPIOPE_USE_OUTPUT_2

#ifdef GPIOPE_USE_OUTPUT_3
GPIOPE_DEFINE_OUTPUT(3)
#endif // GPIOPE_USE_OUTPUT_3

#ifdef GPIOPE_USE_OUTPUT_4
GPIOPE_DEFINE_OUTPUT(4)
#endif // GPIOPE_USE_OUTPUT_4

#ifdef GPIOPE_USE_OUTPUT_5
GPIOPE_DEFINE_OUTPUT(5)
#endif // GPIOPE_USE_OUTPUT_5

#ifdef GPIOPE_USE_OUTPUT_6
GPIOPE_DEFINE_OUTPUT(6)
#endif // GPIOPE_USE_OUTPUT_6

#ifdef GPIOPE_USE_OUTPUT_7
GPIOPE_DEFINE_OUTPUT(7)
#endif // GPIOPE_USE_OUTPUT_7

#ifdef GPIOPE_USE_OUTPUT_8
GPIOPE_DEFINE_OUTPUT(8)
#endif // GPIOPE_USE_OUTPUT_8

#ifdef GPIOPE_USE_OUTPUT_9
GPIOPE_DEFINE_OUTPUT(9)
#endif // GPIOPE_USE_OUTPUT_9

#ifdef GPIOPE_USE_OUTPUT_10
GPIOPE_DEFINE_OUTPUT(10)
#endif // GPIOPE_USE_OUTPUT_10

#ifdef GPIOPE_USE_OUTPUT_11
GPIOPE_DEFINE_OUTPUT(11)
#endif // GPIOPE_USE_OUTPUT_11

#ifdef GPIOPE_USE_OUTPUT_12
GPIOPE_DEFINE_OUTPUT(12)
#endif // GPIOPE_USE_OUTPUT_12

#ifdef GPIOPE_USE_OUTPUT_13
GPIOPE_DEFINE_OUTPUT(13)
#endif // GPIOPE_USE_OUTPUT_13

#ifdef GPIOPE_USE_OUTPUT_14
GPIOPE_DEFINE_OUTPUT(14)
#endif // GPIOPE_USE_OUTPUT_14

#ifdef GPIOPE_USE_OUTPUT_15
GPIOPE_DEFINE_OUTPUT(15)
#endif // GPIOPE_USE_OUTPUT_15

#ifdef GPIOPE_USE_OUTPUT_16
GPIOPE_DEFINE_OUTPUT(16)
#endif // GPIOPE_USE_OUTPUT_16

#ifdef GPIOPE_USE_OUTPUT_17
GPIOPE_DEFINE_OUTPUT(17)
#endif // GPIOPE_USE_OUTPUT_17

#ifdef GPIOPE_USE_OUTPUT_18
GPIOPE_DEFINE_OUTPUT(18)
#endif // GPIOPE_USE_OUTPUT_18

#ifdef GPIOPE_USE_OUTPUT_19
GPIOPE_DEFINE_OUTPUT(19)
#endif // GPIOPE_USE_OUTPUT_19

#ifdef GPIOPE_USE_OUTPUT_20
GPIOPE_DEFINE_OUTPUT(20)
#endif // GPIOPE_USE_OUTPUT_20

#ifdef GPIOPE_USE_OUTPUT_21
GPIOPE_DEFINE_OUTPUT(21)
#endif // GPIOPE_USE_OUTPUT_21

#ifdef GPIOPE_USE_OUTPUT_22
GPIOPE_DEFINE_OUTPUT(22)
#endif // GPIOPE_USE_OUTPUT_22

#ifdef GPIOPE_USE_OUTPUT_23
GPIOPE_DEFINE_OUTPUT(23)
#endif // GPIOPE_USE_OUTPUT_23

#ifdef GPIOPE_USE_OUTPUT_24
GPIOPE_DEFINE_OUTPUT(24)
#endif // GPIOPE_USE_OUTPUT_24

#ifdef GPIOPE_USE_OUTPUT_25
GPIOPE_DEFINE_OUTPUT(25)
#endif // GPIOPE_USE_OUTPUT_25

#ifdef GPIOPE_USE_OUTPUT_26
GPIOPE_DEFINE_OUTPUT(26)
#endif // GPIOPE_USE_OUTPUT_26

#ifdef GPIOPE_USE_OUTPUT_27
GPIOPE_DEFINE_OUTPUT(27)
#endif // GPIOPE_USE_OUTPUT_27

#ifdef GPIOPE_USE_OUTPUT_28
GPIOPE_DEFINE_OUTPUT(28)
#endif // GPIOPE_USE_OUTPUT_28

#ifdef GPIOPE_USE_OUTPUT_29
GPIOPE_DEFINE_OUTPUT(29)
#endif // GPIOPE_USE_OUTPUT_29

#ifdef GPIOPE_USE_OUTPUT_30
GPIOPE_DEFINE_OUTPUT(30)
#endif // GPIOPE_USE_OUTPUT_30

#ifdef GPIOPE_USE_OUTPUT_31
GPIOPE_DEFINE_OUTPUT(31)
#endif // GPIOPE_USE_OUTPUT_31

#ifdef GPIOPE_USE_OUTPUT_32
GPIOPE_DEFINE_OUTPUT(32)
#endif // GPIOPE_USE_OUTPUT_32

#ifdef GPIOPE_USE_OUTPUT_33
GPIOPE_DEFINE_OUTPUT(33)
#endif // GPIOPE_USE_OUTPUT_33

#ifdef GPIOPE_USE_OUTPUT_34
GPIOPE_DEFINE_OUTPUT(34)
#endif // GPIOPE_USE_OUTPUT_34

#ifdef GPIOPE_USE_OUTPUT_35
GPIOPE_DEFINE_OUTPUT(35)
#endif // GPIOPE_USE_OUTPUT_35

#ifdef GPIOPE_USE_OUTPUT_36
GPIOPE_DEFINE_OUTPUT(36)
#endif // GPIOPE_USE_OUTPUT_36

#ifdef GPIOPE_USE_OUTPUT_37
GPIOPE_DEFINE_OUTPUT(37)
#endif // GPIOPE_USE_OUTPUT_37

#ifdef GPIOPE_USE_OUTPUT_38
GPIOPE_DEFINE_OUTPUT(38)
#endif // GPIOPE_USE_OUTPUT_38

#ifdef GPIOPE_USE_OUTPUT_39
GPIOPE_DEFINE_OUTPUT(39)
#endif // GPIOPE_USE_OUTPUT_39

#ifdef GPIOPE_USE_OUTPUT_40
GPIOPE_DEFINE_OUTPUT(40)
#endif // GPIOPE_USE_OUTPUT_40

#ifdef GPIOPE_USE_OUTPUT_41
GPIOPE_DEFINE_OUTPUT(41)
#endif // GPIOPE_USE_OUTPUT_41

#ifdef GPIOPE_USE_OUTPUT_42
GPIOPE_DEFINE_OUTPUT(42)
#endif // GPIOPE_USE_OUTPUT_42

#ifdef GPIOPE_USE_OUTPUT_43
GPIOPE_DEFINE_OUTPUT(43)
#endif // GPIOPE_USE_OUTPUT_43

#ifdef GPIOPE_USE_OUTPUT_44
GPIOPE_DEFINE_OUTPUT(44)
#endif // GPIOPE_USE_OUTPUT_44

#ifdef GPIOPE_USE_OUTPUT_45
GPIOPE_DEFINE_OUTPUT(45)
#endif // GPIOPE_USE_OUTPUT_45

#ifdef GPIOPE_USE_OUTPUT_46
GPIOPE_DEFINE_OUTPUT(46)
#endif // GPIOPE_USE_OUTPUT_46

#ifdef GPIOPE_USE_OUTPUT_47
GPIOPE_DEFINE_OUTPUT(47)
#endif // GPIOPE_USE_OUTPUT_47

#ifdef GPIOPE_USE_OUTPUT_48
GPIOPE_DEFINE_OUTPUT(48)
#endif // GPIOPE_USE_OUTPUT_48

#ifdef GPIOPE_USE_OUTPUT_49
GPIOPE_DEFINE_OUTPUT(49)
#endif // GPIOPE_USE_OUTPUT_49

#ifdef GPIOPE_USE_OUTPUT_50
GPIOPE_DEFINE_OUTPUT(50)
#endif // GPIOPE_USE_OUTPUT_50

#ifdef GPIOPE_USE_OUTPUT_51
GPIOPE_DEFINE_OUTPUT(51)
#endif // GPIOPE_USE_OUTPUT_51

#ifdef GPIOPE_USE_OUTPUT_52
GPIOPE_DEFINE_OUTPUT(52)
#endif // GPIOPE_USE_OUTPUT_52

#ifdef GPIOPE_USE_OUTPUT_53
GPIOPE_DEFINE_OUTPUT(53)
#endif // GPIOPE_USE_OUTPUT_53

#ifdef GPIOPE_USE_OUTPUT_54
GPIOPE_DEFINE_OUTPUT(54)
#endif // GPIOPE_USE_OUTPUT_54

#ifdef GPIOPE_USE_OUTPUT_55
GPIOPE_DEFINE_OUTPUT(55)
#endif // GPIOPE_USE_OUTPUT_55

#ifdef GPIOPE_USE_OUTPUT_56
GPIOPE_DEFINE_OUTPUT(56)
#endif // GPIOPE_USE_OUTPUT_56

#ifdef GPIOPE_USE_OUTPUT_57
GPIOPE_DEFINE_OUTPUT(57)
#endif // GPIOPE_USE_OUTPUT_57

#ifdef GPIOPE_USE_OUTPUT_58
GPIOPE_DEFINE_OUTPUT(58)
#endif // GPIOPE_USE_OUTPUT_58

#ifdef GPIOPE_USE_OUTPUT_59
GPIOPE_DEFINE_OUTPUT(59)
#endif // GPIOPE_USE_OUTPUT_59

#ifdef GPIOPE_USE_OUTPUT_60
GPIOPE_DEFINE_OUTPUT(60)
#endif // GPIOPE_USE_OUTPUT_60

#ifdef GPIOPE_USE_OUTPUT_61
GPIOPE_DEFINE_OUTPUT(61)
#endif // GPIOPE_USE_OUTPUT_61

#ifdef GPIOPE_USE_OUTPUT_62
GPIOPE_DEFINE_OUTPUT(62)
#endif // GPIOPE_USE_OUTPUT_62

#ifdef GPIOPE_USE_OUTPUT_63
GPIOPE_DEFINE_OUTPUT(63)
#endif // GPIOPE_USE_OUTPUT_63

#ifdef GPIOPE_USE_OUTPUT_64
GPIOPE_DEFINE_OUTPUT(64)
#endif // GPIOPE_USE_OUTPUT_64

#ifdef GPIOPE_USE_OUTPUT_65
GPIOPE_DEFINE_OUTPUT(65)
#endif // GPIOPE_USE_OUTPUT_65

#ifdef GPIOPE_USE_OUTPUT_66
GPIOPE_DEFINE_OUTPUT(66)
#endif // GPIOPE_USE_OUTPUT_66

#ifdef GPIOPE_USE_OUTPUT_67
GPIOPE_DEFINE_OUTPUT(67)
#endif // GPIOPE_USE_OUTPUT_67

#ifdef GPIOPE_USE_OUTPUT_68
GPIOPE_DEFINE_OUTPUT(68)
#endif // GPIOPE_USE_OUTPUT_68

#ifdef GPIOPE_USE_OUTPUT_69
GPIOPE_DEFINE_OUTPUT(69)
#endif // GPIOPE_USE_OUTPUT_69

#ifdef GPIOPE_USE_OUTPUT_70
GPIOPE_DEFINE_OUTPUT(70)
#endif // GPIOPE_USE_OUTPUT_70

#ifdef GPIOPE_USE_OUTPUT_71
GPIOPE_DEFINE_OUTPUT(71)
#endif // GPIOPE_USE_OUTPUT_71

#ifdef GPIOPE_USE_OUTPUT_72
GPIOPE_DEFINE_OUTPUT(72)
#endif // GPIOPE_USE_OUTPUT_72

#ifdef GPIOPE_USE_OUTPUT_73
GPIOPE_DEFINE_OUTPUT(73)
#endif // GPIOPE_USE_OUTPUT_73

#ifdef GPIOPE_USE_OUTPUT_74
GPIOPE_DEFINE_OUTPUT(74)
#endif // GPIOPE_USE_OUTPUT_74

#ifdef GPIOPE_USE_OUTPUT_75
GPIOPE_DEFINE_OUTPUT(75)
#endif // GPIOPE_USE_OUTPUT_75

#ifdef GPIOPE_USE_OUTPUT_76
GPIOPE_DEFINE_OUTPUT(76)
#endif // GPIOPE_USE_OUTPUT_76

#ifdef GPIOPE_USE_OUTPUT_77
GPIOPE_DEFINE_OUTPUT(77)
#endif // GPIOPE_USE_OUTPUT_77

#ifdef GPIOPE_USE_OUTPUT_78
GPIOPE_DEFINE_OUTPUT(78)
#endif // GPIOPE_USE_OUTPUT_78

#ifdef GPIOPE_USE_OUTPUT_79
GPIOPE_DEFINE_OUTPUT(79)
#endif // GPIOPE_USE_OUTPUT_79

#ifdef GPIOPE_USE_OUTPUT_80
GPIOPE_DEFINE_OUTPUT(80)
#endif // GPIOPE_USE_OUTPUT_80

#ifdef GPIOPE_USE_OUTPUT_81
GPIOPE_DEFINE_OUTPUT(81)
#endif // GPIOPE_USE_OUTPUT_81

#ifdef GPIOPE_USE_OUTPUT_82
GPIOPE_DEFINE_OUTPUT(82)
#endif // GPIOPE_USE_OUTPUT_82

#ifdef GPIOPE_USE_OUTPUT_83
GPIOPE_DEFINE_OUTPUT(83)
#endif // GPIOPE_USE_OUTPUT_83

#ifdef GPIOPE_USE_OUTPUT_84
GPIOPE_DEFINE_OUTPUT(84)
#endif // GPIOPE_USE_OUTPUT_84

#ifdef GPIOPE_USE_OUTPUT_85
GPIOPE_DEFINE_OUTPUT(85)
#endif // GPIOPE_USE_OUTPUT_85

#ifdef GPIOPE_USE_OUTPUT_86
GPIOPE_DEFINE_OUTPUT(86)
#endif // GPIOPE_USE_OUTPUT_86

#ifdef GPIOPE_USE_OUTPUT_87
GPIOPE_DEFINE_OUTPUT(87)
#endif // GPIOPE_USE_OUTPUT_87

#ifdef GPIOPE_USE_OUTPUT_88
GPIOPE_DEFINE_OUTPUT(88)
#endif // GPIOPE_USE_OUTPUT_88

#ifdef GPIOPE_USE_OUTPUT_89
GPIOPE_DEFINE_OUTPUT(89)
#endif // GPIOPE_USE_OUTPUT_89

#ifdef GPIOPE_USE_OUTPUT_90
GPIOPE_DEFINE_OUTPUT(90)
#endif // GPIOPE_USE_OUTPUT_90

#ifdef GPIOPE_USE_OUTPUT_91
GPIOPE_DEFINE_OUTPUT(91)
#endif // GPIOPE_USE_OUTPUT_91

#ifdef GPIOPE_USE_OUTPUT_92
GPIOPE_DEFINE_OUTPUT(92)
#endif // GPIOPE_USE_OUTPUT_92

#ifdef GPIOPE_USE_OUTPUT_93
GPIOPE_DEFINE_OUTPUT(93)
#endif // GPIOPE_USE_OUTPUT_93

#ifdef GPIOPE_USE_OUTPUT_94
GPIOPE_DEFINE_OUTPUT(94)
#endif // GPIOPE_USE_OUTPUT_94

#ifdef GPIOPE_USE_OUTPUT_95
GPIOPE_DEFINE_OUTPUT(95)
#endif // GPIOPE_USE_OUTPUT_95

#ifdef GPIOPE_USE_OUTPUT_96
GPIOPE_DEFINE_OUTPUT(96)
#endif // GPIOPE_USE_OUTPUT_96

#ifdef GPIOPE_USE_OUTPUT_97
GPIOPE_DEFINE_OUTPUT(97)
#endif // GPIOPE_USE_OUTPUT_97

#ifdef GPIOPE_USE_OUTPUT_98
GPIOPE_DEFINE_OUTPUT(98)
#endif // GPIOPE_USE_OUTPUT_98

#ifdef GPIOPE_USE_OUTPUT_99
GPIOPE_DEFINE_OUTPUT(99)
#endif // GPIOPE_USE_OUTPUT_99

#ifdef GPIOPE_USE_OUTPUT_100
GPIOPE_DEFINE_OUTPUT(100)
#endif // GPIOPE_USE_OUTPUT_100

#ifdef GPIOPE_USE_OUTPUT_101
GPIOPE_DEFINE_OUTPUT(101)
#endif // GPIOPE_USE_OUTPUT_101

#ifdef GPIOPE_USE_OUTPUT_102
GPIOPE_DEFINE_OUTPUT(102)
#endif // GPIOPE_USE_OUTPUT_102

#ifdef GPIOPE_USE_OUTPUT_103
GPIOPE_DEFINE_OUTPUT(103)
#endif // GPIOPE_USE_OUTPUT_103

#ifdef GPIOPE_USE_OUTPUT_104
GPIOPE_DEFINE_OUTPUT(104)
#endif // GPIOPE_USE_OUTPUT_104

#ifdef GPIOPE_USE_OUTPUT_105
GPIOPE_DEFINE_OUTPUT(105)
#endif // GPIOPE_USE_OUTPUT_105

#ifdef GPIOPE_USE_OUTPUT_106
GPIOPE_DEFINE_OUTPUT(106)
#endif // GPIOPE_USE_OUTPUT_106

#ifdef GPIOPE_USE_OUTPUT_107
GPIOPE_DEFINE_OUTPUT(107)
#endif // GPIOPE_USE_OUTPUT_107

#ifdef GPIOPE_USE_OUTPUT_108
GPIOPE_DEFINE_OUTPUT(108)
#endif // GPIOPE_USE_OUTPUT_108

#ifdef GPIOPE_USE_OUTPUT_109
GPIOPE_DEFINE_OUTPUT(109)
#endif // GPIOPE_USE_OUTPUT_109

#ifdef GPIOPE_USE_OUTPUT_110
GPIOPE_DEFINE_OUTPUT(110)
#endif // GPIOPE_USE_OUTPUT_110

#ifdef GPIOPE_USE_OUTPUT_111
GPIOPE_DEFINE_OUTPUT(111)
#endif // GPIOPE_USE_OUTPUT_111

#ifdef GPIOPE_USE_OUTPUT_112
GPIOPE_DEFINE_OUTPUT(112)
#endif // GPIOPE_USE_OUTPUT_112

#ifdef GPIOPE_USE_OUTPUT_113
GPIOPE_DEFINE_OUTPUT(113)
#endif // GPIOPE_USE_OUTPUT_113

#ifdef GPIOPE_USE_OUTPUT_114
GPIOPE_DEFINE_OUTPUT(114)
#endif // GPIOPE_USE_OUTPUT_114

#ifdef GPIOPE_USE_OUTPUT_115
GPIOPE_DEFINE_OUTPUT(115)
#endif // GPIOPE_USE_OUTPUT_115

#ifdef GPIOPE_USE_OUTPUT_116
GPIOPE_DEFINE_OUTPUT(116)
#endif // GPIOPE_USE_OUTPUT_116

#ifdef GPIOPE_USE_OUTPUT_117
GPIOPE_DEFINE_OUTPUT(117)
#endif // GPIOPE_USE_OUTPUT_117

#ifdef GPIOPE_USE_OUTPUT_118
GPIOPE_DEFINE_OUTPUT(118)
#endif // GPIOPE_USE_OUTPUT_118

#ifdef GPIOPE_USE_OUTPUT_119
GPIOPE_DEFINE_OUTPUT(119)
#endif // GPIOPE_USE_OUTPUT_119

#ifdef GPIOPE_USE_OUTPUT_120
GPIOPE_DEFINE_OUTPUT(120)
#endif // GPIOPE_USE_OUTPUT_120

#ifdef GPIOPE_USE_OUTPUT_121
GPIOPE_DEFINE_OUTPUT(121)
#endif // GPIOPE_USE_OUTPUT_121

#ifdef GPIOPE_USE_OUTPUT_122
GPIOPE_DEFINE_OUTPUT(122)
#endif // GPIOPE_USE_OUTPUT_122

#ifdef GPIOPE_USE_OUTPUT_123
GPIOPE_DEFINE_OUTPUT(123)
#endif // GPIOPE_USE_OUTPUT_123

#ifdef GPIOPE_USE_OUTPUT_124
GPIOPE_DEFINE_OUTPUT(124)
#endif // GPIOPE_USE_OUTPUT_124

#ifdef GPIOPE_USE_OUTPUT_125
GPIOPE_DEFINE_OUTPUT(125)
#endif // GPIOPE_USE_OUTPUT_125

#ifdef GPIOPE_USE_OUTPUT_126
GPIOPE_DEFINE_OUTPUT(126)
#endif // GPIOPE_USE_OUTPUT_126

#ifdef GPIOPE_USE_OUTPUT_127
GPIOPE_DEFINE_OUTPUT(127)
#endif // GPIOPE_USE_OUTPUT_127

void GPIOPE_Setup_Devices() {

    #ifdef GPIOPE_USE_OUTPUT_0
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_0);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_0);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_0, I2C_ADDR_0);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_1
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_1);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_1);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_1, I2C_ADDR_1);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_2
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_2);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_2);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_2, I2C_ADDR_2);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_3
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_3);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_3);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_3, I2C_ADDR_3);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_4
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_4);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_4);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_4, I2C_ADDR_4);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_5
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_5);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_5);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_5, I2C_ADDR_5);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_6
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_6);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_6);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_6, I2C_ADDR_6);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_7
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_7);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_7);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_7, I2C_ADDR_7);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_8
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_8);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_8);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_8, I2C_ADDR_8);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_9
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_9);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_9);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_9, I2C_ADDR_9);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_10
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_10);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_10);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_10, I2C_ADDR_10);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_11
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_11);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_11);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_11, I2C_ADDR_11);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_12
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_12);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_12);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_12, I2C_ADDR_12);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_13
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_13);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_13);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_13, I2C_ADDR_13);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_14
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_14);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_14);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_14, I2C_ADDR_14);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_15
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_15);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_15);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_15, I2C_ADDR_15);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_16
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_16);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_16);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_16, I2C_ADDR_16);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_17
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_17);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_17);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_17, I2C_ADDR_17);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_18
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_18);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_18);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_18, I2C_ADDR_18);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_19
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_19);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_19);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_19, I2C_ADDR_19);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_20
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_20);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_20);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_20, I2C_ADDR_20);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_21
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_21);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_21);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_21, I2C_ADDR_21);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_22
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_22);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_22);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_22, I2C_ADDR_22);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_23
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_23);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_23);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_23, I2C_ADDR_23);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_24
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_24);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_24);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_24, I2C_ADDR_24);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_25
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_25);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_25);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_25, I2C_ADDR_25);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_26
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_26);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_26);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_26, I2C_ADDR_26);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_27
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_27);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_27);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_27, I2C_ADDR_27);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_28
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_28);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_28);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_28, I2C_ADDR_28);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_29
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_29);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_29);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_29, I2C_ADDR_29);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_30
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_30);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_30);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_30, I2C_ADDR_30);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_31
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_31);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_31);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_31, I2C_ADDR_31);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_32
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_32);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_32);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_32, I2C_ADDR_32);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_33
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_33);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_33);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_33, I2C_ADDR_33);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_34
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_34);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_34);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_34, I2C_ADDR_34);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_35
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_35);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_35);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_35, I2C_ADDR_35);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_36
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_36);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_36);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_36, I2C_ADDR_36);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_37
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_37);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_37);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_37, I2C_ADDR_37);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_38
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_38);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_38);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_38, I2C_ADDR_38);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_39
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_39);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_39);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_39, I2C_ADDR_39);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_40
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_40);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_40);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_40, I2C_ADDR_40);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_41
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_41);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_41);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_41, I2C_ADDR_41);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_42
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_42);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_42);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_42, I2C_ADDR_42);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_43
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_43);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_43);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_43, I2C_ADDR_43);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_44
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_44);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_44);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_44, I2C_ADDR_44);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_45
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_45);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_45);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_45, I2C_ADDR_45);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_46
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_46);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_46);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_46, I2C_ADDR_46);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_47
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_47);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_47);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_47, I2C_ADDR_47);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_48
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_48);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_48);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_48, I2C_ADDR_48);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_49
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_49);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_49);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_49, I2C_ADDR_49);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_50
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_50);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_50);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_50, I2C_ADDR_50);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_51
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_51);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_51);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_51, I2C_ADDR_51);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_52
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_52);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_52);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_52, I2C_ADDR_52);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_53
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_53);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_53);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_53, I2C_ADDR_53);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_54
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_54);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_54);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_54, I2C_ADDR_54);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_55
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_55);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_55);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_55, I2C_ADDR_55);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_56
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_56);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_56);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_56, I2C_ADDR_56);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_57
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_57);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_57);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_57, I2C_ADDR_57);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_58
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_58);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_58);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_58, I2C_ADDR_58);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_59
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_59);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_59);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_59, I2C_ADDR_59);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_60
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_60);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_60);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_60, I2C_ADDR_60);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_61
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_61);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_61);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_61, I2C_ADDR_61);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_62
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_62);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_62);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_62, I2C_ADDR_62);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_63
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_63);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_63);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_63, I2C_ADDR_63);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_64
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_64);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_64);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_64, I2C_ADDR_64);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_65
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_65);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_65);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_65, I2C_ADDR_65);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_66
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_66);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_66);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_66, I2C_ADDR_66);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_67
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_67);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_67);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_67, I2C_ADDR_67);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_68
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_68);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_68);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_68, I2C_ADDR_68);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_69
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_69);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_69);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_69, I2C_ADDR_69);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_70
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_70);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_70);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_70, I2C_ADDR_70);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_71
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_71);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_71);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_71, I2C_ADDR_71);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_72
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_72);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_72);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_72, I2C_ADDR_72);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_73
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_73);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_73);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_73, I2C_ADDR_73);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_74
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_74);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_74);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_74, I2C_ADDR_74);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_75
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_75);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_75);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_75, I2C_ADDR_75);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_76
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_76);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_76);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_76, I2C_ADDR_76);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_77
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_77);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_77);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_77, I2C_ADDR_77);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_78
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_78);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_78);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_78, I2C_ADDR_78);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_79
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_79);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_79);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_79, I2C_ADDR_79);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_80
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_80);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_80);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_80, I2C_ADDR_80);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_81
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_81);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_81);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_81, I2C_ADDR_81);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_82
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_82);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_82);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_82, I2C_ADDR_82);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_83
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_83);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_83);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_83, I2C_ADDR_83);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_84
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_84);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_84);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_84, I2C_ADDR_84);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_85
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_85);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_85);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_85, I2C_ADDR_85);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_86
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_86);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_86);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_86, I2C_ADDR_86);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_87
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_87);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_87);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_87, I2C_ADDR_87);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_88
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_88);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_88);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_88, I2C_ADDR_88);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_89
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_89);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_89);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_89, I2C_ADDR_89);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_90
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_90);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_90);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_90, I2C_ADDR_90);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_91
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_91);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_91);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_91, I2C_ADDR_91);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_92
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_92);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_92);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_92, I2C_ADDR_92);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_93
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_93);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_93);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_93, I2C_ADDR_93);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_94
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_94);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_94);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_94, I2C_ADDR_94);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_95
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_95);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_95);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_95, I2C_ADDR_95);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_96
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_96);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_96);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_96, I2C_ADDR_96);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_97
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_97);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_97);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_97, I2C_ADDR_97);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_98
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_98);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_98);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_98, I2C_ADDR_98);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_99
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_99);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_99);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_99, I2C_ADDR_99);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_100
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_100);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_100);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_100, I2C_ADDR_100);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_101
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_101);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_101);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_101, I2C_ADDR_101);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_102
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_102);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_102);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_102, I2C_ADDR_102);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_103
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_103);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_103);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_103, I2C_ADDR_103);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_104
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_104);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_104);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_104, I2C_ADDR_104);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_105
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_105);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_105);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_105, I2C_ADDR_105);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_106
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_106);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_106);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_106, I2C_ADDR_106);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_107
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_107);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_107);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_107, I2C_ADDR_107);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_108
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_108);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_108);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_108, I2C_ADDR_108);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_109
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_109);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_109);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_109, I2C_ADDR_109);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_110
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_110);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_110);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_110, I2C_ADDR_110);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_111
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_111);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_111);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_111, I2C_ADDR_111);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_112
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_112);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_112);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_112, I2C_ADDR_112);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_113
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_113);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_113);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_113, I2C_ADDR_113);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_114
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_114);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_114);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_114, I2C_ADDR_114);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_115
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_115);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_115);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_115, I2C_ADDR_115);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_116
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_116);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_116);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_116, I2C_ADDR_116);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_117
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_117);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_117);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_117, I2C_ADDR_117);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_118
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_118);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_118);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_118, I2C_ADDR_118);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_119
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_119);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_119);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_119, I2C_ADDR_119);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_120
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_120);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_120);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_120, I2C_ADDR_120);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_121
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_121);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_121);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_121, I2C_ADDR_121);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_122
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_122);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_122);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_122, I2C_ADDR_122);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_123
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_123);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_123);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_123, I2C_ADDR_123);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_124
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_124);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_124);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_124, I2C_ADDR_124);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_125
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_125);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_125);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_125, I2C_ADDR_125);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_126
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_126);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_126);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_126, I2C_ADDR_126);
    #endif

    #ifdef GPIOPE_USE_OUTPUT_127
    GPIOPE_Set_All_Portmap_Index_Pin(GPIOPE_OUTPUT_127);
    GPIOPE_Set_All_Portmap_Index_PWM(GPIOPE_OUTPUT_127);
    GPIOPE_QueryDevice(GPIOPE_OUTPUT_127, I2C_ADDR_127);
    #endif

    #ifdef GPIOPE_USE_INPUT_0
    GPIOPE_QueryDevice(GPIOPE_INPUT_0, I2C_ADDR_0);
    #endif

    #ifdef GPIOPE_USE_INPUT_1
    GPIOPE_QueryDevice(GPIOPE_INPUT_1, I2C_ADDR_1);
    #endif

    #ifdef GPIOPE_USE_INPUT_2
    GPIOPE_QueryDevice(GPIOPE_INPUT_2, I2C_ADDR_2);
    #endif

    #ifdef GPIOPE_USE_INPUT_3
    GPIOPE_QueryDevice(GPIOPE_INPUT_3, I2C_ADDR_3);
    #endif

    #ifdef GPIOPE_USE_INPUT_4
    GPIOPE_QueryDevice(GPIOPE_INPUT_4, I2C_ADDR_4);
    #endif

    #ifdef GPIOPE_USE_INPUT_5
    GPIOPE_QueryDevice(GPIOPE_INPUT_5, I2C_ADDR_5);
    #endif

    #ifdef GPIOPE_USE_INPUT_6
    GPIOPE_QueryDevice(GPIOPE_INPUT_6, I2C_ADDR_6);
    #endif

    #ifdef GPIOPE_USE_INPUT_7
    GPIOPE_QueryDevice(GPIOPE_INPUT_7, I2C_ADDR_7);
    #endif

    #ifdef GPIOPE_USE_INPUT_8
    GPIOPE_QueryDevice(GPIOPE_INPUT_8, I2C_ADDR_8);
    #endif

    #ifdef GPIOPE_USE_INPUT_9
    GPIOPE_QueryDevice(GPIOPE_INPUT_9, I2C_ADDR_9);
    #endif

    #ifdef GPIOPE_USE_INPUT_10
    GPIOPE_QueryDevice(GPIOPE_INPUT_10, I2C_ADDR_10);
    #endif

    #ifdef GPIOPE_USE_INPUT_11
    GPIOPE_QueryDevice(GPIOPE_INPUT_11, I2C_ADDR_11);
    #endif

    #ifdef GPIOPE_USE_INPUT_12
    GPIOPE_QueryDevice(GPIOPE_INPUT_12, I2C_ADDR_12);
    #endif

    #ifdef GPIOPE_USE_INPUT_13
    GPIOPE_QueryDevice(GPIOPE_INPUT_13, I2C_ADDR_13);
    #endif

    #ifdef GPIOPE_USE_INPUT_14
    GPIOPE_QueryDevice(GPIOPE_INPUT_14, I2C_ADDR_14);
    #endif

    #ifdef GPIOPE_USE_INPUT_15
    GPIOPE_QueryDevice(GPIOPE_INPUT_15, I2C_ADDR_15);
    #endif

    #ifdef GPIOPE_USE_INPUT_16
    GPIOPE_QueryDevice(GPIOPE_INPUT_16, I2C_ADDR_16);
    #endif

    #ifdef GPIOPE_USE_INPUT_17
    GPIOPE_QueryDevice(GPIOPE_INPUT_17, I2C_ADDR_17);
    #endif

    #ifdef GPIOPE_USE_INPUT_18
    GPIOPE_QueryDevice(GPIOPE_INPUT_18, I2C_ADDR_18);
    #endif

    #ifdef GPIOPE_USE_INPUT_19
    GPIOPE_QueryDevice(GPIOPE_INPUT_19, I2C_ADDR_19);
    #endif

    #ifdef GPIOPE_USE_INPUT_20
    GPIOPE_QueryDevice(GPIOPE_INPUT_20, I2C_ADDR_20);
    #endif

    #ifdef GPIOPE_USE_INPUT_21
    GPIOPE_QueryDevice(GPIOPE_INPUT_21, I2C_ADDR_21);
    #endif

    #ifdef GPIOPE_USE_INPUT_22
    GPIOPE_QueryDevice(GPIOPE_INPUT_22, I2C_ADDR_22);
    #endif

    #ifdef GPIOPE_USE_INPUT_23
    GPIOPE_QueryDevice(GPIOPE_INPUT_23, I2C_ADDR_23);
    #endif

    #ifdef GPIOPE_USE_INPUT_24
    GPIOPE_QueryDevice(GPIOPE_INPUT_24, I2C_ADDR_24);
    #endif

    #ifdef GPIOPE_USE_INPUT_25
    GPIOPE_QueryDevice(GPIOPE_INPUT_25, I2C_ADDR_25);
    #endif

    #ifdef GPIOPE_USE_INPUT_26
    GPIOPE_QueryDevice(GPIOPE_INPUT_26, I2C_ADDR_26);
    #endif

    #ifdef GPIOPE_USE_INPUT_27
    GPIOPE_QueryDevice(GPIOPE_INPUT_27, I2C_ADDR_27);
    #endif

    #ifdef GPIOPE_USE_INPUT_28
    GPIOPE_QueryDevice(GPIOPE_INPUT_28, I2C_ADDR_28);
    #endif

    #ifdef GPIOPE_USE_INPUT_29
    GPIOPE_QueryDevice(GPIOPE_INPUT_29, I2C_ADDR_29);
    #endif

    #ifdef GPIOPE_USE_INPUT_30
    GPIOPE_QueryDevice(GPIOPE_INPUT_30, I2C_ADDR_30);
    #endif

    #ifdef GPIOPE_USE_INPUT_31
    GPIOPE_QueryDevice(GPIOPE_INPUT_31, I2C_ADDR_31);
    #endif

    #ifdef GPIOPE_USE_INPUT_32
    GPIOPE_QueryDevice(GPIOPE_INPUT_32, I2C_ADDR_32);
    #endif

    #ifdef GPIOPE_USE_INPUT_33
    GPIOPE_QueryDevice(GPIOPE_INPUT_33, I2C_ADDR_33);
    #endif

    #ifdef GPIOPE_USE_INPUT_34
    GPIOPE_QueryDevice(GPIOPE_INPUT_34, I2C_ADDR_34);
    #endif

    #ifdef GPIOPE_USE_INPUT_35
    GPIOPE_QueryDevice(GPIOPE_INPUT_35, I2C_ADDR_35);
    #endif

    #ifdef GPIOPE_USE_INPUT_36
    GPIOPE_QueryDevice(GPIOPE_INPUT_36, I2C_ADDR_36);
    #endif

    #ifdef GPIOPE_USE_INPUT_37
    GPIOPE_QueryDevice(GPIOPE_INPUT_37, I2C_ADDR_37);
    #endif

    #ifdef GPIOPE_USE_INPUT_38
    GPIOPE_QueryDevice(GPIOPE_INPUT_38, I2C_ADDR_38);
    #endif

    #ifdef GPIOPE_USE_INPUT_39
    GPIOPE_QueryDevice(GPIOPE_INPUT_39, I2C_ADDR_39);
    #endif

    #ifdef GPIOPE_USE_INPUT_40
    GPIOPE_QueryDevice(GPIOPE_INPUT_40, I2C_ADDR_40);
    #endif

    #ifdef GPIOPE_USE_INPUT_41
    GPIOPE_QueryDevice(GPIOPE_INPUT_41, I2C_ADDR_41);
    #endif

    #ifdef GPIOPE_USE_INPUT_42
    GPIOPE_QueryDevice(GPIOPE_INPUT_42, I2C_ADDR_42);
    #endif

    #ifdef GPIOPE_USE_INPUT_43
    GPIOPE_QueryDevice(GPIOPE_INPUT_43, I2C_ADDR_43);
    #endif

    #ifdef GPIOPE_USE_INPUT_44
    GPIOPE_QueryDevice(GPIOPE_INPUT_44, I2C_ADDR_44);
    #endif

    #ifdef GPIOPE_USE_INPUT_45
    GPIOPE_QueryDevice(GPIOPE_INPUT_45, I2C_ADDR_45);
    #endif

    #ifdef GPIOPE_USE_INPUT_46
    GPIOPE_QueryDevice(GPIOPE_INPUT_46, I2C_ADDR_46);
    #endif

    #ifdef GPIOPE_USE_INPUT_47
    GPIOPE_QueryDevice(GPIOPE_INPUT_47, I2C_ADDR_47);
    #endif

    #ifdef GPIOPE_USE_INPUT_48
    GPIOPE_QueryDevice(GPIOPE_INPUT_48, I2C_ADDR_48);
    #endif

    #ifdef GPIOPE_USE_INPUT_49
    GPIOPE_QueryDevice(GPIOPE_INPUT_49, I2C_ADDR_49);
    #endif

    #ifdef GPIOPE_USE_INPUT_50
    GPIOPE_QueryDevice(GPIOPE_INPUT_50, I2C_ADDR_50);
    #endif

    #ifdef GPIOPE_USE_INPUT_51
    GPIOPE_QueryDevice(GPIOPE_INPUT_51, I2C_ADDR_51);
    #endif

    #ifdef GPIOPE_USE_INPUT_52
    GPIOPE_QueryDevice(GPIOPE_INPUT_52, I2C_ADDR_52);
    #endif

    #ifdef GPIOPE_USE_INPUT_53
    GPIOPE_QueryDevice(GPIOPE_INPUT_53, I2C_ADDR_53);
    #endif

    #ifdef GPIOPE_USE_INPUT_54
    GPIOPE_QueryDevice(GPIOPE_INPUT_54, I2C_ADDR_54);
    #endif

    #ifdef GPIOPE_USE_INPUT_55
    GPIOPE_QueryDevice(GPIOPE_INPUT_55, I2C_ADDR_55);
    #endif

    #ifdef GPIOPE_USE_INPUT_56
    GPIOPE_QueryDevice(GPIOPE_INPUT_56, I2C_ADDR_56);
    #endif

    #ifdef GPIOPE_USE_INPUT_57
    GPIOPE_QueryDevice(GPIOPE_INPUT_57, I2C_ADDR_57);
    #endif

    #ifdef GPIOPE_USE_INPUT_58
    GPIOPE_QueryDevice(GPIOPE_INPUT_58, I2C_ADDR_58);
    #endif

    #ifdef GPIOPE_USE_INPUT_59
    GPIOPE_QueryDevice(GPIOPE_INPUT_59, I2C_ADDR_59);
    #endif

    #ifdef GPIOPE_USE_INPUT_60
    GPIOPE_QueryDevice(GPIOPE_INPUT_60, I2C_ADDR_60);
    #endif

    #ifdef GPIOPE_USE_INPUT_61
    GPIOPE_QueryDevice(GPIOPE_INPUT_61, I2C_ADDR_61);
    #endif

    #ifdef GPIOPE_USE_INPUT_62
    GPIOPE_QueryDevice(GPIOPE_INPUT_62, I2C_ADDR_62);
    #endif

    #ifdef GPIOPE_USE_INPUT_63
    GPIOPE_QueryDevice(GPIOPE_INPUT_63, I2C_ADDR_63);
    #endif

    #ifdef GPIOPE_USE_INPUT_64
    GPIOPE_QueryDevice(GPIOPE_INPUT_64, I2C_ADDR_64);
    #endif

    #ifdef GPIOPE_USE_INPUT_65
    GPIOPE_QueryDevice(GPIOPE_INPUT_65, I2C_ADDR_65);
    #endif

    #ifdef GPIOPE_USE_INPUT_66
    GPIOPE_QueryDevice(GPIOPE_INPUT_66, I2C_ADDR_66);
    #endif

    #ifdef GPIOPE_USE_INPUT_67
    GPIOPE_QueryDevice(GPIOPE_INPUT_67, I2C_ADDR_67);
    #endif

    #ifdef GPIOPE_USE_INPUT_68
    GPIOPE_QueryDevice(GPIOPE_INPUT_68, I2C_ADDR_68);
    #endif

    #ifdef GPIOPE_USE_INPUT_69
    GPIOPE_QueryDevice(GPIOPE_INPUT_69, I2C_ADDR_69);
    #endif

    #ifdef GPIOPE_USE_INPUT_70
    GPIOPE_QueryDevice(GPIOPE_INPUT_70, I2C_ADDR_70);
    #endif

    #ifdef GPIOPE_USE_INPUT_71
    GPIOPE_QueryDevice(GPIOPE_INPUT_71, I2C_ADDR_71);
    #endif

    #ifdef GPIOPE_USE_INPUT_72
    GPIOPE_QueryDevice(GPIOPE_INPUT_72, I2C_ADDR_72);
    #endif

    #ifdef GPIOPE_USE_INPUT_73
    GPIOPE_QueryDevice(GPIOPE_INPUT_73, I2C_ADDR_73);
    #endif

    #ifdef GPIOPE_USE_INPUT_74
    GPIOPE_QueryDevice(GPIOPE_INPUT_74, I2C_ADDR_74);
    #endif

    #ifdef GPIOPE_USE_INPUT_75
    GPIOPE_QueryDevice(GPIOPE_INPUT_75, I2C_ADDR_75);
    #endif

    #ifdef GPIOPE_USE_INPUT_76
    GPIOPE_QueryDevice(GPIOPE_INPUT_76, I2C_ADDR_76);
    #endif

    #ifdef GPIOPE_USE_INPUT_77
    GPIOPE_QueryDevice(GPIOPE_INPUT_77, I2C_ADDR_77);
    #endif

    #ifdef GPIOPE_USE_INPUT_78
    GPIOPE_QueryDevice(GPIOPE_INPUT_78, I2C_ADDR_78);
    #endif

    #ifdef GPIOPE_USE_INPUT_79
    GPIOPE_QueryDevice(GPIOPE_INPUT_79, I2C_ADDR_79);
    #endif

    #ifdef GPIOPE_USE_INPUT_80
    GPIOPE_QueryDevice(GPIOPE_INPUT_80, I2C_ADDR_80);
    #endif

    #ifdef GPIOPE_USE_INPUT_81
    GPIOPE_QueryDevice(GPIOPE_INPUT_81, I2C_ADDR_81);
    #endif

    #ifdef GPIOPE_USE_INPUT_82
    GPIOPE_QueryDevice(GPIOPE_INPUT_82, I2C_ADDR_82);
    #endif

    #ifdef GPIOPE_USE_INPUT_83
    GPIOPE_QueryDevice(GPIOPE_INPUT_83, I2C_ADDR_83);
    #endif

    #ifdef GPIOPE_USE_INPUT_84
    GPIOPE_QueryDevice(GPIOPE_INPUT_84, I2C_ADDR_84);
    #endif

    #ifdef GPIOPE_USE_INPUT_85
    GPIOPE_QueryDevice(GPIOPE_INPUT_85, I2C_ADDR_85);
    #endif

    #ifdef GPIOPE_USE_INPUT_86
    GPIOPE_QueryDevice(GPIOPE_INPUT_86, I2C_ADDR_86);
    #endif

    #ifdef GPIOPE_USE_INPUT_87
    GPIOPE_QueryDevice(GPIOPE_INPUT_87, I2C_ADDR_87);
    #endif

    #ifdef GPIOPE_USE_INPUT_88
    GPIOPE_QueryDevice(GPIOPE_INPUT_88, I2C_ADDR_88);
    #endif

    #ifdef GPIOPE_USE_INPUT_89
    GPIOPE_QueryDevice(GPIOPE_INPUT_89, I2C_ADDR_89);
    #endif

    #ifdef GPIOPE_USE_INPUT_90
    GPIOPE_QueryDevice(GPIOPE_INPUT_90, I2C_ADDR_90);
    #endif

    #ifdef GPIOPE_USE_INPUT_91
    GPIOPE_QueryDevice(GPIOPE_INPUT_91, I2C_ADDR_91);
    #endif

    #ifdef GPIOPE_USE_INPUT_92
    GPIOPE_QueryDevice(GPIOPE_INPUT_92, I2C_ADDR_92);
    #endif

    #ifdef GPIOPE_USE_INPUT_93
    GPIOPE_QueryDevice(GPIOPE_INPUT_93, I2C_ADDR_93);
    #endif

    #ifdef GPIOPE_USE_INPUT_94
    GPIOPE_QueryDevice(GPIOPE_INPUT_94, I2C_ADDR_94);
    #endif

    #ifdef GPIOPE_USE_INPUT_95
    GPIOPE_QueryDevice(GPIOPE_INPUT_95, I2C_ADDR_95);
    #endif

    #ifdef GPIOPE_USE_INPUT_96
    GPIOPE_QueryDevice(GPIOPE_INPUT_96, I2C_ADDR_96);
    #endif

    #ifdef GPIOPE_USE_INPUT_97
    GPIOPE_QueryDevice(GPIOPE_INPUT_97, I2C_ADDR_97);
    #endif

    #ifdef GPIOPE_USE_INPUT_98
    GPIOPE_QueryDevice(GPIOPE_INPUT_98, I2C_ADDR_98);
    #endif

    #ifdef GPIOPE_USE_INPUT_99
    GPIOPE_QueryDevice(GPIOPE_INPUT_99, I2C_ADDR_99);
    #endif

    #ifdef GPIOPE_USE_INPUT_100
    GPIOPE_QueryDevice(GPIOPE_INPUT_100, I2C_ADDR_100);
    #endif

    #ifdef GPIOPE_USE_INPUT_101
    GPIOPE_QueryDevice(GPIOPE_INPUT_101, I2C_ADDR_101);
    #endif

    #ifdef GPIOPE_USE_INPUT_102
    GPIOPE_QueryDevice(GPIOPE_INPUT_102, I2C_ADDR_102);
    #endif

    #ifdef GPIOPE_USE_INPUT_103
    GPIOPE_QueryDevice(GPIOPE_INPUT_103, I2C_ADDR_103);
    #endif

    #ifdef GPIOPE_USE_INPUT_104
    GPIOPE_QueryDevice(GPIOPE_INPUT_104, I2C_ADDR_104);
    #endif

    #ifdef GPIOPE_USE_INPUT_105
    GPIOPE_QueryDevice(GPIOPE_INPUT_105, I2C_ADDR_105);
    #endif

    #ifdef GPIOPE_USE_INPUT_106
    GPIOPE_QueryDevice(GPIOPE_INPUT_106, I2C_ADDR_106);
    #endif

    #ifdef GPIOPE_USE_INPUT_107
    GPIOPE_QueryDevice(GPIOPE_INPUT_107, I2C_ADDR_107);
    #endif

    #ifdef GPIOPE_USE_INPUT_108
    GPIOPE_QueryDevice(GPIOPE_INPUT_108, I2C_ADDR_108);
    #endif

    #ifdef GPIOPE_USE_INPUT_109
    GPIOPE_QueryDevice(GPIOPE_INPUT_109, I2C_ADDR_109);
    #endif

    #ifdef GPIOPE_USE_INPUT_110
    GPIOPE_QueryDevice(GPIOPE_INPUT_110, I2C_ADDR_110);
    #endif

    #ifdef GPIOPE_USE_INPUT_111
    GPIOPE_QueryDevice(GPIOPE_INPUT_111, I2C_ADDR_111);
    #endif

    #ifdef GPIOPE_USE_INPUT_112
    GPIOPE_QueryDevice(GPIOPE_INPUT_112, I2C_ADDR_112);
    #endif

    #ifdef GPIOPE_USE_INPUT_113
    GPIOPE_QueryDevice(GPIOPE_INPUT_113, I2C_ADDR_113);
    #endif

    #ifdef GPIOPE_USE_INPUT_114
    GPIOPE_QueryDevice(GPIOPE_INPUT_114, I2C_ADDR_114);
    #endif

    #ifdef GPIOPE_USE_INPUT_115
    GPIOPE_QueryDevice(GPIOPE_INPUT_115, I2C_ADDR_115);
    #endif

    #ifdef GPIOPE_USE_INPUT_116
    GPIOPE_QueryDevice(GPIOPE_INPUT_116, I2C_ADDR_116);
    #endif

    #ifdef GPIOPE_USE_INPUT_117
    GPIOPE_QueryDevice(GPIOPE_INPUT_117, I2C_ADDR_117);
    #endif

    #ifdef GPIOPE_USE_INPUT_118
    GPIOPE_QueryDevice(GPIOPE_INPUT_118, I2C_ADDR_118);
    #endif

    #ifdef GPIOPE_USE_INPUT_119
    GPIOPE_QueryDevice(GPIOPE_INPUT_119, I2C_ADDR_119);
    #endif

    #ifdef GPIOPE_USE_INPUT_120
    GPIOPE_QueryDevice(GPIOPE_INPUT_120, I2C_ADDR_120);
    #endif

    #ifdef GPIOPE_USE_INPUT_121
    GPIOPE_QueryDevice(GPIOPE_INPUT_121, I2C_ADDR_121);
    #endif

    #ifdef GPIOPE_USE_INPUT_122
    GPIOPE_QueryDevice(GPIOPE_INPUT_122, I2C_ADDR_122);
    #endif

    #ifdef GPIOPE_USE_INPUT_123
    GPIOPE_QueryDevice(GPIOPE_INPUT_123, I2C_ADDR_123);
    #endif

    #ifdef GPIOPE_USE_INPUT_124
    GPIOPE_QueryDevice(GPIOPE_INPUT_124, I2C_ADDR_124);
    #endif

    #ifdef GPIOPE_USE_INPUT_125
    GPIOPE_QueryDevice(GPIOPE_INPUT_125, I2C_ADDR_125);
    #endif

    #ifdef GPIOPE_USE_INPUT_126
    GPIOPE_QueryDevice(GPIOPE_INPUT_126, I2C_ADDR_126);
    #endif

    #ifdef GPIOPE_USE_INPUT_127
    GPIOPE_QueryDevice(GPIOPE_INPUT_127, I2C_ADDR_127);
    #endif
}

#endif // GPIOPE_MASTER_MODE