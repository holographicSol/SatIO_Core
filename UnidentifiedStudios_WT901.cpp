/*
    WT901 Library. Written by Benjamin Jack Cullen.

    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#include "UnidentifiedStudios_WT901.h"
#include <Arduino.h>
#include "wit_c_sdk.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>

// UART2 configuration for WT901
#define WT901_UART_NUM    UART_NUM_2
#define WT901_UART_TX_PIN 36
#define WT901_UART_RX_PIN 35
#define WT901_BUF_SIZE    1024

static const char *WT901_TAG = "WT901";
static bool wt901_uart_installed = false;

struct GyroData gyroData = {
  .gyro_0_s_cDataUpdate = 0,
  .gyro_0_fAcc = {0.0f, 0.0f, 0.0f},
  .gyro_0_fGyro = {0.0f, 0.0f, 0.0f},
  .gyro_0_fAngle = {0.0f, 0.0f, 0.0f},
  .gyro_0_ang_x = 0.0f,
  .gyro_0_ang_y = 0.0f,
  .gyro_0_ang_z = 0.0f,
  .gyro_0_acc_x = 0.0f,
  .gyro_0_acc_y = 0.0f,
  .gyro_0_acc_z = 0.0f,
  .gyro_0_gyr_x = 0.0f,
  .gyro_0_gyr_y = 0.0f,
  .gyro_0_gyr_z = 0.0f,
  .gyro_0_mag_x = 0,
  .gyro_0_mag_y = 0,
  .gyro_0_mag_z = 0,
  .gyro_0_c_uiBaud={
    0,      // 0 (unused)
    4800,   // 1 WIT_BAUD_4800
    9600,   // 2 WIT_BAUD_9600
    19200,  // 3 WIT_BAUD_19200
    38400,  // 4 WIT_BAUD_38400
    57600,  // 5 WIT_BAUD_57600
    115200, // 6 WIT_BAUD_115200
    230400, // 7 WIT_BAUD_230400
    460800, // 8 WIT_BAUD_460800
    921600  // 9 WIT_BAUD_921600
  },
  .gyro_0_current_uiBaud = 0
};

/* Rule 8.7: internal linkage; only wt901_uart_init() and Gyro0AutoScan()
   need this within this translation unit. */
static void wt901_uart_init(int32_t baud_rate)
{
    if (wt901_uart_installed == true)
    {
        /* Driver already installed: changing baud rate only requires
           reconfiguring it, not reinstalling it. */
        uart_set_baudrate(WT901_UART_NUM, baud_rate);
        uart_flush_input(WT901_UART_NUM);
        ESP_LOGI(WT901_TAG, "UART2 baud rate changed to %ld", baud_rate);
    }
    else
    {
        uart_config_t uart_config = {
            .baud_rate = baud_rate,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .rx_flow_ctrl_thresh = 0,
            .source_clk = UART_SCLK_DEFAULT,
            .flags = {},
        };

        ESP_ERROR_CHECK(uart_param_config(WT901_UART_NUM, &uart_config));
        ESP_ERROR_CHECK(uart_set_pin(WT901_UART_NUM, WT901_UART_TX_PIN, WT901_UART_RX_PIN,
                                      UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
        ESP_ERROR_CHECK(uart_driver_install(WT901_UART_NUM, WT901_BUF_SIZE * 2, WT901_BUF_SIZE, 0, NULL, 0));
        wt901_uart_installed = true;
        ESP_LOGI(WT901_TAG, "UART2 initialized at %d baud", baud_rate);
    }
}

/* Rule 8.7: internal linkage; only this file reads from the WT901 UART. */
static int wt901_uart_read(uint8_t *buf, size_t max_len)
{
    int len = 0;
    size_t available = 0;

    uart_get_buffered_data_len(WT901_UART_NUM, &available);
    if (available > 0U)
    {
        len = uart_read_bytes(WT901_UART_NUM, buf, (available < max_len) ? available : max_len, 0);
    }

    return len; /* Rule 15.5: single point of exit */
}

bool readGyro(void)
{
    uint8_t buf[128];
    int len;
    bool updated = false;

    /* Feed every byte read this call into the vendor SDK's protocol
       decoder; it updates gyro_0_s_cDataUpdate and the sReg[] register
       cache as complete frames are recognised. */
    len = wt901_uart_read(buf, sizeof(buf));
    for (int i = 0; i < len; i++)
    {
        WitSerialDataIn(buf[i]);
    }

    if (gyroData.gyro_0_s_cDataUpdate != 0U)
    {
        for (int i = 0; i < 3; i++)
        {
            gyroData.gyro_0_fGyro[i] = sReg[GX + i] / 32768.0f * 2000.0f;
            gyroData.gyro_0_fAngle[i] = sReg[Roll + i] / 32768.0f * 180.0f;
            gyroData.gyro_0_fAcc[i] = sReg[AX + i] / 32768.0f * 16.0f;

        }
        // printf("acc: x=%f y=%f z=%f\n", gyroData.gyro_0_fAcc[0], gyroData.gyro_0_fAcc[1], gyroData.gyro_0_fAcc[2]);
        // printf("ang: x=%f y=%f z=%f\n", gyroData.gyro_0_fAngle[0], gyroData.gyro_0_fAngle[1], gyroData.gyro_0_fAngle[2]);
        // printf("gyr: x=%f y=%f z=%f\n", gyroData.gyro_0_fGyro[0], gyroData.gyro_0_fGyro[1], gyroData.gyro_0_fGyro[2]);

        gyroData.gyro_0_acc_x = gyroData.gyro_0_fAcc[0];
        gyroData.gyro_0_acc_y = gyroData.gyro_0_fAcc[1];
        gyroData.gyro_0_acc_z = gyroData.gyro_0_fAcc[2];

        gyroData.gyro_0_ang_x = gyroData.gyro_0_fAngle[0];
        gyroData.gyro_0_ang_y = gyroData.gyro_0_fAngle[1];
        gyroData.gyro_0_ang_z = gyroData.gyro_0_fAngle[2];

        gyroData.gyro_0_gyr_x = gyroData.gyro_0_fGyro[0];
        gyroData.gyro_0_gyr_y = gyroData.gyro_0_fGyro[1];
        gyroData.gyro_0_gyr_z = gyroData.gyro_0_fGyro[2];

        gyroData.gyro_0_mag_x = sReg[HX];
        gyroData.gyro_0_mag_y = sReg[HY];
        gyroData.gyro_0_mag_z = sReg[REG_HZ];

        // ----------------------------------------------------------------------------------------------------
        // Time consuming block. This may be useful in the future if we wish to itemize update true.
        // ----------------------------------------------------------------------------------------------------
        // if ((gyroData.gyro_0_s_cDataUpdate & GYRO_0_ACC_UPDATE) != 0U)
        // {
        //     gyroData.gyro_0_s_cDataUpdate = (uint8_t)(gyroData.gyro_0_s_cDataUpdate & ~GYRO_0_ACC_UPDATE);
        //     gyroData.gyro_0_acc_x = gyroData.gyro_0_fAcc[0];
        //     gyroData.gyro_0_acc_y = gyroData.gyro_0_fAcc[1];
        //     gyroData.gyro_0_acc_z = gyroData.gyro_0_fAcc[2];
        //     updated = true;
        // }

        // if ((gyroData.gyro_0_s_cDataUpdate & GYRO_0_ANGLE_UPDATE) != 0U)
        // {
        //     gyroData.gyro_0_s_cDataUpdate = (uint8_t)(gyroData.gyro_0_s_cDataUpdate & ~GYRO_0_ANGLE_UPDATE);
        //     gyroData.gyro_0_ang_x = gyroData.gyro_0_fAngle[0];
        //     gyroData.gyro_0_ang_y = gyroData.gyro_0_fAngle[1];
        //     gyroData.gyro_0_ang_z = gyroData.gyro_0_fAngle[2];
        //     updated = true;
        // }

        // if ((gyroData.gyro_0_s_cDataUpdate & GYRO_0_UPDATE) != 0U)
        // {
        //     gyroData.gyro_0_s_cDataUpdate = (uint8_t)(gyroData.gyro_0_s_cDataUpdate & ~GYRO_0_UPDATE);
        //     gyroData.gyro_0_gyr_x = gyroData.gyro_0_fGyro[0];
        //     gyroData.gyro_0_gyr_y = gyroData.gyro_0_fGyro[1];
        //     gyroData.gyro_0_gyr_z = gyroData.gyro_0_fGyro[2];
        //     updated = true;
        // }

        // if ((gyroData.gyro_0_s_cDataUpdate & GYRO_0_MAG_UPDATE) != 0U)
        // {
        //     gyroData.gyro_0_s_cDataUpdate = (uint8_t)(gyroData.gyro_0_s_cDataUpdate & ~GYRO_0_MAG_UPDATE);
        //     gyroData.gyro_0_mag_x = sReg[HX];
        //     gyroData.gyro_0_mag_y = sReg[HY];
        //     gyroData.gyro_0_mag_z = sReg[REG_HZ];
        //     updated = true;
        // }
        // ----------------------------------------------------------------------------------------------------
        updated = true;
    }
    return updated; /* Rule 15.5: single point of exit */
}

void Gyro0UartSend(uint8_t *p_data, uint32_t uiSize)
{
    printf("[Gyro0] TX: ");
    for (uint32_t i = 0; i < uiSize; i++)
    {
        printf("%02X ", p_data[i]);
    }
    printf("\n");
    uart_write_bytes(WT901_UART_NUM, (const char *)p_data, uiSize);
    uart_wait_tx_done(WT901_UART_NUM, 1000 / portTICK_PERIOD_MS);
}

void Gyro0Delayms(uint16_t ucMs)
{
    delay(ucMs);
}

void Gyro0DataUpdata(uint32_t uiReg, uint32_t uiRegNum)
{
    /* uiReg is an accumulator counting up through the registers the SDK
       just received, not the loop counter; only i controls this loop
       (Rule 14.2). */
    for (unsigned int i = 0; i < uiRegNum; i++)
    {
        switch (uiReg)
        {
            case AZ:
                gyroData.gyro_0_s_cDataUpdate |= GYRO_0_ACC_UPDATE;
                break;
            case GZ:
                gyroData.gyro_0_s_cDataUpdate |= GYRO_0_UPDATE;
                break;
            case REG_HZ:
                gyroData.gyro_0_s_cDataUpdate |= GYRO_0_MAG_UPDATE;
                break;
            case Yaw:
                gyroData.gyro_0_s_cDataUpdate |= GYRO_0_ANGLE_UPDATE;
                break;
            default:
                gyroData.gyro_0_s_cDataUpdate |= GYRO_0_READ_UPDATE;
                break;
        }
        uiReg++;
    }
}

void testWT901(void)
{
    unsigned long start;
    int count = 0;

    delay(500); /* let the gyro stabilize */
    printf("[Gyro0] Testing raw data...\n");

    /* unsigned subtraction wraps correctly even if millis() overflows
       mid-test, so this bound holds regardless of when start was taken. */
    start = millis();
    while ((millis() - start) < 2000UL)
    {
        uint8_t b;

        if (wt901_uart_read(&b, 1) > 0)
        {
            printf("[Gyro0] Raw: 0x%02X\n", b);
            if (b == 0x55U)
            {
                count++;
            }
        }
    }

    printf("[Gyro0] 0x55 packets seen: %d\n", count);
}

void Gyro0AutoScan(void)
{
    unsigned int i;
    bool found = false;

    /* Start at index 1 to skip the unused index-0 entry in gyro_0_c_uiBaud.
       Rule 15.4/15.5: no break or early return — `found` is the single
       point of control for both this loop and the retry loop below. */
    for (i = 1U; (i < (unsigned int)MAX_GYRO_BAUDRATES) && (found == false); i++)
    {
        unsigned int retries_remaining;

        printf("[Gyro0] Trying baud rate: %ld\n", gyroData.gyro_0_c_uiBaud[i]);

        wt901_uart_init(gyroData.gyro_0_c_uiBaud[i]);
        uart_flush_input(WT901_UART_NUM);

        gyroData.gyro_0_s_cDataUpdate = 0;
        retries_remaining = 2U;

        do
        {
            uint8_t buf[128];
            int len;

            (void)WitReadReg(AX, 3); /* request data; return value not needed here */
            delay(200);              /* let a reply arrive */

            len = wt901_uart_read(buf, sizeof(buf));
            for (int j = 0; j < len; j++)
            {
                WitSerialDataIn(buf[j]);
            }

            if (gyroData.gyro_0_s_cDataUpdate != 0U)
            {
                printf("[Gyro0] Found baud rate: %ld\n", gyroData.gyro_0_c_uiBaud[i]);
                gyroData.gyro_0_current_uiBaud = gyroData.gyro_0_c_uiBaud[i];
                found = true;
            }
            else
            {
                retries_remaining--;
            }
        }
        while ((found == false) && (retries_remaining != 0U));
    }

    if (found == false)
    {
        printf("[Gyro0] Sensor not found (check connection).\n");
    }
}

void initWT901(void)
{
    int desired_baud;

    printf("[Gyro0] initializing...\n");
    (void)WitInit(WIT_PROTOCOL_NORMAL, 0x50);

    printf("[Gyro0] register serial write.\n");
    (void)WitSerialWriteRegister(Gyro0UartSend);

    printf("[Gyro0] register call back.\n");
    (void)WitRegisterCallBack(Gyro0DataUpdata);

    printf("[Gyro0] register delay\n");
    (void)WitDelayMsRegister(Gyro0Delayms);

    printf("[Gyro0] performing baud rate autoscan...\n");
    Gyro0AutoScan();
    printf("[Gyro0] current baudrate: %ld\n", gyroData.gyro_0_current_uiBaud);

    desired_baud = 230400;
    if (gyroData.gyro_0_current_uiBaud != desired_baud)
    {
        printf("[Gyro0] changing baud rate to: %d\n", desired_baud);

        printf("[Gyro0] Unlocking registers...\n");
        if (WitWriteReg(KEY, KEY_UNLOCK) != WIT_HAL_OK)
        {
            printf("[Gyro0] Error unlocking registers.\n");
        }
        delay(100);

        printf("[Gyro0] Writing BAUD register (0x04) with value %d...\n", WIT_BAUD_230400);
        if (WitWriteReg(BAUD, WIT_BAUD_230400) != WIT_HAL_OK)
        {
            printf("[Gyro0] Error writing baud rate.\n");
        }
        delay(100);

        printf("[Gyro0] Saving to flash...\n");
        if (WitWriteReg(SAVE, SAVE_PARAM) != WIT_HAL_OK)
        {
            printf("[Gyro0] Error saving settings.\n");
        }
        else
        {
            printf("[Gyro0] Settings saved to flash.\n");
        }

        printf("[Gyro0] Waiting for sensor to apply new baud...\n");
        delay(1000); /* let the sensor restart at the new baud rate */

        wt901_uart_init(gyroData.gyro_0_c_uiBaud[WIT_BAUD_230400]);
        delay(200);

        printf("[Gyro0] performing baud rate autoscan...\n");
        Gyro0AutoScan();
        printf("[Gyro0] current baudrate: %ld\n", gyroData.gyro_0_current_uiBaud);
    }
    else
    {
        printf("[Gyro0] no need to change baudrate.\n");
    }

    if (WitSetOutputRate(RRATE_200HZ) != WIT_HAL_OK)
    {
        printf("[Gyro0] Error setting return rate. (RRATE_200HZ)\n");
    }
    else
    {
        printf("[Gyro0] Return rate modified successfully (RRATE_200HZ)\n");
    }

    if (WitSetBandwidth(BANDWIDTH_256HZ) != WIT_HAL_OK)
    {
        printf("[Gyro0] Error setting bandwidth (BANDWIDTH_256HZ).\n");
    }
    else
    {
        printf("[Gyro0] Bandwidth modified successfully (BANDWIDTH_256HZ)\n");
    }
}

void WT901CalAcc(void)
{
    if (WitStartAccCali() != WIT_HAL_OK)
    {
        printf("error calibrating gyro0: acceleration\n");
    }
}

void WT901CalMagStart(void)
{
    if (WitStartMagCali() != WIT_HAL_OK)
    {
        printf("error calibrating gyro0: mag cal start\n");
    }
}

void WT901CalMagEnd(void)
{
    if (WitStopMagCali() != WIT_HAL_OK)
    {
        printf("error calibrating gyro0: mag cal end\n");
    }
}
