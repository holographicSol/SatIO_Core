# SatIO

![plot](../SDCARD/UnidentifiedStudios.png)

*Written by Benjamin Jack Cullen — project may be renamed to SatIO.*

SatIO is a Value Hive & Programmable Switch, for building devices with and or on top of.

---

## Philosophy & Architecture

**The Hive** — SatIO creates and stores sensory data and extrapolated sensor data, that can be used by SatIO to switch I/O via programmable compounds of logic, and or can be simply passed out over serial for another device to read.

**The Matrix** — This is a scalable, programmable switch that utilizes values from across the Hive, to perform calculations that result in either true or false. High/Low output is determined by the result. Devices built on top of SatIO can be distinguished by their matrix configurations, being different devices for different applications, and similar in that each of them are running on and or with SatIO.
The switches can be used for, digital output, analog output, mapped output. For driving a peripheral, providing a peripheral with event triggers, or simply lighting up an led. With an ATMEGA2560 as the output controller, there are up to +-60 programmable matrix switches available, that's +-60 pins for SatIO to drive, through the programmable matrix logic.

### Philosophy

1. **Value Creation** — Safety, Stability, Accuracy. As SatIO develops, so should value creation, leading to safer, more accurate values in the hive over time. This can create a desirable improvement curve where in contrast, building a system from scratch every new project, has the potential to reintroduce bugs and 're-solve' the same problems. As SatIO develops, so does any system built on top of SatIO. Value creation should ideally be both read and write to and from the Hive.
2. **Value Utilization** — The Matrix utilizes the values to switch output high/low and or analog. Serial output also utilizes the values by handing the values out to other devices on a bus. Value utilization should ideally only ever read from the Hive.

### Matrix Philosophy

1. **Elemental** — Allow comparing any value from the Hive to any other value from the Hive and or to a user defined value.
2. **Compound** — Allow stacking compounds of (1), so that multiple things can be calculated to result in a single true/false.
3. **I/O** — Each available/required output pin can have it's own (2) Compound logic.

**Why:** This means that many 'special functions' do not need to be created in order to calculate something, because the answer may already exist, via some combination of available logic in the Matrix.

**Further more:** Inference via Bayesian Reasoning? Moon tracking for example can be used to track the moon, it can also be used for one example; to track the tide, if the system is aware of moon/planetary positioning and datetime then marine life values may also be inferred relative to the inferred tide values and known datetime. There are lot of values in the Hive, that can be used with different reasoning, in many different combinations, with a network effect in mind for inference, if required. Or more simply — *'SatIO is one hell of a switch'*.

---

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
mapping --save
mapping --load
mapping --delete
mapping -s n       Specify map slot n.
mapping -m n       Specify slot -s mode. (0 : map min to max) (1 : center map x0) (2 : center map x1)
mapping -c0 n      Configuration map slot -s  value to map. See available map values.
mapping -c1 n      Configuration map slot -s. (mode 0 : in_min)  (mode 1 : approximate center value)
mapping -c2 n      Configuration map slot -s. (mode 0 : in_max)  (mode 1 : Neg_range : 0 to approximate center value)
mapping -c3 n      Configuration map slot -s. (mode 0 : out_min) (mode 1 : Pos_range : ADC max - neg range)
mapping -c4 n      Configuration map slot -s. (mode 0 : out_max) (mode 1 : out_max)
mapping -c5 n      Configuration map slot -s. (mode 1 only : DEADZONE : expected fluctuation at center)
```

**Example** — map analog stick axis x0 on admplex0 channel 0 into map slot 0:

```
mapping -s 0 -m 1 -c0 16 -c1 1974 -c2 1974 -c3 1894 -c4 255 -c5 50
```

**Example** — map analog stick axis x1 on admplex0 channel 1 into map slot 1:

```
mapping -s 1 -m 2 -c0 17 -c1 1974 -c2 1974 -c3 1894 -c4 255 -c5 50
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
matrix -p n                 Set port for switch -s.
matrix -fn n                Set function -f for switch -s. See available matrix functions.
matrix -fx n                Set function -f value x for switch -s.
matrix -fy n                Set function -f value y for switch -s.
matrix -fz n                Set function -f value z for switch -s.
matrix -fi n                Set function -f logic inverted for switch -s.
matrix -fo n                Set function -f operator for switch -s.
matrix --pwm0 n             Set switch -s uS time off period (0uS = remain on)
matrix --pwm1 n             Set switch -s uS time on period  (0uS = remain off after on)
matrix --flux n             Set switch -s output fluctuation threshold.
matrix --oride n            Override switch -s output values.
matrix --computer-assist n  Enable/disable computer assist for switch -s.
matrix --omode n            Set switch -s output mode: (0 : matrix logic) (1 : mapped value analog/digital).
matrix --map-slot n         Set switch -s output as map slot n value.
```

**Example** — default all switch configurations:

```
matrix --new
```

**Example** — stat switch zero:

```
stat --matrix 0
```

**Example** — set switch zero:

```
matrix -s 0 -f 0 -p 33 -fn 85 -fx 1 -fo 1 --pwm0 1000000 --pwm1 15000 --computer-assist 1
```

**Example** — set mapped output mode:

```
matrix -s 0 --omode 1
```

**Example** — set matrix logic output mode:

```
matrix -s 0 --omode 0
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
ins --reset-forced n  Reset INS remains on after conditions met.
```

---

## SatIO

```
SatIO --coord-update-mode-gps     Use GPS latitude, longitude values.
SatIO --coord-update-mode-static  Do not update latitude, longitude unless --set-coord or otherwise.
SatIO --set-coord -lat n -lon n   Set degrees latitude and longitude (ensure --coord-update-mode-static before --set-coord).
SatIO --utc-offset n              Set +-seconds offset time.
SatIO --auto-datetime-on          Enable set datetime automatically  (--auto-datetime-on overrides any datetime -set).
SatIO --auto-datetime-off         Disable set datetime automatically (ensure --auto-datetime-off before using -set time).
SatIO --set-datetime --year n --month n --mday n --hour n --minute n --second n  (must be UTC except if utc offset 0).

SatIO --speed-mode-gps     Use GPS speed values.
SatIO --speed-mode-static  Do not update speed unless --set-speed or otherwise.
SatIO --set-speed n        Set speed in meters per second (ensure --speed-mode-static before --set-speed).
SatIO --speed-unit-KTS     Use default knots.
SatIO --speed-unit-KPH     Convert knots per second to K/PH.
SatIO --speed-unit-MPH     Convert knots per second to M/PH.
SatIO --speed-unit-mPS     Convert knots per second to meters per second.

SatIO --altitude-mode-gps         Use GPS altitude values.
SatIO --altitude-mode-static      Do not update speed unless --set-altitude or otherwise.
SatIO --set-altitude n            Set altitude in meters (ensure --altitude-mode-static before --set-altitude).
SatIO --altitude-unit-meters      Use default meters altitude.
SatIO --altitude-unit-kilometers  Convert meters to kilometers.
SatIO --altitude-unit-miles       Convert meters to miles.

SatIO --ground-heading-update-mode-gps     Use GPS ground heading values.
SatIO --ground-heading-update-mode-static  Do not update heading unless --set-ground-heading or otherwise.
SatIO --set-ground-heading                 Set ground heading in degrees (0-360. Ensure --ground-heading-update-mode-static before --ground-heading).
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

setdelay --admplex0               Specify max task frequency in uS.
setdelay --gyro0                  Specify max task frequency in uS.
setdelay --universe               Specify max task frequency in uS.
setdelay --gps                    Specify max task frequency in uS.
setdelay --switch                 Specify max task frequency in uS.
setdelay --storage                Specify max task frequency in uS.
```

**Example:**

```
setdelay --admplex0 20 --gyro0 200 --gps 10
```

---

## Stat

```
stat -e     Enable print.
stat -d     Disable print.
stat -t     Enables/disables serial print stats and counters. Takes arguments -e, -d.
stat --partition-table      Print partition table.
stat --memory-ram           Print ram information.
stat --sdcard               Print matrix information.
stat --system               Print system configuration.
stat --matrix               Print matrix configuration.
stat --matrix n             Print matrix switch n configuration.
stat --matrix -A            Print configuration of all matrix switches.
stat --mapping              Print configuration of all mapping slots.
stat --sentence -A          Print all sentences. Takes arguments -e, -d.
stat --sentence --SatIO     Takes arguments -e, -d.
stat --sentence --ins       Takes arguments -e, -d.
stat --sentence --gngga     Takes arguments -e, -d.
stat --sentence --gnrmc     Takes arguments -e, -d.
stat --sentence --gpatt     Takes arguments -e, -d.
stat --sentence --matrix    Takes arguments -e, -d.
stat --sentence --pcinput   Takes arguments -e, -d.
stat --sentence --admplex0  Takes arguments -e, -d.
stat --sentence --gyro0     Takes arguments -e, -d.
stat --sentence --sun       Takes arguments -e, -d.
stat --sentence --mercury   Takes arguments -e, -d.
stat --sentence --venus     Takes arguments -e, -d.
stat --sentence --earth     Takes arguments -e, -d.
stat --sentence --luna      Takes arguments -e, -d.
stat --sentence --mars      Takes arguments -e, -d.
stat --sentence --jupiter   Takes arguments -e, -d.
stat --sentence --saturn    Takes arguments -e, -d.
stat --sentence --uranus    Takes arguments -e, -d.
stat --sentence --neptune   Takes arguments -e, -d.
stat --sentence --meteors   Takes arguments -e, -d.
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
- [ ] SRTM data. Use NASA shuttle radar topographical mission data.
- [ ] Ability to add custom IIC sensor modules after flashing.
- [ ] PCB fabrication.
- [ ] UI after migration complete.

---

**Zip:** [drive.google.com/.../SatIO](https://drive.google.com/drive/folders/13yynSxkKL-zxb7iLSkg0v0VXkSLgmtW-?usp=drive_link)
