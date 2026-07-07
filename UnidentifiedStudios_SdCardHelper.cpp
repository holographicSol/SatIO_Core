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

#include "bsp/esp32_p4_wifi6_touch_lcd_xc.h"
#include <limits.h>
#include "esp_log.h"
#include "lvgl.h"
#include <stdlib.h>       // malloc/free
#include "ff.h"           // FatFs core
#include "diskio.h"       // Disk I/O
#include "diskio_impl.h"  // ESP32 disk impl
#include "esp_vfs_fat.h"  // VFS integration
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "esp_err.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include "bsp/esp-bsp.h"

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
#include "esp_spiffs.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "driver/uart.h"

#include "REG.h"
#include "wit_c_sdk.h"

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
#include "UnidentifiedStudios_Mapping.h"
#include "UnidentifiedStudios_Matrix.h"
#include "UnidentifiedStudios_CMD.h"
#include "UnidentifiedStudios_SystemData.h"
#include "UnidentifiedStudios_SdCardHelper.h"
#include "UnidentifiedStudios_TaskHandler.h"
#include "UnidentifiedStudios_I2C.h"

struct SDCardStruct sdcardData = {
    .sdcard_inserted=false,
    .allow_mount=true,
    .sdcard_mounted=false,
    // .sdcard_mountpoint={0},
    // .sdcard_type=CARD_NONE,
    // .sdcard_type_names = {
    //     "None", "MMC", "SD", "SDHC/SDXC", "Unknown"
    // },
    // .sdcard_card_size=0,
    // .sdcard_total_bytes=0,
    // .sdcard_used_bytes=0,
    // .sdcard_sector_size=0,
    // .card=NULL,
    // .host={},
    // .slot_config={},
    // .ldo_config={},
};

struct sdcardFlagStruct sdcardFlagData = {

  .no_delay_flag=false,

  .success_flag=0,
  .rw_failed=false,
  .rw_running=false,

  .mount_sdcard_flag=false,
  .unmount_sdcard_flag=false,

  .list_dir_flag=false,

  .save_mapping=false,
  .load_mapping=false,
  .delete_mapping=false,

  .save_matrix=false,
  .load_matrix=false,
  .delete_matrix=false,

  .save_system=false,
  .load_system=false,
  .delete_system=false,

  .write_log=false,
};

/** ----------------------------------------------------------------------------------------
 * @brief Get the size of a file in bytes.
 * 
 * @param filename Path to the file on the SD card.
 * @return Size of the file in bytes, or 0 if the file cannot be accessed.
 */
uint32_t get_file_size(const char* filename) {
    /* Rule 15.5: single point of exit via a result variable. */
    struct stat st;
    uint32_t file_size;

    if (stat(filename, &st) == 0) {
        printf("File '%s' size: %lu bytes\n", filename, (uint32_t)st.st_size);
        file_size = (uint32_t)st.st_size;
    }
    else {
        printf("get_file_size: Cannot stat '%s'\n", filename);
        file_size = 0;
    }

    return file_size;
}

/** ----------------------------------------------------------------------------------------
 * @brief Open a file.
 * 
 * @param drv Pointer to the LVGL file system driver (not used in this implementation).
 * @param path Path to the file to open.
 * @param mode File open mode (read, write, etc.).
 * @return Pointer to the opened file object, or NULL on failure.
 */
void * sd_file_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode)
{
    printf("LVGL_FS: Got path '%s'\n", path);

    char full_path[256];
    snprintf(full_path, sizeof(full_path), "/sdcard/%s", path);
    
    printf("LVGL_FS: Full path '%s'\n", full_path);
    
    FILE * f = fopen(full_path, "r");
    if (f == NULL) {
        printf("LVGL_FS: FAILED '%s'\n", full_path);
    } else {
        printf("LVGL_FS: SUCCESS '%s' -> %p\n", full_path, f);
    }
    
    return (void *)f;
}

/** ----------------------------------------------------------------------------------------
 * @brief Close the file.
 * 
 * @param drv Pointer to the LVGL file system driver (not used in this implementation).
 * @param file_p Pointer to the file object (cast to FILE*).
 * @return LV_FS_RES_OK.
 * 
 */
lv_fs_res_t sd_file_close(lv_fs_drv_t * drv, void * file_p)
{
    FILE * f = (FILE *)file_p;
    fclose(f);
    printf("LVGL_FS: Closed\n");
    return LV_FS_RES_OK;
}

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
lv_fs_res_t sd_file_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br)
{
    FILE * f = (FILE *)file_p;
    *br = fread(buf, 1, btr, f);
    // printf("LVGL_FS: Read %ld/%lu\n", *br, btr);
    return (*br > 0 || feof(f)) ? LV_FS_RES_OK : LV_FS_RES_UNKNOWN;
}

/** ----------------------------------------------------------------------------------------
 * @brief Seek to a specific position in the file.
*/
lv_fs_res_t sd_file_seek(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence)
{
    FILE * f = (FILE *)file_p;
    int stdio_whence;
    int res;

    /* a chain of plain if/else if is used instead of nested ?: so each
       whence value is easy to read on its own line. */
    if (whence == LV_FS_SEEK_CUR) {stdio_whence = SEEK_CUR;}
    else if (whence == LV_FS_SEEK_END) {stdio_whence = SEEK_END;}
    else {stdio_whence = SEEK_SET;}

    res = fseek(f, pos, stdio_whence);
    return res == 0 ? LV_FS_RES_OK : LV_FS_RES_UNKNOWN;
}

/** ----------------------------------------------------------------------------------------
 * @brief Get current file position in bytes.
 */
lv_fs_res_t sd_file_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p)
{
    FILE * f = (FILE *)file_p;
    *pos_p = ftell(f);
    return LV_FS_RES_OK;
}

/** ----------------------------------------------------------------------------------------
 * @brief Open a directory.
 * 
 * @param drv Pointer to the LVGL file system driver (not used in this implementation).
 * @param path Path to the directory to open.
 * @return Pointer to the opened directory object, or NULL on failure.
 */
void * sd_dir_open(lv_fs_drv_t * drv, const char * path)
{
    printf("LVGL_FS: Got dir path '%s'\n", path);

    char full_path[256];
    snprintf(full_path, sizeof(full_path), "/sdcard/%s", path);
    
    printf("LVGL_FS: Full dir path '%s'\n", full_path);
    
    DIR * d = opendir(full_path);
    if (d == NULL) {
        printf("LVGL_FS: DIR FAILED '%s'\n", full_path);
    } else {
        printf("LVGL_FS: DIR SUCCESS '%s' -> %p\n", full_path, d);
    }
    
    return (void *)d;
}

/** ----------------------------------------------------------------------------------------
 * @brief Read the next directory entry.
 */
lv_fs_res_t sd_dir_read(lv_fs_drv_t * drv, void * dir_p, char * fn, uint32_t btr)
{
    DIR * d = (DIR *)dir_p;
    struct dirent * entry;
    bool found = false;

    while ((entry = readdir(d)) != NULL) {
        // Skip . and .. entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // btr is max buffer size - use min(entry name len, btr)
        size_t fn_len = strlen(entry->d_name) + 1;
        size_t copy_len = (fn_len < btr) ? fn_len : btr;
        strncpy(fn, entry->d_name, copy_len - 1);
        fn[copy_len - 1] = '\0';

        printf("LVGL_FS: Found '%s'\n", fn);
        found = true;
        break; /* single break in this loop (Rule 15.4) */
    }

    if (found == false) {
        printf("LVGL_FS: End of directory\n");
        *fn = '\0';  // Clear buffer at EOF
    }

    return LV_FS_RES_OK; /* Rule 15.5: single point of exit */
}

/** ----------------------------------------------------------------------------------------
 * @brief Close the directory.
 * 
 * @param drv Pointer to the LVGL file system driver (not used in this implementation).
 * @param dir_p Pointer to the directory object (cast to DIR*).
 * @return LV_FS_RES_OK.
 */
lv_fs_res_t sd_dir_close(lv_fs_drv_t * drv, void * dir_p)
{
    DIR * d = (DIR *)dir_p;
    closedir(d);
    printf("LVGL_FS: Directory closed\n");
    return LV_FS_RES_OK;
}

/** ----------------------------------------------------------------------------------------
 * @brief Load file from SD card into PSRAM and return pointer.
 * 
 * @param filename Path to the file on the SD card.
 * @param f_size Size of the file in bytes.
 * @return Pointer to the file data in PSRAM, or NULL on failure.
 * 
 */
uint8_t * load_file_bytes_to_psram(const char * filename, uint32_t f_size) {
    /* Rule 15.5: single point of exit; proceed guards the read once
       allocation has been confirmed instead of returning early. */
    uint8_t * psram_pointer = (uint8_t *)heap_caps_malloc(f_size, MALLOC_CAP_SPIRAM);

    if (psram_pointer == NULL) {
        printf("ERROR: PSRAM allocation failed\n");
    }
    else {
        FILE * f;

        printf("PSRAM allocated at %p\n", psram_pointer);

        f = fopen(filename, "rb");
        if (f != NULL) {
            size_t bytes_read = fread(psram_pointer, 1, f_size, f);
            fclose(f);
            printf("Read %zu/%lu bytes from SD\n", bytes_read, f_size);
        }
        else {
            printf("ERROR: Cannot open %s\n", filename);
            heap_caps_free(psram_pointer);
            psram_pointer = NULL;
        }
    }

    return psram_pointer;
}

/** ----------------------------------------------------------------------------------------
 * @brief Mount the SD card using the Board Support Package (BSP) function.
 * 
 */
bool mount_sd(void) {
    /* Rule 15.5: single point of exit via a result variable. */
    esp_err_t err = bsp_sdcard_mount();
    bool mounted;

    if (err == ESP_OK) {
        printf("SD card mounted successfully\n");
        mounted = true;
    }
    else {
        printf("Failed to mount SD card, error: %s\n", esp_err_to_name(err));
        mounted = false;
    }

    return mounted;
}

/** ----------------------------------------------------------------------------------------
 * @brief Unmount the SD card using the Board Support Package (BSP) function.
 * 
 */
bool unmount_sd(void) {
    /* Rule 15.5: single point of exit via a result variable. */
    esp_err_t err = bsp_sdcard_unmount();
    bool unmounted;

    if (err == ESP_OK) {
        printf("SD card unmounted successfully\n");
        unmounted = true;
    }
    else {
        printf("Failed to unmount SD card, error: %s\n", esp_err_to_name(err));
        unmounted = false;
    }

    return unmounted;
}

/** ----------------------------------------------------------------------------------------
 * @brief List directory.
 * 
 * @param dirname Path to the directory on the SD card to list.
 * 
 */
void list_directory(const char * dirname) {
    /* Rule 15.5: single point of exit; proceed guards the listing instead
       of returning early when the directory cannot be opened. */
    DIR *dir = opendir(dirname);

    if (dir == NULL) {
        printf("Failed to open %s\n", dirname);
    }
    else {
        struct dirent *entry;

        printf("%s contents:\n", dirname);
        printf("%-20s %12s\n", "Name", "Size (bytes)");
        printf("----------------------------------------\n");

        while ((entry = readdir(dir)) != NULL) {
            char fullpath[512];
            struct stat st;

            snprintf(fullpath, sizeof(fullpath), "%s/%s", dirname, entry->d_name);

            if (stat(fullpath, &st) == 0) {
                printf("%-20s %12llu\n", entry->d_name, (unsigned long long)st.st_size);
            } else {
                printf("%-20s %12s\n", entry->d_name, "?");
            }
        }
        closedir(dir);
    }
}

/** ----------------------------------------------------------------------------------------
 * @brief Get the number of free bytes on the mounted FAT volume.
 *
 * @return Free space in bytes, or 0 if it cannot be read.
 */
size_t getFreeBytes(void) {
  /* Rule 15.5: single point of exit via a result variable.
   *
   * esp_vfs_fat_info() is used instead of a raw f_getfree("0:", ...) call:
   * taskStorage() mounts/unmounts the SD card every loop iteration, and
   * ff_diskio_get_drive() hands out the first free drive slot on each
   * mount rather than always re-using slot 0 -- so a hardcoded "0:" can
   * silently resolve to a stale/empty FatFs table entry (FR_NO_FILESYSTEM)
   * even while the card is genuinely mounted and usable. esp_vfs_fat_info()
   * looks up the *current* drive for BSP_SD_MOUNT_POINT through the same
   * registration table fopen()/fread()/fwrite() already use correctly,
   * so it can't desync from the drive number actually in use.
   */
  uint64_t total_bytes = 0;
  uint64_t free_bytes_u64 = 0;
  esp_err_t err = esp_vfs_fat_info(BSP_SD_MOUNT_POINT, &total_bytes, &free_bytes_u64);
  size_t free_bytes;

  if (err != ESP_OK) {
    printf("esp_vfs_fat_info failed: %s\n", esp_err_to_name(err));
    free_bytes = 0;
  }
  else {
    free_bytes = (size_t)free_bytes_u64;
  }

  // uncomment to debug
  //   printf("[getFreeBytes] total_bytes=%lld\n", total_bytes);
  //   printf("[getFreeBytes] free_bytes_u64=%lld\n", free_bytes_u64);
  //   printf("[getFreeBytes] free_bytes=%d\n", free_bytes);

  return free_bytes;
}

bool isAvailableBytes(size_t num_bytes) {
  return num_bytes < getFreeBytes(); /* Rule 15.5: single point of exit */
}

/** ----------------------------------------------------------------------------------------
 * @brief Check if SD card root directory is ready.
 *
 * @return true if the SD card root directory can be opened, false otherwise.
 *
 */
bool is_fs_ready(const char * dirname) {
    /* Rule 15.5: single point of exit via a result variable. */
    DIR *dir = opendir(dirname);
    bool ready;

    if (dir != NULL) {
        closedir(dir);
        ready = true;
    }
    else {
        ready = false;
    }

    return ready;
}

/** ----------------------------------------------------------------------------------------
 * @brief Register LVGL callbacks for SD card file system access.
 * 
 */
void sd_lvgl_register(void)
{
    /* lv_fs_drv_register() stores the address passed to it rather than
       copying the struct, so fs_drv must outlive this function; static
       storage duration keeps the registered driver valid for the program's
       lifetime instead of becoming a dangling pointer once this returns. */
    static lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);
    fs_drv.letter = 'S';
    fs_drv.open_cb = sd_file_open;
    fs_drv.close_cb = sd_file_close;
    fs_drv.read_cb = sd_file_read;
    fs_drv.seek_cb = sd_file_seek;
    fs_drv.tell_cb = sd_file_tell;

    fs_drv.dir_open_cb = sd_dir_open;      // Opens directory
    fs_drv.dir_read_cb = sd_dir_read;      // Reads directory entries
    fs_drv.dir_close_cb = sd_dir_close;    // Closes directory

    lv_fs_drv_register(&fs_drv);
    // printf("LVGL S: drive registered\n");
}

/** ----------------------------------------------------------------------------------------
 * @brief Unmount
 *
 */
void sdcard_unmount(void) {
    // unmount
    // printf("Attempting SD card unmount...\n");
    if (sdcardData.sdcard_mounted == true) {
        esp_err_t err = bsp_sdcard_unmount();
        if (err != ESP_OK) {
            // printf("Failed to unmount SD card, error: %s\n", esp_err_to_name(err));
        }
        if (err == ESP_OK) {
            // printf("SD card unmounted successfully\n");
        }
    }
}

/** ----------------------------------------------------------------------------------------
 * @brief Mount
 * 
 * @note Mount is expected to fail if card is not present. Should be caled periodically to
 * handle hotplug events.
 * 
 */
void sdcard_mount(void) {
    /* Rule 15.5: single point of exit; proceed guards the mount attempt
       instead of returning early when mounting is currently disallowed. */
    if (sdcardData.allow_mount == true) {
        // mount
        // printf("Attempting SD card mount...\n");
        esp_err_t err = bsp_sdcard_mount();
        if (err != ESP_OK) {
            // printf("Failed to mount SD card, error: %s\n", esp_err_to_name(err));
            sdcardData.sdcard_mounted = false;

            // (todo) clear any pending operations
            // sdcardFlagData.save_mapping = false;
            // sdcardFlagData.load_mapping = false;
            // sdcardFlagData.delete_mapping = false;

            // sdcardFlagData.save_matrix = false;
            // sdcardFlagData.load_matrix = false;
            // sdcardFlagData.delete_matrix = false;

            // sdcardFlagData.save_system = false;
            // sdcardFlagData.load_system = false;
            // sdcardFlagData.delete_system = false;
        }
        else {
            // printf("SD card mounted successfully\n");
            // sd_lvgl_register(); // register file related callback functions with LVGL
            sdcardData.sdcard_mounted = true;
        }
        // printf("SDCLK: %d\n", gpio_get_level((gpio_num_t)SD_CLK));
        // printf("SDCMD: %d\n", gpio_get_level((gpio_num_t)SD_CMD));
        // printf("SDD0: %d\n", gpio_get_level((gpio_num_t)SD_D0));
        // printf("SDD1: %d\n", gpio_get_level((gpio_num_t)SD_D1));
        // printf("SDD2: %d\n", gpio_get_level((gpio_num_t)SD_D2));
        // printf("SDD3: %d\n", gpio_get_level((gpio_num_t)SD_D3)); // card detect
        // printf("SD_PWR: %d\n", gpio_get_level((gpio_num_t)SD_PWR));
    }
}
