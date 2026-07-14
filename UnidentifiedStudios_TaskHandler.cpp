/*
    TaskHandler - Written By Benjamin Jack Cullen.

    MISRA conventions used throughout this library:
    (1) Every if/else/while/for body is a braced compound statement (MISRA C 2012 Rule 15.6).
    (2) Boolean objects are tested directly instead of being compared to true/false
        (MISRA C 2012 Rule 14.4 - essentially Boolean controlling expressions).
    (3) Null pointers are written as nullptr, the type-safe C++ null pointer constant.

    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <rtc_wdt.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>
#include "esp_log.h"
#include "REG.h"
#include <SiderealPlanets.h>
#include <SiderealObjects.h>
#include "UnidentifiedStudios_Config.h"
#include "UnidentifiedStudios_StrVal.h"
#include "UnidentifiedStudios_Meteors.h"
#include "UnidentifiedStudios_WTGPS300P.h"
#include "UnidentifiedStudios_WT901.h"
#include "UnidentifiedStudios_Multiplexers.h"
#include "UnidentifiedStudios_SiderealHelper.h"
#include "UnidentifiedStudios_HexToDig.h"
#include "UnidentifiedStudios_INS.h"
#include "UnidentifiedStudios_SatIO.h"
#include "UnidentifiedStudios_SatIOFile.h"
#include "UnidentifiedStudios_Mapping.h"
#include "UnidentifiedStudios_Matrix.h"
#include "UnidentifiedStudios_CMD.h"
#include "UnidentifiedStudios_SystemData.h"
#include "UnidentifiedStudios_SdCardHelper.h"
#include "UnidentifiedStudios_TaskHandler.h"
#include "UnidentifiedStudios_I2C.h"
#ifdef SatIO_DISPLAY_OPTION_LVGL
#include "UnidentifiedStudios_SatIOLVGL.h"
#endif
#include "UnidentifiedStudios_GPIOPortExpander.h"

TaskHandle_t TaskGPS;
TaskHandle_t TaskGyro;
TaskHandle_t TaskADMplex0;
TaskHandle_t TaskADMplex1;
TaskHandle_t TaskSwitches;
TaskHandle_t TaskStorage;
TaskHandle_t TaskUniverse;
TaskHandle_t TaskDisplayUpdate;
TaskHandle_t TaskSystemTime;
TaskHandle_t TaskSatIOSerialTx;
TaskHandle_t TaskInputPortController;

#ifdef SatIO_DISPLAY_OPTION_HEADLESS
// PRIORITY (same priority so that task Hz (from delay ms) can be tuned without triggering wdt for a starved task)
// TASK_SYSTEM_TIME_PRIORITY is deliberately one above its peers: it is idle
// except for a brief once-a-second burst, so a higher priority costs nothing,
// but it lets that burst preempt same-core tasks immediately on its notify
// instead of waiting for the next FreeRTOS tick's round-robin time slice
// (CONFIG_FREERTOS_HZ=1000, i.e. up to 1 ms of avoidable lateness otherwise).
#define TASK_SYSTEM_TIME_PRIORITY           5
#define TASK_GPS_PRIORITY                   5
#define TASK_GYRO_PRIORITY                  5
#define TASK_ADMPLEX0_PRIORITY              5
#define TASK_ADMPLEX1_PRIORITY              5
// TASK_SWITCHES_PRIORITY is one above its core-1 peers: it wakes rarely (gated
// to TASK_MAX_FREQ_LOW_SWITCHES) but must send its pending I2C writes as soon
// as it wakes rather than wait out a round-robin tick slice (up to ~1ms per
// contending peer at CONFIG_FREERTOS_HZ=1000) behind same-priority tasks.
#define TASK_SWITCHES_PRIORITY              1
#define TASK_UNIVERSE_PRIORITY              5
#define TASK_STORAGE_PRIORITY               5
#define TASK_SatIO_SERIAL_TX_PRIORITY       5
#define TASK_GPIOPE_INPUT_PRIORITY          5
// CORE ASSIGNMENT
#define TASK_SYSTEM_TIME_CORE               1
#define TASK_GPS_CORE                       1
#define TASK_GYRO_CORE                      1

#ifdef SatIO_CD74HC4067_OPTION_USE_0
#define TASK_ADMPLEX0_CORE                  1
#endif
#ifdef SatIO_CD74HC4067_OPTION_USE_1
#define TASK_ADMPLEX1_CORE                  1
#endif

#define TASK_GPIOPE_INPUT_CORE              1
#define TASK_SWITCHES_CORE                  0
#define TASK_UNIVERSE_CORE                  0
#define TASK_STORAGE_CORE                   0
#define TASK_SatIO_SERIAL_TX_CORE           0
// STACK SIZES
#define TASK_SYSTEM_TIME_STACK_SIZE         5120
#define TASK_GPS_STACK_SIZE                 5120
#define TASK_GYRO_STACK_SIZE                4608
#define TASK_ADMPLEX0_STACK_SIZE            4096
#define TASK_ADMPLEX1_STACK_SIZE            4096
#define TASK_SWITCHES_STACK_SIZE            5120
#define TASK_UNIVERSE_STACK_SIZE            20480
#define TASK_STORAGE_STACK_SIZE             6144
#define TASK_SatIO_SERIAL_TX_STACK_SIZE     4096
#define TASK_GPIOPE_INPUT_STACK_SIZE 10240
#endif

#ifdef SatIO_DISPLAY_OPTION_LVGL
// PRIORITY (same priority so that task Hz (from delay ms) can be tuned without triggering wdt for a starved task)
#define TASK_SYSTEM_TIME_PRIORITY           5
#define TASK_GPS_PRIORITY                   5
#define TASK_GYRO_PRIORITY                  5
#define TASK_ADMPLEX0_PRIORITY              5
#define TASK_ADMPLEX1_PRIORITY              5
// TASK_SWITCHES_PRIORITY is one above its core-1 peers: it wakes rarely (gated
// to TASK_MAX_FREQ_LOW_SWITCHES) but must send its pending I2C writes as soon
// as it wakes rather than wait out a round-robin tick slice (up to ~1ms per
// contending peer at CONFIG_FREERTOS_HZ=1000) behind same-priority tasks.
#define TASK_SWITCHES_PRIORITY              6
#define TASK_UNIVERSE_PRIORITY              5
#define TASK_STORAGE_PRIORITY               5
#define TASK_DISPLAY_PRIORITY               5
#define TASK_SatIO_SERIAL_TX_PRIORITY       5
#define TASK_GPIOPE_INPUT_PRIORITY 5
// CORE ASSIGNMENT
#define TASK_SYSTEM_TIME_CORE               0
#define TASK_GPS_CORE                       0
#define TASK_GYRO_CORE                      1

#ifdef SatIO_CD74HC4067_OPTION_USE_0
#define TASK_ADMPLEX0_CORE                  1
#endif
#ifdef SatIO_CD74HC4067_OPTION_USE_1
#define TASK_ADMPLEX1_CORE                  1
#endif

#define TASK_SWITCHES_CORE                  1
#define TASK_UNIVERSE_CORE                  1
#define TASK_STORAGE_CORE                   0
#define TASK_DISPLAY_CORE                   0
#define TASK_SatIO_SERIAL_TX_CORE           1
#define TASK_GPIOPE_INPUT_CORE              1
// STACK SIZES
#define TASK_SYSTEM_TIME_STACK_SIZE         5120
#define TASK_GPS_STACK_SIZE                 5120
#define TASK_GYRO_STACK_SIZE                4608
#define TASK_ADMPLEX0_STACK_SIZE            4096
#define TASK_ADMPLEX1_STACK_SIZE            4096
#define TASK_SWITCHES_STACK_SIZE            5120
#define TASK_UNIVERSE_STACK_SIZE            20480
#define TASK_STORAGE_STACK_SIZE             6144
#define TASK_DISPLAY_STACK_SIZE             32768
#define TASK_SatIO_SERIAL_TX_STACK_SIZE     4096
#define TASK_GPIOPE_INPUT_STACK_SIZE        10240
#endif


/** ----------------------------------------------------------------------------
 * 
 * @brief Notify all Tasks.
 * 
 * Macro for notifying all tasks.
 * 
 * Tasks delayed by xTaskNotifyWait, can receive notifications while waiting.
 * 
 */
static void notifyAllTasks(void) {

  #ifdef SatIO_CD74HC4067_OPTION_USE_0
  if (TaskADMplex0 != nullptr) { xTaskNotifyGive(TaskADMplex0); }
  #endif

  #ifdef SatIO_CD74HC4067_OPTION_USE_1
  if (TaskADMplex1 != nullptr) { xTaskNotifyGive(TaskADMplex1); }
  #endif

  #ifdef SatIO_USE_GYRO_0
  if (TaskGyro != nullptr) { xTaskNotifyGive(TaskGyro); }
  #endif

  #ifdef SatIO_USE_GPS_0
  if (TaskGPS != nullptr) { xTaskNotifyGive(TaskGPS); }
  #endif

  #ifdef SatIO_USE_UNIVERSE
  if (TaskUniverse != nullptr) { xTaskNotifyGive(TaskUniverse); }
  #endif
  
  #ifdef SatIO_USE_MATRIX
  if (TaskSwitches != nullptr) { xTaskNotifyGive(TaskSwitches); }
  #endif

  #ifdef GPIOPE_USE_INPUT
  if (TaskInputPortController != nullptr) { xTaskNotifyGive(TaskInputPortController); }
  #endif

  #ifdef SatIO_USE_STORAGE
  if (TaskStorage != nullptr) { xTaskNotifyGive(TaskStorage); }
  #endif

  #ifdef SatIO_USE_DISPLAY
  if (TaskDisplayUpdate != nullptr) { xTaskNotifyGive(TaskDisplayUpdate); }
  #endif

  if (TaskSatIOSerialTx != nullptr) { xTaskNotifyGive(TaskSatIOSerialTx); }
  
}

void setTasksDelayLowPower() {
  pwrConfigCurrent = pwrConfigLowPower;
  notifyAllTasks();
}

void setTasksDelayBalanced() {
  pwrConfigCurrent = pwrConfigBalanced;
  notifyAllTasks();
}

void setTasksDelayUltimatePerformance() {
  pwrConfigCurrent = pwrConfigUltimatePerformance;
  notifyAllTasks();
}

void setDelay(TaskHandle_t task_handle, uint32_t delay_in, uint32_t *delay_out) {
  if (delay_in > 0) {
    *delay_out = delay_in;
    xTaskNotifyGive(task_handle);
  }
}

bool isTaskDelayed(TaskHandle_t taskHandle) {
  // Compound statement required for the if-body (MISRA C 2012 Rule 15.6).
  bool res = false;
  if (taskHandle == nullptr) {
    res = false;
  }
  else {
    eTaskState state = eTaskGetState(taskHandle);
    res = (state == eBlocked) || (state == eSuspended);
  }
  return res;
}

/** ----------------------------------------------------------------------------
 * Syncronize Tasks.
 *
 * @brief Time syncronize tasks. Main loop and some tasks will not begin until
 *        this function has completed.
 *
 *        Inital synchronization trigger is GPS milliseconds 00 (any second).
 *
 *        After initial synchronization the system will attempt to synchronize
 *        every GPS seconds zero (minutely).
 */
void syncTasks() {
  Serial.println("[syncronizing system] please wait");
  global_task_sync = false;
  // Boolean object used directly as the controlling expression (MISRA C 2012 Rule 14.4).
  while (SatIOData.systemTime.sync_immediately_flag) {
    getSystemTime();
    system_sync_retry_max--;
    if (system_sync_retry_max <= 0) {
      Serial.println("[sync] took too long!");
      break;
    }
    delayMicroseconds(1);
  }
  global_task_sync = true;
}

/** ----------------------------------------------------------------------------
 * Interval Breach: 1 Second.
 *
 * @brief Stores time-of-day snapshots, rolls every per-task counter into its
 *        "total" field, resets the counter for the next second, and raises the
 *        1-second output flag.
 */

static void totalCounters(SystemConuters &counters) {
  counters.task_freq_t = counters.task_freq_c;
  counters.task_ffreq_t = counters.task_ffreq_c;
}

static void clearCounters(SystemConuters &counters) {
  counters.task_freq_c = 0;
  counters.task_ffreq_c = 0;
}

void stepFCounter(SystemConuters &counters, int32_t steps) {

  int64_t tmp_counter = counters.task_freq_c + steps;

  // i_count_read_gps is int32_t, so the wrap check uses the signed 32-bit
  // limit matching its essential type (MISRA C 2012 Rule 10.4).
  if (tmp_counter >= INT32_MAX - 2) {
    tmp_counter = 0;
  }

  counters.task_freq_c = tmp_counter;
}

void stepFFCounter(SystemConuters &counters, int32_t steps) {

  int64_t tmp_counter = counters.task_ffreq_c + steps;

  // i_count_read_gps is int32_t, so the wrap check uses the signed 32-bit
  // limit matching its essential type (MISRA C 2012 Rule 10.4).
  if (tmp_counter >= INT32_MAX - 2) {
    tmp_counter = 0;
  }

  counters.task_ffreq_c = tmp_counter;
}


static void intervalBreach1Second(void) {
  
  /**
   * @brief Uncomment to update every second.
   * @note Enabling applyPendingDateTimeStore here provides 1 second resolution
   *       for system, local and LMST.
   */
  applyPendingDateTimeStore();
  // printf("system uS unixtime: %lld\n", SatIOData.systemTime.unixtime_uS);

  totalCounters(systemData.counters_st);

  #ifdef SatIO_USE_GPS_0
  totalCounters(systemData.counters_gps);
  #endif

  #ifdef SatIO_USE_GYRO_0
  totalCounters(systemData.counters_gyr0);
  #endif

  #ifdef SatIO_USE_INS
  totalCounters(systemData.counters_ins);
  #endif


  #ifdef SatIO_CD74HC4067_OPTION_USE_0
  totalCounters(systemData.counters_mplex0);
  for (int i_chan=0; i_chan<MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS; i_chan++) {totalCounters(systemData.counters_mplex0_chan[i_chan]);}
  #endif

  #ifdef SatIO_CD74HC4067_OPTION_USE_1
  totalCounters(systemData.counters_mplex1);
  for (int i_chan=0; i_chan<MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS; i_chan++) {totalCounters(systemData.counters_mplex1_chan[i_chan]);}
  #endif

  #ifdef SatIO_USE_MATRIX
  totalCounters(systemData.counters_mtx);
  #endif

  #ifdef GPIOPE_USE_INPUT
  totalCounters(systemData.counters_gpiope0);
  for (int i_chan=0; i_chan<GPIOPE_MAX_SIZE; i_chan++) {totalCounters(systemData.counters_gpioe_chan[i_chan]);}
  #endif

  #ifdef GPIOPE_USE_OUTPUT
  totalCounters(systemData.counters_pco);
  #endif

  #ifdef SatIO_USE_UNIVERSE
  totalCounters(systemData.counters_uni);
  #endif

  totalCounters(systemData.counters_track_planets);

  #ifdef SatIO_USE_DISPLAY
  totalCounters(systemData.counters_dsp);
  #endif

  #ifdef SatIO_USE_STORAGE
  totalCounters(systemData.counters_stg);
  #endif

  totalCounters(systemData.counters_log);

  totalCounters(systemData.counters_SatIO_serial_tx);
  
  // uptime_seconds is int32_t, so the wrap check uses the signed 32-bit limit
  // matching its essential type (MISRA C 2012 Rule 10.4).
  systemData.uptime_seconds++;
  if (systemData.uptime_seconds >= INT32_MAX - 2) {
    systemData.uptime_seconds = 0;
    printf("[reset uptime_seconds] %ld\n", systemData.uptime_seconds);
  }

  outputStat(); // uncomment for full stat
  // ESP_LOGI("GPIOPE_INPUT_0", "max_pins=%d num_analog_pins=%d num_digital_pins=%d",
  //         GPIOPE_INPUT_0.max_pins,
  //         GPIOPE_INPUT_0.num_analog_pins,
  //         GPIOPE_INPUT_0.num_digital_pins);

  clearCounters(systemData.counters_st);

  #ifdef SatIO_USE_GPS_0
  clearCounters(systemData.counters_gps);
  #endif

  #ifdef SatIO_USE_GYRO_0
  clearCounters(systemData.counters_gyr0);
  #endif

  #ifdef SatIO_USE_INS
  clearCounters(systemData.counters_ins);
  #endif

  #ifdef SatIO_CD74HC4067_OPTION_USE_0
  clearCounters(systemData.counters_mplex0);
  for (int i_chan=0; i_chan<MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS; i_chan++) {clearCounters(systemData.counters_mplex0_chan[i_chan]);}
  #endif
  
  #ifdef SatIO_CD74HC4067_OPTION_USE_1
  clearCounters(systemData.counters_mplex1);
  for (int i_chan=0; i_chan<MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS; i_chan++) {clearCounters(systemData.counters_mplex1_chan[i_chan]);}
  #endif

  #ifdef SatIO_USE_MATRIX
  clearCounters(systemData.counters_mtx);
  #endif

  #ifdef GPIOPE_USE_INPUT
  clearCounters(systemData.counters_gpiope0);
  for (int i_chan=0; i_chan<GPIOPE_MAX_SIZE; i_chan++) {clearCounters(systemData.counters_gpioe_chan[i_chan]);}
  #endif

  #ifdef GPIOPE_USE_OUTPUT
  clearCounters(systemData.counters_pco);
  #endif

  #ifdef SatIO_USE_UNIVERSE
  clearCounters(systemData.counters_uni);
  #endif

  clearCounters(systemData.counters_track_planets);

  #ifdef SatIO_USE_DISPLAY
  clearCounters(systemData.counters_dsp);
  #endif

  #ifdef SatIO_USE_STORAGE
  clearCounters(systemData.counters_stg);
  #endif

  clearCounters(systemData.counters_log);

  clearCounters(systemData.counters_SatIO_serial_tx);

  // uncomment to set every second (ensure not called elsewhere)
  setSatIOCoordinates();
  setSatIOAltitude();
  setSatIOSpeed();
  setSatIOGroundHeading();
  setSatIORaDec();
  setGroundHeadingName(atof(gnrmcData.ground_heading));
}

// Callback for the one-shot esp_timer armed by TASK_FREQ_WAIT below. Runs in
// the esp_timer service task's context (ESP_TIMER_TASK dispatch), so calling
// FreeRTOS task-notification APIs here is safe.
static void taskFreqWaitTimerCallback(void *arg) {
  xTaskNotifyGive(static_cast<TaskHandle_t>(arg));
}

// Notification-responsive sub millisecond task frequency gate without increasing RTOS HZ over 1000.
// Tracks an absolute xLastWakeTimeUs like vTaskDelayUntil, but the actual
// wait is a blocking xTaskNotifyWait(portMAX_DELAY): a per-call-site esp_timer
// is armed for the remaining microseconds and notifies this task when it
// fires, giving true microsecond resolution independent of the FreeRTOS tick
// rate. A notification from elsewhere (e.g. Hz change via notifyAllTasks)
// wakes the same wait immediately; the loop then re-reads the current
// setting and re-arms the timer for whatever time remains. When the deadline
// is reached, xLastWakeTimeUs advances by exactly one period.
// period_us is a full expression (re-evaluated every pass), not just a
// PwrConfig field name, so callers can pass either pwrConfigCurrent.X for a
// fixed Hz or a locally computed variable for a dynamic period.
#define TASK_FREQ_WAIT(period_us)                                               \
  do {                                                                          \
    static int64_t xLastWakeTimeUs = 0;                                         \
    static esp_timer_handle_t xWakeTimer = nullptr;                             \
    if (xWakeTimer == nullptr) {                                                \
      xLastWakeTimeUs = esp_timer_get_time();                                   \
      const esp_timer_create_args_t xWakeTimerArgs = {                          \
        .callback = &taskFreqWaitTimerCallback,                                 \
        .arg = static_cast<void *>(xTaskGetCurrentTaskHandle()),                \
        .dispatch_method = ESP_TIMER_TASK,                                      \
        .name = "task_freq_wait",                                               \
        .skip_unhandled_events = false,                                         \
      };                                                                        \
      esp_timer_create(&xWakeTimerArgs, &xWakeTimer);                           \
    }                                                                           \
    int64_t xPeriodUs;                                                          \
    int64_t xRemainingUs;                                                       \
    do {                                                                        \
      xPeriodUs    = static_cast<int64_t>(period_us);                          \
      xRemainingUs = (xLastWakeTimeUs + xPeriodUs) - esp_timer_get_time();      \
      if (xRemainingUs > 0) {                                                   \
        (void)esp_timer_stop(xWakeTimer);                                       \
        (void)esp_timer_start_once(xWakeTimer, static_cast<uint64_t>(xRemainingUs)); \
        xTaskNotifyWait(0xFFFFFFFF, 0xFFFFFFFF, nullptr, portMAX_DELAY);        \
      }                                                                         \
    } while (xRemainingUs > 0);                                                 \
    (void)esp_timer_stop(xWakeTimer);                                           \
    xLastWakeTimeUs += xPeriodUs;                                               \
  } while (0)

/**
 * todo:
 *  modify internal/external/peripheral clocks.
 *  sleep modes.
 */

#ifdef SatIO_USE_GPS_0
bool taskFrequencyGPS()         { TASK_FREQ_WAIT(pwrConfigCurrent.TASK_MAX_FREQ_GPS);         return true; }
#endif

#ifdef SatIO_USE_GYRO_0
bool taskFrequencyGyro()        { TASK_FREQ_WAIT(pwrConfigCurrent.TASK_MAX_FREQ_GYRO);        return true; }
#endif

#ifdef SatIO_USE_MATRIX
bool taskFrequencySwitches()    { TASK_FREQ_WAIT(pwrConfigCurrent.TASK_MAX_FREQ_SWITCHES);    return true; }
#endif

#ifdef SatIO_USE_STORAGE
bool taskFrequencyStorage()     { TASK_FREQ_WAIT(pwrConfigCurrent.TASK_MAX_FREQ_STORAGE);     return true; }
#endif

#ifdef SatIO_CD74HC4067_OPTION_USE_0
bool taskFrequencyADMplex0()    { TASK_FREQ_WAIT(pwrConfigCurrent.TASK_MAX_FREQ_ADMPLEX0);    return true; }
#endif

#ifdef SatIO_CD74HC4067_OPTION_USE_1
bool taskFrequencyADMplex1()    { TASK_FREQ_WAIT(pwrConfigCurrent.TASK_MAX_FREQ_ADMPLEX1);    return true; }
#endif

#ifdef SatIO_USE_UNIVERSE
bool taskFrequencyUniverse()    { TASK_FREQ_WAIT(pwrConfigCurrent.TASK_MAX_FREQ_UNIVERSE);    return true; }
#endif

#ifdef SatIO_USE_DISPLAY
bool taskFrequencyDisplay()     { TASK_FREQ_WAIT(pwrConfigCurrent.TASK_MAX_FREQ_DISPLAY);     return true; }
#endif

bool taskFrequencySatIOSerialTx() { TASK_FREQ_WAIT(pwrConfigCurrent.TASK_MAX_FREQ_SatIO_SERIAL_TX); return true; }

#ifdef GPIOPE_USE_INPUT
bool taskFrequencyInputPortController() { TASK_FREQ_WAIT(pwrConfigCurrent.TASK_MAX_FREQ_GPIOE_INPUT); return true; }
#endif

/** ----------------------------------------------------------------------------
 * System Time Task.
 *
 * @brief 1Hz tv_now update & 1 second interval timing, syncronized with real time.
 *        Wait time is variable, like the other tasks and is automatically adjusted.
 *        tv_now is currently used for internal timings syncronized with real time UTC+-0.
 *        If a greater resolution of tv_now is required then increase the frequency of this task.
 */
bool gps_sync_ready = false;
int64_t gps_read_done_uS = 0;
time_t  prev_tv_sec;

static void taskSystemTime(void *pvParameters) {
  (void)pvParameters; // FreeRTOS task signature requires the parameter; it is unused here (MISRA C 2012 Rule 2.7).
  esp_task_wdt_add(nullptr);
  int64_t xNextTickUs = 1000000LL;

  for (;;) {
    TASK_FREQ_WAIT(xNextTickUs);

    xSemaphoreTake(dataMutex, portMAX_DELAY);

    // --------------------------------------------
    // Update System Unixtime
    // --------------------------------------------
    /**
     * @brief Refreshed here, on demand, immediately before intervalBreach1Second()
     *        (via applyPendingDateTimeStore()) reads tv_now. The same sample is
     *        used below to compute the next tick's wait against the second boundary.
     */
    xSemaphoreTake(systemTimeMutex, portMAX_DELAY);
    getSystemTime();
    xNextTickUs = 1000000LL - static_cast<int64_t>(tv_now.tv_usec);
    xSemaphoreGive(systemTimeMutex);
    if (xNextTickUs <= 0) { xNextTickUs = 1000000LL; }

    // gated for >1Hz task iteration
    if (tv_now.tv_sec != prev_tv_sec) {
      prev_tv_sec = tv_now.tv_sec;
      intervalBreach1Second();
    }
    esp_task_wdt_reset();

    // --------------------------------------------
    // Task frequency counter
    // --------------------------------------------
    stepFCounter(systemData.counters_st, 1);
    systemData.counters_st.flag_c = true;
    xSemaphoreGive(dataMutex);
  }
}
void createTaskSystemTime() {
  xTaskCreatePinnedToCore(
    taskSystemTime,             /* Function to implement the task */
    "TaskSystemTime",           /* Name of the task */
    TASK_SYSTEM_TIME_STACK_SIZE, /* Stack size in words */
    nullptr,             /* Task input parameter */
    TASK_SYSTEM_TIME_PRIORITY,   /* Priority of the task */
    &TaskSystemTime,            /* Task handle. */
    TASK_SYSTEM_TIME_CORE);      /* Core where the task should run */
}

#ifdef SatIO_USE_GPS_0
/** ----------------------------------------------------------------------------
 * GPS Task.
 *
 * @brief Reads and validates GPS sentences, commits the resulting position
 *        and timing data, and updates the inertial navigation system.
 */
static void taskGPS(void *pvParameters) {
  (void)pvParameters; // FreeRTOS task signature requires the parameter; it is unused here (MISRA C 2012 Rule 2.7).
  esp_task_wdt_add(nullptr);
  for (;;) {

    // Delay Task
    if (taskFrequencyGPS() == true) {

      if (readGPS() == true)
      {
        gps_read_done_uS = esp_timer_get_time();
        if (validateGPSData() == true)
        {
          xSemaphoreTake(dataMutex, portMAX_DELAY);

          // --------------------------------------------
          // Sync System Unixtime With GPS 
          // --------------------------------------------
          syncTimeGPS();
          esp_task_wdt_reset();

          // --------------------------------------------
          // Set INS data
          // --------------------------------------------
          set_ins(SatIOData.system_degrees_latitude,
                  SatIOData.system_degrees_longitude,
                  SatIOData.system_altitude,
                  SatIOData.system_ground_heading,
                  SatIOData.system_speed,
                  atof(gnggaData.gps_precision_factor),
                  gyroData.gyro_0_ang_z);

          // --------------------------------------------
          // Task frequency counter
          // --------------------------------------------
          stepFFCounter(systemData.counters_gps, 1);
          systemData.counters_gps.flag_c = true;
          #ifdef SatIO_SERIAL_TX_OPTION_CURRENT_TASK
          outputSerialGPS();
          #endif
          xSemaphoreGive(dataMutex);
        }
      }
    }
    // --------------------------------------------
    // Task frequency counter
    // --------------------------------------------
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    stepFCounter(systemData.counters_gps, 1);
    xSemaphoreGive(dataMutex);
  }
}
void createTaskGPS() {
  xTaskCreatePinnedToCore(
    taskGPS,             /* Function to implement the task */
    "TaskGPS",           /* Name of the task */
    TASK_GPS_STACK_SIZE, /* Stack size in words */
    nullptr,             /* Task input parameter */
    TASK_GPS_PRIORITY,   /* Priority of the task */
    &TaskGPS,            /* Task handle. */
    TASK_GPS_CORE);      /* Core where the task should run */
}
#endif

#ifdef SatIO_USE_STORAGE
/** ----------------------------------------------------------------------------
 * Storage Task.
 *
 * @brief Performs many operations including:
 *  (1) Card insertion checks.
 *  (2) Mount automatically.
 *  (3) Unmount automatically.
 *  (4) Read/write operations.
 *  (5) Other storage operations.
 *  (6) Powers down the sdcard when not in use.
 */
static void taskStorage(void *pvParameters) {
  (void)pvParameters; // FreeRTOS task signature requires the parameter; it is unused here (MISRA C 2012 Rule 2.7).
  esp_task_wdt_add(nullptr);
  for (;;) {

    // Delay Task
    if (taskFrequencyStorage() == true) {
      esp_task_wdt_reset();
      // ------------------------------------------------
      // SDCard Detect/Mount
      // ------------------------------------------------
      sdcard_mount();
      esp_task_wdt_reset();

      // ------------------------------------------------
      // Check Flags
      // ------------------------------------------------
      xSemaphoreTake(dataMutex, portMAX_DELAY);
      if (systemData.logging_enabled) {
        Serial.printf("[log] setting write flag true\n");
        sdcardFlagData.write_log = true;
      }
      sdcardFlagHandler();
      xSemaphoreGive(dataMutex);
      esp_task_wdt_reset();

      // ------------------------------------------------
      // SDCard Power Down / Unmount
      // ------------------------------------------------
      sdcard_unmount();
      esp_task_wdt_reset();

    // --------------------------------------------
    // Task frequency counter
    // --------------------------------------------
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    stepFCounter(systemData.counters_stg, 1);
    systemData.counters_stg.flag_c = true;
    xSemaphoreGive(dataMutex);
    }
  }
}
void createTaskStorage() {
  xTaskCreatePinnedToCore(
    taskStorage,             /* Function to implement the task */
    "TaskStorage",           /* Name of the task */
    TASK_STORAGE_STACK_SIZE, /* Stack size in words */
    nullptr,                 /* Task input parameter */
    TASK_STORAGE_PRIORITY,   /* Priority of the task */
    &TaskStorage,            /* Task handle. */
    TASK_STORAGE_CORE);      /* Core where the task should run */
}
#endif

#ifdef SatIO_USE_GYRO_0
/** ----------------------------------------------------------------------------
 * Gyro Task.
 *
 * @brief Reads and stores gyroscopic data, and feeds it into the inertial
 *        navigation position estimate.
 */
static void taskGyro(void *pvParameters) {
  (void)pvParameters; // FreeRTOS task signature requires the parameter; it is unused here (MISRA C 2012 Rule 2.7).
  esp_task_wdt_add(nullptr);
  while (!global_task_sync) {
    esp_task_wdt_reset();
    vTaskDelay(1);
  }
  for (;;) {

    // Delay Task
    if (taskFrequencyGyro() == true) {

      if (readGyro() == true) {
        esp_task_wdt_reset();
        xSemaphoreTake(dataMutex, portMAX_DELAY);

        // --------------------------------------------
        // Task frequency counter
        // --------------------------------------------
        stepFFCounter(systemData.counters_gyr0, 1);
        systemData.counters_gyr0.flag_c = true;
        #ifdef SatIO_SERIAL_TX_OPTION_CURRENT_TASK
        outputSerialGyro0();
        #endif

        // ----------------------------------------------
        // Estimate INS data. (Can be used without GPS).
        // INS data produced from user/gps=system is fed back into INS in loop.
        // ----------------------------------------------
        if (ins_estimate_position(gyroData.gyro_0_ang_y,
                                    gyroData.gyro_0_ang_z,
                                    SatIOData.system_ground_heading,
                                    SatIOData.system_speed,
                                    SatIOData.systemTime.unixtime_uS)) {
        // --------------------------------------------
        // Task frequency counter
        // --------------------------------------------
        stepFFCounter(systemData.counters_ins, 1);
        systemData.counters_ins.flag_c = true;

        esp_task_wdt_reset();
        }
        xSemaphoreGive(dataMutex);
      }
    }
    // --------------------------------------------
    // Task frequency counter
    // --------------------------------------------
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    stepFCounter(systemData.counters_gyr0, 1);
    xSemaphoreGive(dataMutex);
  }
}
void createTaskGyro() {
  xTaskCreatePinnedToCore(
    taskGyro,             /* Function to implement the task */
    "TaskGyro",           /* Name of the task */
    TASK_GYRO_STACK_SIZE, /* Stack size in words */
    nullptr,              /* Task input parameter */
    TASK_GYRO_PRIORITY,   /* Priority of the task */
    &TaskGyro,            /* Task handle. */
    TASK_GYRO_CORE);      /* Core where the task should run */
}
#endif

#ifdef SatIO_CD74HC4067_OPTION_USE_0
/** ----------------------------------------------------------------------------
 * Multiplexer Task (ADMplex0).
 *
 * @brief Reads all analog/digital multiplexer channels on ad_mux_0.
 */
static void taskADMplex0(void *pvParameters) {
  (void)pvParameters; // FreeRTOS task signature requires the parameter; it is unused here (MISRA C 2012 Rule 2.7).
  esp_task_wdt_add(nullptr);
  while (!global_task_sync) {
    esp_task_wdt_reset();
    vTaskDelay(1);
  }
  for (;;) {

    // Delay Task
    if (taskFrequencyADMplex0() == true) {

      // ------------------------------------------------
      // Read multiplexer channels (customize as required).
      // ------------------------------------------------
      // Disabled channels are left NAN by setADMultiplexerChannelEnabled()/initADMultiplexer(),
      // so this loop only ever touches channels currently enabled.
      // Each channel is additionally rate-limited to its own chan_freq_uS: the
      // task itself still wakes at TASK_MAX_FREQ_ADMPLEX0 (unchanged), but a
      // channel is only actually read once that many microseconds have passed
      // since its last read, so e.g. channel 0 can run at 1Hz alongside
      // channel 1 at 1000Hz within the same task, bounded by TASK_MAX_FREQ_ADMPLEX0.
      static int64_t admplex0_chan_last_read_uS[MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS] = {0};
      // Snapshot once per pass so the counter loop below can't disagree with the
      // read loop about which channels were enabled this cycle (a concurrent
      // CLI/LVGL disable between the two loops would otherwise drop a channel's
      // count for a cycle it was actually read in).
      bool admplex0_chan_was_enabled[MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS] = {false};
      bool admplex0_chan_did_read[MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS] = {false};
      // int64_t admplex0_now_uS = esp_timer_get_time();
      for (uint8_t i_chan = 0; i_chan < MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS; i_chan++) {
        if (ad_mux_0.enabled[i_chan] == true) {
          admplex0_chan_was_enabled[i_chan] = true;
          // check chan_freq_uS timer
          if ((esp_timer_get_time() - admplex0_chan_last_read_uS[i_chan]) >= (int64_t)ad_mux_0.chan_freq_uS[i_chan]) {
            // read/write
            readADMultiplexerAnalogChannel(ad_mux_0, i_chan);
            admplex0_chan_last_read_uS[i_chan] = esp_timer_get_time();
            // just set a flag to save time between channel r/w
            admplex0_chan_did_read[i_chan] = true;
          }
        }
      }
      esp_task_wdt_reset();

      // --------------------------------------------
      // Task frequency counter
      // --------------------------------------------
      xSemaphoreTake(dataMutex, portMAX_DELAY);
      // Per-channel Hz: task_freq_t is how often an enabled channel was checked
      // this second (its ceiling); task_ffreq_t is how often it was actually
      // read (its achieved Hz, gated by chan_freq_uS above).
      for (uint8_t i_chan = 0; i_chan < MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS; i_chan++) {
        if (admplex0_chan_was_enabled[i_chan] == true) {
          stepFCounter(systemData.counters_mplex0_chan[i_chan], 1);
          if (admplex0_chan_did_read[i_chan] == true) {stepFFCounter(systemData.counters_mplex0_chan[i_chan], 1);}
        }
      }
      stepFFCounter(systemData.counters_mplex0, 1);
      systemData.counters_mplex0.flag_c = true;
      #ifdef SatIO_SERIAL_TX_OPTION_CURRENT_TASK
      outputSerialADMplex0();
      #endif
      xSemaphoreGive(dataMutex);
    }

    // --------------------------------------------
    // Task frequency counter
    // --------------------------------------------
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    stepFCounter(systemData.counters_mplex0, 1);
    xSemaphoreGive(dataMutex);
  }
}
void createTaskADMplex0() {
  xTaskCreatePinnedToCore(
    taskADMplex0,             /* Function to implement the task */
    "TaskADMplex0",           /* Name of the task */
    TASK_ADMPLEX0_STACK_SIZE, /* Stack size in words */
    nullptr,                  /* Task input parameter */
    TASK_ADMPLEX0_PRIORITY,   /* Priority of the task */
    &TaskADMplex0,            /* Task handle. */
    TASK_ADMPLEX0_CORE);      /* Core where the task should run */
}
#endif

#ifdef SatIO_CD74HC4067_OPTION_USE_1
/** ----------------------------------------------------------------------------
 * Multiplexer Task (ADMplex1).
 *
 * @brief Reads all analog/digital multiplexer channels on ad_mux_1.
 */

static void taskADMplex1(void *pvParameters) {
  (void)pvParameters; // FreeRTOS task signature requires the parameter; it is unused here (MISRA C 2012 Rule 2.7).
  esp_task_wdt_add(nullptr);
  while (!global_task_sync) {
    esp_task_wdt_reset();
    vTaskDelay(1);
  }
  for (;;) {

    // Delay Task
    if (taskFrequencyADMplex1() == true) {

      // ------------------------------------------------
      // Read multiplexer channels (customize as required).
      // ------------------------------------------------
      // Disabled channels are left NAN by setADMultiplexerChannelEnabled()/initADMultiplexer(),
      // so this loop only ever touches channels currently enabled.
      // Each channel is additionally rate-limited to its own chan_freq_uS: the
      // task itself still wakes at TASK_MAX_FREQ_ADMPLEX1 (unchanged), but a
      // channel is only actually read once that many microseconds have passed
      // since its last read, so e.g. channel 0 can run at 1Hz alongside
      // channel 1 at 1000Hz within the same task, bounded by TASK_MAX_FREQ_ADMPLEX1.
      static int64_t admplex1_chan_last_read_uS[MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS] = {0};
      // Snapshot once per pass so the counter loop below can't disagree with the
      // read loop about which channels were enabled this cycle (a concurrent
      // CLI/LVGL disable between the two loops would otherwise drop a channel's
      // count for a cycle it was actually read in).
      bool admplex1_chan_was_enabled[MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS] = {false};
      bool admplex1_chan_did_read[MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS] = {false};
      // int64_t admplex1_now_uS = esp_timer_get_time();
      for (uint8_t i_chan = 0; i_chan < MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS; i_chan++) {
        if (ad_mux_1.enabled[i_chan] == true) {
          admplex1_chan_was_enabled[i_chan] = true;
          // check chan_freq_uS timer
          if ((esp_timer_get_time() - admplex1_chan_last_read_uS[i_chan]) >= (int64_t)ad_mux_1.chan_freq_uS[i_chan]) {
            // read/write
            readADMultiplexerAnalogChannel(ad_mux_1, i_chan);
            admplex1_chan_last_read_uS[i_chan] = esp_timer_get_time();
            // just set a flag to save time between channel r/w
            admplex1_chan_did_read[i_chan] = true;
          }
        }
      }
      esp_task_wdt_reset();

      // --------------------------------------------
      // Task frequency counter
      // --------------------------------------------
      xSemaphoreTake(dataMutex, portMAX_DELAY);
      // Per-channel Hz: task_freq_t is how often an enabled channel was checked
      // this second (its ceiling); task_ffreq_t is how often it was actually
      // read (its achieved Hz, gated by chan_freq_uS above).
      for (uint8_t i_chan = 0; i_chan < MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS; i_chan++) {
        if (admplex1_chan_was_enabled[i_chan] == true) {
          stepFCounter(systemData.counters_mplex1_chan[i_chan], 1);
          if (admplex1_chan_did_read[i_chan] == true) {stepFFCounter(systemData.counters_mplex1_chan[i_chan], 1);}
        }
      }
      stepFFCounter(systemData.counters_mplex1, 1);
      systemData.counters_mplex1.flag_c = true;
      #ifdef SatIO_SERIAL_TX_OPTION_CURRENT_TASK
      outputSerialADMplex1();
      #endif
      xSemaphoreGive(dataMutex);
    }

    // --------------------------------------------
    // Task frequency counter
    // --------------------------------------------
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    stepFCounter(systemData.counters_mplex1, 1);
    xSemaphoreGive(dataMutex);
  }
}
void createTaskADMplex1() {
  xTaskCreatePinnedToCore(
    taskADMplex1,             /* Function to implement the task */
    "TaskADMplex1",           /* Name of the task */
    TASK_ADMPLEX1_STACK_SIZE, /* Stack size in words */
    nullptr,                  /* Task input parameter */
    TASK_ADMPLEX1_PRIORITY,   /* Priority of the task */
    &TaskADMplex1,            /* Task handle. */
    TASK_ADMPLEX1_CORE);      /* Core where the task should run */
}
#endif

#ifdef SatIO_USE_MATRIX
/** ----------------------------------------------------------------------------
 * Switch Task.
 *
 * @brief Performs various operations including:
 *  (1) Matrix calculations.
 *  (2) Mapping values.
 *  (3) Sets output values.
 *  (4) Instructing the portcontroller accordingly.
 */
static void taskSwitches(void *pvParameters) {
  (void)pvParameters; // FreeRTOS task signature requires the parameter; it is unused here (MISRA C 2012 Rule 2.7).
  esp_task_wdt_add(nullptr);
  while (!global_task_sync) {
    esp_task_wdt_reset();
    vTaskDelay(1);
  }
  for (;;) {

    // Delay Task
    if (taskFrequencySwitches() == true) {
      esp_task_wdt_reset();
      
      #ifdef SatIO_USE_MATRIX
      xSemaphoreTake(dataMutex, portMAX_DELAY);
      // ------------------------------------------------
      // Calculate.
      // ------------------------------------------------
      unsigned long matrix_start_t = esp_timer_get_time();
      if (matrixSwitch()) {
        esp_task_wdt_reset();
        
        // --------------------------------------------
        // Task frequency counter
        // --------------------------------------------
        stepFFCounter(systemData.counters_mtx, 1);
        systemData.counters_mtx.flag_c = true;
        #ifdef SatIO_SERIAL_TX_OPTION_CURRENT_TASK
        outputSerialMatrix();
        #endif
      }
      esp_task_wdt_reset();

      // ------------------------------------------------
      // Mapping.
      // ------------------------------------------------
      map_values();
      esp_task_wdt_reset();

      // ------------------------------------------------
      // Output Values.
      // ------------------------------------------------
      setOutputValues();
      esp_task_wdt_reset();
      #endif

      #ifdef GPIOPE_USE_OUTPUT
      int32_t count_write = 0;

      // test re-query (almost ready to utilize up to i2caddr max gpiope's if defined)
      // GPIOPE_QueryDevice(GPIOPE_OUTPUT_9, I2C_ADDR_9);

      // todo: write all defined gpiope's (replace address set with gpiope output device select)

      // Clamp to MAX_MATRIX_SWITCHES
      for (uint8_t Mi = 0; Mi < MAX_MATRIX_SWITCHES; Mi++) {
        if (matrixData.matrix_switch_write_required[0][Mi] == true) {

          // Clear the flag now that the value has been sent.
          matrixData.matrix_switch_write_required[0][Mi] = false;

          // Get user specified address
          uint8_t address = matrixData.gpiope_address[0][Mi];

          // Check if device defined
          GPIOPortExpander* gpiope = isGPIOPE_OUTPUT(address);

          // printf("address=%d\n", address);

          if (gpiope) {

            // use output value or override the value.
            int32_t value_to_send = matrixData.computer_assist[0][Mi]
                              ? matrixData.output_value[0][Mi]
                              : matrixData.override_output_value[0][Mi];
            
            uint8_t port_map_index = matrixData.matrix_port_map[0][Mi];

            // printf("port_map_index=%d\n", port_map_index);
            
            // unsigned long write_gpiope_t0 = esp_timer_get_time();
            // reduce time to write:
            // send command byte once using: GPIOPE_Write_Portmap_Pin_NoID() (requires request ID set once elsewhere)
            // send: uint8 index & a uint8 value (for pins we only need a uint8 currently) 
            GPIOPE_Write_Portmap_Pin(*gpiope, port_map_index, value_to_send);

            // printf("write_gpiope_t=%lld\n", esp_timer_get_time()-write_gpiope_t0);
            // printf("matrix_switch_t=%lld\n", esp_timer_get_time()-matrix_start_t);

            count_write++;
          }
          else {
            printf("specified unknown gpiope device! address=%d\n", address);
          }
        }
      }
      esp_task_wdt_reset();
      // --------------------------------------------
      // Task frequency counter
      // --------------------------------------------
      stepFFCounter(systemData.counters_pco, count_write);
      systemData.counters_pco.flag_c = true;
      #endif

    }

    // --------------------------------------------
    // Task frequency counter
    // --------------------------------------------
    stepFCounter(systemData.counters_mtx, 1);
    xSemaphoreGive(dataMutex);
  }
}
void createTaskSwitches() {
  xTaskCreatePinnedToCore(
    taskSwitches,             /* Function to implement the task */
    "TaskSwitches",           /* Name of the task */
    TASK_SWITCHES_STACK_SIZE, /* Stack size in words */
    nullptr,                  /* Task input parameter */
    TASK_SWITCHES_PRIORITY,   /* Priority of the task */
    &TaskSwitches,            /* Task handle. */
    TASK_SWITCHES_CORE);      /* Core where the task should run */
}
#endif

#ifdef GPIOPE_USE_INPUT
/** ----------------------------------------------------------------------------
 * Input Controller Task. Intended to be used for 0-N input GPIO Port Expanders
 *
 * @brief Performs various operations including:
 *        Reads and stores values from input port controller.
 */
static void taskInputPortController(void *pvParameters) {
  (void)pvParameters; // FreeRTOS task signature requires the parameter; it is unused here (MISRA C 2012 Rule 2.7).
  esp_task_wdt_add(nullptr);
  while (!global_task_sync) {
    esp_task_wdt_reset();
    vTaskDelay(1);
  }
  for (;;) {

    // Delay Task
    if (taskFrequencyInputPortController() == true) {

      // ------------------------------------------------
      // Read GPIOPE Input
      // ------------------------------------------------

      // test re-query (almost ready to utilize up to i2c addr max gpiope's if defined)
      // option read if interrupted, all gpiope's should interrupt on the same pin
      // GPIOPE_QueryDevice(GPIOPE_INPUT_11, I2C_ADDR_11);

      // todo: read all defined gpiope's

      static int64_t gpioe_chan_last_read_uS[GPIOPE_MAX_SIZE] = {0};
      bool gpioe_chan_was_enabled[GPIOPE_MAX_SIZE] = {false};
      bool gpioe_chan_did_read[GPIOPE_MAX_SIZE] = {false};

      // Iterate through address range (unlike for output).
      // Replace with more efficient implementation.
      for (int address = 0; address < 128; address++) {

        // Check if device defined
        GPIOPortExpander* gpiope = isGPIOPE_INPUT(address); // <- requires config

        if (gpiope) {

          uint8_t gpioe_max_values = (uint8_t)gpiope->max_input_values;
          for (uint8_t i_chan = 0; i_chan < gpiope->max_pins; i_chan++) {
            if (gpiope->enabled[i_chan] == true) {
              gpioe_chan_was_enabled[i_chan] = true;
              if ((esp_timer_get_time() - gpioe_chan_last_read_uS[i_chan]) >= (int64_t)gpiope->chan_freq_uS[i_chan]) {
                if (GPIOPE_Read_Pin(*gpiope, i_chan)) {
                  gpioe_chan_last_read_uS[i_chan] = esp_timer_get_time();
                  gpioe_chan_did_read[i_chan] = true;
                } else {
                  printf("ERROR: readInputPortControllerReadPins (pin_index=%d)\n", i_chan);
                }
              }
            }
          }

          esp_task_wdt_reset();
          // --------------------------------------------
          // Task frequency counter
          // --------------------------------------------
          for (uint8_t i_chan = 0; i_chan < gpioe_max_values; i_chan++) {
            if (gpioe_chan_was_enabled[i_chan] == true) {
              if (gpioe_chan_did_read[i_chan] == true) {
                systemData.counters_gpiope0.flag_c = true;
                stepFFCounter(systemData.counters_gpioe_chan[i_chan], 1);
              }
            }
          }
        }
        else {
          /* terminate statement */
        }
      }

      xSemaphoreTake(dataMutex, portMAX_DELAY);
      #ifdef SatIO_SERIAL_TX_OPTION_CURRENT_TASK
      outputSerialGPIOPEnput();
      #endif
      xSemaphoreGive(dataMutex);
    }

    // --------------------------------------------
    // Task frequency counter
    // --------------------------------------------
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    stepFCounter(systemData.counters_gpiope0, 1);
    xSemaphoreGive(dataMutex);
  }
}
void createTaskInputPortController() {
  xTaskCreatePinnedToCore(
    taskInputPortController,             /* Function to implement the task */
    "TaskInputPortControllers",           /* Name of the task */
    TASK_GPIOPE_INPUT_STACK_SIZE, /* Stack size in words */
    nullptr,                  /* Task input parameter */
    TASK_GPIOPE_INPUT_PRIORITY,   /* Priority of the task */
    &TaskInputPortController,            /* Task handle. */
    TASK_GPIOPE_INPUT_CORE);      /* Core where the task should run */
}
#endif

#ifdef SatIO_USE_UNIVERSE
/** ----------------------------------------------------------------------------
 * Universe Task.
 *
 * @brief Stores various information about the universe!
 *
 *        Tracks planets and meteor showers once per the configured interval,
 *        and continuously evaluates the nearest catalogue object to both the
 *        system zenith and the zenith offset by the gyroscope attitude.
 */
static void taskUniverse(void *pvParameters) {
  (void)pvParameters; // FreeRTOS task signature requires the parameter; it is unused here (MISRA C 2012 Rule 2.7).
  esp_task_wdt_add(nullptr);
  while (!global_task_sync) {
    esp_task_wdt_reset();
    vTaskDelay(1);
  }
  for (;;) {

    // Delay Task
    if (taskFrequencyUniverse() == true) {
      esp_task_wdt_reset();
      
      // ------------------------------------------------
      // Set Sidereal Data for Planet/Object Tracking.
      // ------------------------------------------------
      // TODO: throttle track plans seperately so that star nav can rip if required
      xSemaphoreTake(dataMutex, portMAX_DELAY);
      setSiderealData(
        SatIOData.system_degrees_latitude,
        SatIOData.system_degrees_longitude,
        SatIOData.systemTime.year,
        SatIOData.systemTime.month,
        SatIOData.systemTime.mday,
        SatIOData.systemTime.hour,
        SatIOData.systemTime.minute,
        SatIOData.systemTime.second,
        SatIOData.systemTime.hour,
        SatIOData.localTime.minute,
        SatIOData.localTime.second,
        SatIOData.system_altitude);
      
      // edit
      storeLST(siderealPlanetData.local_sidereal_time);

      esp_task_wdt_reset();

      // ------------------------------------------------
      // Track Planets/Meteors
      // ------------------------------------------------
      trackPlanets();
      esp_task_wdt_reset();
      setMeteorShowerWarning(SatIOData.localTime.month, SatIOData.localTime.mday);
      esp_task_wdt_reset();
      
      // ------------------------------------------------
      // Set RA & Dec for system zenith. (add to matrix)
      // ------------------------------------------------
      siderealPlanetData.local_sidereal_attitude = myAstro.getSiderealAttitude(0, 0, 0);
        esp_task_wdt_reset();

      #ifdef SatIO_USE_GYRO_0
      // ------------------------------------------------
      // Set RA & Dec for system zenith +- Gyro. (add to matrix)
      // ------------------------------------------------

      siderealPlanetData.gyro_0_sidereal_attitude = myAstro.getSiderealAttitude(
        gyroData.gyro_0_ang_x,  // roll
        gyroData.gyro_0_ang_y,  // pitch
        gyroData.gyro_0_ang_z   // yaw
      );
      esp_task_wdt_reset();
      #endif

      // ------------------------------------------------
      // StarNav Dynamic Test Zenith Every Interval
      // ------------------------------------------------
      setStarNav(
        siderealPlanetData.local_sidereal_attitude.ra_h,
        siderealPlanetData.local_sidereal_attitude.ra_m,
        siderealPlanetData.local_sidereal_attitude.ra_s,
        siderealPlanetData.local_sidereal_attitude.dec_d,
        siderealPlanetData.local_sidereal_attitude.dec_m,
        siderealPlanetData.local_sidereal_attitude.dec_s
      );
      esp_task_wdt_reset();
      // printf("---------------------------------------------\n");
      // printf("Table Index:   %d\n", siderealObjectData.object_table_i);
      // printf("Table:         %s\n", siderealObjectData.object_table_name);
      // printf("Number:        %d\n", siderealObjectData.object_number);
      // printf("Name:          %s\n", siderealObjectData.object_name);
      // printf("Type:          %s\n", siderealObjectData.object_type);
      // printf("Constellation: %s\n", siderealObjectData.object_con);
      // printf("Distance:      %f\n", siderealObjectData.object_dist);
      // printf("Azimuth:       %f\n", siderealObjectData.object_az);
      // printf("Altitude:      %f\n", siderealObjectData.object_alt);
      // printf("Rise:          %f\n", siderealObjectData.object_r);
      // printf("Set:           %f\n", siderealObjectData.object_s);
      // printf("---------------------------------------------\n");

      // ------------------------------------------------
      // StarNav Dynamic Test Zenith+-Gyro Offset
      // ------------------------------------------------
      setStarNav(
        siderealPlanetData.gyro_0_sidereal_attitude.ra_h,
        siderealPlanetData.gyro_0_sidereal_attitude.ra_m,
        siderealPlanetData.gyro_0_sidereal_attitude.ra_s,
        siderealPlanetData.gyro_0_sidereal_attitude.dec_d,
        siderealPlanetData.gyro_0_sidereal_attitude.dec_m,
        siderealPlanetData.gyro_0_sidereal_attitude.dec_s
      );
      esp_task_wdt_reset();
      // printf("---------------------------------------------\n");
      // printf("Table Index:   %d\n", siderealObjectData.object_table_i);
      // printf("Table:         %s\n", siderealObjectData.object_table_name);
      // printf("Number:        %d\n", siderealObjectData.object_number);
      // printf("Name:          %s\n", siderealObjectData.object_name);
      // printf("Type:          %s\n", siderealObjectData.object_type);
      // printf("Constellation: %s\n", siderealObjectData.object_con);
      // printf("Distance:      %f\n", siderealObjectData.object_dist);
      // printf("Azimuth:       %f\n", siderealObjectData.object_az);
      // printf("Altitude:      %f\n", siderealObjectData.object_alt);
      // printf("Rise:          %f\n", siderealObjectData.object_r);
      // printf("Set:           %f\n", siderealObjectData.object_s);
      // printf("---------------------------------------------\n");

      // --------------------------------------------
      // Task frequency counter
      // --------------------------------------------
      stepFFCounter(systemData.counters_uni, 1);
      systemData.counters_uni.flag_c = true;
      #ifdef SatIO_SERIAL_TX_OPTION_CURRENT_TASK
      outputSerialUniverse();
      #endif

      esp_task_wdt_reset();
      xSemaphoreGive(dataMutex);
    }

    // --------------------------------------------
    // Task frequency counter
    // --------------------------------------------
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    stepFCounter(systemData.counters_uni, 1);
    xSemaphoreGive(dataMutex);
  }
}
void createTaskUniverse() {
  xTaskCreatePinnedToCore(
    taskUniverse,             /* Function to implement the task */
    "TaskUniverse",           /* Name of the task */
    TASK_UNIVERSE_STACK_SIZE, /* Stack size in words */
    nullptr,                  /* Task input parameter */
    TASK_UNIVERSE_PRIORITY,   /* Priority of the task */
    &TaskUniverse,            /* Task handle. */
    TASK_UNIVERSE_CORE);      /* Core where the task should run */
}
#endif

#ifdef SatIO_SERIAL_TX_OPTION_NEW_TASK
/** ----------------------------------------------------------------------------
 * SatIO Serial Tx Task.
 *
 * @brief Transmits the aggregated SatIO ($SatIO) and port controller input
 *        ($GPIOEI) sentences over the serial port, each gated by its own
 *        systemData output-enabled flag.
 */
static void taskSatIOSerialTx(void *pvParameters) {
  (void)pvParameters; // FreeRTOS task signature requires the parameter; it is unused here (MISRA C 2012 Rule 2.7).
  esp_task_wdt_add(nullptr);
  while (!global_task_sync) {
    esp_task_wdt_reset();
    vTaskDelay(1);
  }
  for (;;) {

    // Delay Task
    if (taskFrequencySatIOSerialTx() == true) {
      xSemaphoreTake(dataMutex, portMAX_DELAY);
      esp_task_wdt_reset();

      // --------------------------------------------
      // Output.
      // --------------------------------------------
      outputSerialGPS();
      outputSerialSatIO();
      outputSerialADMplex0();
      outputSerialADMplex1();
      outputSerialGyro0();
      outputSerialUniverse();
      outputSerialMatrix();
      outputSerialGPIOPEnput();
      esp_task_wdt_reset();

      // --------------------------------------------
      // Task frequency counter
      // --------------------------------------------
      stepFFCounter(systemData.counters_SatIO_serial_tx, 1);
      xSemaphoreGive(dataMutex);
    }

    // --------------------------------------------
    // Task frequency counter
    // --------------------------------------------
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    stepFCounter(systemData.counters_SatIO_serial_tx, 1);
    xSemaphoreGive(dataMutex);
  }
}
void createTaskSatIOSerialTx() {
  xTaskCreatePinnedToCore(
    taskSatIOSerialTx,             /* Function to implement the task */
    "TaskSatIOSerialTx",           /* Name of the task */
    TASK_SatIO_SERIAL_TX_STACK_SIZE, /* Stack size in words */
    nullptr,                       /* Task input parameter */
    TASK_SatIO_SERIAL_TX_PRIORITY, /* Priority of the task */
    &TaskSatIOSerialTx,            /* Task handle. */
    TASK_SatIO_SERIAL_TX_CORE);    /* Core where the task should run */
}
#endif

#ifdef SatIO_USE_DISPLAY
/** ----------------------------------------------------------------------------
 * Display Update Task.
 *
 * @brief Drives LVGL screen updates at the lowest user-task priority so it is
 *        preempted by every other task regardless of how long a frame takes.
 *
 *        Acquires the BSP display lock before calling update_display() so all
 *        LVGL API calls are thread-safe.  The lock is released between frames
 *        so the BSP LVGL task can service touch events and DMA completions
 *        during the idle window.
 */
static void taskDisplayUpdate(void *pvParameters) {
  bool locked = false;
  (void)pvParameters;
  for (;;) {

    // Delay Task
    if (taskFrequencyDisplay() == true) {
      locked = bsp_display_lock(portMAX_DELAY);
      if (locked) {

        #ifdef SatIO_DISPLAY_OPTION_LVGL
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        update_display_lvgl();
        xSemaphoreGive(dataMutex);
        bsp_display_unlock();
        #endif
        
      }
      // --------------------------------------------
      // Task frequency counter
      // --------------------------------------------
      xSemaphoreTake(dataMutex, portMAX_DELAY);
      stepFFCounter(systemData.counters_dsp, 1);
      systemData.counters_dsp.flag_c = true;
      xSemaphoreGive(dataMutex);
    }

    // --------------------------------------------
    // Task frequency counter
    // --------------------------------------------
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    stepFCounter(systemData.counters_dsp, 1);
    xSemaphoreGive(dataMutex);
  }
}

void createTaskDisplayUpdate() {
  (void)xTaskCreatePinnedToCore(
    taskDisplayUpdate,          /* Function to implement the task */
    "TaskDisplayUpdate",        /* Name of the task */
    TASK_DISPLAY_STACK_SIZE,    /* Stack size in words */
    nullptr,                    /* Task input parameter */
    TASK_DISPLAY_PRIORITY,      /* Priority of the task */
    &TaskDisplayUpdate,         /* Task handle. */
    TASK_DISPLAY_CORE);         /* Core where the task should run */
}
#endif