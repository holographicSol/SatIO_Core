/*
  SDCard Helper - Written By Benjamin Jack Cullen.
  Requirements:
    (1) Modified SD_MMC library to release LDO on end().
    (2) Optional: Modified ffconf library for exFat support (FAT only by default).
  
  Steps for intended use (looping on a task):
  (1) Firstly, in loop call sdcardBegin() to initialize, card detect, automatically mount/unmount.
  (2) Secondly, in loop call sdcardFlagHandler() to actually utilize the card.
  (3) Optionally (lastly) in loop call sdcardSleepMode0() to save power during intervals.

  - sdcardEnd() is handled internally when required in various scenarios.

  Intended to be MISRA Compliant (untested, unverified, in-progress).

*/

#ifndef SDCARD_HELPER_H
#define SDCARD_HELPER_H

#include <stdint.h>
#include "sd_protocol_types.h"
#include "lvgl.h"
#include <sys/time.h>
#include <rtc_wdt.h>
#include <esp_task_wdt.h>
#include "esp_pm.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_partition.h"
#include "esp_spiffs.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "driver/uart.h"
#include "UnidentifiedStudios_Config.h"

#define MAX_CARD_TYPES 5
/**
 * Defaults are for ESP32-P4.
 */
#define SD_CLK 43
#define SD_CMD 44
#define SD_D0  39
#define SD_D1  40
#define SD_D2  41
#define SD_D3  42
#define SD_PWR 45
#define SD_TEST_RW false
#define SD_SET_PINS false
#define SD_1BITMODE false
#define SD_FORMAT_ON_FAIL false
#define SD_FREQ BOARD_MAX_SDMMC_FREQ
#define SD_MAX_OPEN_FILES 5

/**
 * @struct SDCardStruct
 */
struct SDCardStruct {
    bool sdcard_inserted;
    bool allow_mount;
    bool sdcard_mounted;
    // char sdcard_mountpoint[56];
    // uint8_t sdcard_type;
    // char sdcard_type_names[MAX_CARD_TYPES][12];
    // uint64_t sdcard_card_size;
    // uint64_t sdcard_total_bytes;
    // uint64_t sdcard_used_bytes;
    // int sdcard_sector_size;
    // sdmmc_card_t *card;
    // sdmmc_host_t host;
    // sdmmc_slot_config_t slot_config;
    // sd_pwr_ctrl_ldo_config_t ldo_config;
};
extern struct SDCardStruct sdcardData;

/**
 * @struct sdcardFlagStruct
 */
struct sdcardFlagStruct {
  bool no_delay_flag;

  // 0: null 1: false 2: ok
  int success_flag;

  bool rw_failed;
  bool rw_running;

  bool mount_sdcard_flag;
  bool unmount_sdcard_flag;

  bool list_dir_flag;

  bool save_mapping;
  bool load_mapping;
  bool delete_mapping;

  bool save_matrix;
  bool load_matrix;
  bool delete_matrix;

  bool save_system;
  bool load_system;
  bool delete_system;

  bool write_log;
};
extern struct sdcardFlagStruct sdcardFlagData;

/** ----------------------------------------------------------------------------------------
 * @brief Get the size of a file in bytes.
 * 
 * @param filename Path to the file on the SD card.
 * @return Size of the file in bytes, or 0 if the file cannot be accessed.
 */
uint32_t get_file_size(const char* filename);

/** ----------------------------------------------------------------------------------------
 * @brief Open a file.
 * 
 * @param drv Pointer to the LVGL file system driver (not used in this implementation).
 * @param path Path to the file to open.
 * @param mode File open mode (read, write, etc.).
 * @return Pointer to the opened file object, or NULL on failure.
 */
void * sd_file_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode);

/** ----------------------------------------------------------------------------------------
 * @brief Close the file.
 * 
 * @param drv Pointer to the LVGL file system driver (not used in this implementation).
 * @param file_p Pointer to the file object (cast to FILE*).
 * @return LV_FS_RES_OK.
 * 
 */
lv_fs_res_t sd_file_close(lv_fs_drv_t * drv, void * file_p);

/** ----------------------------------------------------------------------------------------
 * @brief Read from file into buffer.
 * 
 * Reads up to btr bytes from the file into the provided buffer and sets br to the number of
 * bytes actually read.
 * 
 * @param drv Pointer to the LVGL file system driver (not used in this implementation).
 * @param file_p Pointer to the file object (cast to FILE*).
 * @param buf Pointer to the buffer where the read data should be stored.
 * @param btr Number of bytes to read from the file.
 * @param br Pointer to a variable where the actual number of bytes read should be stored.
 * @return LV_FS_RES_OK if the read was successful (bytes read > 0 or end of file reached),
 *         otherwise LV_FS_RES_UNKNOWN.
 */
lv_fs_res_t sd_file_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br);

/** ----------------------------------------------------------------------------------------
 * @brief Seek to a specific position in the file.
*/
lv_fs_res_t sd_file_seek(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence);

/** ----------------------------------------------------------------------------------------
 * @brief Get current file position in bytes.
 */
lv_fs_res_t sd_file_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p);

/** ----------------------------------------------------------------------------------------
 * @brief Open a directory.
 * 
 * @param drv Pointer to the LVGL file system driver (not used in this implementation).
 * @param path Path to the directory to open.
 * @return Pointer to the opened directory object, or NULL on failure.
 */
void * sd_dir_open(lv_fs_drv_t * drv, const char * path);

/** ----------------------------------------------------------------------------------------
 * @brief Read the next directory entry.
 */
lv_fs_res_t sd_dir_read(lv_fs_drv_t * drv, void * dir_p, char * fn, uint32_t btr);

/** ----------------------------------------------------------------------------------------
 * @brief Close the directory.
 * 
 * @param drv Pointer to the LVGL file system driver (not used in this implementation).
 * @param dir_p Pointer to the directory object (cast to DIR*).
 * @return LV_FS_RES_OK.
 */
lv_fs_res_t sd_dir_close(lv_fs_drv_t * drv, void * dir_p);

/** ----------------------------------------------------------------------------------------
 * @brief Load file from SD card into PSRAM and return pointer.
 * 
 * @param filename Path to the file on the SD card.
 * @param f_size Size of the file in bytes.
 * @return Pointer to the file data in PSRAM, or NULL on failure.
 * 
 */
uint8_t * load_file_bytes_to_psram(const char * filename, uint32_t f_size);

/** ----------------------------------------------------------------------------------------
 * @brief Mount the SD card using the Board Support Package (BSP) function.
 * 
 */
bool mount_sd(void);

/** ----------------------------------------------------------------------------------------
 * @brief Unmount the SD card using the Board Support Package (BSP) function.
 * 
 */
bool unmount_sd(void);

/** ----------------------------------------------------------------------------------------
 * @brief List directory.
 * 
 * @param dirname Path to the directory on the SD card to list.
 * 
 */
void list_directory(const char * dirname);

bool isAvailableBytes(size_t num_bytes);

/** ----------------------------------------------------------------------------------------
 * @brief Check if SD card root directory is ready.
 * 
 * @return true if the SD card root directory can be opened, false otherwise.
 * 
 */
bool is_fs_ready(const char * dirname);

/** ----------------------------------------------------------------------------------------
 * @brief Register LVGL callbacks for SD card file system access.
 * 
 */
void sd_lvgl_register(void);

/** ----------------------------------------------------------------------------------------
 * @brief Mount SD card
 * 
 */
void sdcard_mount(void);

/** ----------------------------------------------------------------------------------------
 * @brief Unmount SD card.
 *
 */
void sdcard_unmount(void);

#endif