# SatIO

![plot](./SDCARD/UnidentifiedStudios.png)

*SatIO - Written by Benjamin Jack Cullen*

SatIO is a realtime data hive & programmable switch, for building devices with and or on top of.

---
## Philosophy & Design

1. **Value Creation** — Safety, Stability, Accuracy. As SatIO develops, so should value creation, leading to safer, more accurate values over time. This can create a desirable improvement curve where in contrast, building a system from scratch every new project, has the potential to reintroduce bugs and 're-solve' the same problems. As SatIO develops, so does any system built on top of SatIO. Value creation should ideally be both read and write to values.

2. **Value Utilization**  — 

- **The Matrix** — utilizes the values from across the system, to switch output high/low/analog/mapped, according to programmable
   logic whereby any value in the system can be compared to any other value in the system and or comapred to a user defined
   value. The comaprisons use some basic operators, <>==, and in range. Every value in the matrix is treated as a double
   and there are around 120 system values that can be used in the matrix as 'matrix functions'. Each matrix function accepts function values as X,Y,Z, that are also stored in a matrix (See usage below for more matrix programmability options). This means that the Matrix combinational potential is considerably high, and all possible combinations and all possible reasonings for any given combination may be impossible for one person to ever know or comprehend (see below for Inference via Bayesian Reasoning). The matrix is the core of SatIO and SatIO's potential, it makes SatIO a computer, however it does not make SatIO Turing Complete.
   Each Matrix Switch can hold more than one Matrix Function, all of which must result in true, in order to switch, and locic can run in an inverted mode, that inverts
   Matrix Function return boolean. Switches can be linked to other switches by using a special Matrix Function called Switch Link, which allows logic to be stacked
   accross switches and again inverted and or further gated, using the same or different output port. All Matrix Switches have programmable output ports and are -1 by default. Logic can be modulated up to int64_max, for PWM on N co-processor(s) that strictly handle I/O and modulation.
   **Possible Matrix Use Cases** include triggers for interrupting peripheral microcontrollers, throughput of analog signals and digital output, allowing for directly driving a sensor and or being used as an event manager for peripheral devices.

   **Elemental** — Allow comparing any value from the system to any other value from the system and or to a user defined value.

   **Compound** — Allow stacking compounds of (1), so that multiple things can be calculated to result in a single true/false.

   **I/O** — Each available/required output pin can have it's own (2) Compound logic.

   **XYZ** — A lot can be calculated with 3 comparitors, x, y and z. Currently, the only reason to use y is for ranging, and z is used as an index number to access a comparitor in an array. This is ultimately the simplicity of the matrix, whereby anything being claculated is a programmable primary comparitor x, being compared to a programmable secondary comparitor x, optionally in range of y, with z sometimes being used to populate the primary comparitor x with a value from a specified array at index z. Together with elemental and compound logic, this helps fascilitate general, high potential for calculations, without any special functions for any given, potential calculation.
   **X** Secondary Comparitor.   
   **Y** Range.
   **Z** Index Primary Compraitor Array.

   **Computer Assist Bool** is automation, and takes control over a specified output port according to how the matrix switch for that port is configured. Note
   that a switch is rendered fully automatic by Computer Assist being enabled for the same the same switch, meaning the computer will decide when to
   switch, how to modulate the switch, and why, all determined from the users programmable logic in the Matrix.

   **Computer Intention Bool** provides insight into what the computer has calculated. Weather or not Computer Assist is enabled, Computer Intention is
   visible for every switch, allowing a user or other system to see what the computer wants to do, with and or without actually doing it. Computer Assist
   can be enabled at any point for any switch, before and or during Computer Intent true/false. This 'layer' also fascilitates optional design choices being made, like the potential for semi-automatic functionality on a switch by switch basis, whereby Computer intention can be used as an indicator and the user
   can ultimately decide weather or not to switch, by flipping Computer Assist on/off.

   **Switch Intention Bool** is set according to Computer Intention, providing Computer Assist is enabled, for the same switch. Computer intention is
   always set, Switch Intention is only ever set if Computer Assist is enabled for the same switch.

   **Output Values** can be digital (directly derived from switch intention), or analog from a mapped value in the system. Varoius output modes determine
   what values will be output if and when a Switch Intention true.

   **Mapped Values** supports user defined mapping from a selection of values in the system. Mapped values can be used in two ways, [1] with the intention
   of using the mapped value in a Matrix Switch Function as a comparitor, and or [2] with the intention to use the mapped value as an Output Value. Map slots are not aligned with Matrix Switch slots, so that multiple Matrix Switches and or Output values can utilize any map slot, simultaniously, without needing to map anything twice or more, unecessarily. This means that a map slot index was required for Matrix Switches, whereby the map slot index values ARE aligned with Matrix Switches, and the values within 
   the Index, point to user defined Map Slots.

- **Data Sharing** — Values from across the system such as sensor data and data extrapolated from sensor data, can be output for other systems, devices, micrcontrollers to read.
   This allows for using SatIO with LLM's, as a module for another project, etc, increasing the flexibility and use cases for including SatIO in a project.

- **Dynamic/Static/Simulation** — System values like time, location, altitude, speed, etc can be set
from real (dynamic) sensor data and or can be individually specified by the user (static). This allows
for various options and scenarios like running as a station, simulation, and or where dynamically updating
the system values is not an option, like for example if there is no GPS. The system always uses system values,
and the system values are set according to a values mode: GPS, Gyro, User. This means that calculations
(like many calculations in the Universe task for example) that depend on certain values, can still function correctly, if the correct user values are set, like datetime and location for example.

**Why:** This means that many 'special functions' do not need to be created in order to calculate something, because the answer may already exist, via some combination of available logic in the Matrix.

**Further more:** Inference via Bayesian Reasoning? Moon tracking for example can be used to track the moon, it can also be used for one example; to track the tide, if the system is aware of moon/planetary positioning and datetime then marine life values may also be inferred relative to the inferred tide values and known datetime. There are lot of values in the Hive, that can be used with different reasoning, in many different combinations, with a network effect in mind for inference, if required. Or more simply — *'SatIO is one hell of a switch'*.

---

### StarNav

**Earth Zenith Right Ascension and Declination** is calculated for 90 degrees altitude (zenith) at a local/remote position relative to Earth. Earth Zenith RA dec is then used to set a 'Gyroscopic Ra Dec', which is offset from Earth Zenith Ra Dec relative to the gyroscopes attitude in roll, pitch and yaw. This makes navigation of the celestial sphere possible, relative to some system/user-specified, local/remote position. This means that even though Earth Zenith RA dec is constantly changing as we face different areas of the celestial sphere, an upright (relative to earth) gyroscope's RA/Dec should always be equal+- to Earth Zenith Ra Dec, providing the gyroscope is correctly calibrated.

**Gyroscopic Ra Dec** can be used for pointing the gyro anywhere in the Celestial Sphere and know +- where exactly that is, in Right Ascension, Declination and Altitude and Azimuth. This could be thought as at least a 3 dimensional compass, fused with roll, pitch, yaw, and Earth Zenith RA Dec, while say yaw alone would be a 2 dimensional ground heading (NSEW). Altitude degrees +90 should always be facing away from earth, while -90 degrees Altitude, is always the way back home.

**Object Tracking** Users will not easily be able to steady the boresight on any celestial object with one’s own hands, because the system is highly sensitive to even very small changes in attitude (it would be like trying to use a telescope on one’s shoulders to focus on a spiral galaxy, that is hundreds of light years away). The system is intended to be scientific. A user would require a mount, tripod or turret to stabilize the gyroscope, to **mechanically** track an object in the celestial sphere. https://github.com/DavidArmstrong/SiderealObjects is used to identify and track objects with a provided Ra and Dec, the library is extensive, providing a Star Table, NGC Table, IC Table, Messier Table, Caldwell Table and Herchell 400. The library included has been highly modified to include Object constellation, type and description, along with expanding the Star names and Star types. Object tracking is available via one-shot CLI commands and is also performed internally, automatically for the system to scan the celestial sphere within a +- range of Gyroscopic RA and Dec. 

---

### GPIOPE

**GPIO Port Expander** — Matrix switch output is on co-processors running GPIOPE.

   **Matrix Output:** For a matrix switch to be able to output, GPIOPE must first be configured for any switch that requires output to a GPIOPE device. This allows for either all output to be on one GPIOPE device or multiple, so that each switch could be assigned its own output device or simply all use the same output device.
   
   **Port Map:** The matrix has a programmable list (matrix_port_map) which can be used to specify which GPIOPE port map slot to use, the GPIOPE port map slot
   contains the actual pin number, which can be specified for that slot with GPIOPE.
   
   **I2C Address:** Each matrix switch can also be assigned a GPIOPE address, which should correspond to a configured GPIOPE device.
   
   **Summary:** GPIOPEs are now independent from the matrix, allowing each GPIOPE to have its own specific configuration, to be saved and loaded, while a matrix
   switch can be pointed at a configured GPIOPE device if required. This generally makes input/output expansion more flexible because complete, and potentially
   unique portmaps and configurations are now stored per GPIOPE device, rather than storing a single port map and configuration in the matrix, which had some
   limitations that have now been resolved by this separation of matrix and port expansion devices.

---

## System

```
system --save               Takes no further arguments.
system --load               Takes no further arguments.
system --restore-defaults   Takes no further arguments.
system -log                 Automatically log data to disk (See performance for timing). Takes arguments -e, -d.
```

---

## Mapping

Many values can be mapped and then used in the matrix and or sent directly to the port controller. Supports standard mapping and mapping from center (Dual/Split Axis).

```
map --new      Clears all mapping in memory.
map --save
map --load
map --delete
map -s n       Specify map slot n.
map -m n       Specify slot -s mode. (0 : map min to max) (1 : center map x0) (2 : center map x1)
map -c0 n      Configuration map slot -s value to map. See available map values.
map -c1 n      Configuration map slot -s. (mode 0 : in_min)  (mode 1 : approximate center value)
map -c2 n      Configuration map slot -s. (mode 0 : in_max)  (mode 1 : Neg_range : 0 to approximate center value)
map -c3 n      Configuration map slot -s. (mode 0 : out_min) (mode 1 : Pos_range : ADC max - neg range)
map -c4 n      Configuration map slot -s. (mode 0 : out_max) (mode 1 : out_max)
map -c5 n      Configuration map slot -s. (mode 1 only : DEADZONE : expected fluctuation at center)
```
---

## Matrix

Setup the matrix:

```
matrix --new                Clears matrix in memory.
matrix --save n             Specify file slot.
matrix --load n             Specify file slot.
matrix --delete n           Specify file slot.
matrix --startup-enable
matrix --startup-disable
matrix -s n                 Specify switch index n.
matrix -f n                 Specify function index n.
matrix -fn n                Set function -f for switch -s. See available matrix functions.
matrix -fx n                Set function -f value x for switch -s.
matrix -fy n                Set function -f value y for switch -s.
matrix -fz n                Set function -f value z for switch -s.
matrix -fi n                Set function -f logic inverted for switch -s.
matrix -fo n                Set function -f operator for switch -s. See available switch function operators.
matrix --xyz-mode-x n       Specify function comparitor mode for X. See function XYZ modes.
matrix --xyz-mode-y n       Specify function comparitor mode for Y. See function XYZ modes.
matrix --xyz-mode-z n       Specify function comparitor mode for Z. See function XYZ modes.
matrix --pwm0 n             Set switch -s uS time off period (0uS = remain on)
matrix --pwm1 n             Set switch -s uS time on period  (0uS = remain off after on)
matrix --flux n             Set switch -s output fluctuation threshold.
matrix --oride n            Override switch -s output values.
matrix --uvalue n           Set switch -s user output value.
matrix --map-slot n         Set switch -s output as map slot n value.
matrix --omode n            Set switch -s output mode: (0 : matrix logic) (1 : mapped value analog/digital).
matrix -p n                 Set GPIOPE port slot for switch -s.
matrix --gpiope n           Set GPIOPE I2C address for switch -s.
matrix --computer-assist n  Enable/disable computer assist for switch -s.
```

---

## Multiplexer

```
admplex0 -c n --enable    Enable channel n on ADMPlex0 (read every task cycle, subject to --freq).
admplex0 -c n --disable   Disable channel n on ADMPlex0 (data reports NAN while disabled).
admplex0 -c n --freq uS   Minimum microseconds between reads of channel n (0 = read every task cycle).
admplex0 --all --freq uS  Set every channel's freq in one call.
admplex1 -c n --enable    Enable channel n on ADMPlex1.
admplex1 -c n --disable   Disable channel n on ADMPlex1.
admplex1 -c n --freq uS   Minimum microseconds between reads of channel n on ADMPlex1.
admplex1 --all --freq uS  Set every channel's freq in one call.
```

**Example** — run admplex0 channel 3 at ~1Hz alongside the rest of the enabled channels:

```
admplex0 -c 3 --enable --freq 1000000
```

---

## GPIO Port Expander: Input

```
gpiope --input             Point GPIOPE at input devices.
gpiope -a                  Specify address -a.
gpiope -c n --enable       Enable pin n on the input port controller (read every task cycle, subject to --freq).
gpiope -c n --disable      Disable pin n on the input port controller (data reports 0 while disabled).
gpiope -c n --freq uS      Minimum microseconds between reads of pin n (0 = read every task cycle).
gpiope --all --enable      Enable every pin in one call.
gpiope --all --disable     Disable every pin in one call.
gpiope --all --freq uS     Set every pin's freq in one call.
```

**Example** — run gpiope pin 5 at ~1Hz alongside the rest of the enabled pins:

```
gpiope -c 5 --enable --freq 1000000
```

---

## GPIO Port Expander: Output

```
gpiope --output                     Point GPIOPE at ouptut devices.
gpiope -a                           Specify address -a.
gpiope -i                           Specify port map index -i.
gpiope -p                           Specify pin number -p.
gpiope -pwm0                        Specify PWM off time in microseconds.
gpiope -pwm1                        Specify PWM on time in microseconds. 
```

**Example** — Set portmap slot 0 with a pin number 54 for address 9:

```
gpiope -a 9 -i 0 -p 54
```

**Example** — Set portmap slot 0 PWM for address 9:

```
gpiope -a 9 -i 0 --pwm0 0 --pwm1 0
```

**Extra Example** — Point matrix switch 0 at GPIOPE address 9, slot 0:

```
matrix -s 0 --gpiope 9 -p 0
```

---

## INS

Customizable Inertial navigation system.

```
ins -m n              Set INS mode n. (0 : Off) (1 : Dynamic, set by gps every 100ms) (2 : Fixed, remains on after conditions met).
ins -gyro n           INS uses gyro for attitude. (0 : gyro heading) (1 : gps heading).
ins -p n              Set INS minimum required gps precision factor to initialize.
ins -s n              Set INS minimum required speed to initialize.
ins -r n              Set INS maximum required heading range difference to initialize (difference between gps heading and gyro heading).
ins --reset-forced    Reset INS remains on after conditions met.
```

---

### Time

```
satio --utc-offset n             Set +-seconds offset time.
satio --auto-datetime-on         Enable set datetime automatically  (--auto-datetime-on overrides any datetime -set).
satio --auto-datetime-off        Disable set datetime automatically (ensure --auto-datetime-off before using -set time).
satio --set-datetime --year n --month n --mday n --hour n --minute n --second n  (expects UTC +- 0).
```

### Location

```
satio --coord-value-mode-gps      Use GPS coordinates.
satio --coord-value-mode-user     Use user defined coordinates.
satio --set-coord -lat n -lon n   Set degrees latitude and longitude.
```

### Speed

```
satio --speed-value-mode-gps   Use GPS speed.
satio --speed-value-mode-user  Use user defined speed.
satio --set-speed n            Set speed in meters per second.
```

### Altitude

```
satio --altitude-value-mode-gps   Use GPS altitude values.
satio --altitude-value-mode-user  Use user defined altitude.
satio --set-altitude n            Set altitude in meters.
```

### Ground Heading

```
satio --ground-heading-value-mode-gps   Use GPS ground heading values.
satio --ground-heading-value-mode-user  Use user defined ground heading.
satio --set-ground-heading n            Set ground heading in degrees.
```

### RA/Dec

```
satio --ra-dec-value-mode-gyro  Use gyro-derived RA/Dec.
satio --ra-dec-value-mode-user  Use user defined RA/Dec target.
satio --set-ra-dec --ra-h n --ra-m n --ra-s n --dec-d n --dec-m n --dec-s n  Set user RA/Dec target (ra-h 0-23, dec-d -90 to 90).
```

---

## Gyro

```
gyro --calacc        Calibrate the accelerometer.
gyro --calmag-start  Begin calibrating the magnetometer.
gyro --calmag-end    End calibrating the magnetometer.
```

---

## SDCard

*(currently disabled via CLI while being used internally)*

```
sdcard --mount
sdcard --unmount
```

---

## Performance

```
powercfg --power-saving          Sets power configuration to low power consumption mode.
powercfg --power-balanced        Sets power configuration to balanced.
powercfg --ultimate-performance  Sets power configuration to ultimate performance mode.

powercfg --setdelay --admplex0   Specify max task frequency in uS.
powercfg --setdelay --admplex1   Specify max task frequency in uS.
powercfg --setdelay --gyro0      Specify max task frequency in uS.
powercfg --setdelay --universe   Specify max task frequency in uS.
powercfg --setdelay --gps        Specify max task frequency in uS.
powercfg --setdelay --switch     Specify max task frequency in uS.
powercfg --setdelay --storage    Specify max task frequency in uS.
powercfg --setdelay --gpiope    Specify max task frequency in uS.
```

**Example:**

```
powercfg --setdelay --admplex0 1000 --gyro0 200 --gps 10
```

---

## StarNav

```
starnav RA_HOUR RA_MIN RA_SEC DEC_D DEC_M DEC_S
```

**Example:**

```
starnav 6 45 8.9 -16 42 58.0
```

---

## Stat

Stat can be useful for providing real time, real world data to other systems, devices, microcontrollers and LLM's.

For integrity, Stat sentences are checksummed, so that weather using them over UART or over a radio, data integrity can be checked and verified.

Stat is also setup for diagnositics.

```
stat -e                     Enable print.
stat -d                     Disable print.
stat -t                     Enables/disables serial print stats and counters (includes partition table, RAM, and SD card info). Takes arguments -e, -d.
stat -t --datetime          Toggles the datetime table. Takes arguments -e, -d.
stat -t --taskrates         Toggles the task rates (Hz) table. Takes arguments -e, -d.
stat -t --position          Toggles the position/target and RA/Dec tables. Takes arguments -e, -d.
stat -t --gyro              Toggles the orientation/sensors (gyro) table. Takes arguments -e, -d.
stat -t --admplex           Toggles the ADMPlex per-channel Hz table(s). Takes arguments -e, -d.
stat -t --gpiope            Toggles the GPIOPE input per-channel table(s). Takes arguments -e, -d.
stat -t --matrix            Toggles the Computer Assist / matrix table. Takes arguments -e, -d.
stat --system               Print system configuration.
stat --matrix n             Print matrix switch n configuration.
stat --matrix -A            Print configuration of all matrix switches.
stat -map n                 Print map slot n data.
stat -map -A                Print all map slot data.
stat --sentence -A          Print all sentences. Takes arguments -e, -d.
stat --sentence --satio     Takes arguments -e, -d.
stat --sentence --gngga     Takes arguments -e, -d.
stat --sentence --gnrmc     Takes arguments -e, -d.
stat --sentence --gpatt     Takes arguments -e, -d.
stat --sentence --matrix    Takes arguments -e, -d.
stat --sentence --gpiope    Takes arguments -e, -d.
stat --sentence --admplex0  Takes arguments -e, -d.
stat --sentence --admplex1  Takes arguments -e, -d.
stat --sentence --gyro0     Takes arguments -e, -d.
stat --sentence --sun       Takes arguments -e, -d.
stat --sentence --earth     Takes arguments -e, -d.
stat --sentence --luna      Takes arguments -e, -d.
stat --sentence --mercury   Takes arguments -e, -d.
stat --sentence --venus     Takes arguments -e, -d.
stat --sentence --mars      Takes arguments -e, -d.
stat --sentence --jupiter   Takes arguments -e, -d.
stat --sentence --saturn    Takes arguments -e, -d.
stat --sentence --uranus    Takes arguments -e, -d.
stat --sentence --neptune   Takes arguments -e, -d.
stat --sentence --meteors   Takes arguments -e, -d.
stat --sentence --xmatrix   Print/toggle matrix-config sentence output. Takes arguments -e, -d.
stat --sentence --xmap      Print/toggle mapping-config sentence output. Takes arguments -e, -d.
```

---

## Other

```
-v    Enable verbosity.
-vv   Enable extra verbosity.
help
```

---

## To Do

- [ ] AI I2C modules returning int's as classifiers.
- [ ] SRTM data. Use NASA shuttle radar topographical mission data for ground elevation in meters.
- [ ] PCB fabrication.

---
