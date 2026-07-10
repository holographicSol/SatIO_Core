/*
    main.cpp - Application entry point. Written By Benjamin Jack Cullen.
    Boots the system: configures the watchdog, brings up LVGL, opens the
    diagnostic UART, brings up the I2C buses and the GPS UART, starts every
    FreeRTOS task, synchronizes them, then shows the home screen.
    MISRA conventions used throughout this file:
    (1) Logging uses ESP_LOGI/ESP_LOGW instead of the <stdio.h> formatted
        output functions (MISRA C 2012 Rule 21.6).
    (2) No dynamic memory allocation; a buffer that must outlive a single
        call is a function-local static instead of a heap allocation
        (MISRA C 2012 Rule 21.3).
    (3) Every object is given the narrowest scope its use requires:
        function-local static where only one function needs it, file-scope
        static where more than one function needs it (MISRA C 2012 Rule 8.9).
    (4) Every switch statement has an explicit default clause and every
        switch-clause ends in break (MISRA C 2012 Rule 16.3 / 16.4).
    (5) Every if/while body is a braced compound statement, including bodies
        that do nothing (MISRA C 2012 Rule 15.6).
    (6) A function's return value that is not needed by the caller is
        discarded explicitly with a (void) cast (MISRA C 2012 Rule 17.7).
    (7) Unsigned struct-literal members carry the 'U' suffix so their type
        is unambiguous (MISRA C 2012 Rule 7.2).
    (8) No commented-out code (MISRA C 2012 Dir 4.4).
    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/
#include "bsp/esp32_p4_wifi6_touch_lcd_xc.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "ff.h"           // FatFs core
#include "diskio.h"       // Disk I/O
#include "diskio_impl.h"  // ESP32 disk impl
#include "esp_vfs_fat.h"  // VFS integration
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "esp_err.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include <dirent.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include <float.h>
#include <math.h>
#include <Arduino.h>
#include <sys/time.h>
#include <rtc_wdt.h>
#include <esp_task_wdt.h>
#include "esp_pm.h"
#include "esp_attr.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_spiffs.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "driver/uart.h"
#include "esp_rom_uart.h"
#include "./REG.h"
#include "./UnidentifiedStudios_Config.h"
#include "./UnidentifiedStudios_StrVal.h"
#include "./UnidentifiedStudios_Meteors.h"
#include "./UnidentifiedStudios_WTGPS300P.h"
#include "./UnidentifiedStudios_WT901.h"
#include "./UnidentifiedStudios_Multiplexers.h"
#include "./UnidentifiedStudios_SiderealHelper.h"
#include "./UnidentifiedStudios_HexToDig.h"
#include "./UnidentifiedStudios_INS.h"
#include "./UnidentifiedStudios_SatIO.h"
#include "./UnidentifiedStudios_Mapping.h"
#include "./UnidentifiedStudios_Matrix.h"
#include "./UnidentifiedStudios_GPIOPortExpander.h"
#include "./UnidentifiedStudios_CMD.h"
#include "./UnidentifiedStudios_SystemData.h"
#include "./UnidentifiedStudios_SdCardHelper.h"
#include "./UnidentifiedStudios_TaskHandler.h"
#include "./UnidentifiedStudios_I2C.h"
#include "./wit_c_sdk.h"
#ifdef SatIO_DISPLAY_OPTION_LVGL
#include "lvgl.h"
#include "./UnidentifiedStudios_SatIOLVGL.h"
#include "./UnidentifiedStudios_AstroClock.h"
#endif
#define UART0_NUM               UART_NUM_0
#define UART0_BUF_SIZE          (1024)
#define UART0_RD_BUF_SIZE       (UART0_BUF_SIZE)
#define UART0_QUEUE_LENGTH      (20)
#define UART0_TASK_STACK_SIZE   (3072)
#define UART0_TASK_PRIORITY     (12)
static const char *UART0_TAG    = "UART0_EVENTS";
static const char *APP_MAIN_TAG = "APP_MAIN";
// Shared by app_main() (creates the driver and its queue) and
// uart0_event_task() (reads the queue), so file scope is required; this is
// the documented exception to keeping objects function-local (MISRA C 2012
// Rule 8.9).
static QueueHandle_t uart0_queue;
/** ----------------------------------------------------------------------------
 * UART0 Event Task.
 *
 * (1) The receive buffer and the line-accumulation buffer/index are
 *     function-local statics rather than heap allocations, so the task has
 *     no dynamic-memory dependency and no allocation can fail at runtime
 *     (MISRA C 2012 Rule 21.3); they are scoped to this function because it
 *     is their only user (MISRA C 2012 Rule 8.9).
 * (2) The task parameter is required by the FreeRTOS task signature but is
 *     unused here, so the discard is made explicit with a (void) cast
 *     (MISRA C 2012 Rule 2.7).
 * (3) Reads raw bytes from UART0 and accumulates them into a line buffer.
 *     When a '\r' or '\n' terminator arrives, the completed line is copied
 *     into serial0Data.BUFFER_RX and CmdProcess() is called to act on it.
 *     FIFO-overflow and buffer-full events flush the driver and reset the
 *     line buffer so the next line starts cleanly.
 */
static void uart0_event_task(void *pvParameters) {
    (void)pvParameters; // MISRA C 2012 Rule 2.7: unused FreeRTOS task parameter.
    static uint8_t dtmp[UART0_RD_BUF_SIZE];
    static char uart0_line_buffer[UART0_BUF_SIZE];
    static int uart0_line_pos = 0;
    uart_event_t event;
    int len = 0;
    uart_flush_input(UART0_NUM);
    for (;;) {
        // Rx
        if (xQueueReceive(uart0_queue, (void *)&event, portMAX_DELAY)) {
            memset(dtmp, 0, UART0_RD_BUF_SIZE); // Standard, portable equivalent of bzero().
            switch (event.type) {
                case UART_DATA: {
                    // event.size reflects bytes queued in the driver's RX ring buffer,
                    // which is larger than dtmp; clamp so a burst never overflows dtmp
                    // (excess bytes stay in the ring buffer for the next event).
                    size_t to_read = (event.size < sizeof(dtmp)) ? event.size : sizeof(dtmp);
                    len = uart_read_bytes(UART0_NUM, dtmp, to_read, 1000 / portTICK_PERIOD_MS);
                    // Accumulate data into line buffer, process on newline
                    for (int i = 0; i < len; i++) {
                        char c = (char)dtmp[i];
                        // Check for line terminator
                        if ((c == '\n') || (c == '\r')) {
                            if (uart0_line_pos > 0) {
                                // Complete line received - process it
                                uart0_line_buffer[uart0_line_pos] = '\0';
                                // Copy to serial0Data.BUFFER_RX for CmdProcess
                                memset(serial0Data.BUFFER_RX, 0, sizeof(serial0Data.BUFFER_RX));
                                strncpy(serial0Data.BUFFER_RX, uart0_line_buffer, sizeof(serial0Data.BUFFER_RX) - 1);
                                ESP_LOGI(UART0_TAG, "Received data: %s", serial0Data.BUFFER_RX);
                                // uart0_event_task is not core-pinned and CmdProcess()
                                // writes SatIOData/systemData directly (e.g. matrix/time
                                // config commands), so it shares dataMutex with every
                                // other SatIOData/systemData producer.
                                xSemaphoreTake(dataMutex, portMAX_DELAY);
                                CmdProcess();
                                xSemaphoreGive(dataMutex);
                                // Reset line buffer and keep scanning dtmp: a single
                                // UART_DATA event's bytes can contain more than one
                                // newline-terminated command (e.g. "CMD1\r\nCMD2\r\n"),
                                // and `break` here would exit the for loop, silently
                                // discarding any commands after the first.
                                uart0_line_pos = 0;
                                continue;
                            }
                            // Skip empty lines (consecutive \r\n)
                        } else {
                            // Accumulate character (with overflow protection)
                            if (uart0_line_pos < (UART0_BUF_SIZE - 1)) {
                                uart0_line_buffer[uart0_line_pos++] = c;
                            } else {
                                // Buffer overflow - discard and reset
                                ESP_LOGW(UART0_TAG, "Line buffer overflow, discarding");
                                uart0_line_pos = 0;
                            }
                        }
                    }
                    break;
                }
                case UART_FIFO_OVF: {
                    ESP_LOGW(UART0_TAG, "UART FIFO overflow");
                    uart_flush_input(UART0_NUM);
                    xQueueReset(uart0_queue);
                    uart0_line_pos = 0;  // Reset line buffer
                    break;
                }
                case UART_BUFFER_FULL: {
                    ESP_LOGW(UART0_TAG, "UART buffer full");
                    uart_flush_input(UART0_NUM);
                    xQueueReset(uart0_queue);
                    uart0_line_pos = 0;  // Reset line buffer
                    break;
                }
                case UART_BREAK:
                    ESP_LOGI(UART0_TAG, "UART RX break");
                    break;
                case UART_PARITY_ERR:
                    ESP_LOGI(UART0_TAG, "UART parity error");
                    break;
                case UART_FRAME_ERR:
                    ESP_LOGI(UART0_TAG, "UART frame error");
                    break;
                default:
                    ESP_LOGI(UART0_TAG, "UART event type: %d", event.type);
                    break;
            }
        }
    }
}
/** ----------------------------------------------------------------------------
 * Matrix Function Index Table.
 *
 * @brief Prints every currently-compiled INDEX_MATRIX_SWITCH_FUNCTION_* entry
 *        alongside its index and display name.
 * @note The mapping shifts whenever a build option that gates a block of the
 *       enum (UnidentifiedStudios_Matrix.h) is toggled, so this table is the
 *       authoritative reference for the current build -- read it from the
 *       boot log rather than assuming indices from a prior build still hold.
 */
static void printMatrixFunctionIndexTable(void) {
    printf("---------------------------------------------\n");
    printf("Matrix Function Index (%ld entries)\n", (long)MAX_MATRIX_FUNCTION_NAMES);
    printf("---------------------------------------------\n");
    for (int32_t i = 0; i < MAX_MATRIX_FUNCTION_NAMES; i++) {
        printf("[%3ld] %s\n", (long)i, matrixData.matrix_function_names[i]);
    }
    printf("---------------------------------------------\n");
}
/** ----------------------------------------------------------------------------
 * Arduino Core Entry Point.
 *
 * External linkage is required because the Arduino-ESP32 core calls this
 * function by name once app_main() starts the Arduino runtime (MISRA C 2012
 * Rule 8.7: internal linkage is used everywhere it is not required, and this
 * is the documented exception). The body is intentionally empty because
 * app_main() performs every initialization step this project needs.
 */
void setup() {
}
/** -----------------------------------------------------------------------------------------------
 * @brief Main application entry point.
 *
 * External linkage (via extern "C") is required because the ESP-IDF startup
 * code calls this function by name (MISRA C 2012 Rule 8.7).
 */
extern "C" void app_main(void)
{
    // ----------------------------------------------------------------------------
    // Warmup delay: some devices require at least one second start.
    // ----------------------------------------------------------------------------
    // const uint32_t startup_delay_ms = 1000U; // Named so the duration is documented once.
    // delay(startup_delay_ms);
    /** ----------------------------------------------------------------------------
     * Watchdog Configuration.
     *
     * (1) timeout_ms and idle_core_mask are uint32_t struct members, so
     *     their literals carry the 'U' suffix to make the unsigned type
     *     unambiguous (MISRA C 2012 Rule 7.2).
     * (2) esp_task_wdt_reconfigure()'s esp_err_t result is checked by
     *     ESP_ERROR_CHECK so a configuration failure is never silently
     *     ignored (MISRA C 2012 Rule 17.7).
     * (3) Reconfigures the watchdog to a 5 second timeout that only logs a
     *     warning, rather than panicking, if a task fails to check in.
     */
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 5000U,      // 5 second timeout.
        .idle_core_mask = 0U,     // Idle tasks are not watched.
        .trigger_panic = false,   // Log a warning instead of panicking on timeout.
    };
    ESP_ERROR_CHECK(esp_task_wdt_reconfigure(&wdt_config));
    // delay(5000);
    /** ----------------------------------------------------------------------------
     * sdcard_mount() (UnidentifiedStudios_SdCardHelper.cpp) is called on every
     * taskStorage() cycle to catch SD card hotplug events, which re-enters
     * bsp_sdcard_mount() -> sdmmc_host_init() even when already mounted. The
     * driver's "SDMMC host already initialized, skipping init flow" INFO log
     * is expected in that case, not an error, so it's silenced at the source.
     */
    esp_log_level_set("sdmmc_periph", ESP_LOG_WARN);
    /** ----------------------------------------------------------------------------
     * Initialize Mutexes
     */
    initSystemTimeMutex(); // must exist before any task can touch tv_now/timeinfo
    initDataMutex();       // must exist before any task can touch SatIOData/systemData
    /** ----------------------------------------------------------------------------
     * LVGL Initialization
     */
    #ifdef SatIO_DISPLAY_OPTION_LVGL
    initSatIOUI();
    #endif
    /** ----------------------------------------------------------------------------
     * Diagnostic UART0 Setup.
     *
     * (1) Each esp_err_t-returning driver call is checked by
     *     ESP_ERROR_CHECK, and xTaskCreate()'s result is discarded
     *     explicitly with a (void) cast, so every return value is either
     *     used or its discard is made visible (MISRA C 2012 Rule 17.7).
     * (2) Configures UART0 for command/diagnostic traffic and starts the
     *     task that parses incoming lines and dispatches them to
     *     CmdProcess().
     */
    uart_config_t uart0_config = {
        .baud_rate = 921600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
        .flags = {},
    };
    ESP_ERROR_CHECK(uart_param_config(UART0_NUM, &uart0_config));
    ESP_ERROR_CHECK(uart_set_pin(UART0_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART0_NUM, UART0_BUF_SIZE * 2, UART0_BUF_SIZE, UART0_QUEUE_LENGTH, &uart0_queue, 0));
    (void)xTaskCreate(uart0_event_task, "uart0_event_task", UART0_TASK_STACK_SIZE, NULL, UART0_TASK_PRIORITY, NULL);
    ESP_LOGI(APP_MAIN_TAG, "UART0 ready - send data to GPIO1 (RX0)");
    /** ----------------------------------------------------------------------------
     * Matrix Function Index Table
     */
    printMatrixFunctionIndexTable();
    /** ----------------------------------------------------------------------------
     * System Time
     */
    printf("Initializing system time");
    SatIOData.systemTime.sync_immediately_flag=true;
    initSystemTime();
    /** ----------------------------------------------------------------------------
     * I2C Bus 2.
     *
     * (1) begin()'s and setBufferSize()'s results are not needed here, so
     *     each discard is made explicit with a (void) cast (MISRA C 2012
     *     Rule 17.7).
     * (2) Brings up I2C bus and clears every output on the port
     *     controller attached to it.
     */
    printf("Initializing I2C bus 2");
    (void)iic_2.setPins(IIC_BUS2_SDA, IIC_BUS2_SCL);
    (void)iic_2.setBufferSize(MAX_IIC_BUFFER_SIZE);
    iic_2.setTimeOut(I2C_TIMEOUT_MS_BUS2);
    (void)iic_2.begin(IIC_BUS2_SDA, IIC_BUS2_SCL, I2C_CLOCK_Hz_BUS2);
    iic_2.setClock(I2C_CLOCK_Hz_BUS2);
    /** ----------------------------------------------------------------------------
     * I2C Bus 0.
     *
     * (1) begin()'s and setBufferSize()'s results are not needed here, so
     *     each discard is made explicit with a (void) cast (MISRA C 2012
     *     Rule 17.7).
     * (2) Brings up I2C bus.
     */
    printf("Initializing I2C bus 0");
    (void)iic_0.setPins(IIC_BUS0_SDA, IIC_BUS0_SCL);
    (void)iic_0.setBufferSize(MAX_IIC_BUFFER_SIZE);
    iic_0.setTimeOut(I2C_TIMEOUT_MS_BUS0);
    (void)iic_0.begin(IIC_BUS0_SDA, IIC_BUS0_SCL, I2C_CLOCK_Hz_BUS0);
    iic_0.setClock(I2C_CLOCK_Hz_BUS0);

    delay(3000);

    /** ----------------------------------------------------------------------------
     * GPIOPortExpander.
     * 
     * Automatically populate GPIOPortExpander instances.
     *
     */
    #ifdef SatIO_USE_GPIOPE_OUTPUT_0
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_0, I2C_ADDR_OUTPUT_GPIOE_0);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_1
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_1, I2C_ADDR_OUTPUT_GPIOE_1);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_2
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_2, I2C_ADDR_OUTPUT_GPIOE_2);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_3
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_3, I2C_ADDR_OUTPUT_GPIOE_3);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_4
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_4, I2C_ADDR_OUTPUT_GPIOE_4);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_5
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_5, I2C_ADDR_OUTPUT_GPIOE_5);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_6
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_6, I2C_ADDR_OUTPUT_GPIOE_6);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_7
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_7, I2C_ADDR_OUTPUT_GPIOE_7);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_8
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_8, I2C_ADDR_OUTPUT_GPIOE_8);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_9
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_9, I2C_ADDR_OUTPUT_GPIOE_9);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_10
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_10, I2C_ADDR_OUTPUT_GPIOE_10);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_11
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_11, I2C_ADDR_OUTPUT_GPIOE_11);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_12
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_12, I2C_ADDR_OUTPUT_GPIOE_12);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_13
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_13, I2C_ADDR_OUTPUT_GPIOE_13);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_14
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_14, I2C_ADDR_OUTPUT_GPIOE_14);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_15
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_15, I2C_ADDR_OUTPUT_GPIOE_15);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_16
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_16, I2C_ADDR_OUTPUT_GPIOE_16);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_17
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_17, I2C_ADDR_OUTPUT_GPIOE_17);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_18
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_18, I2C_ADDR_OUTPUT_GPIOE_18);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_19
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_19, I2C_ADDR_OUTPUT_GPIOE_19);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_20
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_20, I2C_ADDR_OUTPUT_GPIOE_20);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_21
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_21, I2C_ADDR_OUTPUT_GPIOE_21);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_22
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_22, I2C_ADDR_OUTPUT_GPIOE_22);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_23
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_23, I2C_ADDR_OUTPUT_GPIOE_23);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_24
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_24, I2C_ADDR_OUTPUT_GPIOE_24);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_25
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_25, I2C_ADDR_OUTPUT_GPIOE_25);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_26
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_26, I2C_ADDR_OUTPUT_GPIOE_26);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_27
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_27, I2C_ADDR_OUTPUT_GPIOE_27);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_28
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_28, I2C_ADDR_OUTPUT_GPIOE_28);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_29
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_29, I2C_ADDR_OUTPUT_GPIOE_29);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_30
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_30, I2C_ADDR_OUTPUT_GPIOE_30);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_31
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_31, I2C_ADDR_OUTPUT_GPIOE_31);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_32
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_32, I2C_ADDR_OUTPUT_GPIOE_32);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_33
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_33, I2C_ADDR_OUTPUT_GPIOE_33);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_34
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_34, I2C_ADDR_OUTPUT_GPIOE_34);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_35
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_35, I2C_ADDR_OUTPUT_GPIOE_35);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_36
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_36, I2C_ADDR_OUTPUT_GPIOE_36);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_37
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_37, I2C_ADDR_OUTPUT_GPIOE_37);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_38
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_38, I2C_ADDR_OUTPUT_GPIOE_38);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_39
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_39, I2C_ADDR_OUTPUT_GPIOE_39);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_40
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_40, I2C_ADDR_OUTPUT_GPIOE_40);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_41
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_41, I2C_ADDR_OUTPUT_GPIOE_41);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_42
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_42, I2C_ADDR_OUTPUT_GPIOE_42);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_43
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_43, I2C_ADDR_OUTPUT_GPIOE_43);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_44
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_44, I2C_ADDR_OUTPUT_GPIOE_44);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_45
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_45, I2C_ADDR_OUTPUT_GPIOE_45);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_46
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_46, I2C_ADDR_OUTPUT_GPIOE_46);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_47
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_47, I2C_ADDR_OUTPUT_GPIOE_47);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_48
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_48, I2C_ADDR_OUTPUT_GPIOE_48);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_49
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_49, I2C_ADDR_OUTPUT_GPIOE_49);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_50
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_50, I2C_ADDR_OUTPUT_GPIOE_50);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_51
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_51, I2C_ADDR_OUTPUT_GPIOE_51);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_52
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_52, I2C_ADDR_OUTPUT_GPIOE_52);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_53
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_53, I2C_ADDR_OUTPUT_GPIOE_53);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_54
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_54, I2C_ADDR_OUTPUT_GPIOE_54);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_55
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_55, I2C_ADDR_OUTPUT_GPIOE_55);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_56
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_56, I2C_ADDR_OUTPUT_GPIOE_56);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_57
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_57, I2C_ADDR_OUTPUT_GPIOE_57);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_58
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_58, I2C_ADDR_OUTPUT_GPIOE_58);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_59
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_59, I2C_ADDR_OUTPUT_GPIOE_59);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_60
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_60, I2C_ADDR_OUTPUT_GPIOE_60);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_61
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_61, I2C_ADDR_OUTPUT_GPIOE_61);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_62
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_62, I2C_ADDR_OUTPUT_GPIOE_62);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_63
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_63, I2C_ADDR_OUTPUT_GPIOE_63);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_64
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_64, I2C_ADDR_OUTPUT_GPIOE_64);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_65
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_65, I2C_ADDR_OUTPUT_GPIOE_65);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_66
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_66, I2C_ADDR_OUTPUT_GPIOE_66);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_67
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_67, I2C_ADDR_OUTPUT_GPIOE_67);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_68
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_68, I2C_ADDR_OUTPUT_GPIOE_68);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_69
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_69, I2C_ADDR_OUTPUT_GPIOE_69);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_70
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_70, I2C_ADDR_OUTPUT_GPIOE_70);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_71
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_71, I2C_ADDR_OUTPUT_GPIOE_71);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_72
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_72, I2C_ADDR_OUTPUT_GPIOE_72);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_73
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_73, I2C_ADDR_OUTPUT_GPIOE_73);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_74
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_74, I2C_ADDR_OUTPUT_GPIOE_74);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_75
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_75, I2C_ADDR_OUTPUT_GPIOE_75);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_76
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_76, I2C_ADDR_OUTPUT_GPIOE_76);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_77
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_77, I2C_ADDR_OUTPUT_GPIOE_77);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_78
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_78, I2C_ADDR_OUTPUT_GPIOE_78);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_79
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_79, I2C_ADDR_OUTPUT_GPIOE_79);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_80
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_80, I2C_ADDR_OUTPUT_GPIOE_80);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_81
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_81, I2C_ADDR_OUTPUT_GPIOE_81);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_82
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_82, I2C_ADDR_OUTPUT_GPIOE_82);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_83
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_83, I2C_ADDR_OUTPUT_GPIOE_83);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_84
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_84, I2C_ADDR_OUTPUT_GPIOE_84);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_85
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_85, I2C_ADDR_OUTPUT_GPIOE_85);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_86
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_86, I2C_ADDR_OUTPUT_GPIOE_86);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_87
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_87, I2C_ADDR_OUTPUT_GPIOE_87);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_88
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_88, I2C_ADDR_OUTPUT_GPIOE_88);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_89
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_89, I2C_ADDR_OUTPUT_GPIOE_89);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_90
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_90, I2C_ADDR_OUTPUT_GPIOE_90);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_91
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_91, I2C_ADDR_OUTPUT_GPIOE_91);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_92
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_92, I2C_ADDR_OUTPUT_GPIOE_92);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_93
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_93, I2C_ADDR_OUTPUT_GPIOE_93);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_94
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_94, I2C_ADDR_OUTPUT_GPIOE_94);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_95
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_95, I2C_ADDR_OUTPUT_GPIOE_95);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_96
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_96, I2C_ADDR_OUTPUT_GPIOE_96);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_97
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_97, I2C_ADDR_OUTPUT_GPIOE_97);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_98
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_98, I2C_ADDR_OUTPUT_GPIOE_98);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_99
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_99, I2C_ADDR_OUTPUT_GPIOE_99);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_100
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_100, I2C_ADDR_OUTPUT_GPIOE_100);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_101
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_101, I2C_ADDR_OUTPUT_GPIOE_101);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_102
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_102, I2C_ADDR_OUTPUT_GPIOE_102);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_103
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_103, I2C_ADDR_OUTPUT_GPIOE_103);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_104
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_104, I2C_ADDR_OUTPUT_GPIOE_104);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_105
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_105, I2C_ADDR_OUTPUT_GPIOE_105);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_106
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_106, I2C_ADDR_OUTPUT_GPIOE_106);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_107
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_107, I2C_ADDR_OUTPUT_GPIOE_107);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_108
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_108, I2C_ADDR_OUTPUT_GPIOE_108);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_109
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_109, I2C_ADDR_OUTPUT_GPIOE_109);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_110
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_110, I2C_ADDR_OUTPUT_GPIOE_110);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_111
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_111, I2C_ADDR_OUTPUT_GPIOE_111);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_112
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_112, I2C_ADDR_OUTPUT_GPIOE_112);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_113
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_113, I2C_ADDR_OUTPUT_GPIOE_113);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_114
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_114, I2C_ADDR_OUTPUT_GPIOE_114);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_115
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_115, I2C_ADDR_OUTPUT_GPIOE_115);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_116
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_116, I2C_ADDR_OUTPUT_GPIOE_116);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_117
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_117, I2C_ADDR_OUTPUT_GPIOE_117);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_118
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_118, I2C_ADDR_OUTPUT_GPIOE_118);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_119
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_119, I2C_ADDR_OUTPUT_GPIOE_119);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_120
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_120, I2C_ADDR_OUTPUT_GPIOE_120);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_121
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_121, I2C_ADDR_OUTPUT_GPIOE_121);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_122
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_122, I2C_ADDR_OUTPUT_GPIOE_122);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_123
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_123, I2C_ADDR_OUTPUT_GPIOE_123);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_124
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_124, I2C_ADDR_OUTPUT_GPIOE_124);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_125
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_125, I2C_ADDR_OUTPUT_GPIOE_125);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_126
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_126, I2C_ADDR_OUTPUT_GPIOE_126);
    #endif

    #ifdef SatIO_USE_GPIOPE_OUTPUT_127
    queryGPIOPortExpanderInfo(GPIOPE_OUTPUT_127, I2C_ADDR_OUTPUT_GPIOE_127);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_0
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_0, I2C_ADDR_INPUT_GPIOE_0);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_1
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_1, I2C_ADDR_INPUT_GPIOE_1);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_2
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_2, I2C_ADDR_INPUT_GPIOE_2);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_3
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_3, I2C_ADDR_INPUT_GPIOE_3);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_4
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_4, I2C_ADDR_INPUT_GPIOE_4);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_5
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_5, I2C_ADDR_INPUT_GPIOE_5);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_6
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_6, I2C_ADDR_INPUT_GPIOE_6);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_7
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_7, I2C_ADDR_INPUT_GPIOE_7);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_8
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_8, I2C_ADDR_INPUT_GPIOE_8);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_9
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_9, I2C_ADDR_INPUT_GPIOE_9);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_10
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_10, I2C_ADDR_INPUT_GPIOE_10);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_11
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_11, I2C_ADDR_INPUT_GPIOE_11);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_12
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_12, I2C_ADDR_INPUT_GPIOE_12);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_13
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_13, I2C_ADDR_INPUT_GPIOE_13);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_14
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_14, I2C_ADDR_INPUT_GPIOE_14);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_15
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_15, I2C_ADDR_INPUT_GPIOE_15);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_16
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_16, I2C_ADDR_INPUT_GPIOE_16);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_17
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_17, I2C_ADDR_INPUT_GPIOE_17);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_18
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_18, I2C_ADDR_INPUT_GPIOE_18);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_19
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_19, I2C_ADDR_INPUT_GPIOE_19);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_20
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_20, I2C_ADDR_INPUT_GPIOE_20);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_21
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_21, I2C_ADDR_INPUT_GPIOE_21);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_22
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_22, I2C_ADDR_INPUT_GPIOE_22);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_23
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_23, I2C_ADDR_INPUT_GPIOE_23);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_24
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_24, I2C_ADDR_INPUT_GPIOE_24);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_25
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_25, I2C_ADDR_INPUT_GPIOE_25);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_26
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_26, I2C_ADDR_INPUT_GPIOE_26);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_27
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_27, I2C_ADDR_INPUT_GPIOE_27);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_28
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_28, I2C_ADDR_INPUT_GPIOE_28);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_29
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_29, I2C_ADDR_INPUT_GPIOE_29);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_30
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_30, I2C_ADDR_INPUT_GPIOE_30);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_31
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_31, I2C_ADDR_INPUT_GPIOE_31);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_32
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_32, I2C_ADDR_INPUT_GPIOE_32);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_33
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_33, I2C_ADDR_INPUT_GPIOE_33);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_34
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_34, I2C_ADDR_INPUT_GPIOE_34);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_35
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_35, I2C_ADDR_INPUT_GPIOE_35);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_36
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_36, I2C_ADDR_INPUT_GPIOE_36);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_37
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_37, I2C_ADDR_INPUT_GPIOE_37);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_38
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_38, I2C_ADDR_INPUT_GPIOE_38);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_39
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_39, I2C_ADDR_INPUT_GPIOE_39);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_40
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_40, I2C_ADDR_INPUT_GPIOE_40);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_41
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_41, I2C_ADDR_INPUT_GPIOE_41);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_42
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_42, I2C_ADDR_INPUT_GPIOE_42);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_43
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_43, I2C_ADDR_INPUT_GPIOE_43);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_44
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_44, I2C_ADDR_INPUT_GPIOE_44);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_45
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_45, I2C_ADDR_INPUT_GPIOE_45);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_46
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_46, I2C_ADDR_INPUT_GPIOE_46);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_47
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_47, I2C_ADDR_INPUT_GPIOE_47);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_48
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_48, I2C_ADDR_INPUT_GPIOE_48);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_49
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_49, I2C_ADDR_INPUT_GPIOE_49);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_50
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_50, I2C_ADDR_INPUT_GPIOE_50);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_51
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_51, I2C_ADDR_INPUT_GPIOE_51);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_52
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_52, I2C_ADDR_INPUT_GPIOE_52);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_53
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_53, I2C_ADDR_INPUT_GPIOE_53);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_54
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_54, I2C_ADDR_INPUT_GPIOE_54);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_55
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_55, I2C_ADDR_INPUT_GPIOE_55);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_56
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_56, I2C_ADDR_INPUT_GPIOE_56);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_57
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_57, I2C_ADDR_INPUT_GPIOE_57);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_58
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_58, I2C_ADDR_INPUT_GPIOE_58);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_59
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_59, I2C_ADDR_INPUT_GPIOE_59);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_60
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_60, I2C_ADDR_INPUT_GPIOE_60);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_61
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_61, I2C_ADDR_INPUT_GPIOE_61);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_62
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_62, I2C_ADDR_INPUT_GPIOE_62);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_63
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_63, I2C_ADDR_INPUT_GPIOE_63);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_64
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_64, I2C_ADDR_INPUT_GPIOE_64);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_65
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_65, I2C_ADDR_INPUT_GPIOE_65);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_66
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_66, I2C_ADDR_INPUT_GPIOE_66);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_67
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_67, I2C_ADDR_INPUT_GPIOE_67);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_68
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_68, I2C_ADDR_INPUT_GPIOE_68);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_69
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_69, I2C_ADDR_INPUT_GPIOE_69);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_70
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_70, I2C_ADDR_INPUT_GPIOE_70);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_71
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_71, I2C_ADDR_INPUT_GPIOE_71);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_72
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_72, I2C_ADDR_INPUT_GPIOE_72);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_73
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_73, I2C_ADDR_INPUT_GPIOE_73);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_74
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_74, I2C_ADDR_INPUT_GPIOE_74);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_75
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_75, I2C_ADDR_INPUT_GPIOE_75);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_76
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_76, I2C_ADDR_INPUT_GPIOE_76);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_77
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_77, I2C_ADDR_INPUT_GPIOE_77);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_78
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_78, I2C_ADDR_INPUT_GPIOE_78);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_79
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_79, I2C_ADDR_INPUT_GPIOE_79);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_80
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_80, I2C_ADDR_INPUT_GPIOE_80);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_81
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_81, I2C_ADDR_INPUT_GPIOE_81);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_82
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_82, I2C_ADDR_INPUT_GPIOE_82);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_83
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_83, I2C_ADDR_INPUT_GPIOE_83);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_84
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_84, I2C_ADDR_INPUT_GPIOE_84);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_85
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_85, I2C_ADDR_INPUT_GPIOE_85);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_86
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_86, I2C_ADDR_INPUT_GPIOE_86);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_87
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_87, I2C_ADDR_INPUT_GPIOE_87);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_88
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_88, I2C_ADDR_INPUT_GPIOE_88);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_89
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_89, I2C_ADDR_INPUT_GPIOE_89);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_90
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_90, I2C_ADDR_INPUT_GPIOE_90);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_91
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_91, I2C_ADDR_INPUT_GPIOE_91);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_92
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_92, I2C_ADDR_INPUT_GPIOE_92);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_93
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_93, I2C_ADDR_INPUT_GPIOE_93);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_94
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_94, I2C_ADDR_INPUT_GPIOE_94);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_95
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_95, I2C_ADDR_INPUT_GPIOE_95);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_96
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_96, I2C_ADDR_INPUT_GPIOE_96);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_97
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_97, I2C_ADDR_INPUT_GPIOE_97);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_98
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_98, I2C_ADDR_INPUT_GPIOE_98);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_99
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_99, I2C_ADDR_INPUT_GPIOE_99);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_100
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_100, I2C_ADDR_INPUT_GPIOE_100);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_101
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_101, I2C_ADDR_INPUT_GPIOE_101);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_102
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_102, I2C_ADDR_INPUT_GPIOE_102);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_103
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_103, I2C_ADDR_INPUT_GPIOE_103);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_104
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_104, I2C_ADDR_INPUT_GPIOE_104);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_105
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_105, I2C_ADDR_INPUT_GPIOE_105);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_106
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_106, I2C_ADDR_INPUT_GPIOE_106);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_107
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_107, I2C_ADDR_INPUT_GPIOE_107);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_108
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_108, I2C_ADDR_INPUT_GPIOE_108);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_109
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_109, I2C_ADDR_INPUT_GPIOE_109);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_110
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_110, I2C_ADDR_INPUT_GPIOE_110);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_111
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_111, I2C_ADDR_INPUT_GPIOE_111);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_112
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_112, I2C_ADDR_INPUT_GPIOE_112);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_113
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_113, I2C_ADDR_INPUT_GPIOE_113);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_114
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_114, I2C_ADDR_INPUT_GPIOE_114);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_115
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_115, I2C_ADDR_INPUT_GPIOE_115);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_116
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_116, I2C_ADDR_INPUT_GPIOE_116);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_117
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_117, I2C_ADDR_INPUT_GPIOE_117);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_118
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_118, I2C_ADDR_INPUT_GPIOE_118);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_119
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_119, I2C_ADDR_INPUT_GPIOE_119);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_120
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_120, I2C_ADDR_INPUT_GPIOE_120);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_121
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_121, I2C_ADDR_INPUT_GPIOE_121);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_122
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_122, I2C_ADDR_INPUT_GPIOE_122);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_123
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_123, I2C_ADDR_INPUT_GPIOE_123);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_124
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_124, I2C_ADDR_INPUT_GPIOE_124);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_125
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_125, I2C_ADDR_INPUT_GPIOE_125);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_126
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_126, I2C_ADDR_INPUT_GPIOE_126);
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_127
    queryGPIOPortExpanderInfo(GPIOPE_INPUT_127, I2C_ADDR_INPUT_GPIOE_127);
    #endif
    
    /** ----------------------------------------------------------------------------
     * Serial1: GPS UART.
     *
     * (1) setPins()'s and setRxBufferSize()'s results are not needed here,
     *     so each discard is made explicit with a (void) cast (MISRA C 2012
     *     Rule 17.7).
     * (2) The while-loop body is an explicit (empty) compound statement
     *     rather than a bare semicolon (MISRA C 2012 Rule 15.6).
     * (3) Brings up the UART used by the GPS receiver: GPIO34 for RX, TX
     *     unused because the GPS module is read-only from this system's
     *     perspective.
     */
    #ifdef SatIO_USE_GPS_0
    printf("Serial1 (GPS) starting");
    const int8_t pin_not_used               = -1;
    const int8_t gps_uart_rxd_pin           = 34;
    const int8_t gps_uart_txd_pin           = pin_not_used;
    const size_t gps_uart_rx_buffer_size    = 2000U;
    const unsigned long gps_uart_timeout_ms = 10UL;
    const uint32_t gps_uart_baud_rate       = 115200U;
    (void)Serial1.setPins(gps_uart_rxd_pin, gps_uart_txd_pin, pin_not_used, pin_not_used);
    (void)Serial1.setRxBufferSize(gps_uart_rx_buffer_size);
    Serial1.setTimeout(gps_uart_timeout_ms);
    Serial1.begin(gps_uart_baud_rate);
    while (!Serial1) {
        // Block until the UART peripheral reports ready.
    }
    Serial1.flush();
    printf("Serial1 baud rate: %lu", (unsigned long)gps_uart_baud_rate);
    printf("Serial1 hardware remap: RX=%d TX=%d", gps_uart_rxd_pin, gps_uart_txd_pin);
    #endif
    // Full ~0-3.3V input range; applies to every ADC channel.
    analogSetAttenuation(ADC_11db);
    // ----------------------------------------------------------------------------
    // Create Tasks.
    // ----------------------------------------------------------------------------
    // System Time
    printf("creating system time task");
    createTaskSystemTime();
    
    // Storage
    #ifdef SatIO_USE_STORAGE
    sdcardFlagData.load_system = true;
    printf("creating storage task");
    createTaskStorage(); // (target: 2Hz)
    #endif

    // delay(5000);

    // GPS
    #ifdef SatIO_USE_GPS_0
    printf("creating GPS task");
    createTaskGPS(); // (target: 10Hz)
    #endif

    // Gyro
    #ifdef SatIO_USE_GYRO_0
    initWT901();
    printf("creating gyro task");
    createTaskGyro(); // (target: 200Hz)
    #endif
    
    // Admplex 0
    #ifdef SatIO_CD74HC4067_OPTION_USE_0
    initADMultiplexer(ad_mux_0);
    setReadModeADMultiplexer(ad_mux_0);
    createTaskADMplex0(); // (target: x16 chan >= 250-350Hz, x4+ chan >= 1KHz)  Fast general input
    #endif

    // Admplex 1
    #ifdef SatIO_CD74HC4067_OPTION_USE_1
    initADMultiplexer(ad_mux_1);
    setReadModeADMultiplexer(ad_mux_1);
    createTaskADMplex1(); // (target: x16 chan >= 250-350Hz, x4+ chan >= 1KHz)  Fast general input
    #endif

    // Auxiliary Input
    #ifdef SatIO_USE_GPIOPE_INPUT
    printf("creating auxiliary input task");
    createTaskInputPortController(); // (target: ?) Large general input
    #endif

    // Matrix
    #ifdef SatIO_USE_MATRIX
    printf("creating auxiliary output task");
    createTaskSwitches(); // (target: max 1KHz) Fast general calc -> output
    #endif
    
    // Universe
    #ifdef SatIO_USE_UNIVERSE
    printf("creating universe task");
    myAstroBegin();
    createTaskUniverse(); // (target: +1Hz)
    #endif

    // SatIO Serial Tx
    #ifdef SatIO_SERIAL_TX_OPTION_NEW_TASK
    printf("creating SatIO serial tx task");
    createTaskSatIOSerialTx(); // (target: >= 200Hz)
    #endif

    // Attempt to approximately synchronize tasks
    printf("attempting to synchronize tasks");
    syncTasks();
    // ESP_LOGI(APP_MAIN_TAG, "waiting for tasks to settle");
    // const uint32_t task_settle_delay_ms = 5000U; // Gives every task time for a first pass before the UI starts.
    // delay(task_settle_delay_ms);
    // Display
    #ifdef SatIO_USE_DISPLAY
    printf("starting SatIO UI");
    flag_display_home_screen = true;
    createTaskDisplayUpdate();
    #endif
    // app_main() may now return: every task created above keeps running
    // under the FreeRTOS scheduler, and the ESP-IDF idle task takes over
    // this thread.
}
