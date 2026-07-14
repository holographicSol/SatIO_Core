# SatIO

![plot](./SDCARD/UnidentifiedStudios.png)

*Written by Benjamin Jack Cullen — project may be renamed to SatIO.*

SatIO is a Value Hive & Programmable Switch, for building devices with and or on top of.

---

## Philosophy & Architecture

**The Hive** — SatIO creates and stores sensory data and extrapolated sensor data, that can be used by SatIO to switch I/O via programmable compounds of logic, and or can be simply passed out over serial for another device to read.

**The Matrix** — This is a scalable, programmable switch that utilizes values from across the Hive, to perform calculations that result in either true or false. High/Low output is determined by the result. Devices built on top of SatIO can be distinguished by their matrix configurations, being different devices for different applications, and similar in that each of them is running on and or with SatIO.
The switches can be used for, digital output, analog output, mapped output. For driving a peripheral, providing a peripheral with event triggers, or simply lighting up a led. With an ATMEGA2560 as the output controller, there are up to +-60 programmable matrix switches available, that's +-60 pins for SatIO to drive, through the programmable matrix logic.

### Philosophy

1. **Value Creation** — Safety, Stability, Accuracy. As SatIO develops, so should value creation, leading to safer, more accurate values in the hive over time. This can create a desirable improvement curve where in contrast, building a system from scratch every new project, has the potential to reintroduce bugs and 're-solve' the same problems. As SatIO develops, so does any system built on top of SatIO. Value creation should ideally be both read and write to and from the Hive.
2. **Value Utilization** —
- **The Matrix** utilizes the values from across the system, to switch output high/low/analog/mapped, according to programmable
   logic whereby any value in the system can be compared to any other value in the system and or comapred to a user defined
   value. The comaprisons use some basic operators, <>==, and in range. Every value in the matrix is treated as a double
   and there are around 120 system values that can be used in the matrix as 'matrix functions'. Each matrix function accepts function values as X,Y,Z, that are also stored in a matrix (See usage below for more matrix programmability options). This means that the Matrix combinational potential is considerably high, and all possible combinations and all possible reasonings for any given combination may be impossible for one person to ever know or comprehend (see below for Inference via Bayesian Reasoning). The matrix is the core of SatIO and SatIO's potential, it makes SatIO a computer, however it does not make SatIO Turing Complete.
   Each Matrix Switch can hold more than one Matrix Function, all of which must result in true, in order to switch, and locic can run in an inverted mode, that inverts
   Matrix Function return boolean. Switches can be linked to other switches by using a special Matrix Function called Switch Link, which allows logic to be stacked
   accross switches and again inverted and or further gated, using the same or different output port. All Matrix Switches have programmable output ports and are -1 by default. Logic can be modulated up to int64_max, for PWM on N co-processor(s) that strictly handle I/O and modulation.
-  **Computer Assist Bool** is automation, and takes control over a specified output port according to how the matrix switch for that port is configured. Note
   that a switch is rendered fully automatic by Computer Assist being enabled for the same the same switch, meaning the computer will decide when to
   switch, how to modulate the switch, and why, all determined from the users programmable logic in the Matrix.
-  **Computer Intention Bool** provides insight into what the computer has calculated. Weather or not Computer Assist is enabled, Computer Intention is
   visible for every switch, allowing a user or other system to see what the computer wants to do, with and or without actually doing it. Computer Assist
   can be enabled at any point for any switch, before and or during Computer Intent true/false. This 'layer' also fascilitates optional design choices being made, like the potential for semi-automatic functionality on a switch by switch basis, whereby Computer intention can be used as an indicator and the user
   can ultimately decide weather or not to switch, by flipping Computer Assist on/off.
-  **Switch Intention Bool** is set according to Computer Intention, providing Computer Assist is enabled, for the same switch. Computer intention is
   always set, Switch Intention is only ever set if Computer Assist is enabled for the same switch.
   **Output Values** can be digital (directly derived from switch intention), or analog from a mapped value in the system. Varoius output modes determine
   what values will be output if and when a Switch Intention true.
   **Mapped Values** supports user defined mapping from a selection of values in the system. Mapped values can be used in two ways, [1] with the intention
   of using the mapped value in a Matrix Switch Function as a comparitor, and or [2] with the intention to use the mapped value as an Output Value. Map slots are not aligned with Matrix Switch slots, so that multiple Matrix Switches and or Output values can utilize any map slot, simultaniously, without needing to map anything twice or more, unecessarily. This means that a map slot index was required for Matrix Switches, whereby the map slot index values ARE aligned with Matrix Switches, and the values within the Index, point to user defined Map Slots.
3. **Dynamic/Static/Simulation** — System values like time, location, altitude, speed, etc can be set
from real (dynamic) sensor data and or can be individually specified by the user (static). This allows
for various options and scenarios like running as a station, simulation, and or where dynamically updating
the system values is not an option, like for example if there is no GPS. The system always uses system values,
and the system values are set according to a values mode: GPS, Gyro, User. This means that calculations
(like many calculations in the Universe task for example) that depend on certain values, can still function correctly, if the correct values are input manually and or automated input from another system.

### Matrix Philosophy

1. **Elemental** — Allow comparing any value from the Hive to any other value from the Hive and or to a user defined value.
2. **Compound** — Allow stacking compounds of (1), so that multiple things can be calculated to result in a single true/false.
3. **I/O** — Each available/required output pin can have it's own (2) Compound logic.

**Why:** This means that many 'special functions' do not need to be created in order to calculate something, because the answer may already exist, via some combination of available logic in the Matrix.

**Further more:** Inference via Bayesian Reasoning? Moon tracking for example can be used to track the moon, it can also be used for one example; to track the tide, if the system is aware of moon/planetary positioning and datetime then marine life values may also be inferred relative to the inferred tide values and known datetime. There are lot of values in the Hive, that can be used with different reasoning, in many different combinations, with a network effect in mind for inference, if required. Or more simply — *'SatIO is one hell of a switch'*.

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



## Matrix Switch Logic

Logic may require or not require values X, Y, Z. All of the following values can be used in the matrix.

### Primary Comparators

```
[0]   NONE
[1]   ON
[2]   Switch Link
[3]   Time HHMMSS
[4]   Week Day
[5]   Month Day
[6]   Month
[7]   Year
[8]   SatIO Deg Lat
[9]   SatIO Deg Lon
[10]  SatIO INS Lat
[11]  SatIO INS Lon
[12]  SatIO INS Heading
[13]  SatIO INS Alt
[14]  GNGGA Status
[15]  GNGGA Sat Count
[16]  GNGGA Precision
[17]  GNGGA Altitude
[18]  GNRMC Ground Speed
[19]  GNRMC Heading
[20]  GPATT Line
[21]  GPATT Static
[22]  GPATT Run State
[23]  GPATT INS
[24]  GPATT Mileage
[25]  GPATT GST
[26]  GPATT Yaw
[27]  GPATT Roll
[28]  GPATT Pitch
[29]  GNGGA Valid CS
[30]  GNRMC Valid CS
[31]  GPATT Valid CS
[32]  GNGGA Valid CD
[33]  GNRMC Valid CD
[34]  GPATT Valid CD
[35]  GNRMC Pos Stat A
[36]  GNRMC Pos Stat V
[37]  GNRMC Mode Ind A
[38]  GNRMC Mode Ind D
[39]  GNRMC Mode Ind E
[40]  GNRMC Mode Ind N
[41]  GNRMC Hemi North
[42]  GNRMC Hemi South
[43]  GNRMC Hemi East
[44]  GNRMC Hemi West
[45]  G0 G-Force X
[46]  G0 G-Force Y
[47]  G0 G-Force Z
[48]  G0 Incline X
[49]  G0 Incline Y
[50]  G0 Incline Z
[51]  G0 Mag Field X
[52]  G0 Mag Field Y
[53]  G0 Mag Field Z
[54]  G0 Velocity X
[55]  G0 Velocity Y
[56]  G0 Velocity Z
[57]  Meteor
[58]  Sun Azimuth
[59]  Sun Altitude
[60]  Sun Helio Ecl Lat
[61]  Sun Helio Ecl Lon
[62]  Luna Azimuth
[63]  Luna Altitude
[64]  Luna Phase
[65]  Mercury Azimuth
[66]  Mercury Altitude
[67]  Mercury H.Ecliptic Lat
[68]  Mercury H.Ecliptic Lon
[69]  Mercury Ecliptic Lat
[70]  Mercury Ecliptic Lon
[71]  Venus Azimuth
[72]  Venus Altitude
[73]  Venus H.Ecliptic Lat
[74]  Venus H.Ecliptic Lon
[75]  Venus Ecliptic Lat
[76]  Venus Ecliptic Lon
[77]  Earth Ecliptic Lon
[78]  Mars Azimuth
[79]  Mars Altitude
[80]  Mars H.Ecliptic Lat
[81]  Mars H.Ecliptic Lon
[82]  Mars Ecliptic Lat
[83]  Mars Ecliptic Lon
[84]  Jupiter Azimuth
[85]  Jupiter Altitude
[86]  Jupiter H.Ecliptic Lat
[87]  Jupiter H.Ecliptic Lon
[88]  Jupiter Ecliptic Lat
[89]  Jupiter Ecliptic Lon
[90]  Saturn Azimuth
[91]  Saturn Altitude
[92]  Saturn H.Ecliptic Lat
[93]  Saturn H.Ecliptic Lon
[94]  Saturn Ecliptic Lat
[95]  Saturn Ecliptic Lon
[96]  Uranus Azimuth
[97]  Uranus Altitude
[98]  Uranus H.Ecliptic Lat
[99]  Uranus H.Ecliptic Lon
[100] Uranus Ecliptic Lat
[101] Uranus Ecliptic Lon
[102] Neptune Azimuth
[103] Neptune Altitude
[104] Neptune H.Ecliptic Lat
[105] Neptune H.Ecliptic Lon
[106] Neptune Ecliptic Lat
[107] Neptune Ecliptic Lon
[108] AD Multiplexer 0
[109] Map Slot
[110] SD Card Inserted
[111] SD Card Mounted
[112] Port Con 0
[113] Local Mean Solar Time
[114] Local Mean Solar Date
[115] Local Sidereal Time
[116] Local Zenith RA
[117] Local Zenith Dec
[118] Gyro 0 RA
[119] Gyro 0 Dec
```

---

## Available Switch Function Operators

Many matrix functions accept operators, where required:

```
[0] None
[1] Equal
[2] Over
[3] Under
[4] Range
```

---

## Function XYZ Modes

```
[0] User Value.   A value that is set by the user.
[1] System Value. A value that is set by the system.
```

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

Many values can be mapped and then used in the matrix and or sent directly to the port controller.

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

### Available Map Values

```
[0]  Digital
[1]  YawGPATT
[2]  RollGPATT
[3]  PitchGPATT
[4]  Gyro0AccX
[5]  Gyro0AccY
[6]  Gyro0AccZ
[7]  Gyro0AngX
[8]  Gyro0AngY
[9]  Gyro0AngZ
[10] Gyro0MagX
[11] Gyro0MagY
[12] Gyro0MagZ
[13] Gyro0GyroX
[14] Gyro0GyroY
[15] Gyro0GyroZ
[16] ADMPlex0_0
[17] ADMPlex0_1
[18] ADMPlex0_2
[19] ADMPlex0_3
[20] ADMPlex0_4
[21] ADMPlex0_5
[22] ADMPlex0_6
[23] ADMPlex0_7
[24] ADMPlex0_8
[25] ADMPlex0_9
[26] ADMPlex0_10
[27] ADMPlex0_11
[28] ADMPlex0_12
[29] ADMPlex0_13
[30] ADMPlex0_14
[31] ADMPlex0_15
```

**Example** — map analog stick axis x0 on admplex0 channel 0 into map slot 0:

```
map -s 0 -m 1 -c0 16 -c1 1974 -c2 1974 -c3 1894 -c4 255 -c5 50
```

**Example** — map analog stick axis x1 on admplex0 channel 1 into map slot 1:

```
map -s 1 -m 2 -c0 17 -c1 1974 -c2 1974 -c3 1894 -c4 255 -c5 50
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
matrix --computer-assist n  Enable/disable computer assist for switch -s.
matrix --omode n            Set switch -s output mode: (0 : matrix logic) (1 : mapped value analog/digital).
matrix --map-slot n         Set switch -s output as map slot n value.
matrix -p n                 Set GPIOPE port slot for switch -s.
matrix --gpiope n           Set GPIOPE I2C address for switch -s.
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
SatIO --utc-offset n             Set +-seconds offset time.
SatIO --auto-datetime-on         Enable set datetime automatically  (--auto-datetime-on overrides any datetime -set).
SatIO --auto-datetime-off        Disable set datetime automatically (ensure --auto-datetime-off before using -set time).
SatIO --set-datetime --year n --month n --mday n --hour n --minute n --second n  (expects UTC +- 0).
```

### Location

```
SatIO --coord-value-mode-gps      Use GPS coordinates.
SatIO --coord-value-mode-user     Use user defined coordinates.
SatIO --set-coord -lat n -lon n   Set degrees latitude and longitude.
```

### Speed

```
SatIO --speed-value-mode-gps   Use GPS speed.
SatIO --speed-value-mode-user  Use user defined speed.
SatIO --set-speed n            Set speed in meters per second.
```

### Altitude

```
SatIO --altitude-value-mode-gps   Use GPS altitude values.
SatIO --altitude-value-mode-user  Use user defined altitude.
SatIO --set-altitude n            Set altitude in meters.
```

### Ground Heading

```
SatIO --ground-heading-value-mode-gps   Use GPS ground heading values.
SatIO --ground-heading-value-mode-user  Use user defined ground heading.
SatIO --set-ground-heading n            Set ground heading in degrees.
```

### RA/Dec

```
SatIO --ra-dec-value-mode-gyro  Use gyro-derived RA/Dec.
SatIO --ra-dec-value-mode-user  Use user defined RA/Dec target.
SatIO --set-ra-dec --ra-h n --ra-m n --ra-s n --dec-d n --dec-m n --dec-s n  Set user RA/Dec target (ra-h 0-23, dec-d -90 to 90).
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

*(currently disabled)*

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
powercfg --setdelay --gpiope0    Specify max task frequency in uS.
```

**Example:**

```
powercfg --setdelay --admplex0 1000 --gyro0 200 --gps 10
```

---

## StarNav

*(currently disabled)*

```
starnav RA_HOUR RA_MIN RA_SEC DEC_D DEC_M DEC_S
```

**Example:**

```
starnav 6 45 8.9 -16 42 58.0
```

---

## Stat

```
stat -e     Enable print.
stat -d     Disable print.
stat -t     Enables/disables serial print stats and counters (includes partition table, RAM, and SD card info). Takes arguments -e, -d.
stat --system               Print system configuration.
stat --matrix n             Print matrix switch n configuration.
stat --matrix -A            Print configuration of all matrix switches.
stat -map n                 Print map slot n data.
stat -map -A                Print all map slot data.
stat --sentence -A          Print all sentences. Takes arguments -e, -d.
stat --sentence --SatIO     Takes arguments -e, -d.
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

**Zip:** [drive.google.com/.../SatIO](https://drive.google.com/drive/folders/13yynSxkKL-zxb7iLSkg0v0VXkSLgmtW-?usp=drive_link)

