/*
    Serial Information Command - Written By Benjamin Jack Cullen

    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#include "UnidentifiedStudios_CMD.h"
#include <Arduino.h>

#include <FS.h>
#include "SD_MMC.h"
#include "SPIFFS.h"
#include <stdlib.h>
#include <limits.h>
#include <float.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <rtc_wdt.h>
#include <esp_task_wdt.h>
#include "UnidentifiedStudios_SatIOFile.h"
#include "UnidentifiedStudios_Matrix.h"
#include "UnidentifiedStudios_Config.h"
#include "UnidentifiedStudios_TaskHandler.h"
#include "UnidentifiedStudios_Config.h"
#include "UnidentifiedStudios_SystemData.h"
#include "UnidentifiedStudios_Multiplexers.h"
#include "UnidentifiedStudios_GPIOPortExpander.h"
#include "UnidentifiedStudios_Matrix.h"
#include "UnidentifiedStudios_WTGPS300P.h"
#include "UnidentifiedStudios_WT901.h"
#include "UnidentifiedStudios_Multiplexers.h"
#include "UnidentifiedStudios_Mapping.h"
#include "UnidentifiedStudios_SiderealHelper.h"
#include "UnidentifiedStudios_SatIO.h"
#include "UnidentifiedStudios_INS.h"
#include "UnidentifiedStudios_Meteors.h"
#include "UnidentifiedStudios_HexToDig.h"
#include "UnidentifiedStudios_SdCardHelper.h"
#include "UnidentifiedStudios_ArgParser.h"
#include "UnidentifiedStudios_SystemData.h"

bool debug_bool=true;
void debug(String debug_str) {if (debug_bool==true) {printf("%s\n", debug_str.c_str());}}

ArgParser parser;
PlainArgParser plainparser;

#ifdef SatIO_SERIAL_TX_OPTION_NEW_TASK
struct Serial0Struct serial0Data = {
  .BUFFER_RX = {0},
  .BUFFER_TX = {0},
};
#endif

#ifdef SatIO_SERIAL_TX_OPTION_CURRENT_TASK
struct Serial0Struct serial0Data = {
  .BUFFER_RX = {0},
  .BUFFER_TX_GPS = {0},
  .BUFFER_TX_ADMPLEX0 = {0},
  .BUFFER_TX_ADMPLEX1 = {0},
  .BUFFER_TX_GYRO0 = {0},
  .BUFFER_TX_UNI = {0},
  .BUFFER_TX_SWITCHES = {0},
  .BUFFER_TX_GPIOPE = {0},
};
#endif

/* Rule 8.7: internal linkage; only validateChecksumSerial0()/createChecksumSerial0() call this. */
static int getCheckSumSerial0(const char *string)
{
    size_t len = strlen(string);
    size_t i;
    int result = 0;

    /* XOR every byte between the leading '$' and the trailing '*'. i is the
       sole loop control variable (Rule 14.2). */
    for (i = 0U; i < len; i++)
    {
        unsigned char c = (unsigned char)string[i];

        if (c == '*')
        {
            break; /* single break in this loop (Rule 15.4) */
        }
        if (c != '$')
        {
            result ^= c;
        }
    }

    return result;
}

/* Rule 8.7: internal linkage; only used within this file. */
static bool validateChecksumSerial0(const char *buffer)
{
    bool result;
    size_t len = strlen(buffer);

    if (len < 3U)
    {
        printf("validateChecksumSerial0: buffer too short (len=%u): \"%s\"\n", (unsigned)len, buffer);
        result = false;
    }
    else
    {
        char checksum_chars[2];
        int checksum_of_buffer;
        int16_t checksum_in_buffer;

        /* The last 2 characters before the sentence's final byte are the
           transmitted checksum's hex digits. */
        checksum_chars[0] = buffer[len - 3U];
        checksum_chars[1] = buffer[len - 2U];

        checksum_of_buffer = getCheckSumSerial0(buffer);
        checksum_in_buffer = h2d2(checksum_chars[0], checksum_chars[1]);

        if (checksum_in_buffer == H2D_INVALID_VALUE)
        {
            printf("validateChecksumSerial0: invalid hex digits in checksum field: '%c%c' buffer=\"%s\"\n",
                   checksum_chars[0], checksum_chars[1], buffer);
            result = false;
        }
        else if ((int)checksum_in_buffer == checksum_of_buffer)
        {
            result = true;
        }
        else
        {
            printf("validateChecksumSerial0: checksum mismatch (calc=0x%02X, recv=0x%02X) buffer=\"%s\"\n",
                   checksum_of_buffer, checksum_in_buffer, buffer);
            result = false;
        }
    }

    return result; /* Rule 15.5: single point of exit */
}

/* Rule 8.7: internal linkage; only used within this file.
   Writes into a caller-supplied buffer (rather than a shared struct field)
   so that concurrent callers on different tasks can never clobber each
   other's checksum between computing it and appending it to their sentence. */
static void createChecksumSerial0(const char *buffer, char *checksum_out, size_t checksum_out_size)
{
    int checksum_of_buffer = getCheckSumSerial0(buffer);

    /* Always exactly 2 hex digits (checksum_of_buffer is a byte-wide XOR, so
       0x00-0xFF fits): validateChecksumSerial0() and h2d2() both assume the
       sentence ends "*XX" and read the last 2 characters unconditionally, so
       an unpadded single digit (e.g. "*7") would misalign that read. */
    (void)snprintf(checksum_out, checksum_out_size, "%02X", checksum_of_buffer);
}

bool val_global_element_size(const char *data)
{
    return strlen(data) <= (size_t)MAX_GLOBAL_ELEMENT_SIZE; /* Rule 15.5: single point of exit */
}

bool val_switch_index(const char *data)
{
    return (str_is_int8(data) == true) && (atol(data) < MAX_MATRIX_SWITCHES);
}

bool val_function_index(const char *data)
{
    return (str_is_int8(data) == true) && (atol(data) < MAX_MATRIX_SWITCH_FUNCTIONS);
}

bool val_function_name_index(const char *data)
{
    return (val_global_element_size(data) == true) && (str_is_long(data) == true) &&
           (atol(data) < MAX_MATRIX_FUNCTION_NAMES);
}

bool val_function_xyz(const char *data)
{
    return str_is_double(data);
}

bool val_function_operator(const char *data)
{
    return str_is_int8(data);
}

bool val_switch_port(const char *data)
{
    return str_is_int8(data);
}

bool val_mappable_value_index(const char *data)
{
    return (str_is_int8(data) == true) && (atoi(data) < MAX_MAPPABLE_VALUES);
}

bool val_map_slot_index(const char *data)
{
    return (str_is_int8(data) == true) && (atoi(data) < MAX_MAP_SLOTS);
}

bool val_ins_mode(const char *data)
{
    return (str_is_int8(data) == true) && (atol(data) < MAX_INS_MODE);
}

bool val_ins_gps_precision(const char *data)
{
    return str_is_float(data);
}

bool val_ins_minimum_speed(const char *data)
{
    return str_is_float(data);
}

bool val_ins_heading_range_diff(const char *data)
{
    return str_is_float(data);
}

static void PrintHelp(void) {
  puts(
  R"(
  [ System ]

      system --save               Takes no further arguments.
      system --load               Takes no further arguments.
      system --restore-defaults   Takes no further arguments.
      system -log                 Automatically log data to disk (See performance for timing). Takes arguments -e, -d.

  [ Matrix ]

      matrix --new                Clears matrix in memory.
      matrix --save n             Specify file slot.
      matrix --load n             Specify file slot.
      matrix --delete n           Specify file slot.
      matrix --startup-enable
      matrix --startup-disable
      matrix -s n                 Specify switch index n.
      matrix -f n                 Specify function index n.
      matrix -fn n                Set function -f for switch -s. Primary Comparitors:
                                  [0] NONE
                                  [1] ON
                                  [2] Switch Link
                                  [3] Time HHMMSS
                                  [4] Week Day
                                  [5] Month Day
                                  [6] Month
                                  [7] Year
                                  [8] SatIO Deg Lat
                                  [9] SatIO Deg Lon
                                  [10] SatIO INS Lat
                                  [11] SatIO INS Lon
                                  [12] SatIO INS Heading
                                  [13] SatIO INS Alt
                                  [14] GNGGA Status
                                  [15] GNGGA Sat Count
                                  [16] GNGGA Prescion
                                  [17] GNGGA Altitude
                                  [18] GNRMC Ground Speed
                                  [19] GNRMC Heading
                                  [20] GPATT Line
                                  [21] GPATT Static
                                  [22] GPATT Run State
                                  [23] GPATT INS
                                  [24] GPATT Mileage
                                  [25] GPATT GST
                                  [26] GPATT Yaw
                                  [27] GPATT Roll
                                  [28] GPATT Pitch
                                  [29] GNGGA Valid CS
                                  [30] GNRMC Valid CS
                                  [31] GPATT Valid CS
                                  [32] GNGGA Valid CD
                                  [33] GNRMC Valid CD
                                  [34] GPATT Valid CD
                                  [35] GNRMC Pos Stat A
                                  [36] GNRMC Pos Stat V
                                  [37] GNRMC Mode Ind A
                                  [38] GNRMC Mode Ind D
                                  [39] GNRMC Mode Ind E
                                  [40] GNRMC Mode Ind N
                                  [41] GNRMC Hemi North
                                  [42] GNRMC Hemi South
                                  [43] GNRMC Hemi East
                                  [44] GNRMC Hemi West
                                  [45] G0 G-Force X
                                  [46] G0 G-Force Y
                                  [47] G0 G-Force Z
                                  [48] G0 Incline X
                                  [49] G0 Incline Y
                                  [50] G0 Incline Z
                                  [51] G0 Mag Field X
                                  [52] G0 Mag Field Y
                                  [53] G0 Mag Field Z
                                  [54] G0 Velocity X
                                  [55] G0 Velocity Y
                                  [56] G0 Velocity Z
                                  [57] Meteor
                                  [58] Sun Azimuth
                                  [59] Sun Altitude
                                  [60] Sun Helio Ecl Lat
                                  [61] Sun Helio Ecl Lon
                                  [62] Luna Azimuth
                                  [63] Luna Altitude
                                  [64] Luna Phase
                                  [65] Mercury Azimuth
                                  [66] Mercury Altitude
                                  [67] Mercury H.Ecliptic Lat
                                  [68] Mercury H.Ecliptic Lon
                                  [69] Mercury Ecliptic Lat
                                  [70] Mercury Ecliptic Lon
                                  [71] Venus Azimuth
                                  [72] Venus Altitude
                                  [73] Venus H.Ecliptic Lat
                                  [74] Venus H.Ecliptic Lon
                                  [75] Venus Ecliptic Lat
                                  [76] Venus Ecliptic Lon
                                  [77] Earth Ecliptic Lon
                                  [78] Mars Azimuth
                                  [79] Mars Altitude
                                  [80] Mars H.Ecliptic Lat
                                  [81] Mars H.Ecliptic Lon
                                  [82] Mars Ecliptic Lat
                                  [83] Mars Ecliptic Lon
                                  [84] Jupiter Azimuth
                                  [85] jupiter Altitude
                                  [86] Jupiter H.Ecliptic Lat
                                  [87] Jupiter H.Ecliptic Lon
                                  [88] Jupiter Ecliptic Lat
                                  [89] Jupiter Ecliptic Lon
                                  [90] Saturn Azimuth
                                  [91] Saturn Altitude
                                  [92] Saturn H.Ecliptic Lat
                                  [93] Saturn H.Ecliptic Lon
                                  [94] Saturn Ecliptic Lat
                                  [95] Saturn Ecliptic Lon
                                  [96] Uranus Azimuth
                                  [97] Uranus Altitude
                                  [98] Uranus H.Ecliptic Lat
                                  [99] Uranus H.Ecliptic Lon
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
      matrix --xyz-mode-x         Specify function comparitor mode. Used with -s and -f.
      matrix --xyz-mode-y         Specify function comparitor mode. Used with -s and -f.
      matrix --xyz-mode-z         Specify function comparitor mode. Used with -s and -f.
                                  [0] User Value. A value that is set by the user.
                                  [1] System Value. A value that is set by the system. Allows primary comparitors to be compared to other primary comparitors.
                                  In mode 0, user defined value is compared to a primary comparitor.
                                  In mode 1, primary comparitors are compared to each other. 
      matrix -fx n                Set function -f value x for switch -s.
      matrix -fy n                Set function -f value y for switch -s.
      matrix -fz n                Set function -f value z for switch -s.
      matrix -fi n                Set function -f logic inverted for switch -s.
      matrix -fo n                Set function -f operator for switch -s.
                                  [0] None
                                  [1] Equal
                                  [2] Over
                                  [3] Under
                                  [4] Range
      matrix --flux n             Set switch -s output fluctuation threshold.
      matrix --oride n            Override switch -s output values.
      matrix --uvalue n           Set switch -s user output value.
      matrix --map-slot n         Set switch -s output as map slot n value.
      matrix --omode n            Set switch -s output mode: (0 : matrix logic) (1 : mapped value analog/digital).
      matrix -p n                 Set port slot for switch -s (Pins in slots are defined by gpiope command).  
      matrix --gpiope n           Set GPIOPE I2C address for switch -s.
      matrix --computer-assist n  Enable/disable computer assist for switch -s.

      example set matrix logic 0 function 0 to detect sdcard mounted:
      matrix -s 0 -f 0 -p 33 -fn 85 -fx 1 -fo 1 --pwm0 1000000 --pwm1 15000 --computer-assist 1
      matrix -s 0 --omode 0

  [ Mapping ]

      map --new      Clears all mapping in memory.
      map --save
      map --load
      map --delete
      map -s n       Specify map slot n.
      map -m n       Specify slot -s mode. (0 : map min to max) (1 : center map x0) (2 : center map x1)
      map -c0 n      Configuration map slot -s value to map. See available map values.
                     [0] Digital
                     [1] YawGPATT
                     [2] RollGPATT
                     [3] PitchGPATT
                     [4] Gyro0AccX
                     [5] Gyro0AccY
                     [6] Gyro0AccZ
                     [7] Gyro0AngX
                     [8] Gyro0AngY
                     [9] Gyro0AngZ
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
      map -c1 n      Configuration map slot -s. (mode 0 : in_min)  (mode 1 : approximate center value)
      map -c2 n      Configuration map slot -s. (mode 0 : in_max)  (mode 1 : Neg_range : 0 to approximate center value)
      map -c3 n      Configuration map slot -s. (mode 0 : out_min) (mode 1 : Pos_range : ADC max - neg range)
      map -c4 n      Configuration map slot -s. (mode 0 : out_max) (mode 1 : out_max)
      map -c5 n      Configuration map slot -s. (mode 1 only : DEADZONE : expected flutuation at center)

      example map analog stick axis x0 on admplex0 channel 0 into map slot 0:
      map -s 0 -m 1 -c0 16 -c1 1974 -c2 1974 -c3 1894 -c4 255 -c5 50
      matrix -s 0 --map-slot 0
      Optional: matrix -s 0 --omode 1

      example map analog stick axis x1 on admplex0 channel 1 into map slot 1:
      map -s 1 -m 2 -c0 17 -c1 1974 -c2 1974 -c3 1894 -c4 255 -c5 50
      matrix -s 1 --map-slot 1
      Optional: matrix -s 1 --omode 1

  [ Multiplexer ]

      admplex0 -c n --enable   Enable channel n on ADMPlex0 (read every task cycle, subject to --freq).
      admplex0 -c n --disable  Disable channel n on ADMPlex0 (data reports NAN while disabled).
      admplex0 -c n --freq uS  Minimum microseconds between reads of channel n (0 = read every task cycle).
      admplex1 -c n --enable   Enable channel n on ADMPlex1.
      admplex1 -c n --disable  Disable channel n on ADMPlex1.
      admplex1 -c n --freq uS  Minimum microseconds between reads of channel n on ADMPlex1.

      example: run admplex0 channel 3 at ~1Hz alongside the rest of the enabled channels:
      admplex0 -c 3 --enable --freq 1000000

  [ Port Expander Input ]

      gpiope --input             Point GPIOPE at input devices.
      gpiope -c n --enable       Enable pin n on the input port expander (read every task cycle, subject to --freq).
      gpiope -c n --disable      Disable pin n on the input port expander (data reports 0 while disabled).
      gpiope -c n --freq uS      Minimum microseconds between reads of pin n (0 = read every task cycle).
      gpiope --all --enable      Enable every pin in one call.
      gpiope --all --disable     Disable every pin in one call.
      gpiope --all --freq uS     Set every pin's freq in one call.

      example: run gpiope pin 5 at ~1Hz alongside the rest of the enabled pins:
      gpiope -c 5 --enable --freq 1000000

  [ Port Expander Output ]

      gpiope --output                     Point GPIOPE at ouptut devices.
      gpiope -a                           Specify address -a.
      gpiope -i                           Specify port map index -i.
      gpiope -p                           Specify pin number -p.
      gpiope -pwm0                        Specify PWM off time in microseconds.
      gpiope -pwm1                        Specify PWM on time in microseconds.

      Example set portmap slot 0 with a pin number 54 for address 9:
      gpiope -a 9 -i 0 -p 54

      Example set portmap slot 0 PWM for address 9:
      gpiope -a 9 -i 0 --pwm0 0 --pwm1 0

      Extra example point matrix switch 0 at GPIOPE address 9, slot 0:
      matrix -s 0 --gpiope 9 -p 0

  [ INS ]

      ins -m n              Set INS mode n. (0 : Off) (1 : Dynamic, set by gps every 100ms) (2 : Fixed, remains on after conditions met).
      ins -gyro n           INS uses gyro for attitude. (0 : gyro heading) (1 : gps heading).
      ins -p n              Set INS minmum required gps precision factor to initialize (higher requires less gps precision).
      ins -s n              Set INS minmum required speed to initialize (lower requires less speed).
      ins -r n              Set INS minmum required range difference difference between gps heading and gyro heading (higher allows more difference).
      ins --reset-forced    Reset INS remains on after conditions met.

  [ SatIO ]

      [ Time ]
      satio --utc-offset n             Set +-seconds offset time.
      satio --auto-datetime-on         Enable set datetime automatically  (--auto-datetime-on overrides any datetime -set).
      satio --auto-datetime-off        Disable set datetime automatically (ensure --auto-datetime-off before using -set time).
      satio --set-datetime --year n --month n --mday n --hour n --minute n --second n  (expects UTC +- 0).

      [ Location ]
      satio --coord-value-mode-gps             Use GPS coordinates.
      satio --coord-value-mode-user            User user defined coordinates.
      satio --set-coord -lat n -lon n          Set degrees latitude and longitude.

      [ Speed ]
      satio --speed-value-mode-gps             Use GPS speed.
      satio --speed-value-mode-user            User user defined speed.
      satio --set-speed n                      Set speed in meters per second.

      [ Altitude ]
      satio --altitude-value-mode-gps          Use GPS altitude values.
      satio --altitude-value-mode-user         User user defined altitude.
      satio --set-altitude n                   Set altitude in meters.
      
      [ Ground Heading ]
      satio --ground-heading-value-mode-gps    Use GPS ground heading values.
      satio --ground-heading-value-mode-user   User user defined ground heading.
      satio --set-ground-heading               Set ground heading in degrees.

      [ RA/Dec ]
      satio --ra-dec-value-mode-gyro           Use gyro-derived RA/Dec.
      satio --ra-dec-value-mode-user           Use user defined RA/Dec target.
      satio --set-ra-dec --ra-h n --ra-m n --ra-s n --dec-d n --dec-m n --dec-s n  Set user RA/Dec target (ra-h 0-23, dec-d -90 to 90).

  [ Gyro ]

      gyro --calacc        Callibrate the accelerometer.
      gyro --calmag-start  Begin calibrating the magnetometer.
      gyro --calmag-end    End calibrating the magnetometer.

  [ SDCard ] (currently disabled)

      sdcard --mount
      sdcard --unmount

  [ PERFORMANCE ]

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

      example: powercfg --setdelay --admplex0 20 --gyro0 200 --gps 10

  [ StarNav ] (currently disabled)

      starnav RA_HOUR RA_MIN RA_SEC DEC_D DEC_M DEC_S

      example: starnav 6 45 8.9 -16 42 58.0

  [ Stat ]

      stat -e     Enable print.
      stat -d     Disable print.
      stat -t     Enables/disables serial print stats and counters (includes partition table, RAM, and SD card info). Takes arguments -e, -d.
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

  [ Other ]

      -v    Enable verbosoity.
      -vv   Enable extra verbosoity.
      help
  )"
  );
}

/* Rule 8.7: internal linkage; these debug-print helpers are only called from CmdProcess() in this file. */
static void PrintSystemData(void) {
    printf("-----------------------------------------------------\n");
    printf("[System] \n");
    printf("[serial_command] %d\n", systemData.serial_command);
    printf("[output_satio_all] %d\n", systemData.output_satio_all);
    printf("[output_satio_enabled] %d\n", systemData.output_satio_enabled);
    printf("[output_gngga_enabled] %d\n", systemData.output_gngga_enabled);
    printf("[output_gnrmc_enabled] %d\n", systemData.output_gnrmc_enabled);
    printf("[output_gpatt_enabled] %d\n", systemData.output_gpatt_enabled);
    printf("[output_matrix_enabled] %d\n", systemData.output_matrix_enabled);
    printf("[output_input_portcontroller] %d\n", systemData.output_input_portcontroller);
    printf("[output_admplex0_enabled] %d\n", systemData.output_admplex0_enabled);
    printf("[output_admplex1_enabled] %d\n", systemData.output_admplex1_enabled);
    printf("[output_gyro_0_enabled] %d\n", systemData.output_gyro_0_enabled);
    printf("[output_sun_enabled] %d\n", systemData.output_sun_enabled);
    printf("[output_luna_enabled] %d\n", systemData.output_luna_enabled);
    printf("[output_mercury_enabled] %d\n", systemData.output_mercury_enabled);
    printf("[output_venus_enabled] %d\n", systemData.output_venus_enabled);
    printf("[output_mars_enabled] %d\n", systemData.output_mars_enabled);
    printf("[output_jupiter_enabled] %d\n", systemData.output_jupiter_enabled);
    printf("[output_saturn_enabled] %d\n", systemData.output_saturn_enabled);
    printf("[output_uranus_enabled] %d\n", systemData.output_uranus_enabled);
    printf("[output_neptune_enabled] %d\n", systemData.output_neptune_enabled);
    printf("[output_meteors_enabled] %d\n", systemData.output_meteors_enabled);
    printf("-----------------------------------------------------\n");
}

static void PrintSatIOData(void) {
    printf("-----------------------------------------------------\n");
    printf("[SatIO] \n");
    printf("[utc_second_offset] %lld\n", SatIOData.systemTime.second_offset);
    printf("[utc_auto_offset_flag] %d\n", SatIOData.systemTime.auto_offset_flag);
    printf("[set_time_automatically] %d\n", SatIOData.systemTime.set_time_automatically);
    printf("-----------------------------------------------------\n");
}

static void PrintMappingNData(int map_slot) {
  if (map_slot>=0 && map_slot<MAX_MAP_SLOTS) {
    printf("-----------------------------------------------------\n");
    printf("[slot] %d\n", map_slot);
    printf("[map_mode] %d\n", mappingData.map_mode[0][map_slot]);
    printf("[map slot idx] %d\n", (int)matrixData.index_mapped_value[0][map_slot]);
    printf("[map config 0] %ld\n", mappingData.mapping_config[0][map_slot][0]);
    printf("[map config 1] %ld\n", mappingData.mapping_config[0][map_slot][1]);
    printf("[map config 2] %ld\n", mappingData.mapping_config[0][map_slot][2]);
    printf("[map config 3] %ld\n", mappingData.mapping_config[0][map_slot][3]);
    printf("[map config 4] %ld\n", mappingData.mapping_config[0][map_slot][4]);
    printf("[map config 5] %ld\n", mappingData.mapping_config[0][map_slot][5]);
    printf("-----------------------------------------------------\n");
  }
}

static void PrintMappingData(void) {
  for (int Mi=0; Mi<MAX_MAP_SLOTS; Mi++) {PrintMappingNData(Mi);}
}

static void PrintMatrixNData(int matrix_index) {
  if (matrix_index>=0 && matrix_index<MAX_MATRIX_SWITCHES) {
    printf("-----------------------------------------------------\n");
    printf("[matrix switch] %d\n", matrix_index);
    printf("[computer assist] %d\n", matrixData.computer_assist[0][matrix_index]);
    printf("[output mode] %d\n", (int)matrixData.output_mode[0][matrix_index]);
    printf("[user output value] %ld\n", matrixData.user_output_value[0][matrix_index]);
    printf("[flux] %lu\n", matrixData.flux_value[0][matrix_index]);
    // printf("[pwm] 0: %lu 1: %lu\n",
      // matrixData.output_pwm[0][matrix_index][0],
      // matrixData.output_pwm[0][matrix_index][1]);
    printf("[gpiope port slot] %d\n", matrixData.matrix_port_map[0][matrix_index]);
    printf("[active] %d\n", matrixData.switch_intention[0][matrix_index]);
    printf("-----------------------------------------------------\n");
    for (int Fi=0; Fi<MAX_MATRIX_SWITCH_FUNCTIONS; Fi++) {
      //                                         matrixData.matrix_function[0][switch_idx][func_idx]
      printf("[function %d name] %s\n",
        Fi, matrixData.matrix_function_names[ matrixData.matrix_function[0][matrix_index][Fi] ]);
      printf("[function %d matrix_function_operator_name] %d\n",
        Fi, (int)matrixData.matrix_switch_operator_index[0][matrix_index][Fi]);
      printf("[function %d inverted] %d\n",
        Fi, matrixData.matrix_switch_inverted_logic[0][matrix_index][Fi]);
      printf("[function %d x] %f\n",
        Fi, matrixData.matrix_function_xyz[0][matrix_index][Fi][INDEX_MATRIX_FUNTION_X]);
      printf("[function %d y] %f\n",
        Fi, matrixData.matrix_function_xyz[0][matrix_index][Fi][INDEX_MATRIX_FUNTION_Y]);
      printf("[function %d z] %f\n",
        Fi, matrixData.matrix_function_xyz[0][matrix_index][Fi][INDEX_MATRIX_FUNTION_Z]);
      printf("-----------------------------------------------------\n");
    }
  }
}

static void PrintMatrixData(void) {
    for (int Mi=0; Mi<MAX_MATRIX_SWITCHES; Mi++) {PrintMatrixNData(Mi);}
}

void setAllSentenceOutput(bool enable) {
  systemData.output_satio_all = enable;
  systemData.output_satio_enabled=enable;
  systemData.output_gngga_enabled=enable;
  systemData.output_gnrmc_enabled=enable;
  systemData.output_gpatt_enabled=enable;
  systemData.output_matrix_enabled=enable;
  systemData.output_input_portcontroller=enable;
  systemData.output_admplex0_enabled=enable;
  systemData.output_admplex1_enabled=enable;
  systemData.output_gyro_0_enabled=enable;
  systemData.output_sun_enabled=enable;
  systemData.output_mercury_enabled=enable;
  systemData.output_venus_enabled=enable;
  systemData.output_earth_enabled=enable;
  systemData.output_luna_enabled=enable;
  systemData.output_mars_enabled=enable;
  systemData.output_jupiter_enabled=enable;
  systemData.output_saturn_enabled=enable;
  systemData.output_uranus_enabled=enable;
  systemData.output_neptune_enabled=enable;
  systemData.output_meteors_enabled=enable;
}

void setOutputPortControllerAddress(int switch_idx, int address) {
  if ((switch_idx >= 0 && switch_idx < MAX_MATRIX_SWITCHES) && (address >=0 && address < 128)) {
    matrixData.gpiope_address[0][switch_idx] = address;
  }
}

void setMatrixGPIOPEPortSlot(int switch_idx, int slot_n) {
  if (switch_idx>=0 && switch_idx<MAX_MATRIX_SWITCHES && slot_n>=-1 && slot_n<MAX_MATRIX_SWITCHES) {
    matrixData.matrix_port_map[0][switch_idx]=slot_n;
    matrixData.matrix_switch_write_required[0][switch_idx]=true;
  }
}

void setMatrixFunction(int switch_idx, int func_idx, int func_n) {
  printf("func_n %d\n", func_n);
  if (switch_idx>=0 && switch_idx<MAX_MATRIX_SWITCHES && func_idx>=0 && func_idx<MAX_MATRIX_SWITCH_FUNCTIONS &&
      func_n>=0 && func_n<MAX_MATRIX_FUNCTION_NAMES) {
    matrixData.matrix_function[0][switch_idx][func_idx]=func_n;
    matrixData.matrix_switch_write_required[0][switch_idx]=true;
  }
  printf("matrix_function %d\n", (int)matrixData.matrix_function[0][switch_idx][func_idx]);
}

/* X/Y/Z comparitor-mode setters only ever differed by which xyz axis index
   they wrote; collapsed into one axis-parameterized function. */
void setMatrixFunctionComparitorMode(int switch_idx, int func_idx, int axis, int comparitor_mode) {
  if (switch_idx>=0 && switch_idx<MAX_MATRIX_SWITCHES && func_idx>=0 && func_idx<MAX_MATRIX_SWITCH_FUNCTIONS &&
      comparitor_mode>=0 && comparitor_mode<MAX_MATRIX_FUNCTION_XYZ_MODES) {
    matrixData.matrix_function_mode_xyz[0][switch_idx][func_idx][axis]=comparitor_mode;
    matrixData.matrix_switch_write_required[0][switch_idx]=true;
  }
}

/* setMatrixX/Y/Z only ever differed by which xyz axis index they wrote;
   collapsed into one axis-parameterized function. */
void setMatrixXYZ(int switch_idx, int func_idx, int axis, double value) {
  if (switch_idx>=0 && switch_idx<MAX_MATRIX_SWITCHES && func_idx>=0 && func_idx<MAX_MATRIX_SWITCH_FUNCTIONS &&
      value>=-DBL_MAX && value<DBL_MAX) {
    matrixData.matrix_function_xyz[0][switch_idx][func_idx][axis]=value;
    matrixData.matrix_switch_write_required[0][switch_idx]=true;
  }
}

void setMatrixInverted(int switch_idx, int func_idx, int func_i) {
  if (switch_idx>=0 && switch_idx<MAX_MATRIX_SWITCHES && func_idx>=0 && func_idx<MAX_MATRIX_SWITCH_FUNCTIONS && func_i>=0 && func_i<=1) {
    matrixData.matrix_switch_inverted_logic[0][switch_idx][func_idx]=func_i;
    matrixData.matrix_switch_write_required[0][switch_idx]=true;
  }
}

void setMatrixOperator(int switch_idx, int func_idx, int func_o) {
  if (switch_idx>=0 && switch_idx<MAX_MATRIX_SWITCHES && func_idx>=0 && func_idx<MAX_MATRIX_SWITCH_FUNCTIONS && func_o>=0 && func_o<MAX_MATRIX_OPERATORS) {
    matrixData.matrix_switch_operator_index[0][switch_idx][func_idx]=func_o;
    matrixData.matrix_switch_write_required[0][switch_idx]=true;
  }
}

void setUserOutputValue(int switch_idx, int32_t user_value) {
  if (switch_idx>=0 && switch_idx<MAX_MATRIX_SWITCHES && user_value>=INT32_MIN && user_value<=INT32_MAX) {
    matrixData.user_output_value[0][switch_idx]=user_value;
    matrixData.matrix_switch_write_required[0][switch_idx]=true;
  }
}

void setFlux(int switch_idx, uint32_t flux) {
  if (switch_idx>=0 && switch_idx<MAX_MATRIX_SWITCHES && flux<LONG_MAX) {
    matrixData.flux_value[0][switch_idx]=flux;
    matrixData.matrix_switch_write_required[0][switch_idx]=true;
  }
}

void setOutputMode(int switch_idx, int output_mode) {
  if (switch_idx>=0 && switch_idx<MAX_MATRIX_SWITCHES && output_mode>=0 && output_mode<MAX_MATRIX_OUTPUT_MODES) {
    matrixData.output_mode[0][switch_idx]=output_mode;
    matrixData.matrix_switch_write_required[0][switch_idx]=true;
  }
}

/* Retries assigning value to field until it reads back as set, up to
   MAX_MATRIX_OVERRIDE_TIME attempts; used by setOverrideOutputValue() to
   force a write against any concurrent writer of the same field. */
template <typename T>
static void forceAssign(T &field, T value, const char *what) {
  long i_retry = 0;
  field = value;
  while (field != value) {
    field = value;
    i_retry += 1;
    if (i_retry == MAX_MATRIX_OVERRIDE_TIME) {printf("WARNING! Could not override %s!\n", what); break;}
    delayMicroseconds(1);
  }
}

void setOverrideOutputValue(int switch_idx, uint32_t override_value) {
  printf("[setOverrideOutputValue] attempting to override switch %d\n", switch_idx);

  /* override_value is unsigned, so only the upper bound is a real check
     (Rule 14.3, an unsigned value is never < 0). */
  if (switch_idx < 0) {printf("[setOverrideOutputValue] switch_idx must be >= 0.\n");}
  else if (switch_idx >= MAX_MATRIX_SWITCHES) {printf("[setOverrideOutputValue] switch_idx must be < %d.\n", MAX_MATRIX_SWITCHES);}
  else if (override_value > LONG_MAX) {printf("[setOverrideOutputValue] override_value must be <= %ld.\n", LONG_MAX);}
  else {
    forceAssign(matrixData.computer_assist[0][switch_idx], false, "computer_assist");
    forceAssign(matrixData.override_output_value[0][switch_idx], (int32_t)override_value, "override_output_value");
    matrixData.matrix_switch_write_required[0][switch_idx]=true;
    printf("[setOverrideOutputValue] computer_assist %d\n", matrixData.computer_assist[0][switch_idx]);
    printf("[setOverrideOutputValue] override_output_value %ld\n", matrixData.override_output_value[0][switch_idx]);
    printf("[setOverrideOutputValue] matrix_switch_write_required %d\n", matrixData.matrix_switch_write_required[0][switch_idx]);
  }
}

void setComputerAssist(int switch_idx, bool computer_assist) {
  debug("[setComputerAssist] switch_idx:" + String(switch_idx) + "  computer_assist: " + String(computer_assist));
  debug("[setComputerAssist] current computer_assist: " + String(matrixData.computer_assist[0][switch_idx]));
  if (switch_idx>=0 && switch_idx<MAX_MATRIX_SWITCHES) {
    matrixData.computer_assist[0][switch_idx]=computer_assist;
    matrixData.matrix_switch_write_required[0][switch_idx]=true;
  }
  else {debug("[setComputerAssist] failed to change computer assist!");}
  debug("[setComputerAssist] computer_assist: " + String(matrixData.computer_assist[0][switch_idx]));
}

void setINSMode(int ins_mode) {
  if (ins_mode>=0 && ins_mode <MAX_INS_MODE)
    {insData.INS_MODE=ins_mode;}
}

void setINSUseGyroHeading(int use_gyro_heading) {
  if (use_gyro_heading>=0 && use_gyro_heading<=1)
    {insData.INS_USE_GYRO_HEADING=use_gyro_heading;}
}

void setINSGPSPrecision(double ins_precision) {
  if (ins_precision>=0.0 && ins_precision<DBL_MAX)
    {insData.INS_REQ_GPS_PRECISION=ins_precision;}
}

void setINSMinSpeed(double ins_min_speed) {
  if (ins_min_speed>=0.0 && ins_min_speed<DBL_MAX)
    {insData.INS_REQ_MIN_SPEED=ins_min_speed;}
}

void setINSHeadingRangeDiff(double ins_range_diff) {
  if (ins_range_diff>=0.0 && ins_range_diff<DBL_MAX)
    {insData.INS_REQ_HEADING_RANGE_DIFF=ins_range_diff;}
}

void setUTCSecondOffset(int64_t seconds) {
  /* LONG_LONG_MIN/LONG_LONG_MAX are not standard macros (the standard names
     are LLONG_MIN/LLONG_MAX); flagged here rather than changed. */
  if (seconds>=LONG_LONG_MIN && seconds<LONG_LONG_MAX)
    {SatIOData.systemTime.second_offset=seconds;}
}

void setMapConfig(int map_slot,
                  int map_mode,
                  signed long c0,
                  signed long c1,
                  signed long c2,
                  signed long c3,
                  signed long c4,
                  signed long c5) {
  if (map_slot>=0 && map_slot<MAX_MAP_SLOTS &&
      map_mode>=0 && map_mode<MAX_MAP_MODES &&
      c0>=INT32_MIN && c0<INT32_MAX &&
      c1>=INT32_MIN && c1<INT32_MAX &&
      c2>=INT32_MIN && c2<INT32_MAX &&
      c3>=LONG_MIN && c3<INT32_MAX &&
      c4>=INT32_MIN && c4<INT32_MAX &&
      c5>=INT32_MIN && c5<INT32_MAX) {
    mappingData.mapping_config[0][map_slot][0]=c0;
    mappingData.mapping_config[0][map_slot][1]=c1;
    mappingData.mapping_config[0][map_slot][2]=c2;
    mappingData.mapping_config[0][map_slot][3]=c3;
    mappingData.mapping_config[0][map_slot][4]=c4;
    mappingData.mapping_config[0][map_slot][5]=c5;
    mappingData.map_mode[0][map_slot]=map_mode;
    matrixData.matrix_switch_write_required[0][map_slot]=true;
  }
}

void setMapSlot(int matrix_switch,
                int map_slot) {
  // Serial.println("[setMapSlot] matrix_switch:" + String(matrix_switch) + "  map_slot:" + String(map_slot));
  if (matrix_switch>=0 && matrix_switch<MAX_MATRIX_SWITCHES &&
      map_slot>=0 && map_slot<MAX_MAP_SLOTS) {
    matrixData.index_mapped_value[0][matrix_switch]=map_slot;
    matrixData.matrix_switch_write_required[0][matrix_switch]=true;
  }
}

void saveMatrix(int matrix_file_slot) {
  if (matrix_file_slot>=0 && matrix_file_slot<MAX_MATRIX_FILE_SLOTS) {
    SatIOFileData.i_current_matrix_file_path=matrix_file_slot;
    sdcardFlagData.save_matrix=true;
  }
}

void loadMatrix(int matrix_file_slot) {
  if (matrix_file_slot>=0 && matrix_file_slot<MAX_MATRIX_FILE_SLOTS) {
    SatIOFileData.i_current_matrix_file_path=matrix_file_slot;
    sdcardFlagData.load_matrix=true;
  }
}

void deleteMatrix(int matrix_file_slot) {
  if (matrix_file_slot>=0 && matrix_file_slot<MAX_MATRIX_FILE_SLOTS) {
    SatIOFileData.i_current_matrix_file_path=matrix_file_slot;
    sdcardFlagData.delete_matrix=true;
  }
}

void datetimeSetRTC(uint16_t year, uint8_t month, uint8_t mday,
                            uint8_t hour, uint8_t min, uint8_t sec) {
  /*
  SatIO --utc-offset n       Set +-seconds offset time.
  SatIO --auto-datetime-on   Enable set datetime automatically
  SatIO --auto-datetime-off  Disable set datetime automatically (-set requires auto off prior to calling set)
  SatIO --set-datetime --year n --month n --mday n --hour n --minute n --second n  (must be UTC except if utc offset 0)
  SatIO --set-datetime --year 25 --month 10 --mday 16 --hour 5 --minute 0 --second 0
  */
  /* All parameters are unsigned: only the upper bound is a real check
     (Rule 14.3, an unsigned value is never < 0). */
  if (year <UINT16_MAX &&
      month<UINT8_MAX  &&
      mday <UINT8_MAX  &&
      hour <UINT8_MAX  &&
      min  <UINT8_MAX  &&
      sec  <UINT8_MAX) {
    SatIOData.tmp_year_int=year;
    SatIOData.tmp_month_int=month;
    SatIOData.tmp_day_int=mday;
    SatIOData.tmp_hour_int=hour;
    SatIOData.tmp_minute_int=min;
    SatIOData.tmp_second_int=sec;
    // set update flag
  }
}

void datetimeSetDTAuto(bool set_dt_auto) {
  if (set_dt_auto==true) {SatIOData.systemTime.sync_immediately_flag=true; SatIOData.systemTime.set_time_automatically =true;}
  else {SatIOData.systemTime.set_time_automatically=false;}
}

void setCoordinatesDegrees(double latitude, double longitude) {
  if (latitude>=-90.0 && latitude<=90.0 && longitude>=-180.0 && longitude<=180.0) {
    SatIOData.user_degrees_latitude=latitude;
    SatIOData.user_degrees_longitude=longitude;
  }
}

void setAltitude(double altitude) {
  if (altitude>=-DBL_MAX && altitude<DBL_MAX && isnan(altitude)==false) {
    SatIOData.user_altitude=altitude;
  }
}

void setSpeed(double speed) {
  if (speed>=0.0 && speed<DBL_MAX && isnan(speed)==false) {
    SatIOData.user_speed=speed;
  }
}

void setGroundHeading(double heading) {
  if (heading>=-360.0 && heading<=360.0 && isnan(heading)==false) {
    SatIOData.user_ground_heading=heading;
  }
}

void setRaDec(int ra_h, int ra_m, float ra_s, int dec_d, int dec_m, float dec_s) {
  if (ra_h>=0 && ra_h<24 && ra_m>=0 && ra_m<60 && ra_s>=0.0f && ra_s<60.0f &&
      dec_d>=-90 && dec_d<=90 && dec_m>=0 && dec_m<60 && dec_s>=0.0f && dec_s<60.0f) {
    SatIOData.user_sidereal_attitude.ra_h = ra_h;
    SatIOData.user_sidereal_attitude.ra_m = ra_m;
    SatIOData.user_sidereal_attitude.ra_s = ra_s;
    SatIOData.user_sidereal_attitude.dec_d = dec_d;
    SatIOData.user_sidereal_attitude.dec_m = dec_m;
    SatIOData.user_sidereal_attitude.dec_s = dec_s;

    snprintf(SatIOData.user_sidereal_attitude.formatted_ra_str,  sizeof(SatIOData.user_sidereal_attitude.formatted_ra_str),  "%02d:%02d:%02.2f",  ra_h,  ra_m,  ra_s);
    snprintf(SatIOData.user_sidereal_attitude.formatted_dec_str, sizeof(SatIOData.user_sidereal_attitude.formatted_dec_str), "%+02d:%02d:%02.2f", dec_d, dec_m, dec_s);
    snprintf(SatIOData.user_sidereal_attitude.padded_ra_str,     sizeof(SatIOData.user_sidereal_attitude.padded_ra_str),     "%02d%02d%02.2f",    ra_h,  ra_m,  ra_s);
    snprintf(SatIOData.user_sidereal_attitude.padded_dec_str,    sizeof(SatIOData.user_sidereal_attitude.padded_dec_str),    "%+02d%02d%02.2f",   dec_d, dec_m, dec_s);
  }
}

/* Rule 8.7: internal linkage; only referenced (currently commented out
   pending exclusive r/w access, see CmdProcess()) within this file. */
static void star_nav(void) {
  // star sirius test: starnav 6 45 8.9 -16 42 58.0
  // ngc test:         starnav 2 20 35.0 -23 7 0.0
  // ic test:          starnav 17 46 18 5 43 00
  // other obj test:   starnav 1 36 0 61 17 0
  // messier test:     starnav 16 41 40 36 28 0
  // caldwel test:     starnav 1 19 32.6 58 17 27
  // Herschel test:    starnav 0 29 56.0 60 14 0.0
  // new stars test    starnav 0 02 07.2 -14 40 34
  // caldwel test:     starnav 00 13 0 72 32 0
  simple_argparser_init_from_buffer(&plainparser, serial0Data.BUFFER_RX, 1);
  if ((str_is_int8(plainparser.tokens[0])==true) &&
      (str_is_int8(plainparser.tokens[1])==true) &&
      (str_is_float(plainparser.tokens[2])==true) &&
      (str_is_int8(plainparser.tokens[3])==true) &&
      (str_is_int8(plainparser.tokens[4])==true) &&
      (str_is_float(plainparser.tokens[5])==true)
    )
  {
    printf("attempting to identify object..\n");
    // Local instance, not the shared siderealObjectSingle global: a CLI-triggered
    // identify must not clobber whatever setStarNav()/starNavSweep() last stored there.
    SiderealObjectSingle cli_obj = siderealObjectSingle;
    // this is identify (so first identify object)
    IdentifyObject(
      &cli_obj,
      atoi(plainparser.tokens[0]),
      atoi(plainparser.tokens[1]),
      atof(plainparser.tokens[2]),
      atoi(plainparser.tokens[3]),
      atoi(plainparser.tokens[4]),
      atof(plainparser.tokens[5])
    );
    //
    /*
      Once identified we can track object (requires modified SiderealObjects lib).
    */

    trackObject(&cli_obj, cli_obj.object_table_i, cli_obj.object_number);
    printf("---------------------------------------------\n");
    printf("Table Index:   %d\n", cli_obj.object_table_i);
    printf("Table:         %s\n", cli_obj.object_table_name);
    printf("Number:        %d\n", cli_obj.object_number);
    printf("Name:          %s\n", cli_obj.object_name);
    printf("Type:          %s\n", cli_obj.object_type);
    printf("Constellation: %s\n", cli_obj.object_con);
    printf("Distance:      %f\n", cli_obj.object_dist);
    printf("Azimuth:       %f\n", cli_obj.object_az);
    printf("Altitude:      %f\n", cli_obj.object_alt);
    printf("Rise:          %f\n", cli_obj.object_r);
    printf("Set:           %f\n", cli_obj.object_s);
    printf("---------------------------------------------\n");
  }
  else {printf("identify object: bad input data\n");}
}

/* Rule 8.7: internal linkage; only referenced (currently commented out) within this file. */
static void unmountSDCard(void) {
  sdcardFlagData.no_delay_flag=true;
  sdcardFlagData.unmount_sdcard_flag=true;
}

static void mountSDCard(void) {
  sdcardFlagData.no_delay_flag=true;
  sdcardFlagData.mount_sdcard_flag=true;
}

/*
  Debug ArgParse.
  Expected behaviour:
  command: foo -a -b -c
  flags:   a b c
  command: foo -a 1 -b 2 -c 3
  flags:   a="1" b="2" c="3"
  Note:
    - For best practice only use ArgParser if flags are required, else use PlainArgParser for simple tokenization.
    - Use PlainArgParser if processing negative numbers.
    - short flags: 1-3 alphanumeric chars. example: -a, -a1, -a12, -abc.
    - long flags: 1-256 alphanumeric chars. example: --foobar, --foo-bar, --foobar123.
    - see ArgParser for more details.
*/
size_t pos_count;
const char** pos;
bool verbose;
bool verbose_1;
bool enable;

/* Rule 8.7: internal linkage; only called from CmdProcess() within this file. */
static void printArgParse(void) {
  printf("-------------------------------------------");
  printf("[debug] First command: ");
  if (pos_count > 0) {printf("%s\n", pos[0]);}
  else {printf("none\n");}
  printf("[debug] Positionals (");
  printf("%zu", pos_count);
  printf("): ");
  for (size_t j = 0; j < pos_count; ++j)
    {printf("%s", pos[j]); if (j < pos_count - 1) {printf(" ");}}
  printf("\n");
  printf("----\n");
  printf("[debug] Flag count: ");
  printf("%zu\n", parser.flag_count);
  printf("[debug] Flags: ");
  for (size_t k = 0; k < parser.flag_count; ++k)
    {printf("%s", parser.flags[k]); const char* val = parser.values[k];
      if (val[0] != '\0') {printf("=\"%s\"", val);}
      if (k < parser.flag_count - 1) {printf(" ");}
  }
  printf("\n");
  printf("-------------------------------------------\n");
}

// ---------------------------------------------------------------------------------------------------------------
/*
  Serial RXD : Command Process.
*/
// ---------------------------------------------------------------------------------------------------------------

void CmdProcess(void) {
  if (strlen(serial0Data.BUFFER_RX) >= 2U) {
    bool proceed = true;

    // Debug Serial Buffer.
    printf("[CmdProcess] Received data: %s\n", serial0Data.BUFFER_RX);

    // Initialize argparse.
    argparser_reset(&parser);
    if (argparser_init_from_buffer(&parser, serial0Data.BUFFER_RX) == false) {
      printf("[cmd] Failed to initialize parser from buffer\n");
      proceed = false;
    }

    if (proceed == true) {
      pos = argparser_get_positionals(&parser, &pos_count);

      // Verbosity.
      verbose = (argparser_get_bool(&parser, "v") == true) || (argparser_get_bool(&parser, "verbose") == true);
      verbose_1 = (argparser_get_bool(&parser, "vv") == true) || (argparser_get_bool(&parser, "verbose1") == true);
      if (verbose_1 == true) {verbose=true;}
      if (verbose==false) {verbose_1=false;}
      printf("[cmd] verbose: %d\n", verbose);
      printf("[cmd] verbose1: %d\n", verbose_1);
      // Enable/Disable
      enable=false;
      if ((argparser_has_flag(&parser, "disable") == true) || (argparser_has_flag(&parser, "d") == true)) {enable=false;}
      else if ((argparser_has_flag(&parser, "enable") == true) || (argparser_has_flag(&parser, "e") == true)) {enable=true;}
      // Debug Arg Parse.
      printArgParse();
      // Check if command provided.
      if (pos_count == 0U) {
        printf("[cmd] No command provided.\n");
        proceed = false;
      }
    }

    if (proceed == true) {
      // Commands. help -v
      if ((strcmp(pos[0], "help")==0) || (strcmp(pos[0], "h")==0))
        {printf("Usage: [buffer with] [--flag value] [-f value] [positional...]\n");
        if (verbose == true) {PrintHelp();}
      }
      else if (strcmp(pos[0], "stat")==0) {

        if (argparser_has_flag(&parser, "t") == true) {
          if (argparser_has_flag(&parser, "datetime") == true) {systemData.output_stat_datetime=enable; printf("setting stat datetime output enabled: %d\n", systemData.output_stat_datetime);}
          if (argparser_has_flag(&parser, "taskrates") == true) {systemData.output_stat_task_rates=enable; printf("setting stat task rates output enabled: %d\n", systemData.output_stat_task_rates);}
          if (argparser_has_flag(&parser, "position") == true) {systemData.output_stat_position=enable; printf("setting stat position output enabled: %d\n", systemData.output_stat_position);}
          if (argparser_has_flag(&parser, "gyro") == true) {systemData.output_stat_gyro=enable; printf("setting stat gyro output enabled: %d\n", systemData.output_stat_gyro);}
          if (argparser_has_flag(&parser, "admplex") == true) {systemData.output_stat_admplex=enable; printf("setting stat admplex output enabled: %d\n", systemData.output_stat_admplex);}
          if (argparser_has_flag(&parser, "gpiope") == true) {systemData.output_stat_gpiope=enable; printf("setting stat gpiope output enabled: %d\n", systemData.output_stat_gpiope);}
          if (argparser_has_flag(&parser, "matrix") == true) {systemData.output_stat_matrix=enable; printf("setting stat matrix output enabled: %d\n", systemData.output_stat_matrix);}
        }

        if (strcmp(serial0Data.BUFFER_RX, "stat --system")==0) {PrintSystemData();}

        if (strncmp(serial0Data.BUFFER_RX, "stat --matrix ", strlen("stat --matrix "))==0) {
          if (argparser_has_flag(&parser, "A") == true) {PrintMatrixData();}
          else {PrintMatrixNData(argparser_get_int8(&parser, "matrix", -1));}
        }

        else if (strncmp(serial0Data.BUFFER_RX, "stat -map ", strlen("stat -map "))==0) {
          if (argparser_has_flag(&parser, "A") == true) {PrintMappingData();}
          else {PrintMappingNData(argparser_get_int8(&parser, "map", -1));}
        }

        else if (argparser_has_flag(&parser, "sentence") == true) {
          if (argparser_has_flag(&parser, "A") == true) {systemData.output_satio_all=enable; setAllSentenceOutput(enable);}
          if (argparser_has_flag(&parser, "satio") == true) {systemData.output_satio_enabled=enable; printf("setting satio output enabled: %d\n", systemData.output_satio_enabled);}
          if (argparser_has_flag(&parser, "gngga") == true) {systemData.output_gngga_enabled=enable; printf("setting gngga output enabled: %d\n", systemData.output_gngga_enabled);}
          if (argparser_has_flag(&parser, "gnrmc") == true) {systemData.output_gnrmc_enabled=enable; printf("setting gnrmc output enabled: %d\n", systemData.output_gnrmc_enabled);}
          if (argparser_has_flag(&parser, "gpatt") == true) {systemData.output_gpatt_enabled=enable; printf("setting gpatt output enabled: %d\n", systemData.output_gpatt_enabled);}
          if (argparser_has_flag(&parser, "matrix") == true)
            {
              systemData.output_matrix_enabled=enable;
              printf("setting matrix output enabled: %d\n", systemData.output_matrix_enabled);
            }
          if (argparser_has_flag(&parser, "gpiope") == true) {systemData.output_input_portcontroller=enable; printf("setting input_portcontroller_0 output enabled: %d\n", systemData.output_input_portcontroller);}
          if (argparser_has_flag(&parser, "admplex0") == true) {systemData.output_admplex0_enabled=enable; printf("setting admplex0 output enabled: %d\n", systemData.output_admplex0_enabled);}
          if (argparser_has_flag(&parser, "admplex1") == true) {systemData.output_admplex1_enabled=enable; printf("setting admplex1 output enabled: %d\n", systemData.output_admplex1_enabled);}
          if (argparser_has_flag(&parser, "gyro0") == true) {systemData.output_gyro_0_enabled=enable; printf("setting gyro_0 output enabled: %d\n", systemData.output_gyro_0_enabled);}
          if (argparser_has_flag(&parser, "sun") == true) {systemData.output_sun_enabled=enable; printf("setting sun output enabled: %d\n", systemData.output_sun_enabled);}
          if (argparser_has_flag(&parser, "earth") == true) {systemData.output_earth_enabled=enable; printf("setting earth output enabled: %d\n", systemData.output_earth_enabled);}
          if (argparser_has_flag(&parser, "luna") == true) {systemData.output_luna_enabled=enable; printf("setting luna output enabled: %d\n", systemData.output_luna_enabled);}
          if (argparser_has_flag(&parser, "mercury") == true) {systemData.output_mercury_enabled=enable; printf("setting mercury output enabled: %d\n", systemData.output_mercury_enabled);}
          if (argparser_has_flag(&parser, "venus") == true) {systemData.output_venus_enabled=enable; printf("setting venus output enabled: %d\n", systemData.output_venus_enabled);}
          if (argparser_has_flag(&parser, "mars") == true) {systemData.output_mars_enabled=enable; printf("setting mars output enabled: %d\n", systemData.output_mars_enabled);}
          if (argparser_has_flag(&parser, "jupiter") == true) {systemData.output_jupiter_enabled=enable; printf("setting jupiter output enabled: %d\n", systemData.output_jupiter_enabled);}
          if (argparser_has_flag(&parser, "saturn") == true) {systemData.output_saturn_enabled=enable; printf("setting saturn output enabled: %d\n", systemData.output_saturn_enabled);}
          if (argparser_has_flag(&parser, "uranus") == true) {systemData.output_uranus_enabled=enable; printf("setting uranus output enabled: %d\n", systemData.output_uranus_enabled);}
          if (argparser_has_flag(&parser, "neptune") == true) {systemData.output_neptune_enabled=enable; printf("setting neptune output enabled: %d\n", systemData.output_neptune_enabled);}
          if (argparser_has_flag(&parser, "meteors") == true) {systemData.output_meteors_enabled=enable; printf("setting meteors output enabled: %d\n", systemData.output_meteors_enabled);}
          if (argparser_has_flag(&parser, "xmatrix") == true) {systemData.output_config_matrix_enabled=enable; printf("setting config_matrix output enabled: %d\n", systemData.output_config_matrix_enabled);}
          if (argparser_has_flag(&parser, "xmap") == true) {systemData.output_config_mapping_enabled=enable; printf("setting config_mapping output enabled: %d\n", systemData.output_config_mapping_enabled);}
        }

      }
      /*
        StarNav via CLI
        Temporarily disabled for user so that system has exclusive r/w access for associated values
        that StarNav already uses.
        Possible update: create seperate functions & values for identification and tracking of objects
        so that this command line feature can be safely re-enabled.
      */
      else if (strcmp(pos[0], "starnav")==0) {star_nav();}

      else {
        /* not "help"/"h"/"stat": fall through to the systemData.serial_command dispatch below */
      }

      if (systemData.serial_command == true) {
        // system
        if (strcmp(pos[0], "system")==0) {
          if (argparser_has_flag(&parser, "save") == true) {sdcardFlagData.save_system=true;}
          else if (argparser_has_flag(&parser, "load") == true) {sdcardFlagData.load_system=true;}
          else if (argparser_has_flag(&parser, "restore-defaults") == true) {restore_system_defaults();}
          else if (argparser_has_flag(&parser, "log") == true) {
            printf("setting log enabled: %d\n", enable);
            systemData.logging_enabled=enable;}
          else {
            /* no recognized "system" flag: nothing to do */
          }
        }
        // mapping
        else if (strcmp(pos[0], "map")==0) {
          if (argparser_has_flag(&parser, "new") == true) {set_all_mapping_default();}
          else if (argparser_has_flag(&parser, "save") == true) {sdcardFlagData.save_mapping=true;}
          else if (argparser_has_flag(&parser, "load") == true) {sdcardFlagData.load_mapping=true;}
          else if (argparser_has_flag(&parser, "delete") == true) {sdcardFlagData.delete_mapping=true;}
          else {
            int s = argparser_get_int8(&parser, "s", -1);
            if (s != -1) {
              setMapConfig(s, argparser_get_int8(&parser, "m", mappingData.map_mode[0][s]),
                           argparser_get_int32(&parser, "c0", mappingData.mapping_config[0][s][0]),
                           argparser_get_int32(&parser, "c1", mappingData.mapping_config[0][s][1]),
                           argparser_get_int32(&parser, "c2", mappingData.mapping_config[0][s][2]),
                           argparser_get_int32(&parser, "c3", mappingData.mapping_config[0][s][3]),
                           argparser_get_int32(&parser, "c4", mappingData.mapping_config[0][s][4]),
                           argparser_get_int32(&parser, "c5", mappingData.mapping_config[0][s][5]));
            }
          }
        }

        // gpiope
        else if (strcmp(pos[0], "gpiope")==0) {

          bool has_a = argparser_has_flag(&parser, "a") == true;
          bool has_i = argparser_has_flag(&parser, "i") == true;
          int8_t address = argparser_get_int8(&parser, "a", -1); // address
          int8_t portmap_index = argparser_get_int8(&parser, "i", -1); // index

          // --------------------------------------------------------------------------------------------------------
          // Output
          // --------------------------------------------------------------------------------------------------------
          if (argparser_has_flag(&parser, "output") == true) {

            // Check if device defined for specified address
            GPIOPortExpander* gpiope = isGPIOPE_OUTPUT(address);
            if (gpiope != nullptr) {

              // set port by index
              if (has_a && has_i && argparser_has_flag(&parser, "p") == true) {
                int8_t new_port = argparser_get_int8(&parser, "p", -1); // new port
                  GPIOPE_Set_Portmap_Index_As_Pin(*gpiope, portmap_index, new_port);
                  GPIOPE_QueryDevice(*gpiope, I2C_ADDR_9);
              }

              // set pwm by index
              if (has_a && has_i && argparser_has_flag(&parser, "pwm0") && argparser_has_flag(&parser, "pwm1") == true) {
                uint32_t pwm0 = argparser_get_uint32(&parser, "pwm0", 0);
                uint32_t pwm1 = argparser_get_uint32(&parser, "pwm1", 0);
                  GPIOPE_Set_Portmap_Index_As_PWM(*gpiope, portmap_index, pwm0, pwm1);
                  GPIOPE_QueryDevice(*gpiope, I2C_ADDR_9);
              }
            }
          }
          // --------------------------------------------------------------------------------------------------------
          // Input
          // --------------------------------------------------------------------------------------------------------
          else if (argparser_has_flag(&parser, "input") == true) {

              // Check if device defined for specified address
              GPIOPortExpander* gpiope = isGPIOPE_INPUT(address);
              if (gpiope != nullptr) {

              // all channels
              if (argparser_has_flag(&parser, "all") == true) {
                // enable/disable all channels
                if (argparser_has_flag(&parser, "enable") == true || argparser_has_flag(&parser, "e") == true ||
                    argparser_has_flag(&parser, "disable") == true || argparser_has_flag(&parser, "d") == true) {
                  for (uint8_t p=0; p<(uint8_t)gpiope->max_pins; p++) {GPIOPE_Set_Channel_Enabled(*gpiope, p, enable);}
                }
                // set all channels frequency
                if (argparser_has_flag(&parser, "freq") == true) {
                  uint64_t gpioe_freq_all = argparser_get_uint64(&parser, "freq", 0);
                  for (uint8_t p=0; p<(uint8_t)gpiope->max_pins; p++) {GPIOPE_Set_Channel_Frequency(*gpiope, p, gpioe_freq_all);}
                }
              }
              // specific channel
              else if (argparser_has_flag(&parser, "c") == true) {
                uint8_t gpioe_c = argparser_get_uint8(&parser, "c", 0);
                // enable/disable specified channel
                if (argparser_has_flag(&parser, "enable") == true || argparser_has_flag(&parser, "e") == true ||
                    argparser_has_flag(&parser, "disable") == true || argparser_has_flag(&parser, "d") == true) {
                  GPIOPE_Set_Channel_Enabled(*gpiope, gpioe_c, enable);
                }
                // set frequency for specified channel
                if (argparser_has_flag(&parser, "freq") == true) {GPIOPE_Set_Channel_Frequency(*gpiope, gpioe_c, argparser_get_uint64(&parser, "freq", 0));}
              }
              else {
                printf("gpiope unchanged. specified unknown gpiope device!");
              }
            }
          }
        }

        // matrix
        else if (strcmp(pos[0], "matrix")==0) {
          if (argparser_has_flag(&parser, "startup-enable") == true) {matrixData.load_matrix_on_startup=true;}
          else if (argparser_has_flag(&parser, "startup-disable") == true) {matrixData.load_matrix_on_startup=false;}
          else if (argparser_has_flag(&parser, "new") == true) {override_all_computer_assists(); set_all_matrix_default();}
          else if (argparser_has_flag(&parser, "save") == true) {saveMatrix(argparser_get_int8(&parser, "save", -1));}
          else if (argparser_has_flag(&parser, "load") == true) {loadMatrix(argparser_get_int8(&parser, "load", -1));}
          else if (argparser_has_flag(&parser, "delete") == true) {deleteMatrix(argparser_get_int8(&parser, "delete", -1));}
          else {
          /* "s" gates every setter below; "f" additionally gates the per-function ones.
             Both are fetched once here since each flag's presence was already checked
             individually per setter below, every defaulted value below is only ever used
             when its flag is confirmed present. */
          bool has_s = argparser_has_flag(&parser, "s") == true;
          bool has_f = argparser_has_flag(&parser, "f") == true;
          int s = argparser_get_int8(&parser, "s", -1);
          int f = argparser_get_int8(&parser, "f", 0);

          // now sets an index number for accessing gpiope portmap list
          if (has_s && argparser_has_flag(&parser, "p") == true) {setMatrixGPIOPEPortSlot(s, argparser_get_int8(&parser, "p", 0));}
          if (has_s && argparser_has_flag(&parser, "gpiope") == true) {setOutputPortControllerAddress(s, argparser_get_uint8(&parser, "gpiope", 0));}

          if (has_s && has_f && argparser_has_flag(&parser, "fn") == true) {setMatrixFunction(s, f, argparser_get_int8(&parser, "fn", 0));}
          if (has_s && has_f && argparser_has_flag(&parser, "fx") == true) {setMatrixXYZ(s, f, INDEX_MATRIX_FUNTION_X, argparser_get_double(&parser, "fx", 0));}
          if (has_s && has_f && argparser_has_flag(&parser, "fy") == true) {setMatrixXYZ(s, f, INDEX_MATRIX_FUNTION_Y, argparser_get_double(&parser, "fy", 0));}
          if (has_s && has_f && argparser_has_flag(&parser, "fz") == true) {setMatrixXYZ(s, f, INDEX_MATRIX_FUNTION_Z, argparser_get_double(&parser, "fz", 0));}
          if (has_s && has_f && argparser_has_flag(&parser, "fi") == true) {setMatrixInverted(s, f, argparser_get_int8(&parser, "fi", 0));}
          if (has_s && has_f && argparser_has_flag(&parser, "fo") == true) {setMatrixOperator(s, f, argparser_get_int8(&parser, "fo", 0));}
          
          
          if (has_s && argparser_has_flag(&parser, "uvalue") == true) {setUserOutputValue(s, argparser_get_int32(&parser, "uvalue", -1));}
          if (has_s && argparser_has_flag(&parser, "flux") == true) {setFlux(s, argparser_get_uint32(&parser, "flux", -1));}
          if (has_s && argparser_has_flag(&parser, "oride") == true) {setOverrideOutputValue(s, argparser_get_int32(&parser, "oride", -1));}
          if (has_s && argparser_has_flag(&parser, "computer-assist") == true) {setComputerAssist(s, argparser_get_int8(&parser, "computer-assist", -1));}
          if (has_s && argparser_has_flag(&parser, "omode") == true) {setOutputMode(s, argparser_get_int8(&parser, "omode", -1));}
          if (has_s && argparser_has_flag(&parser, "map-slot") == true) {setMapSlot(s, argparser_get_int8(&parser, "map-slot", -1));}
          if (has_s && has_f && argparser_has_flag(&parser, "xyz-mode-x") == true) {setMatrixFunctionComparitorMode(s, f, INDEX_MATRIX_FUNTION_X, argparser_get_int8(&parser, "xyz-mode-x", 0));}
          if (has_s && has_f && argparser_has_flag(&parser, "xyz-mode-y") == true) {setMatrixFunctionComparitorMode(s, f, INDEX_MATRIX_FUNTION_Y, argparser_get_int8(&parser, "xyz-mode-y", 0));}
          if (has_s && has_f && argparser_has_flag(&parser, "xyz-mode-z") == true) {setMatrixFunctionComparitorMode(s, f, INDEX_MATRIX_FUNTION_Z, argparser_get_int8(&parser, "xyz-mode-z", 0));}
          }
        }
        // ins
        else if (strcmp(pos[0], "ins")==0) {
          if (argparser_has_flag(&parser, "m") == true) {setINSMode(argparser_get_int8(&parser, "m", -1));}
          if (argparser_has_flag(&parser, "gyro") == true) {setINSUseGyroHeading(argparser_get_int8(&parser, "gyro", -1));}
          if (argparser_has_flag(&parser, "p") == true) {setINSGPSPrecision(argparser_get_double(&parser, "p", -1));}
          if (argparser_has_flag(&parser, "s") == true) {setINSMinSpeed(argparser_get_double(&parser, "s", -1));}
          if (argparser_has_flag(&parser, "r") == true) {setINSHeadingRangeDiff(argparser_get_double(&parser, "r", -1));}
          if (argparser_has_flag(&parser, "reset-forced") == true) {insData.INS_FORCED_ON_FLAG=false;}
        }
        // SatIO
        else if (strcmp(pos[0], "satio")==0) {
          // time
          if (argparser_has_flag(&parser, "utc-offset") == true) {setUTCSecondOffset(argparser_get_int64(&parser, "utc-offset", 0));}
          if (argparser_has_flag(&parser, "auto-datetime-on") == true) {datetimeSetDTAuto(true);}
          if (argparser_has_flag(&parser, "auto-datetime-off") == true) {datetimeSetDTAuto(false);}
          if (argparser_has_flag(&parser, "set-datetime") == true) {
            datetimeSetRTC(argparser_get_uint16(&parser, "year", -1),
                                  argparser_get_uint8(&parser,  "month", -1),
                                  argparser_get_uint8(&parser,  "mday", -1),
                                  argparser_get_uint8(&parser,  "hour", -1),
                                  argparser_get_uint8(&parser,  "minute", -1),
                                  argparser_get_uint8(&parser,  "second", -1));}
          // location
          if (argparser_has_flag(&parser, "set-coord") == true && argparser_has_flag(&parser, "lat") == true && argparser_has_flag(&parser, "lon") == true) {
            setCoordinatesDegrees(argparser_get_double(&parser, "lat", NAN), argparser_get_double(&parser, "lon", NAN));
          }
          if (argparser_has_flag(&parser, "coord-value-mode-gps") == true) {SatIOData.location_value_mode=SATIO_MODE_GPS;}
          if (argparser_has_flag(&parser, "coord-value-mode-user") == true) {SatIOData.location_value_mode=SATIO_MODE_USER;}
          // speed
          if (argparser_has_flag(&parser, "set-speed") == true) {
            setSpeed(argparser_get_double(&parser, "set-speed", NAN));
          }
          if (argparser_has_flag(&parser, "speed-value-mode-gps") == true) {SatIOData.speed_value_mode=SATIO_MODE_GPS;}
          if (argparser_has_flag(&parser, "speed-value-mode-user") == true) {SatIOData.speed_value_mode=SATIO_MODE_USER;}
          // altitude
          if (argparser_has_flag(&parser, "set-altitude") == true) {
            setAltitude(argparser_get_double(&parser, "set-altitude", NAN));
          }
          if (argparser_has_flag(&parser, "altitude-value-mode-gps") == true) {SatIOData.altitude_value_mode=SATIO_MODE_GPS;}
          if (argparser_has_flag(&parser, "altitude-value-mode-user") == true) {SatIOData.altitude_value_mode=SATIO_MODE_USER;}
          // ground heading
          if (argparser_has_flag(&parser, "set-ground-heading") == true) {
            setGroundHeading(argparser_get_double(&parser, "set-ground-heading", NAN));
          }
          if (argparser_has_flag(&parser, "ground-heading-value-mode-gps") == true) {SatIOData.ground_heading_value_mode=SATIO_MODE_GPS;}
          if (argparser_has_flag(&parser, "ground-heading-value-mode-user") == true) {SatIOData.ground_heading_value_mode=SATIO_MODE_USER;}
          // ra/dec
          if (argparser_has_flag(&parser, "set-ra-dec") == true) {
            setRaDec(argparser_get_int8(&parser, "ra-h", -1),
                     argparser_get_int8(&parser, "ra-m", -1),
                     argparser_get_float(&parser, "ra-s", -1.0f),
                     argparser_get_int8(&parser, "dec-d", -1),
                     argparser_get_int8(&parser, "dec-m", -1),
                     argparser_get_float(&parser, "dec-s", -1.0f));
          }
          if (argparser_has_flag(&parser, "ra-dec-value-mode-gyro") == true) {SatIOData.sidereal_attitude_value_mode=SATIO_MODE_GYRO;}
          if (argparser_has_flag(&parser, "ra-dec-value-mode-user") == true) {SatIOData.sidereal_attitude_value_mode=SATIO_MODE_USER;}
        }
        // gyro
        else if (strcmp(pos[0], "gyro")==0) {
          if (argparser_has_flag(&parser, "calacc") == true) {WT901CalAcc();}
          if (argparser_has_flag(&parser, "calmag-start") == true) {WT901CalMagStart();}
          else if (argparser_has_flag(&parser, "calmag-stop") == true) {WT901CalMagEnd();}
          else { /* no more options */ }
        }
        // admplex0
        else if (strcmp(pos[0], "admplex0")==0) {
          // admplex0 --all --freq X : set every channel's freq in one call
          if (argparser_has_flag(&parser, "all") == true) {
            if (argparser_has_flag(&parser, "freq") == true) {
              uint64_t admplex0_freq_all = argparser_get_uint64(&parser, "freq", 0);
              for (uint8_t c=0; c<MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS; c++) {setADMultiplexerChannelFreq(ad_mux_0, c, admplex0_freq_all);}
            }
          }
          else if (argparser_has_flag(&parser, "c") == true) {
            uint8_t admplex0_c = argparser_get_uint8(&parser, "c", 0);
            if (argparser_has_flag(&parser, "enable") == true || argparser_has_flag(&parser, "e") == true ||
                argparser_has_flag(&parser, "disable") == true || argparser_has_flag(&parser, "d") == true) {
              setADMultiplexerChannelEnabled(ad_mux_0, admplex0_c, enable);
            }
            if (argparser_has_flag(&parser, "freq") == true) {setADMultiplexerChannelFreq(ad_mux_0, admplex0_c, argparser_get_uint64(&parser, "freq", 0));}
          }
        }
        // admplex1
        else if (strcmp(pos[0], "admplex1")==0) {
          // admplex1 --all --freq X : set every channel's freq in one call
          if (argparser_has_flag(&parser, "all") == true) {
            if (argparser_has_flag(&parser, "freq") == true) {
              uint64_t admplex1_freq_all = argparser_get_uint64(&parser, "freq", 0);
              for (uint8_t c=0; c<MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS; c++) {setADMultiplexerChannelFreq(ad_mux_1, c, admplex1_freq_all);}
            }
          }
          else if (argparser_has_flag(&parser, "c") == true) {
            uint8_t admplex1_c = argparser_get_uint8(&parser, "c", 0);
            if (argparser_has_flag(&parser, "enable") == true || argparser_has_flag(&parser, "e") == true ||
                argparser_has_flag(&parser, "disable") == true || argparser_has_flag(&parser, "d") == true) {
              setADMultiplexerChannelEnabled(ad_mux_1, admplex1_c, enable);
            }
            if (argparser_has_flag(&parser, "freq") == true) {setADMultiplexerChannelFreq(ad_mux_1, admplex1_c, argparser_get_uint64(&parser, "freq", 0));}
          }
        }

        // else if (strcmp(pos[0], "sdcard")==0) {
        //   if (argparser_has_flag(&parser, "mount")) {mountSDCard();}
        //   else if (argparser_has_flag(&parser, "unmount")) {unmountSDCard();}
        // }

        else if (strcmp(pos[0], "powercfg")==0) {
          // powercfg --power-saving
          if (argparser_has_flag(&parser, "power-saving")) {setTasksDelayLowPower();}
          // powercfg --power-balanced
          else if (argparser_has_flag(&parser, "power-balanced")) {setTasksDelayBalanced();}
          // powercfg --ultimate-performance
          else if (argparser_has_flag(&parser, "ultimate-performance")) {setTasksDelayUltimatePerformance();}
          
          else {
            // set max freq hz
            if (argparser_has_flag(&parser, "setdelay")) {

              #ifdef SatIO_CD74HC4067_OPTION_USE_0
              if (argparser_has_flag(&parser, "admplex0"))
                {setDelay(TaskADMplex0, argparser_get_uint32(&parser, "admplex0", pwrConfigCurrent.TASK_MAX_FREQ_ADMPLEX0), &pwrConfigCurrent.TASK_MAX_FREQ_ADMPLEX0);}
              #endif
              #ifdef SatIO_CD74HC4067_OPTION_USE_1
              if (argparser_has_flag(&parser, "admplex1"))
                {setDelay(TaskADMplex1, argparser_get_uint32(&parser, "admplex1", pwrConfigCurrent.TASK_MAX_FREQ_ADMPLEX1), &pwrConfigCurrent.TASK_MAX_FREQ_ADMPLEX1);}
              #endif
              #ifdef SatIO_USE_GYRO_0
              if (argparser_has_flag(&parser, "gyro0"))
                {setDelay(TaskGyro, argparser_get_uint32(&parser, "gyro0", pwrConfigCurrent.TASK_MAX_FREQ_GYRO), &pwrConfigCurrent.TASK_MAX_FREQ_GYRO);}
              #endif

              #ifdef SatIO_USE_UNIVERSE
              if (argparser_has_flag(&parser, "universe"))
                {setDelay(TaskUniverse, argparser_get_uint32(&parser, "universe", pwrConfigCurrent.TASK_MAX_FREQ_UNIVERSE), &pwrConfigCurrent.TASK_MAX_FREQ_UNIVERSE);}
              #endif

              #ifdef SatIO_USE_GPS_0
              if (argparser_has_flag(&parser, "gps"))
                {setDelay(TaskGPS, argparser_get_uint32(&parser, "gps", pwrConfigCurrent.TASK_MAX_FREQ_GPS), &pwrConfigCurrent.TASK_MAX_FREQ_GPS);}
              #endif

              #ifdef SatIO_USE_MATRIX
              if (argparser_has_flag(&parser, "switch"))
                {setDelay(TaskSwitches, argparser_get_uint32(&parser, "switch", pwrConfigCurrent.TASK_MAX_FREQ_SWITCHES), &pwrConfigCurrent.TASK_MAX_FREQ_SWITCHES);}
              #endif

              #ifdef SatIO_USE_STORAGE
              if (argparser_has_flag(&parser, "storage"))
                {setDelay(TaskStorage, argparser_get_uint32(&parser, "storage", pwrConfigCurrent.TASK_MAX_FREQ_STORAGE), &pwrConfigCurrent.TASK_MAX_FREQ_STORAGE);}
              #endif

              if (argparser_has_flag(&parser, "gpiope"))
                {setDelay(TaskInputPortController, argparser_get_uint32(&parser, "gpiope", pwrConfigCurrent.TASK_MAX_FREQ_GPIOE_INPUT), &pwrConfigCurrent.TASK_MAX_FREQ_GPIOE_INPUT);}
            }
          }
        }
      }
    }
  }
}

// ---------------------------------------------------------------------------------------------------------------
/*
  Serial TXD : Output.
*/
// ---------------------------------------------------------------------------------------------------------------
long i_output_config_matrix = 0;

#ifdef SatIO_SERIAL_TX_OPTION_CURRENT_TASK
/*
 * Under this option outputSerialXxx() runs directly on its data-source task
 * (GPS/Gyro/Multiplexer/Universe/Switches), so several of these tasks can be
 * building a sentence at the same time on different cores. Each source gets
 * its own TX buffer so one task's memset()/strncat() sequence can never
 * interleave with another's.
 */
#define TXBUF_GPS      (serial0Data.BUFFER_TX_GPS)
#define TXBUF_ADMPLEX0 (serial0Data.BUFFER_TX_ADMPLEX0)
#define TXBUF_ADMPLEX1 (serial0Data.BUFFER_TX_ADMPLEX1)
#define TXBUF_GYRO0    (serial0Data.BUFFER_TX_GYRO0)
#define TXBUF_UNI      (serial0Data.BUFFER_TX_UNI)
#define TXBUF_SWITCHES (serial0Data.BUFFER_TX_SWITCHES)
#define TXBUF_GPIOPE      (serial0Data.BUFFER_TX_GPIOPE)
#endif

#ifdef SatIO_SERIAL_TX_OPTION_NEW_TASK
/*
 * Under SatIO_SERIAL_TX_OPTION_NEW_TASK every outputSerialXxx() call is made
 * sequentially from the single SatIO-serial-tx task, so one shared buffer is
 * safe and saves RAM.
 */
#define TXBUF_GPS      (serial0Data.BUFFER_TX)
#define TXBUF_ADMPLEX0 (serial0Data.BUFFER_TX)
#define TXBUF_ADMPLEX1 (serial0Data.BUFFER_TX)
#define TXBUF_GYRO0    (serial0Data.BUFFER_TX)
#define TXBUF_UNI      (serial0Data.BUFFER_TX)
#define TXBUF_SWITCHES (serial0Data.BUFFER_TX)
#define TXBUF_GPIOPE      (serial0Data.BUFFER_TX)
#endif

/*
 * strcat() has no bound and can overflow the destination buffer if a
 * sentence's combined length is ever misjudged; every append below goes
 * through this helper instead, which truncates rather than overflowing if
 * the buffer is ever close to full (Rule 21.x: replaces the banned,
 * unbounded strcat()).
 */
static void serial0_buffer_append(char *buffer, size_t buffer_size, const char *text)
{
    size_t used = strlen(buffer);

    if (used < (buffer_size - 1U))
    {
        (void)strncat(buffer, text, (buffer_size - 1U) - used);
    }
}

/*
 * Field-separated sentences append a trailing "," after every field
 * (simplest for both fixed lists and loops), so this drops that last
 * separator before the checksum is computed, guaranteeing the checksum
 * covers the same bytes whether the field list was built by hand or by a
 * loop.
 */
static void serial0_buffer_strip_trailing_comma(char *buffer)
{
    size_t used = strlen(buffer);

    if ((used > 0U) && (buffer[used - 1U] == ','))
    {
        buffer[used - 1U] = '\0';
    }
}

/*
 * Mercury through Neptune are sent identically: tag, then 12 fields in a
 * fixed order, then checksum. Sun/Earth/Luna have their own field sets
 * (the Sun mirrors part of its position into earth_ecliptic_lat/long, the
 * Moon has phase/luminance instead of a heliocentric position) so they
 * stay hand-written below; only this fixed 12-field shape is tabulated.
 */
typedef struct {
    const char *tag;
    double SiderealPlantetsStruct::*ra;
    double SiderealPlantetsStruct::*dec;
    double SiderealPlantetsStruct::*az;
    double SiderealPlantetsStruct::*alt;
    double SiderealPlantetsStruct::*r;
    double SiderealPlantetsStruct::*s;
    double SiderealPlantetsStruct::*helio_lat;
    double SiderealPlantetsStruct::*helio_long;
    double SiderealPlantetsStruct::*radius_vector;
    double SiderealPlantetsStruct::*distance;
    double SiderealPlantetsStruct::*ecliptic_lat;
    double SiderealPlantetsStruct::*ecliptic_long;
} OuterPlanetSentenceSpec;

static const OuterPlanetSentenceSpec mercury_sentence_spec = {
    "$MERCURY,",
    &SiderealPlantetsStruct::mercury_ra, &SiderealPlantetsStruct::mercury_dec,
    &SiderealPlantetsStruct::mercury_az, &SiderealPlantetsStruct::mercury_alt,
    &SiderealPlantetsStruct::mercury_r, &SiderealPlantetsStruct::mercury_s,
    &SiderealPlantetsStruct::mercury_helio_ecliptic_lat, &SiderealPlantetsStruct::mercury_helio_ecliptic_long,
    &SiderealPlantetsStruct::mercury_radius_vector, &SiderealPlantetsStruct::mercury_distance,
    &SiderealPlantetsStruct::mercury_ecliptic_lat, &SiderealPlantetsStruct::mercury_ecliptic_long
};
static const OuterPlanetSentenceSpec venus_sentence_spec = {
    "$VENUS,",
    &SiderealPlantetsStruct::venus_ra, &SiderealPlantetsStruct::venus_dec,
    &SiderealPlantetsStruct::venus_az, &SiderealPlantetsStruct::venus_alt,
    &SiderealPlantetsStruct::venus_r, &SiderealPlantetsStruct::venus_s,
    &SiderealPlantetsStruct::venus_helio_ecliptic_lat, &SiderealPlantetsStruct::venus_helio_ecliptic_long,
    &SiderealPlantetsStruct::venus_radius_vector, &SiderealPlantetsStruct::venus_distance,
    &SiderealPlantetsStruct::venus_ecliptic_lat, &SiderealPlantetsStruct::venus_ecliptic_long
};
static const OuterPlanetSentenceSpec mars_sentence_spec = {
    "$MARS,",
    &SiderealPlantetsStruct::mars_ra, &SiderealPlantetsStruct::mars_dec,
    &SiderealPlantetsStruct::mars_az, &SiderealPlantetsStruct::mars_alt,
    &SiderealPlantetsStruct::mars_r, &SiderealPlantetsStruct::mars_s,
    &SiderealPlantetsStruct::mars_helio_ecliptic_lat, &SiderealPlantetsStruct::mars_helio_ecliptic_long,
    &SiderealPlantetsStruct::mars_radius_vector, &SiderealPlantetsStruct::mars_distance,
    &SiderealPlantetsStruct::mars_ecliptic_lat, &SiderealPlantetsStruct::mars_ecliptic_long
};
static const OuterPlanetSentenceSpec jupiter_sentence_spec = {
    "$JUPITER,",
    &SiderealPlantetsStruct::jupiter_ra, &SiderealPlantetsStruct::jupiter_dec,
    &SiderealPlantetsStruct::jupiter_az, &SiderealPlantetsStruct::jupiter_alt,
    &SiderealPlantetsStruct::jupiter_r, &SiderealPlantetsStruct::jupiter_s,
    &SiderealPlantetsStruct::jupiter_helio_ecliptic_lat, &SiderealPlantetsStruct::jupiter_helio_ecliptic_long,
    &SiderealPlantetsStruct::jupiter_radius_vector, &SiderealPlantetsStruct::jupiter_distance,
    &SiderealPlantetsStruct::jupiter_ecliptic_lat, &SiderealPlantetsStruct::jupiter_ecliptic_long
};
static const OuterPlanetSentenceSpec saturn_sentence_spec = {
    "$SATURN,",
    &SiderealPlantetsStruct::saturn_ra, &SiderealPlantetsStruct::saturn_dec,
    &SiderealPlantetsStruct::saturn_az, &SiderealPlantetsStruct::saturn_alt,
    &SiderealPlantetsStruct::saturn_r, &SiderealPlantetsStruct::saturn_s,
    &SiderealPlantetsStruct::saturn_helio_ecliptic_lat, &SiderealPlantetsStruct::saturn_helio_ecliptic_long,
    &SiderealPlantetsStruct::saturn_radius_vector, &SiderealPlantetsStruct::saturn_distance,
    &SiderealPlantetsStruct::saturn_ecliptic_lat, &SiderealPlantetsStruct::saturn_ecliptic_long
};
static const OuterPlanetSentenceSpec uranus_sentence_spec = {
    "$URANUS,",
    &SiderealPlantetsStruct::uranus_ra, &SiderealPlantetsStruct::uranus_dec,
    &SiderealPlantetsStruct::uranus_az, &SiderealPlantetsStruct::uranus_alt,
    &SiderealPlantetsStruct::uranus_r, &SiderealPlantetsStruct::uranus_s,
    &SiderealPlantetsStruct::uranus_helio_ecliptic_lat, &SiderealPlantetsStruct::uranus_helio_ecliptic_long,
    &SiderealPlantetsStruct::uranus_radius_vector, &SiderealPlantetsStruct::uranus_distance,
    &SiderealPlantetsStruct::uranus_ecliptic_lat, &SiderealPlantetsStruct::uranus_ecliptic_long
};
static const OuterPlanetSentenceSpec neptune_sentence_spec = {
    "$NEPTUNE,",
    &SiderealPlantetsStruct::neptune_ra, &SiderealPlantetsStruct::neptune_dec,
    &SiderealPlantetsStruct::neptune_az, &SiderealPlantetsStruct::neptune_alt,
    &SiderealPlantetsStruct::neptune_r, &SiderealPlantetsStruct::neptune_s,
    &SiderealPlantetsStruct::neptune_helio_ecliptic_lat, &SiderealPlantetsStruct::neptune_helio_ecliptic_long,
    &SiderealPlantetsStruct::neptune_radius_vector, &SiderealPlantetsStruct::neptune_distance,
    &SiderealPlantetsStruct::neptune_ecliptic_lat, &SiderealPlantetsStruct::neptune_ecliptic_long
};

static void buildOuterPlanetSentence(const OuterPlanetSentenceSpec *spec)
{
    char checksum[MAX_GLOBAL_CHECKSUM_SIZE];

    memset(TXBUF_UNI, 0, sizeof(TXBUF_UNI));
    serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), spec->tag);
    serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(siderealPlanetData.*(spec->ra) + String(",")).c_str());
    serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(siderealPlanetData.*(spec->dec) + String(",")).c_str());
    serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(siderealPlanetData.*(spec->az) + String(",")).c_str());
    serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(siderealPlanetData.*(spec->alt) + String(",")).c_str());
    serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(siderealPlanetData.*(spec->r) + String(",")).c_str());
    serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(siderealPlanetData.*(spec->s) + String(",")).c_str());
    serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(siderealPlanetData.*(spec->helio_lat) + String(",")).c_str());
    serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(siderealPlanetData.*(spec->helio_long) + String(",")).c_str());
    serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(siderealPlanetData.*(spec->radius_vector) + String(",")).c_str());
    serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(siderealPlanetData.*(spec->distance) + String(",")).c_str());
    serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(siderealPlanetData.*(spec->ecliptic_lat) + String(",")).c_str());
    serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(siderealPlanetData.*(spec->ecliptic_long) + String("")).c_str());
    serial0_buffer_strip_trailing_comma(TXBUF_UNI);
    createChecksumSerial0(TXBUF_UNI, checksum, sizeof(checksum));
    serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), "*");
    serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), checksum);
    printf("%s\n", TXBUF_UNI);
}

void outputSerialGPS(void) {
  if (systemData.counters_gps.flag_c == true) {
    systemData.counters_gps.flag_c = false;
    if (systemData.output_gngga_enabled == true) {printf("%s\n", gnggaData.outsentence);}
    if (systemData.output_gnrmc_enabled == true) {printf("%s\n", gnrmcData.outsentence);}
    if (systemData.output_gpatt_enabled == true) {printf("%s\n", gpattData.outsentence);}
  }
}

void outputSerialSatIO(void) {
  if (systemData.output_satio_enabled == true) {
    char checksum[MAX_GLOBAL_CHECKSUM_SIZE];

    memset(TXBUF_GPS, 0, sizeof(TXBUF_GPS));
    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), "$SatIO,");
    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(systemData.uptime_seconds) + ",").c_str());

    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(SatIOData.systemTime.padded_time_HHMMSS) + ",").c_str());
    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(SatIOData.systemTime.padded_date_DDMMYYYY) + ",").c_str());
    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(SatIOData.systemTime.sync_padded_time_HHMMSS) + ",").c_str());
    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(SatIOData.systemTime.sync_padded_date_DDMMYYYY) + ",").c_str());

    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(SatIOData.localTime.padded_time_HHMMSS) + ",").c_str());
    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(SatIOData.localTime.padded_date_DDMMYYYY) + ",").c_str());
    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(SatIOData.localTime.sync_padded_time_HHMMSS) + ",").c_str());
    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(SatIOData.localTime.sync_padded_date_DDMMYYYY) + ",").c_str());

    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(SatIOData.localMeanSolarTime.padded_time_HHMMSS) + ",").c_str());
    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(SatIOData.localMeanSolarTime.padded_date_DDMMYYYY) + ",").c_str());
    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(SatIOData.localMeanSolarTime.sync_padded_time_HHMMSS) + ",").c_str());
    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(SatIOData.localMeanSolarTime.sync_padded_date_DDMMYYYY) + ",").c_str());

    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(siderealPlanetData.local_sidereal_time) + ",").c_str());
    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(siderealPlanetData.local_sidereal_attitude.padded_ra_str) + ",").c_str());
    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(siderealPlanetData.local_sidereal_attitude.padded_dec_str) + ",").c_str());
    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(siderealPlanetData.gyro_0_sidereal_attitude.padded_ra_str) + ",").c_str());
    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(siderealPlanetData.gyro_0_sidereal_attitude.padded_dec_str) + ",").c_str());

    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(insData.ins_latitude, 7) + ",").c_str());
    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(insData.ins_longitude, 7) + ",").c_str());
    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(insData.ins_altitude) + ",").c_str());
    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(insData.ins_heading) + ",").c_str());
    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(insData.INS_INITIALIZATION_FLAG) + ",").c_str());
    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(insData.INS_MODE) + ",").c_str());
    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(insData.INS_FORCED_ON_FLAG) + ",").c_str());
    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(insData.INS_REQ_GPS_PRECISION) + ",").c_str());
    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(insData.INS_REQ_HEADING_RANGE_DIFF) + ",").c_str());
    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(insData.INS_REQ_MIN_SPEED) + ",").c_str());
    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(insData.INS_USE_GYRO_HEADING) + ",").c_str());
    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), (String(insData.INS_ENABLED) + "").c_str());
    serial0_buffer_strip_trailing_comma(TXBUF_GPS);
    createChecksumSerial0(TXBUF_GPS, checksum, sizeof(checksum));
    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), "*");
    serial0_buffer_append(TXBUF_GPS, sizeof(TXBUF_GPS), checksum);
    printf("%s\n", TXBUF_GPS);
  }
}

void outputSerialGyro0(void) {
  if (systemData.counters_gyr0.flag_c == true) {
    systemData.counters_gyr0.flag_c = false;
    if (systemData.output_gyro_0_enabled == true) {
      char checksum[MAX_GLOBAL_CHECKSUM_SIZE];

      memset(TXBUF_GYRO0, 0, sizeof(TXBUF_GYRO0));
      serial0_buffer_append(TXBUF_GYRO0, sizeof(TXBUF_GYRO0), "$GYRO0,");
      serial0_buffer_append(TXBUF_GYRO0, sizeof(TXBUF_GYRO0), (String(gyroData.gyro_0_acc_x) + ",").c_str());
      serial0_buffer_append(TXBUF_GYRO0, sizeof(TXBUF_GYRO0), (String(gyroData.gyro_0_acc_y) + ",").c_str());
      serial0_buffer_append(TXBUF_GYRO0, sizeof(TXBUF_GYRO0), (String(gyroData.gyro_0_acc_z) + ",").c_str());
      serial0_buffer_append(TXBUF_GYRO0, sizeof(TXBUF_GYRO0), (String(gyroData.gyro_0_ang_x) + ",").c_str());
      serial0_buffer_append(TXBUF_GYRO0, sizeof(TXBUF_GYRO0), (String(gyroData.gyro_0_ang_y) + ",").c_str());
      serial0_buffer_append(TXBUF_GYRO0, sizeof(TXBUF_GYRO0), (String(gyroData.gyro_0_ang_z) + ",").c_str());
      serial0_buffer_append(TXBUF_GYRO0, sizeof(TXBUF_GYRO0), (String(gyroData.gyro_0_gyr_x) + ",").c_str());
      serial0_buffer_append(TXBUF_GYRO0, sizeof(TXBUF_GYRO0), (String(gyroData.gyro_0_gyr_y) + ",").c_str());
      serial0_buffer_append(TXBUF_GYRO0, sizeof(TXBUF_GYRO0), (String(gyroData.gyro_0_gyr_z) + ",").c_str());
      serial0_buffer_append(TXBUF_GYRO0, sizeof(TXBUF_GYRO0), (String(gyroData.gyro_0_mag_x) + ",").c_str());
      serial0_buffer_append(TXBUF_GYRO0, sizeof(TXBUF_GYRO0), (String(gyroData.gyro_0_mag_y) + ",").c_str());
      serial0_buffer_append(TXBUF_GYRO0, sizeof(TXBUF_GYRO0), (String(gyroData.gyro_0_mag_z) + "").c_str());
      serial0_buffer_strip_trailing_comma(TXBUF_GYRO0);
      createChecksumSerial0(TXBUF_GYRO0, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GYRO0, sizeof(TXBUF_GYRO0), "*");
      serial0_buffer_append(TXBUF_GYRO0, sizeof(TXBUF_GYRO0), checksum);
      printf("%s\n", TXBUF_GYRO0);
    }
  }
}

void outputSerialADMplex0(void) {
  if (systemData.counters_mplex0.flag_c == true) {
    systemData.counters_mplex0.flag_c = false;
    if (systemData.output_admplex0_enabled == true) {
      char checksum[MAX_GLOBAL_CHECKSUM_SIZE];

      memset(TXBUF_ADMPLEX0, 0, sizeof(TXBUF_ADMPLEX0));
      serial0_buffer_append(TXBUF_ADMPLEX0, sizeof(TXBUF_ADMPLEX0), "$MPLEX0,");
      for (int i = 0; i < MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS; i++) {
        serial0_buffer_append(TXBUF_ADMPLEX0, sizeof(TXBUF_ADMPLEX0), (String(ad_mux_0.data[i]) + ",").c_str());
      }
      serial0_buffer_strip_trailing_comma(TXBUF_ADMPLEX0);
      createChecksumSerial0(TXBUF_ADMPLEX0, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_ADMPLEX0, sizeof(TXBUF_ADMPLEX0), "*");
      serial0_buffer_append(TXBUF_ADMPLEX0, sizeof(TXBUF_ADMPLEX0), checksum);
      printf("%s\n", TXBUF_ADMPLEX0);
    }
  }
}

void outputSerialADMplex1(void) {
  if (systemData.counters_mplex1.flag_c == true) {
    systemData.counters_mplex1.flag_c = false;
    if (systemData.output_admplex1_enabled == true) {
      char checksum[MAX_GLOBAL_CHECKSUM_SIZE];

      memset(TXBUF_ADMPLEX1, 0, sizeof(TXBUF_ADMPLEX1));
      serial0_buffer_append(TXBUF_ADMPLEX1, sizeof(TXBUF_ADMPLEX1), "$MPLEX1,");
      for (int i = 0; i < MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS; i++) {
        serial0_buffer_append(TXBUF_ADMPLEX1, sizeof(TXBUF_ADMPLEX1), (String(ad_mux_1.data[i]) + ",").c_str());
      }
      serial0_buffer_strip_trailing_comma(TXBUF_ADMPLEX1);
      createChecksumSerial0(TXBUF_ADMPLEX1, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_ADMPLEX1, sizeof(TXBUF_ADMPLEX1), "*");
      serial0_buffer_append(TXBUF_ADMPLEX1, sizeof(TXBUF_ADMPLEX1), checksum);
      printf("%s\n", TXBUF_ADMPLEX1);
    }
  }
}

void outputSerialUniverse(void) {
  if (systemData.counters_uni.flag_c == true) {
    systemData.counters_uni.flag_c = false;
    if (systemData.output_sun_enabled == true) {
      char checksum[MAX_GLOBAL_CHECKSUM_SIZE];

      memset(TXBUF_UNI, 0, sizeof(TXBUF_UNI));
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), "$SUN,");
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(siderealPlanetData.sun_ra + String(",")).c_str());
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(siderealPlanetData.sun_dec + String(",")).c_str());
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(siderealPlanetData.sun_az + String(",")).c_str());
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(siderealPlanetData.sun_alt + String(",")).c_str());
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(siderealPlanetData.sun_r + String(",")).c_str());
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(siderealPlanetData.sun_s + String("")).c_str());
      serial0_buffer_strip_trailing_comma(TXBUF_UNI);
      createChecksumSerial0(TXBUF_UNI, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), "*");
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), checksum);
      printf("%s\n", TXBUF_UNI);
    }
    if (systemData.output_earth_enabled == true) {
      char checksum[MAX_GLOBAL_CHECKSUM_SIZE];

      memset(TXBUF_UNI, 0, sizeof(TXBUF_UNI));
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), "$EARTH,");
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(siderealPlanetData.earth_ecliptic_lat + String(",")).c_str());
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(siderealPlanetData.earth_ecliptic_long + String(",")).c_str());
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(SatIOData.altitude + String("")).c_str()); // distance to earth sea level
      serial0_buffer_strip_trailing_comma(TXBUF_UNI);
      createChecksumSerial0(TXBUF_UNI, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), "*");
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), checksum);
      printf("%s\n", TXBUF_UNI);
    }
    if (systemData.output_luna_enabled == true) {
      char checksum[MAX_GLOBAL_CHECKSUM_SIZE];

      memset(TXBUF_UNI, 0, sizeof(TXBUF_UNI));
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), "$LUNA,");
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(siderealPlanetData.luna_ra + String(",")).c_str());
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(siderealPlanetData.luna_dec + String(",")).c_str());
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(siderealPlanetData.luna_az + String(",")).c_str());
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(siderealPlanetData.luna_alt + String(",")).c_str());
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(siderealPlanetData.luna_r + String(",")).c_str());
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(siderealPlanetData.luna_s + String(",")).c_str());
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(siderealPlanetData.luna_p + String(",")).c_str());
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(siderealPlanetData.luna_lum + String("")).c_str());
      serial0_buffer_strip_trailing_comma(TXBUF_UNI);
      createChecksumSerial0(TXBUF_UNI, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), "*");
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), checksum);
      printf("%s\n", TXBUF_UNI);
    }
    if (systemData.output_mercury_enabled == true) {buildOuterPlanetSentence(&mercury_sentence_spec);}
    if (systemData.output_venus_enabled == true) {buildOuterPlanetSentence(&venus_sentence_spec);}
    if (systemData.output_mars_enabled == true) {buildOuterPlanetSentence(&mars_sentence_spec);}
    if (systemData.output_jupiter_enabled == true) {buildOuterPlanetSentence(&jupiter_sentence_spec);}
    if (systemData.output_saturn_enabled == true) {buildOuterPlanetSentence(&saturn_sentence_spec);}
    if (systemData.output_uranus_enabled == true) {buildOuterPlanetSentence(&uranus_sentence_spec);}
    if (systemData.output_neptune_enabled == true) {buildOuterPlanetSentence(&neptune_sentence_spec);}
    if (systemData.output_meteors_enabled == true) {
      char checksum[MAX_GLOBAL_CHECKSUM_SIZE];

      memset(TXBUF_UNI, 0, sizeof(TXBUF_UNI));
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), "$METEOR,");
      for (int i=0; i<MAX_METEOR_SHOWERS; i++) {
        serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(String(meteor_shower_warning_system[i][0]) + String(",")).c_str());
        serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), String(String(meteor_shower_warning_system[i][1]) + String(",")).c_str());
      }
      serial0_buffer_strip_trailing_comma(TXBUF_UNI);
      createChecksumSerial0(TXBUF_UNI, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), "*");
      serial0_buffer_append(TXBUF_UNI, sizeof(TXBUF_UNI), checksum);
      printf("%s\n", TXBUF_UNI);
    }
  }
}

void outputSerialMatrix(void) {
  if (systemData.counters_mtx.flag_c == true) {
    systemData.counters_mtx.flag_c = false;
    if (systemData.output_config_matrix_enabled == true) {
      // for (int Mi=0; i_output_config_matrix < MAX_MATRIX_SWITCHES; Mi++) { // uncomment to dump all sentences at once
        char checksum[MAX_GLOBAL_CHECKSUM_SIZE];

        memset(TXBUF_SWITCHES, 0, sizeof(TXBUF_SWITCHES));
        serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), "$XMATRIX,");
        serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), String(String(i_output_config_matrix)+",").c_str());
        for (int Fi=0; Fi < MAX_MATRIX_SWITCH_FUNCTIONS; Fi++) {
          serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), String(String(matrixData.matrix_function[0][i_output_config_matrix][Fi])+",").c_str());
        }
        for (int Fi=0; Fi < MAX_MATRIX_SWITCH_FUNCTIONS; Fi++) {
          serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), String(String(matrixData.matrix_switch_operator_index[0][i_output_config_matrix][Fi])+",").c_str());
        }
        for (int Fi=0; Fi < MAX_MATRIX_SWITCH_FUNCTIONS; Fi++) {
          serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), String(String(matrixData.matrix_switch_inverted_logic[0][i_output_config_matrix][Fi])+",").c_str());
        }
        for (int Fi=0; Fi < MAX_MATRIX_SWITCH_FUNCTIONS; Fi++) {
          serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), String(String(matrixData.matrix_function_xyz[0][i_output_config_matrix][Fi][INDEX_MATRIX_FUNTION_X])+",").c_str());
        }
        for (int Fi=0; Fi < MAX_MATRIX_SWITCH_FUNCTIONS; Fi++) {
          serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), String(String(matrixData.matrix_function_xyz[0][i_output_config_matrix][Fi][INDEX_MATRIX_FUNTION_Y])+",").c_str());
        }
        for (int Fi=0; Fi < MAX_MATRIX_SWITCH_FUNCTIONS; Fi++) {
          serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), String(String(matrixData.matrix_function_xyz[0][i_output_config_matrix][Fi][INDEX_MATRIX_FUNTION_Z])+",").c_str());
        }
        serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), String(String(matrixData.flux_value[0][i_output_config_matrix])+",").c_str());
        // serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), String(String(matrixData.output_pwm[0][i_output_config_matrix][0])+",").c_str());
        // serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), String(String(matrixData.output_pwm[0][i_output_config_matrix][1])+",").c_str());
        serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), String(String(matrixData.output_mode[0][i_output_config_matrix])+",").c_str());
        serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), String(String(matrixData.index_mapped_value[0][i_output_config_matrix])+",").c_str());
        serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), String(String(matrixData.computer_assist[0][i_output_config_matrix])+",").c_str());
        serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), String(String(matrixData.matrix_port_map[0][i_output_config_matrix])+",").c_str());
        // serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), String(String(matrixData.switch_intention[0][i_output_config_matrix])+",").c_str());
        // serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), String(String(matrixData.computer_intention[0][i_output_config_matrix])+",").c_str());
        // serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), String(String(matrixData.output_value[0][i_output_config_matrix])+",").c_str());
        serial0_buffer_strip_trailing_comma(TXBUF_SWITCHES);
        createChecksumSerial0(TXBUF_SWITCHES, checksum, sizeof(checksum));
        serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), "*");
        serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), checksum);
        printf("%s\n", TXBUF_SWITCHES);
        i_output_config_matrix++;
        if (i_output_config_matrix>=MAX_MATRIX_SWITCHES) {i_output_config_matrix=0;}
      // }
    }
    if (systemData.output_config_mapping_enabled == true) {
      for (int Mi=0; Mi < MAX_MAP_SLOTS; Mi++) {
        char checksum[MAX_GLOBAL_CHECKSUM_SIZE];

        memset(TXBUF_SWITCHES, 0, sizeof(TXBUF_SWITCHES));
        serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), "$XMAPP,");
        serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), String(String(Mi)+",").c_str());
        for (int Fi=0; Fi < MAX_MAPPING_PARAMETERS; Fi++) {
          serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), String(String(mappingData.mapping_config[0][Mi][Fi])+",").c_str());
        }
        serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), String(String(mappingData.map_mode[0][Mi])+",").c_str());
        serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), String(String(mappingData.mapped_value[0][Mi])+",").c_str());

        serial0_buffer_strip_trailing_comma(TXBUF_SWITCHES);
        createChecksumSerial0(TXBUF_SWITCHES, checksum, sizeof(checksum));
        serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), "*");
        serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), checksum);
        printf("%s\n", TXBUF_SWITCHES);
      }
    }
    if (systemData.output_matrix_enabled == true) {
      char checksum[MAX_GLOBAL_CHECKSUM_SIZE];

      memset(TXBUF_SWITCHES, 0, sizeof(TXBUF_SWITCHES));
      serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), "$MATRIX,");
      // append matrix switch state data
      for (int i=0; i < MAX_MATRIX_SWITCHES; i++)
        {serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), String(String(matrixData.switch_intention[0][i])+",").c_str());}
      for (int i=0; i < MAX_MATRIX_SWITCHES; i++)
        {serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), String(String(matrixData.computer_intention[0][i])+",").c_str());}
      for (int i=0; i < MAX_MATRIX_SWITCHES; i++)
        {serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), String(String(matrixData.output_value[0][i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_SWITCHES);
      createChecksumSerial0(TXBUF_SWITCHES, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), "*");
      serial0_buffer_append(TXBUF_SWITCHES, sizeof(TXBUF_SWITCHES), checksum);
      printf("%s\n", TXBUF_SWITCHES);
    }
  }
}

void outputSerialGPIOPEnput(void) {
  if (systemData.counters_gpiope_in.flag_c == true) {
    systemData.counters_gpiope_in.flag_c = false;

    if (systemData.output_input_portcontroller == true) {
      char checksum[MAX_GLOBAL_CHECKSUM_SIZE];

      #ifdef GPIOPE_USE_INPUT_0
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE0,");
      for (int i=0; i < GPIOPE_INPUT_0.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_0.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_0

      #ifdef GPIOPE_USE_INPUT_1
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE1,");
      for (int i=0; i < GPIOPE_INPUT_1.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_1.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_1

      #ifdef GPIOPE_USE_INPUT_2
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE2,");
      for (int i=0; i < GPIOPE_INPUT_2.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_2.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_2

      #ifdef GPIOPE_USE_INPUT_3
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE3,");
      for (int i=0; i < GPIOPE_INPUT_3.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_3.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_3

      #ifdef GPIOPE_USE_INPUT_4
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE4,");
      for (int i=0; i < GPIOPE_INPUT_4.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_4.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_4

      #ifdef GPIOPE_USE_INPUT_5
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE5,");
      for (int i=0; i < GPIOPE_INPUT_5.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_5.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_5

      #ifdef GPIOPE_USE_INPUT_6
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE6,");
      for (int i=0; i < GPIOPE_INPUT_6.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_6.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_6

      #ifdef GPIOPE_USE_INPUT_7
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE7,");
      for (int i=0; i < GPIOPE_INPUT_7.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_7.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_7

      #ifdef GPIOPE_USE_INPUT_8
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE8,");
      for (int i=0; i < GPIOPE_INPUT_8.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_8.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_8

      #ifdef GPIOPE_USE_INPUT_9
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE9,");
      for (int i=0; i < GPIOPE_INPUT_9.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_9.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_9

      #ifdef GPIOPE_USE_INPUT_10
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE10,");
      for (int i=0; i < GPIOPE_INPUT_10.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_10.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_10

      #ifdef GPIOPE_USE_INPUT_11
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE11,");
      for (int i=0; i < GPIOPE_INPUT_11.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_11.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_11

      #ifdef GPIOPE_USE_INPUT_12
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE12,");
      for (int i=0; i < GPIOPE_INPUT_12.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_12.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_12

      #ifdef GPIOPE_USE_INPUT_13
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE13,");
      for (int i=0; i < GPIOPE_INPUT_13.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_13.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_13

      #ifdef GPIOPE_USE_INPUT_14
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE14,");
      for (int i=0; i < GPIOPE_INPUT_14.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_14.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_14

      #ifdef GPIOPE_USE_INPUT_15
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE15,");
      for (int i=0; i < GPIOPE_INPUT_15.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_15.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_15

      #ifdef GPIOPE_USE_INPUT_16
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE16,");
      for (int i=0; i < GPIOPE_INPUT_16.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_16.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_16

      #ifdef GPIOPE_USE_INPUT_17
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE17,");
      for (int i=0; i < GPIOPE_INPUT_17.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_17.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_17

      #ifdef GPIOPE_USE_INPUT_18
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE18,");
      for (int i=0; i < GPIOPE_INPUT_18.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_18.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_18

      #ifdef GPIOPE_USE_INPUT_19
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE19,");
      for (int i=0; i < GPIOPE_INPUT_19.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_19.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_19

      #ifdef GPIOPE_USE_INPUT_20
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE20,");
      for (int i=0; i < GPIOPE_INPUT_20.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_20.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_20

      #ifdef GPIOPE_USE_INPUT_21
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE21,");
      for (int i=0; i < GPIOPE_INPUT_21.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_21.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_21

      #ifdef GPIOPE_USE_INPUT_22
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE22,");
      for (int i=0; i < GPIOPE_INPUT_22.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_22.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_22

      #ifdef GPIOPE_USE_INPUT_23
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE23,");
      for (int i=0; i < GPIOPE_INPUT_23.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_23.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_23

      #ifdef GPIOPE_USE_INPUT_24
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE24,");
      for (int i=0; i < GPIOPE_INPUT_24.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_24.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_24

      #ifdef GPIOPE_USE_INPUT_25
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE25,");
      for (int i=0; i < GPIOPE_INPUT_25.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_25.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_25

      #ifdef GPIOPE_USE_INPUT_26
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE26,");
      for (int i=0; i < GPIOPE_INPUT_26.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_26.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_26

      #ifdef GPIOPE_USE_INPUT_27
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE27,");
      for (int i=0; i < GPIOPE_INPUT_27.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_27.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_27

      #ifdef GPIOPE_USE_INPUT_28
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE28,");
      for (int i=0; i < GPIOPE_INPUT_28.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_28.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_28

      #ifdef GPIOPE_USE_INPUT_29
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE29,");
      for (int i=0; i < GPIOPE_INPUT_29.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_29.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_29

      #ifdef GPIOPE_USE_INPUT_30
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE30,");
      for (int i=0; i < GPIOPE_INPUT_30.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_30.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_30

      #ifdef GPIOPE_USE_INPUT_31
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE31,");
      for (int i=0; i < GPIOPE_INPUT_31.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_31.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_31

      #ifdef GPIOPE_USE_INPUT_32
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE32,");
      for (int i=0; i < GPIOPE_INPUT_32.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_32.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_32

      #ifdef GPIOPE_USE_INPUT_33
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE33,");
      for (int i=0; i < GPIOPE_INPUT_33.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_33.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_33

      #ifdef GPIOPE_USE_INPUT_34
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE34,");
      for (int i=0; i < GPIOPE_INPUT_34.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_34.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_34

      #ifdef GPIOPE_USE_INPUT_35
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE35,");
      for (int i=0; i < GPIOPE_INPUT_35.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_35.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_35

      #ifdef GPIOPE_USE_INPUT_36
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE36,");
      for (int i=0; i < GPIOPE_INPUT_36.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_36.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_36

      #ifdef GPIOPE_USE_INPUT_37
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE37,");
      for (int i=0; i < GPIOPE_INPUT_37.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_37.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_37

      #ifdef GPIOPE_USE_INPUT_38
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE38,");
      for (int i=0; i < GPIOPE_INPUT_38.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_38.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_38

      #ifdef GPIOPE_USE_INPUT_39
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE39,");
      for (int i=0; i < GPIOPE_INPUT_39.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_39.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_39

      #ifdef GPIOPE_USE_INPUT_40
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE40,");
      for (int i=0; i < GPIOPE_INPUT_40.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_40.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_40

      #ifdef GPIOPE_USE_INPUT_41
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE41,");
      for (int i=0; i < GPIOPE_INPUT_41.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_41.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_41

      #ifdef GPIOPE_USE_INPUT_42
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE42,");
      for (int i=0; i < GPIOPE_INPUT_42.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_42.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_42

      #ifdef GPIOPE_USE_INPUT_43
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE43,");
      for (int i=0; i < GPIOPE_INPUT_43.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_43.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_43

      #ifdef GPIOPE_USE_INPUT_44
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE44,");
      for (int i=0; i < GPIOPE_INPUT_44.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_44.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_44

      #ifdef GPIOPE_USE_INPUT_45
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE45,");
      for (int i=0; i < GPIOPE_INPUT_45.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_45.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_45

      #ifdef GPIOPE_USE_INPUT_46
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE46,");
      for (int i=0; i < GPIOPE_INPUT_46.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_46.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_46

      #ifdef GPIOPE_USE_INPUT_47
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE47,");
      for (int i=0; i < GPIOPE_INPUT_47.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_47.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_47

      #ifdef GPIOPE_USE_INPUT_48
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE48,");
      for (int i=0; i < GPIOPE_INPUT_48.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_48.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_48

      #ifdef GPIOPE_USE_INPUT_49
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE49,");
      for (int i=0; i < GPIOPE_INPUT_49.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_49.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_49

      #ifdef GPIOPE_USE_INPUT_50
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE50,");
      for (int i=0; i < GPIOPE_INPUT_50.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_50.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_50

      #ifdef GPIOPE_USE_INPUT_51
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE51,");
      for (int i=0; i < GPIOPE_INPUT_51.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_51.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_51

      #ifdef GPIOPE_USE_INPUT_52
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE52,");
      for (int i=0; i < GPIOPE_INPUT_52.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_52.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_52

      #ifdef GPIOPE_USE_INPUT_53
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE53,");
      for (int i=0; i < GPIOPE_INPUT_53.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_53.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_53

      #ifdef GPIOPE_USE_INPUT_54
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE54,");
      for (int i=0; i < GPIOPE_INPUT_54.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_54.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_54

      #ifdef GPIOPE_USE_INPUT_55
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE55,");
      for (int i=0; i < GPIOPE_INPUT_55.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_55.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_55

      #ifdef GPIOPE_USE_INPUT_56
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE56,");
      for (int i=0; i < GPIOPE_INPUT_56.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_56.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_56

      #ifdef GPIOPE_USE_INPUT_57
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE57,");
      for (int i=0; i < GPIOPE_INPUT_57.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_57.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_57

      #ifdef GPIOPE_USE_INPUT_58
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE58,");
      for (int i=0; i < GPIOPE_INPUT_58.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_58.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_58

      #ifdef GPIOPE_USE_INPUT_59
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE59,");
      for (int i=0; i < GPIOPE_INPUT_59.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_59.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_59

      #ifdef GPIOPE_USE_INPUT_60
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE60,");
      for (int i=0; i < GPIOPE_INPUT_60.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_60.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_60

      #ifdef GPIOPE_USE_INPUT_61
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE61,");
      for (int i=0; i < GPIOPE_INPUT_61.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_61.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_61

      #ifdef GPIOPE_USE_INPUT_62
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE62,");
      for (int i=0; i < GPIOPE_INPUT_62.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_62.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_62

      #ifdef GPIOPE_USE_INPUT_63
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE63,");
      for (int i=0; i < GPIOPE_INPUT_63.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_63.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_63

      #ifdef GPIOPE_USE_INPUT_64
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE64,");
      for (int i=0; i < GPIOPE_INPUT_64.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_64.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_64

      #ifdef GPIOPE_USE_INPUT_65
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE65,");
      for (int i=0; i < GPIOPE_INPUT_65.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_65.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_65

      #ifdef GPIOPE_USE_INPUT_66
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE66,");
      for (int i=0; i < GPIOPE_INPUT_66.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_66.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_66

      #ifdef GPIOPE_USE_INPUT_67
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE67,");
      for (int i=0; i < GPIOPE_INPUT_67.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_67.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_67

      #ifdef GPIOPE_USE_INPUT_68
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE68,");
      for (int i=0; i < GPIOPE_INPUT_68.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_68.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_68

      #ifdef GPIOPE_USE_INPUT_69
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE69,");
      for (int i=0; i < GPIOPE_INPUT_69.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_69.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_69

      #ifdef GPIOPE_USE_INPUT_70
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE70,");
      for (int i=0; i < GPIOPE_INPUT_70.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_70.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_70

      #ifdef GPIOPE_USE_INPUT_71
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE71,");
      for (int i=0; i < GPIOPE_INPUT_71.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_71.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_71

      #ifdef GPIOPE_USE_INPUT_72
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE72,");
      for (int i=0; i < GPIOPE_INPUT_72.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_72.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_72

      #ifdef GPIOPE_USE_INPUT_73
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE73,");
      for (int i=0; i < GPIOPE_INPUT_73.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_73.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_73

      #ifdef GPIOPE_USE_INPUT_74
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE74,");
      for (int i=0; i < GPIOPE_INPUT_74.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_74.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_74

      #ifdef GPIOPE_USE_INPUT_75
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE75,");
      for (int i=0; i < GPIOPE_INPUT_75.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_75.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_75

      #ifdef GPIOPE_USE_INPUT_76
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE76,");
      for (int i=0; i < GPIOPE_INPUT_76.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_76.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_76

      #ifdef GPIOPE_USE_INPUT_77
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE77,");
      for (int i=0; i < GPIOPE_INPUT_77.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_77.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_77

      #ifdef GPIOPE_USE_INPUT_78
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE78,");
      for (int i=0; i < GPIOPE_INPUT_78.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_78.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_78

      #ifdef GPIOPE_USE_INPUT_79
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE79,");
      for (int i=0; i < GPIOPE_INPUT_79.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_79.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_79

      #ifdef GPIOPE_USE_INPUT_80
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE80,");
      for (int i=0; i < GPIOPE_INPUT_80.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_80.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_80

      #ifdef GPIOPE_USE_INPUT_81
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE81,");
      for (int i=0; i < GPIOPE_INPUT_81.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_81.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_81

      #ifdef GPIOPE_USE_INPUT_82
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE82,");
      for (int i=0; i < GPIOPE_INPUT_82.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_82.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_82

      #ifdef GPIOPE_USE_INPUT_83
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE83,");
      for (int i=0; i < GPIOPE_INPUT_83.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_83.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_83

      #ifdef GPIOPE_USE_INPUT_84
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE84,");
      for (int i=0; i < GPIOPE_INPUT_84.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_84.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_84

      #ifdef GPIOPE_USE_INPUT_85
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE85,");
      for (int i=0; i < GPIOPE_INPUT_85.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_85.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_85

      #ifdef GPIOPE_USE_INPUT_86
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE86,");
      for (int i=0; i < GPIOPE_INPUT_86.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_86.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_86

      #ifdef GPIOPE_USE_INPUT_87
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE87,");
      for (int i=0; i < GPIOPE_INPUT_87.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_87.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_87

      #ifdef GPIOPE_USE_INPUT_88
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE88,");
      for (int i=0; i < GPIOPE_INPUT_88.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_88.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_88

      #ifdef GPIOPE_USE_INPUT_89
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE89,");
      for (int i=0; i < GPIOPE_INPUT_89.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_89.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_89

      #ifdef GPIOPE_USE_INPUT_90
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE90,");
      for (int i=0; i < GPIOPE_INPUT_90.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_90.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_90

      #ifdef GPIOPE_USE_INPUT_91
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE91,");
      for (int i=0; i < GPIOPE_INPUT_91.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_91.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_91

      #ifdef GPIOPE_USE_INPUT_92
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE92,");
      for (int i=0; i < GPIOPE_INPUT_92.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_92.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_92

      #ifdef GPIOPE_USE_INPUT_93
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE93,");
      for (int i=0; i < GPIOPE_INPUT_93.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_93.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_93

      #ifdef GPIOPE_USE_INPUT_94
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE94,");
      for (int i=0; i < GPIOPE_INPUT_94.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_94.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_94

      #ifdef GPIOPE_USE_INPUT_95
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE95,");
      for (int i=0; i < GPIOPE_INPUT_95.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_95.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_95

      #ifdef GPIOPE_USE_INPUT_96
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE96,");
      for (int i=0; i < GPIOPE_INPUT_96.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_96.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_96

      #ifdef GPIOPE_USE_INPUT_97
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE97,");
      for (int i=0; i < GPIOPE_INPUT_97.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_97.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_97

      #ifdef GPIOPE_USE_INPUT_98
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE98,");
      for (int i=0; i < GPIOPE_INPUT_98.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_98.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_98

      #ifdef GPIOPE_USE_INPUT_99
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE99,");
      for (int i=0; i < GPIOPE_INPUT_99.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_99.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_99

      #ifdef GPIOPE_USE_INPUT_100
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE100,");
      for (int i=0; i < GPIOPE_INPUT_100.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_100.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_100

      #ifdef GPIOPE_USE_INPUT_101
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE101,");
      for (int i=0; i < GPIOPE_INPUT_101.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_101.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_101

      #ifdef GPIOPE_USE_INPUT_102
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE102,");
      for (int i=0; i < GPIOPE_INPUT_102.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_102.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_102

      #ifdef GPIOPE_USE_INPUT_103
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE103,");
      for (int i=0; i < GPIOPE_INPUT_103.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_103.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_103

      #ifdef GPIOPE_USE_INPUT_104
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE104,");
      for (int i=0; i < GPIOPE_INPUT_104.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_104.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_104

      #ifdef GPIOPE_USE_INPUT_105
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE105,");
      for (int i=0; i < GPIOPE_INPUT_105.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_105.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_105

      #ifdef GPIOPE_USE_INPUT_106
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE106,");
      for (int i=0; i < GPIOPE_INPUT_106.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_106.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_106

      #ifdef GPIOPE_USE_INPUT_107
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE107,");
      for (int i=0; i < GPIOPE_INPUT_107.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_107.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_107

      #ifdef GPIOPE_USE_INPUT_108
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE108,");
      for (int i=0; i < GPIOPE_INPUT_108.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_108.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_108

      #ifdef GPIOPE_USE_INPUT_109
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE109,");
      for (int i=0; i < GPIOPE_INPUT_109.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_109.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_109

      #ifdef GPIOPE_USE_INPUT_110
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE110,");
      for (int i=0; i < GPIOPE_INPUT_110.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_110.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_110

      #ifdef GPIOPE_USE_INPUT_111
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE111,");
      for (int i=0; i < GPIOPE_INPUT_111.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_111.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_111

      #ifdef GPIOPE_USE_INPUT_112
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE112,");
      for (int i=0; i < GPIOPE_INPUT_112.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_112.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_112

      #ifdef GPIOPE_USE_INPUT_113
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE113,");
      for (int i=0; i < GPIOPE_INPUT_113.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_113.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_113

      #ifdef GPIOPE_USE_INPUT_114
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE114,");
      for (int i=0; i < GPIOPE_INPUT_114.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_114.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_114

      #ifdef GPIOPE_USE_INPUT_115
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE115,");
      for (int i=0; i < GPIOPE_INPUT_115.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_115.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_115

      #ifdef GPIOPE_USE_INPUT_116
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE116,");
      for (int i=0; i < GPIOPE_INPUT_116.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_116.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_116

      #ifdef GPIOPE_USE_INPUT_117
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE117,");
      for (int i=0; i < GPIOPE_INPUT_117.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_117.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_117

      #ifdef GPIOPE_USE_INPUT_118
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE118,");
      for (int i=0; i < GPIOPE_INPUT_118.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_118.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_118

      #ifdef GPIOPE_USE_INPUT_119
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE119,");
      for (int i=0; i < GPIOPE_INPUT_119.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_119.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_119

      #ifdef GPIOPE_USE_INPUT_120
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE120,");
      for (int i=0; i < GPIOPE_INPUT_120.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_120.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_120

      #ifdef GPIOPE_USE_INPUT_121
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE121,");
      for (int i=0; i < GPIOPE_INPUT_121.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_121.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_121

      #ifdef GPIOPE_USE_INPUT_122
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE122,");
      for (int i=0; i < GPIOPE_INPUT_122.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_122.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_122

      #ifdef GPIOPE_USE_INPUT_123
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE123,");
      for (int i=0; i < GPIOPE_INPUT_123.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_123.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_123

      #ifdef GPIOPE_USE_INPUT_124
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE124,");
      for (int i=0; i < GPIOPE_INPUT_124.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_124.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_124

      #ifdef GPIOPE_USE_INPUT_125
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE125,");
      for (int i=0; i < GPIOPE_INPUT_125.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_125.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_125

      #ifdef GPIOPE_USE_INPUT_126
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE126,");
      for (int i=0; i < GPIOPE_INPUT_126.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_126.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_126

      #ifdef GPIOPE_USE_INPUT_127
      memset(TXBUF_GPIOPE, 0, sizeof(TXBUF_GPIOPE));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "$GPIOE127,");
      for (int i=0; i < GPIOPE_INPUT_127.max_input_values; i++)
        {serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), String(String(GPIOPE_INPUT_127.input_value[i])+",").c_str());}
      serial0_buffer_strip_trailing_comma(TXBUF_GPIOPE);
      createChecksumSerial0(TXBUF_GPIOPE, checksum, sizeof(checksum));
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), "*");
      serial0_buffer_append(TXBUF_GPIOPE, sizeof(TXBUF_GPIOPE), checksum);
      printf("%s\n", TXBUF_GPIOPE);
      #endif // GPIOPE_USE_INPUT_127

    }

  }
}

/**
 * The per-switch matrix dump below pages through MAX_MATRIX_SWITCHES in
 * fixed-width chunks (rather than printing all of them on one very wide
 * line) using this column count and the two helpers that follow.
 * 
 * @note currently the max required strlen is about 12 (for lat/long), so
 *       any longest required/expected values for function xyz should
 *       fit within a column width of 12 characters.
 */
#define STAT_SWITCHES_PER_PAGE 23
#define STAT_COL_FORMAT_D  "%-12d"
#define STAT_COL_FORMAT_LD "%-12ld"
#define STAT_COL_FORMAT_F  "%-12.2f"
#define STAT_COL_WIDTH 12
#define STAT_LABEL_WIDTH 22 // width of labels such as "Computer Assist    :  "
#define STAT_LABEL_FMT "%-22s:  " // keep field width in sync with STAT_LABEL_WIDTH
#define STAT_LABEL_BLANK_FMT "%-25s" // header/blank-label version of STAT_LABEL_FMT (same total width, no colon)
#define STAT_WIDE_COL_WIDTH 20 // wide enough for the longest column label ("System Timing")
#define STAT_WIDE_COL_FORMAT_S  "%-20s"
#define STAT_WIDE_COL_FORMAT_LD "%-20ld"
#define STAT_WIDE_COL_FORMAT_F  "%-20.7f"
#define STAT_GYRO_COL_WIDTH 30 // wide enough for the longest header ("Angle (XZ=+/-180 Y=+/-90deg)")
#define STAT_GYRO_COL_FORMAT_S  "%-30s"
#define STAT_GYRO_COL_FORMAT_F  "%-30.2f"
#define STAT_GYRO_COL_FORMAT_LD "%-30ld"
// StarNav Sweep table: Table needs 1 extra char over STAT_WIDE_COL_WIDTH, and
// Name/Type/Constellation need 8 extra, to fit their longer catalog strings.
#define STAT_STARNAV_TABLE_COL_WIDTH 21
#define STAT_STARNAV_TABLE_COL_FORMAT_S "%-21s"
#define STAT_STARNAV_WIDE_COL_WIDTH 28
#define STAT_STARNAV_WIDE_COL_FORMAT_S  "%-28s"
// Every table in outputStat() shares one separator width: the widest table
// (the GPIOPE/Computer Assist paged channel dumps, STAT_SWITCHES_PER_PAGE
// columns wide) sets the width for every horizontal rule, not just its own.
#define STAT_SEPARATOR_WIDTH (STAT_LABEL_WIDTH + (STAT_SWITCHES_PER_PAGE * STAT_COL_WIDTH))

void printArray(signed long arr[], int start, int end) {
    for (int i = start; i < end; i++) {
        printf(STAT_COL_FORMAT_LD, arr[i]); // Left-align with 5-character width
    }
    printf("\n");
}

static void printSwitchIndexHeader(int start, int end) {
    for (int i = start; i < end; i++) {printf(STAT_COL_FORMAT_D, i);}
    printf("\n");
}

/*
 * Prints a dashed rule STAT_SEPARATOR_WIDTH wide -- the same width
 * everywhere in outputStat(), regardless of which table it separates.
 */
static void printStatSeparator(void) {
    for (int i = 0; i < STAT_SEPARATOR_WIDTH; i++) {putchar('-');}
    putchar('\n');
}

/*
 * Prints the separator/index-header/separator block shared by every paged
 * switch/channel table, labeling [page_start, page_end).
 */
static void printStatPageHeader(int page_start, int page_end) {
    printStatSeparator();
    printf(STAT_LABEL_BLANK_FMT, "");
    printSwitchIndexHeader(page_start, page_end);
    printStatSeparator();
}

/*
 * Prints one paginated "<label>:  v0 v1 v2 ..." table of task_ffreq_t values
 * for an array of per-channel/per-pin counters, paging in
 * STAT_SWITCHES_PER_PAGE-wide chunks. Shared by the ADMPlex and GPIOPE Hz
 * dumps so their paging logic lives in one place.
 */
static void printStatChannelHzTable(const char* label, const SystemConuters* counters, int count) {
    for (int page_start = 0; page_start < count; page_start += STAT_SWITCHES_PER_PAGE) {
        int page_end = page_start + STAT_SWITCHES_PER_PAGE;
        if (page_end > count) {page_end = count;}
        printStatPageHeader(page_start, page_end);
        printf(STAT_LABEL_FMT, label);
        for (int i = page_start; i < page_end; i++) {printf(STAT_COL_FORMAT_LD, (long)counters[i].task_ffreq_t);}
        printf("\n");
    }
}

void outputStat(void) {

  if (systemData.output_stat_datetime == true) {
    // ----------------------------------------------------------------------------------------------------------------------------
    //                                                                                                              PRINT CLOCKS
    // ----------------------------------------------------------------------------------------------------------------------------
    printStatSeparator();
    printf(STAT_LABEL_FMT "%llu\n", "Timestamp (unix uS)",  SatIOData.systemTime.unixtime_uS);
    {
        struct StatTimeDomain { const char* label; const SatIOTimeData* t; };
        const StatTimeDomain domains[] = {
            {"GPS",    &SatIOData.GPSTime},
            {"System", &SatIOData.systemTime},
            {"Local",  &SatIOData.localTime},
            {"LMST",   &SatIOData.localMeanSolarTime},
            {"LST",    &SatIOData.localSiderealTime},
        };
        const int numDomains = sizeof(domains) / sizeof(domains[0]);

        printStatSeparator();
        printf(STAT_LABEL_BLANK_FMT, "");
        for (int i = 0; i < numDomains; i++) {printf(STAT_WIDE_COL_FORMAT_S, domains[i].label);}
        printf("\n");
        printStatSeparator();
        printf(STAT_LABEL_FMT, "Time");
        for (int i = 0; i < numDomains; i++) {printf(STAT_WIDE_COL_FORMAT_S, domains[i].t->padded_time_HHMMSS);}
        printf("\n");
        printf(STAT_LABEL_FMT, "Date");
        for (int i = 0; i < numDomains; i++) {printf(STAT_WIDE_COL_FORMAT_S, domains[i].t->padded_date_DDMMYY);}
        printf("\n");
        printf(STAT_LABEL_FMT, "Sync");
        for (int i = 0; i < numDomains; i++) {printf(STAT_WIDE_COL_FORMAT_S, domains[i].t->sync_padded_time_HHMMSS);}
        printf("\n");
    }
  }

  if (systemData.output_stat_task_rates == true) {
    // ----------------------------------------------------------------------------------------------------------------------------
    //                                                                                                         PRINT TASK RATES (Hz)
    printStatSeparator();
    printf(STAT_LABEL_FMT "%s\n", "PowerConfig", pwrConfigCurrent.name);
    {
        struct StatRatePair { const char* label; long ffreq; long freq; };
        const StatRatePair rates[] = {
            {"System Timing",  systemData.counters_st.task_ffreq_t,              systemData.counters_st.task_freq_t},
            {"GPS",            systemData.counters_gps.task_ffreq_t,             systemData.counters_gps.task_freq_t},
            {"Gyro0",          systemData.counters_gyr0.task_ffreq_t,            systemData.counters_gyr0.task_freq_t},
            // {"INS",         systemData.counters_ins.task_ffreq_t,             systemData.counters_gyr0.task_freq_t}, // INS runs inside the Gyro0 task
            {"Mplex0",         systemData.counters_mplex0.task_ffreq_t,          systemData.counters_mplex0.task_freq_t},
            {"Mplex1",         systemData.counters_mplex1.task_ffreq_t,          systemData.counters_mplex1.task_freq_t},
            {"Universe",       systemData.counters_uni.task_ffreq_t,             systemData.counters_uni.task_freq_t},
            {"GPIOPE In",      systemData.counters_gpiope_in.task_ffreq_t,         systemData.counters_gpiope_in.task_freq_t},
            {"Matrix",         systemData.counters_mtx.task_ffreq_t,             systemData.counters_mtx.task_freq_t},
            {"GPIOPE Out",     systemData.counters_gpiope_out.task_ffreq_t,             systemData.counters_mtx.task_freq_t}, // PCO runs inside the Matrix task
            {"Display",        systemData.counters_dsp.task_ffreq_t,             systemData.counters_dsp.task_freq_t},
            {"SatIO Tx",       systemData.counters_SatIO_serial_tx.task_ffreq_t, systemData.counters_SatIO_serial_tx.task_freq_t},
        };
        const int numRates = sizeof(rates) / sizeof(rates[0]);

        printStatSeparator();
        printf(STAT_LABEL_BLANK_FMT, "");
        for (int i = 0; i < numRates; i++) {printf(STAT_WIDE_COL_FORMAT_S, rates[i].label);}
        printf("\n");
        printStatSeparator();
        printf(STAT_LABEL_FMT, "Function Hz");
        for (int i = 0; i < numRates; i++) {printf(STAT_WIDE_COL_FORMAT_LD, rates[i].ffreq);}
        printf("\n");
        printf(STAT_LABEL_FMT, "Task Hz");
        for (int i = 0; i < numRates; i++) {printf(STAT_WIDE_COL_FORMAT_LD, rates[i].freq);}
        printf("\n");
    }
  }

  if (systemData.output_stat_position == true) {
    // ----------------------------------------------------------------------------------------------------------------------------
    //                                                                                                    PRINT POSITION / TARGET
    // ----------------------------------------------------------------------------------------------------------------------------
    printStatSeparator();
    printf(STAT_LABEL_FMT "%s\n", "Satellites", gnggaData.satellite_count);
    // location
    {
        struct StatPosSource { const char* label; double lat; double lon; double heading; double altitude; double speed; };
        const StatPosSource sources[] = {
            {"GPS",    SatIOData.degrees_latitude,        SatIOData.degrees_longitude,        SatIOData.ground_heading,        SatIOData.altitude,        SatIOData.speed},
            {"User",   SatIOData.user_degrees_latitude,   SatIOData.user_degrees_longitude,   SatIOData.user_ground_heading,   SatIOData.user_altitude,   SatIOData.user_speed},
            {"System", SatIOData.system_degrees_latitude, SatIOData.system_degrees_longitude, SatIOData.system_ground_heading, SatIOData.system_altitude, SatIOData.system_speed},
        };
        const int numSources = sizeof(sources) / sizeof(sources[0]);
        const char* columns[] = {"Latitude", "Longitude", "Heading", "Altitude", "Speed"};
        const int numColumns = sizeof(columns) / sizeof(columns[0]);

        printStatSeparator();
        printf(STAT_LABEL_BLANK_FMT, "");
        for (int i = 0; i < numColumns; i++) {printf(STAT_WIDE_COL_FORMAT_S, columns[i]);}
        printf("\n");
        printStatSeparator();
        for (int i = 0; i < numSources; i++) {
            printf(STAT_LABEL_FMT, sources[i].label);
            printf(STAT_WIDE_COL_FORMAT_F, sources[i].lat);
            printf(STAT_WIDE_COL_FORMAT_F, sources[i].lon);
            printf(STAT_WIDE_COL_FORMAT_F, sources[i].heading);
            printf(STAT_WIDE_COL_FORMAT_F, sources[i].altitude);
            printf(STAT_WIDE_COL_FORMAT_F, sources[i].speed);
            printf("\n");
        }
    }
    #ifdef SatIO_USE_UNIVERSE
    /**
     * Clestial Sphere Attitude.
     * Observe if absolute local zenith RA Dec must always be correct.
     * Observe if level, upright gyro RA Dec are accurate to absolute local zenith RA Dec.
     *
     * Guarded by SatIO_USE_UNIVERSE (not SatIO_USE_GYRO_0): local_sidereal_attitude
     * (Zenith) and the StarNav table below are only ever populated by taskUniverse()
     * (see UnidentifiedStudios_TaskHandler.cpp), which does not exist unless
     * SatIO_USE_UNIVERSE is defined -- these two build options are independently
     * toggleable (see UnidentifiedStudios_Config.h), so a build with only
     * SatIO_USE_GYRO_0 defined must not attempt this block. Only the "Gyro" row
     * additionally needs SatIO_USE_GYRO_0, matching the
     * INDEX_MATRIX_SWITCH_FUNCTION_LOCAL_GYRO_0_RA/DEC guard convention in
     * UnidentifiedStudios_Matrix.h.
     */
    {
        struct StatRaDecSource { const char* label; const char* ra; const char* dec; double az; double alt; };
        const StatRaDecSource sources[] = {
            {"Zenith", siderealPlanetData.local_sidereal_attitude.formatted_ra_str,  siderealPlanetData.local_sidereal_attitude.formatted_dec_str,  siderealPlanetData.local_sidereal_attitude.az,  siderealPlanetData.local_sidereal_attitude.alt},
            #ifdef SatIO_USE_GYRO_0
            {"Gyro",   siderealPlanetData.gyro_0_sidereal_attitude.formatted_ra_str, siderealPlanetData.gyro_0_sidereal_attitude.formatted_dec_str, siderealPlanetData.gyro_0_sidereal_attitude.az, siderealPlanetData.gyro_0_sidereal_attitude.alt},
            #endif
            {"User",   SatIOData.user_sidereal_attitude.formatted_ra_str,            SatIOData.user_sidereal_attitude.formatted_dec_str,            SatIOData.user_sidereal_attitude.az,            SatIOData.user_sidereal_attitude.alt},
            {"System", SatIOData.system_sidereal_attitude.formatted_ra_str,          SatIOData.system_sidereal_attitude.formatted_dec_str,          SatIOData.system_sidereal_attitude.az,          SatIOData.system_sidereal_attitude.alt},
        };
        const int numSources = sizeof(sources) / sizeof(sources[0]);

        printStatSeparator();
        printf(STAT_LABEL_BLANK_FMT, "");
        for (int i = 0; i < numSources; i++) {printf(STAT_WIDE_COL_FORMAT_S, sources[i].label);}
        printf("\n");
        printStatSeparator();
        printf(STAT_LABEL_FMT, "RA");
        for (int i = 0; i < numSources; i++) {printf(STAT_WIDE_COL_FORMAT_S, sources[i].ra);}
        printf("\n");
        printf(STAT_LABEL_FMT, "Dec");
        for (int i = 0; i < numSources; i++) {printf(STAT_WIDE_COL_FORMAT_S, sources[i].dec);}
        printf("\n");
        printf(STAT_LABEL_FMT, "Az");
        for (int i = 0; i < numSources; i++) {printf(STAT_WIDE_COL_FORMAT_F, sources[i].az);}
        printf("\n");
        printf(STAT_LABEL_FMT, "Alt");
        for (int i = 0; i < numSources; i++) {printf(STAT_WIDE_COL_FORMAT_F, sources[i].alt);}
        printf("\n");
      }
      // StarNav Sweep: one row per identified object, in the order the sweep
      // found them. Slots fill contiguously from 0 (see starNavSweep()), so
      // the first invalid slot marks the end of the results.
      {
          const char* columns[] = {"Table Index", "Table", "Number", "Name", "Type", "Constellation", "Distance", "Azimuth", "Altitude", "Rise", "Set"};
          const char* colFormats[] = {
              STAT_WIDE_COL_FORMAT_S,          // Table Index
              STAT_STARNAV_TABLE_COL_FORMAT_S, // Table (+1 char)
              STAT_WIDE_COL_FORMAT_S,          // Number
              STAT_STARNAV_WIDE_COL_FORMAT_S,  // Name (+8 chars)
              STAT_STARNAV_WIDE_COL_FORMAT_S,  // Type (+8 chars)
              STAT_STARNAV_WIDE_COL_FORMAT_S,  // Constellation (+8 chars)
              STAT_WIDE_COL_FORMAT_S,          // Distance
              STAT_WIDE_COL_FORMAT_S,          // Azimuth
              STAT_WIDE_COL_FORMAT_S,          // Altitude
              STAT_WIDE_COL_FORMAT_S,          // Rise
              STAT_WIDE_COL_FORMAT_S,          // Set
          };
          const int numColumns = sizeof(columns) / sizeof(columns[0]);

          // uncomment to print results from StarNav sweep
          // printStatSeparator();
          // printf(STAT_LABEL_BLANK_FMT, "");
          // for (int i = 0; i < numColumns; i++) {printf(colFormats[i], columns[i]);}
          // printf("\n");
          // printStatSeparator();
          // for (int i = 0; i < MAX_STARNAV_OBJECTS; i++) {
          //     if ((siderealObjectSweep.object_table_i[i] < 0) || (siderealObjectSweep.object_number[i] < 0)) {
          //         break;
          //     }
          //     char label[STAT_LABEL_WIDTH];
          //     (void)snprintf(label, sizeof(label), "StarNav%d", i);
          //     printf(STAT_LABEL_FMT, label);
          //     printf(STAT_WIDE_COL_FORMAT_LD, (long)siderealObjectSweep.object_table_i[i]);
          //     printf(STAT_STARNAV_TABLE_COL_FORMAT_S, siderealObjectSweep.object_table_name[i]);
          //     printf(STAT_WIDE_COL_FORMAT_LD, (long)siderealObjectSweep.object_number[i]);
          //     printf(STAT_STARNAV_WIDE_COL_FORMAT_S, siderealObjectSweep.object_name[i]);
          //     printf(STAT_STARNAV_WIDE_COL_FORMAT_S, siderealObjectSweep.object_type[i]);
          //     printf(STAT_STARNAV_WIDE_COL_FORMAT_S, siderealObjectSweep.object_con[i]);
          //     printf(STAT_WIDE_COL_FORMAT_F,  siderealObjectSweep.object_dist[i]);
          //     printf(STAT_WIDE_COL_FORMAT_F,  siderealObjectSweep.object_az[i]);
          //     printf(STAT_WIDE_COL_FORMAT_F,  siderealObjectSweep.object_alt[i]);
          //     printf(STAT_WIDE_COL_FORMAT_F,  siderealObjectSweep.object_r[i]);
          //     printf(STAT_WIDE_COL_FORMAT_F,  siderealObjectSweep.object_s[i]);
          //     printf("\n");
          // }
      }
    #endif // SatIO_USE_UNIVERSE
  }

  #ifdef SatIO_USE_GYRO_0
  if (systemData.output_stat_gyro == true) {
    // ----------------------------------------------------------------------------------------------------------------------------
    //                                                                                                 PRINT ORIENTATION / SENSORS
    // ----------------------------------------------------------------------------------------------------------------------------
    {
        struct StatGyroAxis { const char* label; float angle; float gyro; float accel; int16_t mag; };
        const StatGyroAxis axes[] = {
            {"X", gyroData.gyro_0_ang_x, gyroData.gyro_0_gyr_x, gyroData.gyro_0_acc_x, gyroData.gyro_0_mag_x},
            {"Y", gyroData.gyro_0_ang_y, gyroData.gyro_0_gyr_y, gyroData.gyro_0_acc_y, gyroData.gyro_0_mag_y},
            {"Z", gyroData.gyro_0_ang_z, gyroData.gyro_0_gyr_z, gyroData.gyro_0_acc_z, gyroData.gyro_0_mag_z},
        };
        const int numAxes = sizeof(axes) / sizeof(axes[0]);
        const char* columns[] = {"Angle (XZ=+/-180 Y=+/-90deg)", "Gyro (+/-2000deg/s)", "Acceleration (+/-16 G-Force)", "Magnetometer (+/-2Gauss)"};
        const int numColumns = sizeof(columns) / sizeof(columns[0]);

        printStatSeparator();
        printf(STAT_LABEL_BLANK_FMT, "");
        for (int i = 0; i < numColumns; i++) {printf(STAT_GYRO_COL_FORMAT_S, columns[i]);}
        printf("\n");
        printStatSeparator();
        for (int i = 0; i < numAxes; i++) {
            printf(STAT_LABEL_FMT, axes[i].label);
            printf(STAT_GYRO_COL_FORMAT_F, axes[i].angle);
            printf(STAT_GYRO_COL_FORMAT_F, axes[i].gyro);
            printf(STAT_GYRO_COL_FORMAT_F, axes[i].accel);
            printf(STAT_GYRO_COL_FORMAT_LD, (long)axes[i].mag);
            printf("\n");
        }
    }
  }
  #endif

  if (systemData.output_stat_admplex == true) {
    // ----------------------------------------------------------------------------------------------------------------------------
    //                                                                                             PRINT PER-CHANNEL MULTIPLEXER Hz
    // ----------------------------------------------------------------------------------------------------------------------------
    #ifdef SatIO_CD74HC4067_OPTION_USE_0
    printStatChannelHzTable("ADMPlex0 Ch Hz", systemData.counters_mplex0_chan, MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS);
    #endif
    #ifdef SatIO_CD74HC4067_OPTION_USE_1
    printStatChannelHzTable("ADMPlex1 Ch Hz", systemData.counters_mplex1_chan, MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS);
    #endif
  }

  #ifdef GPIOPE_USE_INPUT
  if (systemData.output_stat_gpiope == true) {
    // ----------------------------------------------------------------------------------------------------------------------------
    //                                                                                                        PRINT PER-PIN GPIOPE Hz
    // ----------------------------------------------------------------------------------------------------------------------------
    struct StatGPIOPEChannel { long hz; bool enabled; int32_t input_value; };
    // Same address scan TaskHandler uses to find the active input device(s).
    // Hz counters (counters_gpiope_in_chan) are still shared per channel index
    // across whichever device(s) answer -- see TaskHandler.cpp -- but
    // Enabled/Input Value are read straight off each device found, so
    // multiple simultaneously-active input devices each get their own
    // titled table instead of clobbering one shared array.
    bool found_any = false;
    for (int address = 0; address < 128; address++) {
      GPIOPortExpander* gpiope = isGPIOPE_INPUT((uint8_t)address);
      if (!gpiope) {continue;}
      found_any = true;

      StatGPIOPEChannel channels[GPIOPE_MAX_SIZE];
      for (int i = 0; i < GPIOPE_MAX_SIZE; i++) {
          channels[i] = {(long)systemData.counters_gpiope_in_chan[i].task_ffreq_t, false, 0};
      }
      for (int i = 0; i < (int)gpiope->max_input_values && i < GPIOPE_MAX_SIZE; i++) {
          channels[i].enabled = gpiope->enabled[i];
          channels[i].input_value = gpiope->input_value[i];
      }

      printStatSeparator();
      printf(STAT_LABEL_FMT "%d\n", "GPIOPE Address", address);

      int channel_count = (int)gpiope->max_pins;
      if (channel_count > GPIOPE_MAX_SIZE) {channel_count = GPIOPE_MAX_SIZE;}
      for (int page_start = 0; page_start < channel_count; page_start += STAT_SWITCHES_PER_PAGE) {
          int page_end = page_start + STAT_SWITCHES_PER_PAGE;
          if (page_end > channel_count) {page_end = channel_count;}
          printStatPageHeader(page_start, page_end);
          printf(STAT_LABEL_FMT, "Input Ch Hz");
          for (int i = page_start; i < page_end; i++) {printf(STAT_COL_FORMAT_LD, channels[i].hz);}
          printf("\n");
          printf(STAT_LABEL_FMT, "Enabled");
          for (int i = page_start; i < page_end; i++) {printf(STAT_COL_FORMAT_D, (int)channels[i].enabled);}
          printf("\n");
          printf(STAT_LABEL_FMT, "Input Value");
          for (int i = page_start; i < page_end; i++) {printf(STAT_COL_FORMAT_LD, (long)channels[i].input_value);}
          printf("\n");
      }
    }
    if (!found_any) {
        printStatSeparator();
        printf(STAT_LABEL_FMT "%d\n", "GPIOPE Address", -1);
    }
  }
  #endif

  #ifdef SatIO_USE_MATRIX
  if (systemData.output_stat_matrix == true) {
    // ----------------------------------------------------------------------------------------------------------------------------
    //                                                                                                        PRINT COMPUTER ASSIST
    // ----------------------------------------------------------------------------------------------------------------------------
    /*
     * Switch indices are paged in chunks of STAT_SWITCHES_PER_PAGE rather
     * than printed on one line; page_end is always clamped to
     * MAX_MATRIX_SWITCHES, so the last (partial) page never reads past the
     * end of matrixData's arrays.
     */
    for (int page_start = 0; page_start < MAX_MATRIX_SWITCHES; page_start += STAT_SWITCHES_PER_PAGE) {
        int page_end = page_start + STAT_SWITCHES_PER_PAGE;
        if (page_end > MAX_MATRIX_SWITCHES) {page_end = MAX_MATRIX_SWITCHES;}
        printStatPageHeader(page_start, page_end);
        printf(STAT_LABEL_FMT, "Computer Assist");
        for (int i=page_start;i<page_end;i++) {printf(STAT_COL_FORMAT_D, matrixData.computer_assist[0][i]);}
        printf("\n");
        printf(STAT_LABEL_FMT, "Switch Intention");
        for (int i=page_start;i<page_end;i++) {printf(STAT_COL_FORMAT_D, matrixData.switch_intention[0][i]);}
        printf("\n");
        printf(STAT_LABEL_FMT, "Computer Intention");
        for (int i=page_start;i<page_end;i++) {printf(STAT_COL_FORMAT_D, matrixData.computer_intention[0][i]);}
        printf("\n");
        printf(STAT_LABEL_FMT, "Output Value");
        printArray(matrixData.output_value[0], page_start, page_end);
    }
  }
  #endif // SatIO_USE_MATRIX
}