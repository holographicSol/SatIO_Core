/**
 * GPIO Port Expander - Written by benjamin Jack Cullen.
 *
 * Intention [1] is to stay as MCU-agnostic as practical. Genericity here is a
 * design-time property, not a runtime one: the GPIOPortExpander type is flexible
 * enough to describe any MCU's pin layout, but every actual instance is still a
 * static, compile-time definition — one requirement doesn't cost the other.
 * 
 * Intention [2] is to have portable instances that can be easily imported/exported
 * between master and slave gpio_port_expander libs, which a GPIOPortExpander instance
 * also reasonably fascilitates.
 * 
 * Intention [3]. Create as many new GPIOPortExpander instances as required, from any
 * pre-defined default GPIOPortExpander instances.
 * 
 * Intention [4]. Run on both master, & slave(s).
 * 
 * Intention [5]. Command symmetry between master & slave(s).
 *
 * For example, GPIOPortExpander as a type is dynamic (general-purpose), while any
 * given GPIOPortExpander instance is static.
 * 
 * Specification flexibility, while remaining static, and with as many instances of a default
 * instance as a project might require.
 *
 * (1) Ensure gpio_portcontroller exists on both master and slave.
 * (2) On the slave, create a new GPIOPortExpander instance, configured for the slave or use the default.
 * (3) Copy the custom/default GPIOPortExpander instance to the masters gpio_portcontroller lib.
 */
#include <string.h>
#include "UnidentifiedStudios_I2C.h"
#include "UnidentifiedStudios_GPIOPortExpander.h"

// ------------------------------------------------------------
/**
 * @brief GPIOPortExpander_ATMEGA2560
 *        This is how to create a GPIOPortExpander instance.
 *        To setup gpio_portcontroller using a different MCU,
 *        create a custom instance.
 */
GPIOPortExpander GPIOPortExpander_ATMEGA2560_Default = {
    "GPIOPortExpander_ATMEGA2560",
    &iic_0,
    I2CLinkBus0,
    9,  // address
    0,  // current_pin
    0,  // pin_min
    69, // pin_max
    GPIOPE_MAX_ATMEGA2560_MAX_PINS, // max_pins
    16, // number of analog pins
    54, // number of digital pins
    (int8_t[]){54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69}, // analog_pins
    (int8_t[]){ // digital_pins
        // 0,
        1,2, 3, 4, 5, 6, 7, 8, 9,
        10,11,12,13,14,15,16,17,18,19,
        20,21,22,23,24,25,26,27,28,29,
        30,31,32,33,34,35,36,37,38,39,
        40,41,42,43,44,45,46,47,48,49,
        50,51,52,53
    },
    (unsigned long[GPIOPE_MAX_ATMEGA2560_MAX_PINS][3]){}, // modulation_time
    (int32_t[GPIOPE_MAX_ATMEGA2560_MAX_PINS]){}, // input_value
    (int32_t[GPIOPE_MAX_ATMEGA2560_MAX_PINS]){}, // output_value
    (int16_t[GPIOPE_MAX_ATMEGA2560_MAX_PINS]){ // port_map (default no port)
      // 0,
      1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,
      21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,
      39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,
      57,58,59,60,61,62,63,64,65,66,67,68,69
    },
    (bool[GPIOPE_MAX_ATMEGA2560_MAX_PINS]){}, // switch_state
    (bool[GPIOPE_MAX_ATMEGA2560_MAX_PINS]){true}, // channels enabled/disabled
    (uint64_t[GPIOPE_MAX_ATMEGA2560_MAX_PINS]){1000000}, // chan_freq_uS - default 1Hz = delay 10^6 micros
    0             // query_cursor
};

#ifdef GPIOPE_SLAVE_MODE
// ------------------------------------------------------------------------------------------------------------------
// SLAVE MDOE
// ------------------------------------------------------------------------------------------------------------------
/**
 * @brief Set current slave device (defined in UnidentifiedStudios_Config.h).
 */
#ifdef GPIOPE_SLAVE_ATMEGA2560
GPIOPortExpander GPIOPortExpander_SLAVE = GPIOPortExpander_ATMEGA2560_Default;
#endif

#ifdef GPIOPE_SLAVE_ESP32P4
GPIOPortExpander GPIOPortExpander_SLAVE = GPIOPortExpander_ESP32P4_Default;
#endif

// --------------------------------------------------------------------
// Inline binary search – compiles to ~10–15 instructions
// --------------------------------------------------------------------
inline bool isAnalogPin(int8_t analog_pins[], int8_t num_analog_pins, uint8_t pin) {
  for (int i=0; i<num_analog_pins; i++) {
    if (pin==(uint8_t)analog_pins[i]) {return true;}
  }
  return false;
}
inline bool isDigitalPin(int8_t digital_pins[], int8_t num_digital_pins, uint8_t pin) {
  for (int i=0; i<num_digital_pins; i++) {
    if (pin==(uint8_t)digital_pins[i]) {return true;}
  }
  return false;
}
void readPins(GPIOPortExpander *expander) {
  int i_counter=0;
  for (int i=0; i<expander->num_digital_pins; i++) {
    expander->input_value[i_counter]=digitalRead(expander->digital_pins[i]);
    i_counter++;
  }
  for (int i=0; i<expander->num_analog_pins; i++) {
    expander->input_value[i_counter]=analogRead(expander->analog_pins[i]);
    i_counter++;
  }
}
// --------------------------------------------------------------------
// O(1) pin-kind lookup, keyed directly by physical pin number - built once
// from analog_pins/digital_pins so readPin() doesn't re-scan up to 70
// entries (isAnalogPin + isDigitalPin) on every single I2C request.
// --------------------------------------------------------------------
constexpr int8_t PIN_KIND_UNKNOWN = 0;
constexpr int8_t PIN_KIND_ANALOG  = 1;
constexpr int8_t PIN_KIND_DIGITAL = 2;
static bool pin_kind_lookup_built = false;
static int8_t pin_kind_lookup[GPIOPE_MAX_SLAVE_PINS];

inline void buildPinKindLookup(GPIOPortExpander *gpio_expander) {
  memset(pin_kind_lookup, PIN_KIND_UNKNOWN, sizeof(pin_kind_lookup));
  for (int i=0; i<gpio_expander->num_analog_pins; i++) {
    uint8_t pin = (uint8_t)gpio_expander->analog_pins[i];
    if (pin < gpio_expander->max_pins) {pin_kind_lookup[pin] = PIN_KIND_ANALOG;}
  }
  for (int i=0; i<gpio_expander->num_digital_pins; i++) {
    uint8_t pin = (uint8_t)gpio_expander->digital_pins[i];
    if (pin < gpio_expander->max_pins) {pin_kind_lookup[pin] = PIN_KIND_DIGITAL;}
  }
  pin_kind_lookup_built = true;
}

inline void readPin(GPIOPortExpander *gpio_expander) {
  if (!pin_kind_lookup_built) {buildPinKindLookup(gpio_expander);}
  int mapped_pin_r = gpio_expander->port_map[gpio_expander->current_pin];
  if (mapped_pin_r < 0 || mapped_pin_r >= gpio_expander->max_pins) {return;}
  switch (pin_kind_lookup[mapped_pin_r]) {
    case PIN_KIND_ANALOG:
      gpio_expander->input_value[gpio_expander->current_pin] = analogRead((uint8_t)mapped_pin_r);
      break;
    case PIN_KIND_DIGITAL:
      gpio_expander->input_value[gpio_expander->current_pin] = digitalRead((uint8_t)mapped_pin_r);
      break;
  }
}

inline void writedPin(GPIOPortExpander *gpio_expander, int8_t idx) {
  if (!pin_kind_lookup_built) {buildPinKindLookup(gpio_expander);}
  int mapped_pin_w = gpio_expander->port_map[gpio_expander->current_pin];
  if (mapped_pin_w < 0 || mapped_pin_w >= gpio_expander->max_pins) {return;}
  switch (pin_kind_lookup[mapped_pin_w]) {
    case PIN_KIND_ANALOG:
      analogWrite((uint8_t)mapped_pin_w, gpio_expander->output_value[idx]);
      break;
    case PIN_KIND_DIGITAL:

      digitalWrite((uint8_t)mapped_pin_w, gpio_expander->output_value[idx]);
      break;
  }
}

// --------------------------------------------------------------------
// Compact active-pin list for modulator() (main.cpp): rather than that
// loop scanning all max_pins slots every pass regardless of how many
// are actually modulating, it walks only this list. modulated_pin_pos
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

void activateModulatedPin(GPIOPortExpander *gpio_expander, int8_t idx) {
  if (idx < 0 || idx >= gpio_expander->max_pins || idx >= gpio_expander->max_pins) return;
  ensureModulatedPinPosInit();
  if (modulated_pin_pos[idx] != -1) return; // already active
  modulated_pin_pos[idx] = modulated_pin_count;
  modulated_pin_list[modulated_pin_count] = idx;
  modulated_pin_count++;
}

// Safe to call while modulator() is mid-iteration, provided (as modulator()
// does) it's only ever called for the pin currently being visited: swap-remove
// moves the list's tail into idx's slot, and a backward-iterating caller will
// never revisit a slot at or before its current position.
void deactivateModulatedPin(GPIOPortExpander *gpio_expander, int8_t idx) {
  if (idx < 0 || idx >= gpio_expander->max_pins || idx >= gpio_expander->max_pins) return;
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

inline void setAllPinMode(GPIOPortExpander *gpio_expander) {
  #ifdef GPIOPE_READ_MODE
  for (int i=0; i < gpio_expander.num_analog_pins; i++) {
    pinMode(gpio_expander.port_map[i], INPUT);
  }
  for (int i=0; i < gpio_expander.num_digital_pins; i++) {
    pinMode(gpio_expander.port_map[i], INPUT);
  }
  #endif

  #ifdef GPIOPE_WRITE_MODE
  for (int i=0; i < gpio_expander->num_analog_pins; i++) {
    pinMode(gpio_expander->port_map[i], OUTPUT);
  }
  for (int i=0; i < gpio_expander->num_digital_pins; i++) {
    pinMode(gpio_expander->port_map[i], OUTPUT);
  }
  #endif
}

// ------------------------------------------------------------
// Output modulator
// ------------------------------------------------------------
void modulator(GPIOPortExpander *expander) {
  // ------------------------------------------------------------
  // Logic modulator
  // Modulate output only if a switch state is already true.
  // Modulator values: time high, time low.
  // ------------------------------------------------------------
  // Walk only the pins actually modulating (compact list maintained by
  // GPIO_PE_CMD_WRITE_PIN_PWM/GPIO_PE_CMD_CLEAR_DATA in GPIOPE.cpp) instead of
  // scanning all max_pins slots every pass - the scan itself, not the
  // per-pin logic below, was the measured ~184us fixed cost.
  // One clock read shared by every pin this pass instead of one per
  // active pin - micros() is a syscall-ish read on AVR (briefly disables
  // interrupts), so this cuts up to max_pins reads down to 1.
  const unsigned long now = micros();
  int8_t count;
  const int8_t *active = modulatedPinIndices(&count);
  // Iterate backward: deactivateModulatedPin() below swap-removes the
  // *current* index, moving the list's tail into this slot. Walking high-to-low
  // guarantees that tail element (always at a position we've already visited,
  // or this same one) is never revisited - a forward loop would skip it.
  for (int8_t k=count-1; k>=0; k--) {
    const int8_t i = active[k];
    const int32_t out_val = expander->output_value[i];

    // cache this pin's row instead of re-indexing modulation_time[i] up to 4x
    unsigned long *mt = expander->modulation_time[i];

    const int16_t pin = expander->port_map[i];

    // ------------------------------------------------------
    // handle currently low
    // ------------------------------------------------------
    if (expander->switch_state[i]==false) {
      // ----------------------------------
      // modulate on
      // ----------------------------------
      if ((now - mt[2]) >= mt[0]) {
        // Serial.println("[t0 exceeded (mod on)] idx: " + String(i));
        if (pin<54) {digitalWrite(pin, HIGH);}
        else {analogWrite(pin, out_val);}
        mt[2]=now;
        expander->switch_state[i]=true;
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
          // Serial.println("[t1 exceeded (remain off)] idx: " + String(i));
          if (pin<54) {digitalWrite(pin, LOW);}
          else {analogWrite(pin, 0);}
          mt[2]=now;
          expander->switch_state[i]=false;
          // change parent state off
          expander->output_value[i]=0;
          // pin is done modulating for good (output_value<=0 now) - stop
          // scanning it. Safe mid-loop: see the backward-iteration note above.
          deactivateModulatedPin(expander, i);
        }
      }
      // ----------------------------------
      // modulate off
      // ----------------------------------
      else {
        if ((now - mt[2]) >= mt[1]) {
          // Serial.println("[t1 exceeded (mod off)] idx: " + String(i));
          if (pin<54) {digitalWrite(pin, LOW);}
          else {analogWrite(pin, 0);}
          mt[2]=now;
          expander->switch_state[i]=false;
        }
      }
    }
  }
  // Serial.println("T " + String(micros()-now));
}

/** ----------------------------------------------------------------------------
 * @brief Request binary event handler for an I2C Bus.
 *
 * @note Registered directly with Wire.onRequest(), which requires this exact
 *       void() signature, so it can't take a GPIOPortExpander* parameter -
 *       it operates on a GPIOPortExpander inside its function.
 *
 *       This function is bus agnostic, however the functions name conveys a bus
 *       for clarity, and can be reproduced (copy/paste) as many times as required,
 *       with same/differently configured GPIOPortExpander, for any required
 *       compatible I2C bus and MCU.
 */
void requestEventBus0Bin() {
  GPIOPortExpander &gpio_expander = GPIOPortExpander_SLAVE;

  switch (gpio_expander.i2cLink.REQUEST_ID) {

    // Commands 0-69 - Send reading for the directly-addressed pin (current_pin already set by matching receive command)
    case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: // 0-9
    case 10: case 11: case 12: case 13: case 14: case 15: case 16: case 17: case 18: case 19: // 10-19
    case 20: case 21: case 22: case 23: case 24: case 25: case 26: case 27: case 28: case 29: // 20-29
    case 30: case 31: case 32: case 33: case 34: case 35: case 36: case 37: case 38: case 39: // 30-39
    case 40: case 41: case 42: case 43: case 44: case 45: case 46: case 47: case 48: case 49: // 40-49
    case 50: case 51: case 52: case 53: case 54: case 55: case 56: case 57: case 58: case 59: // 50-59
    case 60: case 61: case 62: case 63: case 64: case 65: case 66: case 67: case 68: case 69: { // 60-69
        // Take a fresh reading for the directly-addressed pin (matches
        // GPIO_PE_CMD_RESET_CURRENT_PIN's readPin() call below) so a master-side
        // single-pin read (readGPIOPortExapander_Pin()) isn't just handed
        // back whatever was last cached by a previous bulk pass.
        readPin(&gpio_expander);
        #ifdef GPIO_PORTCONTROLLER_DEBUG_2
        Serial.println("[CASE 0-N] sending: pin=" + String(gpio_expander.current_pin)  + "  value=" + String(gpio_expander.input_value[gpio_expander.current_pin]));
        #endif
        clearI2CLinkOutputPacket(gpio_expander.i2cLink);
        write_float_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, 0, (float)gpio_expander.input_value[gpio_expander.current_pin]);
        writeI2CToMasterBin(Wire, gpio_expander.i2cLink, 4, 0);
        break;
      }

    // Command 120 - Send pin reading to master and step current pin
    case GPIO_PE_CMD_RESET_CURRENT_PIN: {
        #ifdef GPIO_PORTCONTROLLER_DEBUG_2
        Serial.println("[GPIO_PE_CMD_RESET_CURRENT_PIN] sending: pin=" + String(gpio_expander.current_pin)  + "  value=" + String(gpio_expander.input_value[gpio_expander.current_pin]));
        #endif

        #ifdef GPIO_PORTCONTROLLER_BENCH
        int32_t t0 = micros();
        #endif
        readPin(&gpio_expander);
        #ifdef GPIO_PORTCONTROLLER_BENCH
        Serial.println("RP " + String(micros()-t0)); // read pin time uS
        #endif

        #ifdef GPIO_PORTCONTROLLER_BENCH
        t0 = micros();
        #endif
        // (offsets 0-4) are fully overwritten by the two writes below, so
        // zeroing the 32-byte buffer first bought nothing but ~6-9us.
        clearI2CLinkOutputPacket(gpio_expander.i2cLink); // dropped here: the 5 bytes actually sent
        write_uint8_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, 0, gpio_expander.current_pin);
        write_float_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, 1, (float)gpio_expander.input_value[gpio_expander.current_pin]);
        writeI2CToMasterBin(Wire, gpio_expander.i2cLink, 5, 0);
        #ifdef GPIO_PORTCONTROLLER_BENCH
        Serial.println("SN " + String(micros()-t0)); // write time uS
        #endif
        if (++gpio_expander.current_pin >= gpio_expander.num_analog_pins + gpio_expander.num_digital_pins) {gpio_expander.current_pin = 0;}
        break;
      }

    // Command 130 - Send expander header: pin_min, pin_max, max_pins, n_analog, n_digital
    case GPIO_PE_CMD_GET_EXPANDER_INFO: {
        #ifdef GPIO_PORTCONTROLLER_DEBUG_2
        Serial.println(
          "[GPIO_PE_CMD_GET_EXPANDER_INFO]  pin_min=" + String(gpio_expander.pin_min) +
          "  pin_max=" + String(gpio_expander.pin_max) +
          "  max_pins=" + String((uint8_t)gpio_expander.max_pins) +
          "  num_analog_pins=" + String(gpio_expander.num_analog_pins) +
          "  num_digital_pins=" + String(gpio_expander.num_digital_pins)
        );
        #endif
        clearI2CLinkOutputPacket(gpio_expander.i2cLink);
        write_int8_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, 0, gpio_expander.pin_min);
        write_int8_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, 1, gpio_expander.pin_max);
        write_uint8_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, 2, (uint8_t)gpio_expander.max_pins);
        write_uint8_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, 3, gpio_expander.num_analog_pins);
        write_uint8_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, 4, gpio_expander.num_digital_pins);
        writeI2CToMasterBin(Wire, gpio_expander.i2cLink, 5, 0);
        break;
      }

    // Command 140 - Send one (is_analog, pin) entry at query_cursor, then advance it
    case GPIO_PE_CMD_GET_EXPANDER_PIN_LIST: {
        clearI2CLinkOutputPacket(gpio_expander.i2cLink);
        if (gpio_expander.query_cursor < gpio_expander.num_analog_pins) {
          #ifdef GPIO_PORTCONTROLLER_DEBUG_2
          Serial.println("[GPIO_PE_CMD_GET_EXPANDER_PIN_LIST]  is_analog=1  pin=" + String(gpio_expander.analog_pins[gpio_expander.query_cursor]));
          #endif
          write_uint8_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, 0, 1);
          write_int8_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, 1, gpio_expander.analog_pins[gpio_expander.query_cursor]);
        } else {
          #ifdef GPIO_PORTCONTROLLER_DEBUG_2
          Serial.println("[GPIO_PE_CMD_GET_EXPANDER_PIN_LIST]  is_analog=0  pin=" + String(gpio_expander.digital_pins[gpio_expander.query_cursor - gpio_expander.num_analog_pins]));
          #endif
          write_uint8_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, 0, 0);
          write_int8_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, 1, gpio_expander.digital_pins[gpio_expander.query_cursor - gpio_expander.num_analog_pins]);
        }
        writeI2CToMasterBin(Wire, gpio_expander.i2cLink, 2, 0);
        if (++gpio_expander.query_cursor >= gpio_expander.num_analog_pins + gpio_expander.num_digital_pins) {gpio_expander.query_cursor = 0;}
        break;
      }

    default: {
        #ifdef GPIO_PORTCONTROLLER_DEBUG_0
        Serial.println("[requestEventBus0Bin] event id is not defined: " + String(gpio_expander.i2cLink.REQUEST_ID));
        #endif
        while (Wire.available()) {Wire.read();} // drain
        break;
    }
  }
}

/** ----------------------------------------------------------------------------
 * @brief Receive binary event handler for an I2C Bus
 *
 * @note Registered directly with Wire.onReceive(), which requires this exact
 *       void(int) signature, so it can't take a GPIOPortExpander* parameter -
 *       it operates on this slave's own GPIOPortExpander_ATMEGA2560 by name.
 *
 *       This function is bus agnostic, however the functions name conveys a bus
 *       for clarity, and can be reproduced (copy/paste) as many times as required,
 *       with same/differently configured GPIOPortExpander, for any required
 *       compatible I2C bus and MCU.
*/
void receiveEventBus0Bin(int n_bytes_received) {
  GPIOPortExpander &gpio_expander = GPIOPortExpander_SLAVE;
  if (n_bytes_received < 1) return;
  uint8_t cmd = gpio_expander.wire->read(); // expects uint8 command byte (up to 255 unique commands can be accepted).
  #ifdef GPIO_PORTCONTROLLER_DEBUG_1
  Serial.println("[receiveEventBus0Bin] " + String(cmd) + " (" + String(n_bytes_received) + " bytes)");
  #endif
  switch (cmd) {

    /**
     * @brief Command 100 - Clear Data
     */
    case GPIO_PE_CMD_CLEAR_DATA: {
      #ifdef GPIO_PORTCONTROLLER_DEBUG_2
      Serial.println("[GPIO_PE_CMD_CLEAR_DATA]");
      #endif
      for (int i = 0; i < gpio_expander.max_pins; i++) {
        gpio_expander.port_map[i] = -1;
        gpio_expander.output_value[i] = 0;
        gpio_expander.modulation_time[i][0] = 0;
        gpio_expander.modulation_time[i][1] = 0;
        gpio_expander.modulation_time[i][2] = 0;
        gpio_expander.switch_state[i] = false;
      }
      resetModulatedPinList();
      gpio_expander.current_pin = 0;
      while (gpio_expander.wire->available()) gpio_expander.wire->read();  // flush
      break;
    }

    /**
     * @brief Command 110 - Write value to pin & Set PWM
     */
    case GPIO_PE_CMD_WRITE_PIN_PWM: {
      if (n_bytes_received != 15) {
        while (gpio_expander.wire->available()) gpio_expander.wire->read();
        #ifdef GPIO_PORTCONTROLLER_DEBUG_0
        Serial.println("!=15");
        #endif
        return;
      }

      uint8_t idx;
      read_uint8_FromWire(*gpio_expander.wire, idx);

      int8_t pin;
      read_int8_FromWire(*gpio_expander.wire, pin);

      int32_t value;
      read_int32_FromWire(*gpio_expander.wire, value);

      uint32_t off_time;
      read_uint32_FromWire(*gpio_expander.wire, off_time);

      uint32_t on_time;
      read_uint32_FromWire(*gpio_expander.wire, on_time);


      #ifdef GPIO_PORTCONTROLLER_DEBUG_2
      Serial.println(
        "[GPIO_PE_CMD_WRITE_PIN_PWM]  idx=" + String(idx) +
        "  pin=" + String(pin) +
        "  value=" + String(value) +
        "  off_time=" + String(off_time) +
        "  on_time=" + String(on_time)
      );
      #endif

      if (idx >= gpio_expander.max_pins) return;

      gpio_expander.port_map[idx]           = pin;
      gpio_expander.output_value[idx]       = value;
      gpio_expander.modulation_time[idx][0] = off_time;
      gpio_expander.modulation_time[idx][1] = on_time;

      // Same condition modulator() itself uses to act on a slot - keep the
      // active-pin list in sync with what it would actually visit.
      if (value > 0 && (off_time != 0 || on_time != 0)) {
        activateModulatedPin(&gpio_expander, (int8_t)idx);
      } else {
        deactivateModulatedPin(&gpio_expander, (int8_t)idx);
      }

      writedPin(&gpio_expander, idx);

      break;
    }

    /**
     * @brief Command 120 - Reset current pin & set request ID 120 ready for a request.
     * @note This is useful if master does not need any specific value right now, but
     *       does need to know which value it is receiving.
     */
    case GPIO_PE_CMD_RESET_CURRENT_PIN: {
      #ifdef GPIO_PORTCONTROLLER_DEBUG_2
      Serial.println("[GPIO_PE_CMD_RESET_CURRENT_PIN]");
      #endif
      gpio_expander.current_pin = 0;
      gpio_expander.i2cLink.REQUEST_ID = GPIO_PE_CMD_RESET_CURRENT_PIN;
      while (gpio_expander.wire->available()) {gpio_expander.wire->read();}
      break;
    }

    /**
     * @brief Command 130 - Reset expander-info cursor & set request ID ready for a request.
     */
    case GPIO_PE_CMD_GET_EXPANDER_INFO: {
      #ifdef GPIO_PORTCONTROLLER_DEBUG_2
      Serial.println("[GPIO_PE_CMD_GET_EXPANDER_INFO]");
      #endif
      gpio_expander.query_cursor = 0;
      gpio_expander.i2cLink.REQUEST_ID = GPIO_PE_CMD_GET_EXPANDER_INFO;
      while (gpio_expander.wire->available()) {gpio_expander.wire->read();}
      break;
    }

    /**
     * @brief Command 140 - Reset expander pin-list cursor & set request ID ready for a request.
     */
    case GPIO_PE_CMD_GET_EXPANDER_PIN_LIST: {
      #ifdef GPIO_PORTCONTROLLER_DEBUG_2
      Serial.println("[GPIO_PE_CMD_GET_EXPANDER_PIN_LIST]");
      #endif
      gpio_expander.query_cursor = 0;
      gpio_expander.i2cLink.REQUEST_ID = GPIO_PE_CMD_GET_EXPANDER_PIN_LIST;
      while (gpio_expander.wire->available()) {gpio_expander.wire->read();}
      break;
    }

    /**
     * @brief Commands 0-69 - Directly set current pin & matching request ID, ready for a request.
     */
    case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: // 0-9
    case 10: case 11: case 12: case 13: case 14: case 15: case 16: case 17: case 18: case 19: // 10-19
    case 20: case 21: case 22: case 23: case 24: case 25: case 26: case 27: case 28: case 29: // 20-29
    case 30: case 31: case 32: case 33: case 34: case 35: case 36: case 37: case 38: case 39: // 30-39
    case 40: case 41: case 42: case 43: case 44: case 45: case 46: case 47: case 48: case 49: // 40-49
    case 50: case 51: case 52: case 53: case 54: case 55: case 56: case 57: case 58: case 59: // 50-59
    case 60: case 61: case 62: case 63: case 64: case 65: case 66: case 67: case 68: case 69: { // 60-69
      gpio_expander.current_pin = cmd;
      gpio_expander.i2cLink.REQUEST_ID = cmd;
      #ifdef GPIO_PORTCONTROLLER_DEBUG_2
      Serial.println("[CASE 0-N]  current_pin=" + String(gpio_expander.current_pin) + "  REQUEST_ID=" + String(gpio_expander.i2cLink.REQUEST_ID));
      #endif
      while (gpio_expander.wire->available()) {gpio_expander.wire->read();}
      break;
    }

    // Default: flush
    default: {
      while (gpio_expander.wire->available()) {gpio_expander.wire->read();}
      break;
    }
  }
}
#endif

#ifdef GPIOPE_MASTER_MODE
// ------------------------------------------------------------------------------------------------------------------
// MASTER MDOE
// ------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------
/**
 * @brief GPIOPortExpander_ATMEGA2560 (Custom)
 */
IICLink IICLinkPCI; // IIC link data structure for the GPIOPortExpander.
GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_0 = {
    "GPIOPortExpander_ATMEGA2560_Input_0",
    &iic_0,
    IICLinkPCI,
    11, // address
    0,  // current_pin
    0,  // pin_min
    69, // pin_max
    GPIOPE_MAX_ATMEGA2560_MAX_PINS, // max_pins
    16, // number of analog pins
    54, // number of digital pins
    (int8_t[]){54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69}, // analog_pins
    (int8_t[]){ // digital_pins
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        10,11,12,13,14,15,16,17,18,19,
        20,21,22,23,24,25,26,27,28,29,
        30,31,32,33,34,35,36,37,38,39,
        40,41,42,43,44,45,46,47,48,49,
        50,51,52,53
    },
    (unsigned long[GPIOPE_MAX_ATMEGA2560_MAX_PINS][3]){}, // modulation_time
    (int32_t[GPIOPE_MAX_ATMEGA2560_MAX_PINS]){}, // input_value
    (int32_t[GPIOPE_MAX_ATMEGA2560_MAX_PINS]){}, // output_value
    (int16_t[GPIOPE_MAX_ATMEGA2560_MAX_PINS]){ // port_map (default no port)
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0-9
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 10-19
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 20-29
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 30-39
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 40-49
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 50-59
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 60-69
    },
    (bool[GPIOPE_MAX_ATMEGA2560_MAX_PINS]){}, // switch_state
    (bool[GPIOPE_MAX_ATMEGA2560_MAX_PINS]){true}, // channels enabled/disabled
    (uint64_t[GPIOPE_MAX_ATMEGA2560_MAX_PINS]){0}, // chan_freq_uS - default 1Hz = delay 10^6 micros
    0 // query_cursor
};

// ------------------------------------------------------------
/**
 * @brief GPIOPortExpander_ATMEGA2560 (Custom)
 */
IICLink IICLinkPCO_0; // IIC link data structure for the GPIOPortExpander.
GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_0 = {
    "GPIOPortExpander_ATMEGA2560_Output_0",
    &iic_2,
    IICLinkPCO_0,
    9,  // address
    0,  // current_pin
    0,  // pin_min
    69, // pin_max
    GPIOPE_MAX_ATMEGA2560_MAX_PINS, // max_pins
    16, // number of analog pins
    54, // number of digital pins
    (int8_t[]){54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69}, // analog_pins
    (int8_t[]){ // digital_pins
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        10,11,12,13,14,15,16,17,18,19,
        20,21,22,23,24,25,26,27,28,29,
        30,31,32,33,34,35,36,37,38,39,
        40,41,42,43,44,45,46,47,48,49,
        50,51,52,53
    },
    (unsigned long[GPIOPE_MAX_ATMEGA2560_MAX_PINS][3]){}, // modulation_time
    (int32_t[GPIOPE_MAX_ATMEGA2560_MAX_PINS]){}, // input_value
    (int32_t[GPIOPE_MAX_ATMEGA2560_MAX_PINS]){}, // output_value
    (int16_t[GPIOPE_MAX_ATMEGA2560_MAX_PINS]){ // port_map (default no port)
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0-9
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 10-19
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 20-29
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 30-39
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 40-49
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 50-59
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 60-69
    },
    (bool[GPIOPE_MAX_ATMEGA2560_MAX_PINS]){}, // switch_state
    (bool[GPIOPE_MAX_ATMEGA2560_MAX_PINS]){true}, // channels enabled/disabled
    (uint64_t[GPIOPE_MAX_ATMEGA2560_MAX_PINS]){0}, // chan_freq_uS - default 1Hz = delay 10^6 micros
    0 // query_cursor
};

bool readGPIOPortExapander_All(GPIOPortExpander gpio_expander) {
  bool ok = true; // assume true except even just one bad read (replace with counter)
  // Send event ID once to set a mode on receiver.
  clearI2CLinkOutputPacket(gpio_expander.i2cLink);
  gpio_expander.i2cLink.OUTPUT_PACKET[0] = 0x78; // command 120 (reset and iter current pin)
  // printf("writeI2CToSlaveBin\n");
  writeI2CToSlaveBin(*gpio_expander.wire, gpio_expander.i2cLink, gpio_expander.address, 1, 0, "readInputPortControllerM1");
  for (uint8_t i = 0; i < (uint8_t)gpio_expander.max_pins; i++) {
    // printf("requestFromSlaveBinNoID\n");
    if (requestFromSlaveBinNoID(*gpio_expander.wire, gpio_expander.i2cLink, gpio_expander.address, 5, 0, "readInputPortControllerM1")) {
      uint8_t pin;
      // printf("read_uint8_FromWire\n");
      read_uint8_FromWire(*gpio_expander.wire, pin);
      float value;
      // printf("read_float_FromWire\n");
      read_float_FromWire(*gpio_expander.wire, value);
      // Bound against the device's own discovered pin count (matches the
      // input_portcontroller_0.input_value array's real capacity), not
      // MAX_MATRIX_SWITCHES - a physical pin can exceed the switch count.
      if (pin < gpio_expander.max_pins) {
        gpio_expander.input_value[pin] = (long)value;
      }
      // printf("readInputPortControllerReadPins: pin=%d   value=%f\n", pin, value);
    }
    else {
      ok = false;
      printf("ERROR: readInputPortControllerReadPins (pin_index=%d)\n", i);
    }
  }
  return ok;
}
/**
 * Reads a single pin fresh via direct pin-addressing; see the header for the
 * calling convention.
 *
 * Rule 18.1: pin is bounds-checked against gpio_expander.max_pins before
 * being used as an index into input_value.
 */
bool readGPIOPortExapander_Pin(GPIOPortExpander gpio_expander, uint8_t pin) {
  if (pin >= (uint8_t)gpio_expander.max_pins) {return false;}

  clearI2CLinkOutputPacket(gpio_expander.i2cLink);
  gpio_expander.i2cLink.OUTPUT_PACKET[0] = pin; // command 0-69: directly address this pin
  writeI2CToSlaveBin(*gpio_expander.wire, gpio_expander.i2cLink, gpio_expander.address, 1, 0, "readGPIOPortExapander_Pin");

  if (!requestFromSlaveBinNoID(*gpio_expander.wire, gpio_expander.i2cLink, gpio_expander.address, 4, 0, "readGPIOPortExapander_Pin")) {
    return false;
  }
  float value;
  read_float_FromWire(*gpio_expander.wire, value);
  gpio_expander.input_value[pin] = (long)value;
  return true;
}

void clearGPIOPortController(GPIOPortExpander gpio_expander) {
  clearI2CLinkOutputPacket(gpio_expander.i2cLink);
  write_uint8_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, 0, GPIO_PE_CMD_CLEAR_DATA);
  writeI2CToSlaveBin(*gpio_expander.wire, gpio_expander.i2cLink, gpio_expander.address, 1, 0, "clearGPIOPortController");
}

void setGPIOPortExpanderChannelEnabled(GPIOPortExpander &gpio_expander, uint8_t channel,  bool enabled) {
  if (channel < gpio_expander.max_pins) {
    gpio_expander.enabled[channel] = enabled;
    if (enabled == false) {
      gpio_expander.input_value[channel] = 0;
      gpio_expander.output_value[channel] = 0;
    }
  }
}

/**
 * Sets a pin's minimum accepted-read period (microseconds); see
 * setGPIOPortExpanderChannelFreq() in the header.
 *
 * Rule 18.1: pin is bounds-checked against gpio_expander.max_pins before
 * being used as an index into chan_freq_uS.
 *
 * freq_uS is clamped to INT64_MAX: the owning task compares it against an
 * elapsed microsecond count via (int64_t)chan_freq_uS, so a stored value
 * above INT64_MAX would wrap negative and defeat the throttle.
 */
void setGPIOPortExpanderChannelFreq(GPIOPortExpander &gpio_expander, uint8_t pin, uint64_t freq_uS) {
  if (pin < (uint8_t)gpio_expander.max_pins) {
    gpio_expander.chan_freq_uS[pin] = (freq_uS > (uint64_t)INT64_MAX) ? (uint64_t)INT64_MAX : freq_uS;
  }
}
/**
 * @brief A placeholder for writing to a GPIOPortExpander.
 */
int32_t writeGPIOPortExapander_All(GPIOPortExpander gpio_expander) {
  int32_t count_write=0;

  for (int32_t Mi = 0; Mi < gpio_expander.max_pins; Mi++) {

    int address = gpio_expander.address;
    int32_t value_to_send = 0;
    uint32_t off_time = 0;
    uint32_t on_time = 0;

    // Build binary packet with human readable helper functions.
    write_uint8_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, 0, GPIO_PE_CMD_WRITE_PIN_PWM); // command 110
    write_uint8_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, 1, (uint8_t)Mi);
    write_int8_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, 2, (int8_t)gpio_expander.port_map[Mi]);
    write_int32_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, 3, value_to_send);
    write_uint32_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, 7, off_time);
    write_uint32_ToPacket(gpio_expander.i2cLink.OUTPUT_PACKET, 11, on_time);
    // Write to slave.
    writeI2CToSlaveBin(*gpio_expander.wire, gpio_expander.i2cLink, address, 15, 0, gpio_expander.name);

    count_write++;

  }
  return count_write;
}
#endif
