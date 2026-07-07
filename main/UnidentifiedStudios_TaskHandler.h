/*
    TaskHandler - Written By Benjamin Jack Cullen.

    MISRA conventions used throughout this library:
    (1) Every if/else/while/for body is a braced compound statement (MISRA C 2012 Rule 15.6).
    (2) Boolean objects are tested directly instead of being compared to true/false
        (MISRA C 2012 Rule 14.4 - essentially Boolean controlling expressions).
    (3) Null pointers are written as nullptr, the type-safe C++ null pointer constant.

    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#ifndef TASK_HANDLER_H
#define TASK_HANDLER_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <Arduino.h>
#include "UnidentifiedStudios_I2C.h"

/* Handles of the FreeRTOS tasks created by this library, exposed so that other
   translation units can query task state or retarget a task's delay/tick setting. */
extern TaskHandle_t TaskStorage;
extern TaskHandle_t TaskADMplex0;
extern TaskHandle_t TaskADMplex1;
extern TaskHandle_t TaskGyro;
extern TaskHandle_t TaskGPS;
extern TaskHandle_t TaskUniverse;
extern TaskHandle_t TaskSwitches;
extern TaskHandle_t TaskDisplayUpdate;
extern TaskHandle_t TaskSystemTime;
extern TaskHandle_t TaskSatIOSerialTx;
extern TaskHandle_t TaskInputPortController;

/* Timestamp (esp_timer_get_time(), uS since boot) captured the moment readGPS()
   returns true in taskGPS. Used to measure readGPS() -> applyPendingDateTime()
   latency across the taskGPS -> taskSystemTime handoff. */
extern int64_t gps_read_done_uS;

void createTaskStorage();
void createTaskADMplex0();
void createTaskADMplex1();
void createTaskGyro();
void createTaskGPS();
void createTaskUniverse();
void createTaskSwitches();
void createTaskDisplayUpdate();
void createTaskSystemTime();
void createTaskSatIOSerialTx();
void createTaskInputPortController();

/**
 * @brief Time syncronize tasks.
 */
void syncTasks();

/**
 * @brief Is Task Delayed.
 * @return Returns bool for task delayed.
 */
bool isTaskDelayed(TaskHandle_t taskHandle);

/**
 * @brief Power Saving. Sets all task delay timings to a default low power mode.
 */
void setTasksDelayLowPower();

/**
 * @brief Balanced. Sets all task delay timings to a default balanced mode.
 */
void setTasksDelayBalanced();

/**
 * @brief Ultimate Performance. Sets all task delay timings to optimum performance.
 */
void setTasksDelayUltimatePerformance();

/**
 * @brief Set task delay in ms.
 */
void setDelay(TaskHandle_t task_handle, uint32_t delay_in, uint32_t *delay_out);

/**
 * @brief GPS task timing gate. Blocks via vTaskDelayUntil until the next
 *        100 ms slot (10 Hz), matching the wtgps300p output rate.
 * @return Always true; false reserved for future disable logic.
 */
bool taskFrequencyGPS(void);
bool taskFrequencyGyro(void);
bool taskFrequencySwitches(void);
bool taskFrequencyStorage(void);
bool taskFrequencyInfoCMD(void);
bool taskFrequencyADMplex0(void);
bool taskFrequencyADMplex1(void);
bool taskFrequencyUniverse(void);
bool taskFrequencyDisplay(void);
bool taskFrequencySatIOSerialTx(void);

#endif // TASK_HANDLER_H
