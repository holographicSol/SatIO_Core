/*
  SatIOFile - Written By Benjamin Jack Cullen.

  Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#include <Arduino.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "UnidentifiedStudios_SatIOFile.h"
#include "UnidentifiedStudios_Matrix.h"
#include "UnidentifiedStudios_StrVal.h"
#include "UnidentifiedStudios_SatIO.h"
#include "UnidentifiedStudios_SystemData.h"
#include "UnidentifiedStudios_Matrix.h"
#include "UnidentifiedStudios_Mapping.h"
#include "UnidentifiedStudios_INS.h"
#include "UnidentifiedStudios_Config.h"
#include "UnidentifiedStudios_Multiplexers.h"
#include "UnidentifiedStudios_WTGPS300P.h"
#include "UnidentifiedStudios_WT901.h"
#include "UnidentifiedStudios_SdCardHelper.h"
#include "UnidentifiedStudios_GPIOPortExpander.h"

struct SatIOFileStruct SatIOFileData = {
    .i_token=0,
    .token={},
    .tmp_chars="",

    .matix_filepaths=
    {
        "/MATRIX/MATRIX_0.csv",
        "/MATRIX/MATRIX_1.csv",
        "/MATRIX/MATRIX_2.csv",
        "/MATRIX/MATRIX_3.csv",
        "/MATRIX/MATRIX_4.csv",
        "/MATRIX/MATRIX_5.csv",
        "/MATRIX/MATRIX_6.csv",
        "/MATRIX/MATRIX_7.csv",
        "/MATRIX/MATRIX_8.csv",
        "/MATRIX/MATRIX_9.csv",
    },
    .current_matrix_filepath="/MATRIX/MATRIX_0.csv", // default
    .i_current_matrix_file_path=0,
    .mapping_filepath="/MAPPING/mapping_conf.csv",
    .system_filepath="/SYSTEM/system_conf.csv",
    .log_dir="/LOG/",
    .log_files = {},
    .log_filepath = "",
    .unixtimestamp=0,
    .tmp_unixtimestamp=0,
    .number_of_log_files=0,
};

/**
 * @brief Yield for other tasks.
 * @note Current setup yields on every N lines loaded/saved, with no special cases.
 * @warning This increases loading/saving time.
 * 
 * yield_every_n_lines   0: very slow read/write (prioritize other tasks).
 * yield_every_n_lines > 0: faster read/write (more time blocking other tasks).
 */
int yield_every_n_lines=8;
int yield_counter=0;

/**
 * @brief Set Read/Write Success Flag.
 *
 * Sustains flag for a period of time while also non-blocking before setting flag back to NULL.
 *
 * @param flag Specify result of a read/write operation.
 *
 * Rule 8.7: internal linkage; only called from sdcardFlagHandler() in this file.
 */
static void set_storage_success_flag(bool flag) {
    if (flag==true) {sdcardFlagData.success_flag=2; vTaskDelay(1000 / portTICK_PERIOD_MS);}
    else            {sdcardFlagData.success_flag=1; vTaskDelay(1000 / portTICK_PERIOD_MS);}
                     sdcardFlagData.success_flag=0;
}

// ----------------------------------------------------------------------------------------
// SD Card File Helper Functions (using standard C I/O with /sdcard prefix)
// ----------------------------------------------------------------------------------------

/**
 * @brief Create full path with /sdcard prefix
 *
 * Rule 8.7: internal linkage; only used within this file.
 */
static void sd_fullpath(const char* path, char* fullpath, size_t size) {
    snprintf(fullpath, size, "/sdcard%s", path);
}

/**
 * @brief Check if file exists
 *
 * Rule 8.7: internal linkage; only used within this file.
 */
static bool sd_exists(const char* path) {
    char fullpath[256];
    sd_fullpath(path, fullpath, sizeof(fullpath));
    struct stat st;
    return (stat(fullpath, &st) == 0);
}

/**
 * @brief Remove file
 *
 * Rule 8.7: internal linkage; only used within this file.
 */
static bool sd_remove(const char* path) {
    char fullpath[256];
    sd_fullpath(path, fullpath, sizeof(fullpath));
    return (remove(fullpath) == 0);
}

/**
 * @brief Get file size
 *
 * Rule 8.7: internal linkage; only used within this file.
 */
static uint64_t sd_file_size(const char* path) {
    /* Rule 15.5: single point of exit via a result variable. */
    char fullpath[256];
    uint64_t file_size;
    struct stat st;

    sd_fullpath(path, fullpath, sizeof(fullpath));
    if (stat(fullpath, &st) == 0) {
        file_size = (uint64_t)st.st_size;
    }
    else {
        file_size = 0;
    }

    return file_size;
}

/**
 * @brief Create directory recursively
 *
 * Rule 8.7: internal linkage; only used within this file.
 */
static bool sd_mkdir(const char* path) {
    char fullpath[256];
    sd_fullpath(path, fullpath, sizeof(fullpath));
    
    /* Rule 21.x: strncpy() alone does not guarantee null-termination when
       fullpath's length is >= sizeof(tmp); reserving the last byte and
       explicitly terminating keeps the strlen() below provably in bounds. */
    char tmp[256];
    strncpy(tmp, fullpath, sizeof(tmp) - 1U);
    tmp[sizeof(tmp) - 1U] = '\0';
    size_t len = strlen(tmp);

    // Remove trailing slash
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    // Create directories recursively
    for (char* p = tmp + 1; *p != '\0'; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return (mkdir(tmp, 0755) == 0 || errno == EEXIST);
}

/**
 * @brief Open file with /sdcard prefix
 *
 * Rule 8.7: internal linkage; only used within this file.
 */
static FILE* sd_fopen(const char* path, const char* mode) {
    char fullpath[256];
    sd_fullpath(path, fullpath, sizeof(fullpath));
    // printf("[sd_fopen] fullpath: %s, mode: %s\n", fullpath, mode);
    fflush(stdout);
    
    // Create parent directory if writing
    if ((strchr(mode, 'w') != NULL) || (strchr(mode, 'a') != NULL)) {
        char dirpath[256];
        strncpy(dirpath, fullpath, sizeof(dirpath) - 1);
        dirpath[sizeof(dirpath) - 1] = '\0';
        char* lastSlash = strrchr(dirpath, '/');

        if ((lastSlash != NULL) && (lastSlash != dirpath)) {
            struct stat dir_st;

            *lastSlash = '\0';

            /* Every sd_fopen("a"/"w") call re-ran the full recursive
               mkdir() walk below even when the directory already existed,
               adding SD I/O latency to every single write/append call;
               skipping the walk once stat() confirms the directory is
               already there avoids that repeated cost (each mkdir() that
               hits the SD card takes real time the task watchdog is
               counting against). */
            if (stat(dirpath, &dir_st) != 0) {
                // printf("[sd_fopen] creating dir: %s\n", dirpath);
                fflush(stdout);
                // Create directory directly with full path (already has /sdcard)
                char tmp[256];
                strncpy(tmp, dirpath, sizeof(tmp) - 1);
                tmp[sizeof(tmp) - 1] = '\0';
                for (char* p = tmp + 1; *p != '\0'; p++) {
                    if (*p == '/') {
                        *p = '\0';
                        int ret = mkdir(tmp, 0755);
                        if (ret != 0 && errno != EEXIST) {
                            printf("[sd_fopen] mkdir(%s) failed: %s\n", tmp, strerror(errno));
                        }
                        *p = '/';
                    }
                }
                int ret = mkdir(tmp, 0755);
                if (ret != 0 && errno != EEXIST) {
                    printf("[sd_fopen] final mkdir(%s) failed: %s\n", tmp, strerror(errno));
                }
                fflush(stdout);
            }
        }
    }

    // Create file
    FILE* f = fopen(fullpath, mode);
    if (f == NULL) {
        printf("[sd_fopen] fopen(%s) failed: errno=%d (%s)\n", fullpath, errno, strerror(errno));
        fflush(stdout);
    }
    return f;
}

/**
 * @brief Get a log filename.
 * @param mode Specify mode: 0=oldest 1=latest.
 *
 * Rule 8.7: internal linkage; only used within this file.
 */
static bool getLogFile(int mode) {
  /* Rule 15.5: single point of exit via a result variable; the two
     failure cases (directory can't be opened, no valid log file found)
     become guards around the rest of the work instead of early returns. */
  bool found = true;

  // Open dir (and create dir if not exist)
  char fullpath[256];
  sd_fullpath(SatIOFileData.log_dir, fullpath, sizeof(fullpath));
  sd_mkdir(SatIOFileData.log_dir); // ensure directory exists

  DIR* root = opendir(fullpath);
  if (root == NULL) {
    found = false;
  }
  else {
    // Set target
    if (mode==0) {SatIOFileData.unixtimestamp=INT64_MAX;} // for finding oldest
    else {SatIOFileData.unixtimestamp=0;} // for finding latest

    // Iterate through files in directory
    SatIOFileData.number_of_log_files=0;
    struct dirent* entry;
    while ((entry = readdir(root)) != NULL) {
      // Skip . and .. and directories
      if (entry->d_name[0] == '.') continue;
      if (entry->d_type == DT_DIR) continue;

      // Create a copy of filename
      memset(SatIOFileData.tmp_chars, 0, sizeof(SatIOFileData.tmp_chars));
      strncpy(SatIOFileData.tmp_chars, entry->d_name, sizeof(SatIOFileData.tmp_chars) - 1U);

      // Tokenize
      int i_san=0;
      SatIOFileData.tmp_unixtimestamp=0;
      SatIOFileData.i_token = 0;
      SatIOFileData.token = strtok(SatIOFileData.tmp_chars, ".");
      while (SatIOFileData.token != NULL) {
        switch (SatIOFileData.i_token) {
            case 0: if (SatIOFileData.token != NULL && str_is_int64(SatIOFileData.token)) {
                i_san++; SatIOFileData.tmp_unixtimestamp = strtoll(SatIOFileData.token, NULL, 10);} break;
            case 1: if (strcmp(SatIOFileData.token, "csv")==0) {i_san++;} break;
        }
        SatIOFileData.token = strtok(NULL, ".");
        SatIOFileData.i_token++;
      }
      // Update unixtimestamp
      if (i_san==2) {
        SatIOFileData.number_of_log_files++;
        if      (mode==0 && SatIOFileData.tmp_unixtimestamp<SatIOFileData.unixtimestamp) {SatIOFileData.unixtimestamp=SatIOFileData.tmp_unixtimestamp;}
        else if (mode==1 && SatIOFileData.tmp_unixtimestamp>SatIOFileData.unixtimestamp) {SatIOFileData.unixtimestamp=SatIOFileData.tmp_unixtimestamp;}
      }
    }
    closedir(root);

    /* mode 0 (oldest) starts unixtimestamp at INT64_MAX and mode 1 (latest)
       starts it at 0; either sentinel surviving the loop above means no
       valid log file was found. */
    if (SatIOFileData.unixtimestamp==0 || SatIOFileData.unixtimestamp==INT64_MAX) {
      found = false;
    }
    else {
      // Store filename of interest
      char fname[128];
      int written = snprintf(fname, sizeof(SatIOFileData.log_filepath), "%s%lld.csv", SatIOFileData.log_dir, (long long)SatIOFileData.unixtimestamp);
      if (written < 0 || (size_t)written >= sizeof(SatIOFileData.log_filepath)) {
          /* A truncated path is not a usable path: fail outright instead of
          letting a caller write to a corrupted filename. */
          printf("[getLogFile] log_filepath truncated, rejecting.\n");
          found = false;
        }
        else {
          memset(SatIOFileData.log_filepath, 0, sizeof(SatIOFileData.log_filepath));
          strncpy(SatIOFileData.log_filepath, fname, written);
      }
    }
  }

  return found;
}

/**
 * @brief Build a new log filename from the current local time.
 * @return false if the filename would have been truncated (log_filepath is
 *         left empty in that case), true otherwise.
 *
 * Rule 8.7: internal linkage; only used within this file.
 */
static bool createNewLogFilename(void) {
  bool ok = true;

  // Use local_unixtime_uS for filename
  char fname[128];
  int written = snprintf(fname, sizeof(SatIOFileData.log_filepath), "%s%lld.csv", SatIOFileData.log_dir, (long long)SatIOData.systemTime.unixtime_uS);
  if (written < 0 || (size_t)written >= sizeof(SatIOFileData.log_filepath)) {
      /* A truncated path is not a usable path: fail outright instead of
      letting a caller write to a corrupted filename. */
      printf("[createNewLogFilename] log_filepath truncated, rejecting.\n");
      memset(SatIOFileData.log_filepath, 0, sizeof(SatIOFileData.log_filepath));
      ok = false;
    }
    else {
      memset(SatIOFileData.log_filepath, 0, sizeof(SatIOFileData.log_filepath));
      strncpy(SatIOFileData.log_filepath, fname, written);
  }

  return ok;
}

/* Rule 8.7: internal linkage; only used within this file. */
static void deleteOldestLogFile(void) {
  if (getLogFile(0)==true && SatIOFileData.number_of_log_files>MAX_LOG_FILES) {
    sd_remove(SatIOFileData.log_filepath);
    if (sd_exists(SatIOFileData.log_filepath) == false) {
        printf("[deleteOldestLogFile] log file deleted successfully.\n");
    }
    else {printf("[deleteOldestLogFile] failed to delete log file.\n");}
  }
}

/* Rule 8.7: internal linkage; only used within this file. */
static void printLogLine(const char* line_str) {
    /* Rule 15.5: single point of exit; proceed/have_filepath guard the
       write instead of returning early when there is no disk space
       available or no valid (un-truncated) log filename could be resolved. */

    size_t line_len = strlen(line_str);
    if (isAvailableBytes(line_len + 1) == false) {
      printf("[printLogLine] No more diskspace available!\n");
    }
    else {
      // Select log filename
      bool have_filepath = true;
      deleteOldestLogFile();
      if (getLogFile(1)==false) {
        have_filepath = createNewLogFilename();
      }

      if (have_filepath == false) {
        printf("[printLogLine] could not resolve a log filename, aborting write.\n");
      }
      else {
        // Ensure file exists (create if needed)
        FILE* f = sd_fopen(SatIOFileData.log_filepath, "a");
        if (f != NULL) {fclose(f);}

        // Check file size
        uint64_t file_size = sd_file_size(SatIOFileData.log_filepath);
        if (file_size + line_len + 1 > MAX_LOG_FILE_SIZE) {
          deleteOldestLogFile();
          have_filepath = createNewLogFilename();
        }

        if (have_filepath == false) {
          printf("[printLogLine] could not resolve a log filename, aborting write.\n");
        }
        else {
          // Write to log file
          f = sd_fopen(SatIOFileData.log_filepath, "a");
          if (f != NULL) {
            fputs(line_str, f);
            fputc('\n', f);
            fclose(f);
          }
          else {printf("[printLogLine] file does not exist: %s\n", SatIOFileData.log_filepath);}
        }
      }
    }
}

void writeLog(void) {
    // printf("--------------------------------------\n");
    // printf("[writeLog] writing log\n");

    // --------------------------------
    // Log Line: Timestamp & Basic Stat
    // --------------------------------
    String line="";
    // --------------------------------
    // Log Line: SatIO
    // --------------------------------
    line = "$SatIO,";
    line=line+ String(SatIOData.systemTime.padded_time_HHMMSS) + ",";
    line=line+ String(SatIOData.systemTime.padded_date_DDMMYYYY) + ",";
    line=line+ String(SatIOData.systemTime.sync_padded_time_HHMMSS) + ",";
    line=line+ String(SatIOData.systemTime.sync_padded_date_DDMMYYYY) + ",";

    line=line+ String(SatIOData.localTime.padded_time_HHMMSS) + ",";
    line=line+ String(SatIOData.localTime.padded_date_DDMMYYYY) + ",";

    line=line+ String(systemData.uptime_seconds) + ",";
    line=line+ String(SatIOData.degrees_latitude) + ",";
    line=line+ String(SatIOData.degrees_longitude) + ",";
    line=line+ String(SatIOData.user_degrees_latitude) + ",";
    line=line+ String(SatIOData.user_degrees_longitude) + ",";
    line=line+ String(SatIOData.system_degrees_latitude) + ",";
    line=line+ String(SatIOData.system_degrees_longitude) + ",";
    line=line+ String(SatIOData.altitude) + ",";
    line=line+ String(SatIOData.speed, 7) + ",";
    line=line+ String(SatIOData.ground_heading, 7) + ",";
    line=line+ String(insData.ins_latitude, 7) + ",";
    line=line+ String(insData.ins_longitude, 7) + ",";
    line=line+ String(insData.ins_altitude) + ",";
    line=line+ String(insData.ins_heading) + ",";
    line=line+ String(insData.INS_INITIALIZATION_FLAG) + ",";
    line=line+ String(insData.INS_MODE) + ",";
    line=line+ String(insData.INS_FORCED_ON_FLAG) + ",";
    line=line+ String(insData.INS_REQ_GPS_PRECISION) + ",";
    line=line+ String(insData.INS_REQ_HEADING_RANGE_DIFF) + ",";
    line=line+ String(insData.INS_REQ_MIN_SPEED) + ",";
    line=line+ String(insData.INS_USE_GYRO_HEADING) + ",";
    line=line+ String(insData.INS_ENABLED) + ",";
    printLogLine(line.c_str());

    // --------------------------------
    // Log Line: Analog/Digital
    // --------------------------------
    line="$MPLEX0,";
    for (int i=0; i<MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS; i++) {line=line+String(ad_mux_0.data[i])+",";}
    printLogLine(line.c_str());

    // --------------------------------
    // Log Line: Port Controller Input
    // --------------------------------
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_0
    line="$PCINPT0,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_0.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_0


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_1
    line="$PCINPT1,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_1.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_1


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_2
    line="$PCINPT2,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_2.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_2


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_3
    line="$PCINPT3,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_3.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_3


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_4
    line="$PCINPT4,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_4.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_4


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_5
    line="$PCINPT5,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_5.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_5


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_6
    line="$PCINPT6,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_6.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_6


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_7
    line="$PCINPT7,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_7.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_7


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_8
    line="$PCINPT8,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_8.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_8


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_9
    line="$PCINPT9,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_9.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_9


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_10
    line="$PCINPT10,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_10.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_10


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_11
    line="$PCINPT11,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_11.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_11


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_12
    line="$PCINPT12,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_12.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_12


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_13
    line="$PCINPT13,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_13.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_13


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_14
    line="$PCINPT14,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_14.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_14


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_15
    line="$PCINPT15,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_15.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_15


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_16
    line="$PCINPT16,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_16.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_16


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_17
    line="$PCINPT17,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_17.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_17


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_18
    line="$PCINPT18,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_18.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_18


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_19
    line="$PCINPT19,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_19.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_19


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_20
    line="$PCINPT20,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_20.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_20


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_21
    line="$PCINPT21,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_21.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_21


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_22
    line="$PCINPT22,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_22.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_22


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_23
    line="$PCINPT23,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_23.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_23


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_24
    line="$PCINPT24,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_24.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_24


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_25
    line="$PCINPT25,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_25.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_25


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_26
    line="$PCINPT26,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_26.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_26


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_27
    line="$PCINPT27,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_27.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_27


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_28
    line="$PCINPT28,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_28.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_28


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_29
    line="$PCINPT29,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_29.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_29


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_30
    line="$PCINPT30,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_30.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_30


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_31
    line="$PCINPT31,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_31.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_31


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_32
    line="$PCINPT32,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_32.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_32


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_33
    line="$PCINPT33,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_33.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_33


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_34
    line="$PCINPT34,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_34.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_34


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_35
    line="$PCINPT35,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_35.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_35


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_36
    line="$PCINPT36,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_36.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_36


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_37
    line="$PCINPT37,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_37.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_37


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_38
    line="$PCINPT38,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_38.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_38


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_39
    line="$PCINPT39,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_39.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_39


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_40
    line="$PCINPT40,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_40.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_40


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_41
    line="$PCINPT41,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_41.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_41


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_42
    line="$PCINPT42,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_42.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_42


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_43
    line="$PCINPT43,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_43.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_43


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_44
    line="$PCINPT44,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_44.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_44


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_45
    line="$PCINPT45,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_45.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_45


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_46
    line="$PCINPT46,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_46.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_46


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_47
    line="$PCINPT47,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_47.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_47


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_48
    line="$PCINPT48,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_48.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_48


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_49
    line="$PCINPT49,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_49.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_49


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_50
    line="$PCINPT50,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_50.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_50


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_51
    line="$PCINPT51,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_51.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_51


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_52
    line="$PCINPT52,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_52.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_52


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_53
    line="$PCINPT53,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_53.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_53


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_54
    line="$PCINPT54,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_54.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_54


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_55
    line="$PCINPT55,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_55.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_55


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_56
    line="$PCINPT56,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_56.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_56


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_57
    line="$PCINPT57,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_57.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_57


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_58
    line="$PCINPT58,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_58.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_58


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_59
    line="$PCINPT59,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_59.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_59


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_60
    line="$PCINPT60,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_60.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_60


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_61
    line="$PCINPT61,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_61.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_61


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_62
    line="$PCINPT62,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_62.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_62


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_63
    line="$PCINPT63,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_63.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_63


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_64
    line="$PCINPT64,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_64.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_64


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_65
    line="$PCINPT65,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_65.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_65


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_66
    line="$PCINPT66,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_66.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_66


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_67
    line="$PCINPT67,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_67.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_67


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_68
    line="$PCINPT68,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_68.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_68


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_69
    line="$PCINPT69,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_69.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_69


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_70
    line="$PCINPT70,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_70.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_70


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_71
    line="$PCINPT71,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_71.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_71


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_72
    line="$PCINPT72,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_72.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_72


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_73
    line="$PCINPT73,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_73.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_73


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_74
    line="$PCINPT74,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_74.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_74


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_75
    line="$PCINPT75,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_75.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_75


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_76
    line="$PCINPT76,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_76.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_76


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_77
    line="$PCINPT77,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_77.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_77


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_78
    line="$PCINPT78,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_78.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_78


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_79
    line="$PCINPT79,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_79.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_79


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_80
    line="$PCINPT80,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_80.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_80


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_81
    line="$PCINPT81,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_81.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_81


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_82
    line="$PCINPT82,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_82.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_82


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_83
    line="$PCINPT83,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_83.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_83


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_84
    line="$PCINPT84,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_84.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_84


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_85
    line="$PCINPT85,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_85.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_85


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_86
    line="$PCINPT86,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_86.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_86


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_87
    line="$PCINPT87,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_87.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_87


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_88
    line="$PCINPT88,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_88.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_88


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_89
    line="$PCINPT89,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_89.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_89


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_90
    line="$PCINPT90,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_90.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_90


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_91
    line="$PCINPT91,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_91.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_91


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_92
    line="$PCINPT92,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_92.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_92


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_93
    line="$PCINPT93,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_93.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_93


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_94
    line="$PCINPT94,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_94.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_94


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_95
    line="$PCINPT95,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_95.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_95


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_96
    line="$PCINPT96,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_96.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_96


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_97
    line="$PCINPT97,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_97.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_97


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_98
    line="$PCINPT98,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_98.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_98


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_99
    line="$PCINPT99,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_99.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_99


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_100
    line="$PCINPT100,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_100.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_100


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_101
    line="$PCINPT101,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_101.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_101


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_102
    line="$PCINPT102,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_102.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_102


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_103
    line="$PCINPT103,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_103.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_103


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_104
    line="$PCINPT104,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_104.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_104


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_105
    line="$PCINPT105,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_105.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_105


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_106
    line="$PCINPT106,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_106.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_106


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_107
    line="$PCINPT107,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_107.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_107


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_108
    line="$PCINPT108,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_108.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_108


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_109
    line="$PCINPT109,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_109.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_109


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_110
    line="$PCINPT110,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_110.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_110


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_111
    line="$PCINPT111,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_111.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_111


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_112
    line="$PCINPT112,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_112.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_112


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_113
    line="$PCINPT113,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_113.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_113


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_114
    line="$PCINPT114,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_114.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_114


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_115
    line="$PCINPT115,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_115.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_115


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_116
    line="$PCINPT116,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_116.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_116


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_117
    line="$PCINPT117,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_117.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_117


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_118
    line="$PCINPT118,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_118.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_118


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_119
    line="$PCINPT119,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_119.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_119


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_120
    line="$PCINPT120,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_120.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_120


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_121
    line="$PCINPT121,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_121.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_121


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_122
    line="$PCINPT122,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_122.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_122


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_123
    line="$PCINPT123,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_123.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_123


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_124
    line="$PCINPT124,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_124.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_124


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_125
    line="$PCINPT125,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_125.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_125


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_126
    line="$PCINPT126,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_126.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_126


    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_127
    line="$PCINPT127,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_127.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_127

    // --------------------------------
    // Log Line: Gyro0
    // --------------------------------
    line="$GYRO0,";
    line=line+ String(gyroData.gyro_0_acc_x) + ",";
    line=line+ String(gyroData.gyro_0_acc_y) + ",";
    line=line+ String(gyroData.gyro_0_acc_z) + ",";
    line=line+ String(gyroData.gyro_0_ang_x) + ",";
    line=line+ String(gyroData.gyro_0_ang_y) + ",";
    line=line+ String(gyroData.gyro_0_ang_z) + ",";
    line=line+ String(gyroData.gyro_0_gyr_x) + ",";
    line=line+ String(gyroData.gyro_0_gyr_y) + ",";
    line=line+ String(gyroData.gyro_0_gyr_z) + ",";
    line=line+ String(gyroData.gyro_0_mag_x) + ",";
    line=line+ String(gyroData.gyro_0_mag_y) + ",";
    line=line+ String(gyroData.gyro_0_mag_z) + ",";
    printLogLine(line.c_str());
}

/* Rule 8.7: internal linkage; only used within this file. */
static void printLine(FILE* f, const char* line) {
    /* Rule 15.5: single point of exit; proceed guards the write instead of
       returning early when f is NULL. */
    size_t line_len = strlen(line);
    if (isAvailableBytes(line_len + 1) == false) {
      printf("[printLine] No more diskspace available!\n");
    }
    else {
        if (f != NULL) {
        fputs(line, f);
        fputc('\n', f);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------------
// MAPPING
// ---------------------------------------------------------------------------------------------------------------------------

/**
 * Enum and get tag are for order agnostic tag+data reads/writes for ease of maintenance/ammendments.
 */

typedef enum {
    MAP_MODE,
    FUNCTION_N,
    MAP_CONFIG_1,
    MAP_CONFIG_2,
    MAP_CONFIG_3,
    MAP_CONFIG_4,
    MAP_CONFIG_5,
} mapping_tag_t;

/* Rule 7.4: a string literal's type is "array of const char", so the
   function returning one must return const char* rather than char*.
   Rule 8.7: internal linkage; only used within this file. */
static const char * getMapTag(int t) {
    switch (t) {
        case MAP_MODE:     return "MAP_MODE";
        case FUNCTION_N:   return "FUNCTION_N";
        case MAP_CONFIG_1: return "MAP_CONFIG_1";
        case MAP_CONFIG_2: return "MAP_CONFIG_2";
        case MAP_CONFIG_3: return "MAP_CONFIG_3";
        case MAP_CONFIG_4: return "MAP_CONFIG_4";
        case MAP_CONFIG_5: return "MAP_CONFIG_5";
        default:           return "?";
    }
}
bool saveMappingFile(const char *filepath) {
    printf("[saveMappingFile] Attempting to save mapping file...\n");

    FILE* f = sd_fopen(filepath, "w");
    if (f == NULL) { printf("[saveMappingFile] Failed to open mapping file.\n"); return false; }
    
    char lineBuf[256];
    for (int i_tag=0; i_tag<MAX_MAPPING_TAGS; i_tag++) {
        const char *tag = getMapTag(i_tag);
        for (int i_map=0; i_map<MAX_MAP_SLOTS; i_map++) {
            if      (i_tag==MAP_MODE)     { snprintf(lineBuf, sizeof(lineBuf), "%s,%d,%d",  tag, i_map, (int)mappingData.map_mode[0][i_map]); printLine(f, lineBuf);}
            else if (i_tag==FUNCTION_N)   { snprintf(lineBuf, sizeof(lineBuf), "%s,%d,%ld", tag, i_map, (long)mappingData.mapping_config[0][i_map][INDEX_MAP_C0]); printLine(f, lineBuf);}
            else if (i_tag==MAP_CONFIG_1) { snprintf(lineBuf, sizeof(lineBuf), "%s,%d,%ld", tag, i_map, (long)mappingData.mapping_config[0][i_map][INDEX_MAP_C1]); printLine(f, lineBuf);}
            else if (i_tag==MAP_CONFIG_2) { snprintf(lineBuf, sizeof(lineBuf), "%s,%d,%ld", tag, i_map, (long)mappingData.mapping_config[0][i_map][INDEX_MAP_C2]); printLine(f, lineBuf);}
            else if (i_tag==MAP_CONFIG_3) { snprintf(lineBuf, sizeof(lineBuf), "%s,%d,%ld", tag, i_map, (long)mappingData.mapping_config[0][i_map][INDEX_MAP_C3]); printLine(f, lineBuf);}
            else if (i_tag==MAP_CONFIG_4) { snprintf(lineBuf, sizeof(lineBuf), "%s,%d,%ld", tag, i_map, (long)mappingData.mapping_config[0][i_map][INDEX_MAP_C4]); printLine(f, lineBuf);}
            else if (i_tag==MAP_CONFIG_5) { snprintf(lineBuf, sizeof(lineBuf), "%s,%d,%ld", tag, i_map, (long)mappingData.mapping_config[0][i_map][INDEX_MAP_C5]); printLine(f, lineBuf);}
        }
    }
    
    fclose(f);
    printf("[saveMappingFile] done.\n");
    return true;
}

bool loadMappingFile(const char *filepath) {
    printf("[loadMappingFile] Attempting to load mapping file...\n");
    
    if (sd_exists(filepath) == false) {printf("[loadMappingFile] Could not find mapping file.\n");}
    
    FILE* f = sd_fopen(filepath, "r");
    if (f == NULL) { printf("[loadMappingFile] Failed to open mapping file.\n"); return false; }
    
    override_all_computer_assists();
    set_all_mapping_default(); // avoid mixing current values with loaded values

    char lineBuffer[1024];
    int currentTag = 0;
    while (fgets(lineBuffer, sizeof(lineBuffer), f) != NULL) {
        size_t len = strlen(lineBuffer);
        while (len > 0 && (lineBuffer[len-1] == '\n' || lineBuffer[len-1] == '\r')) { lineBuffer[--len] = '\0'; }
        if (len == 0) continue;
        // printf("Processing Tag Token Number: %d (data: %s)\n", currentTag, lineBuffer); // uncomment to debug
        
        char *token = strtok(lineBuffer, ",");
        int tokenCount = 0;
        signed int tag_index=-1;
        for (int i_find_tag=0; i_find_tag<MAX_MAPPING_TAGS; i_find_tag++) {if (strcmp(getMapTag(i_find_tag), token)==0) {tag_index=i_find_tag; break;}}
        if (tag_index==-1) {printf("Unrecognized tag found in mapping file: %s\n", token); continue;}

        String data_0; String data_1; String data_2;
        token = strtok(NULL, ","); // remove tag

        while (token != NULL) {if (tokenCount==0) {data_0=token;} else if (tokenCount==1) {data_1=token;} else if (tokenCount==2) {data_2=token;} token = strtok(NULL, ","); tokenCount++;}
        if      (tag_index==MAP_MODE)     {if (str_is_int8(data_0.c_str()) && str_is_int8(data_1.c_str())) {mappingData.map_mode[0][atoi(data_0.c_str())]=atoi(data_1.c_str());}}
        else if (tag_index==FUNCTION_N)   {if (str_is_int8(data_0.c_str()) && str_is_long(data_1.c_str())) {mappingData.mapping_config[0][atoi(data_0.c_str())][INDEX_MAP_C0]=strtol(data_1.c_str(), NULL, 10);}}
        else if (tag_index==MAP_CONFIG_1) {if (str_is_int8(data_0.c_str()) && str_is_long(data_1.c_str())) {mappingData.mapping_config[0][atoi(data_0.c_str())][INDEX_MAP_C1]=strtol(data_1.c_str(), NULL, 10);}}
        else if (tag_index==MAP_CONFIG_2) {if (str_is_int8(data_0.c_str()) && str_is_long(data_1.c_str())) {mappingData.mapping_config[0][atoi(data_0.c_str())][INDEX_MAP_C2]=strtol(data_1.c_str(), NULL, 10);}}
        else if (tag_index==MAP_CONFIG_3) {if (str_is_int8(data_0.c_str()) && str_is_long(data_1.c_str())) {mappingData.mapping_config[0][atoi(data_0.c_str())][INDEX_MAP_C3]=strtol(data_1.c_str(), NULL, 10);}}
        else if (tag_index==MAP_CONFIG_4) {if (str_is_int8(data_0.c_str()) && str_is_long(data_1.c_str())) {mappingData.mapping_config[0][atoi(data_0.c_str())][INDEX_MAP_C4]=strtol(data_1.c_str(), NULL, 10);}}
        else if (tag_index==MAP_CONFIG_5) {if (str_is_int8(data_0.c_str()) && str_is_long(data_1.c_str())) {mappingData.mapping_config[0][atoi(data_0.c_str())][INDEX_MAP_C5]=strtol(data_1.c_str(), NULL, 10);}}
        currentTag++;
    }
    fclose(f);
    if (currentTag == 0) {printf("$MAPPINGLOADFAILED\n");}
    printf("$MAPPINGLOADED\n");
    return true;
}

bool deleteMappingFile(const char *filepath) {
    if (sd_exists(filepath)) {if (sd_remove(filepath)) {printf("$MAPPINGDELETED\n"); return true;}}
    printf("$MAPPINGDELETEFAILED\n");
    return false;
}

// ---------------------------------------------------------------------------------------------------------------------------
// MATRIX
// ---------------------------------------------------------------------------------------------------------------------------

/**
 * Enum and get tag are for order agnostic tag+data reads/writes for ease of maintenance/ammendments.
 */

typedef enum {
    SWITCH_PORT,
    SWITCH_FUNCTION,
    FUNCTION_X,
    FUNCTION_Y,
    FUNCTION_Z,
    FUNCTION_OPERATOR,
    FUNCTION_INVERT,
    SWITCH_OUTPUT_MODE,
    SWITCH_PWM_VALUE_0,
    SWITCH_PWM_VALUE_1,
    SWITCH_FLUX,
    COMPUTER_ASSIST,
    MAP_SLOT,
    XYZ_MODE_X,
    XYZ_MODE_Y,
    XYZ_MODE_Z,
    SWITCH_OPCA
} matrix_tag_t;

/* Rule 7.4: a string literal's type is "array of const char", so the
   function returning one must return const char* rather than char*.
   Rule 8.7: internal linkage; only used within this file. */
static const char * getMatrixTag(int t) {
    switch (t) {
        case SWITCH_PORT:        return "SWITCH_PORT";
        case SWITCH_FUNCTION:    return "SWITCH_FUNCTION";
        case FUNCTION_X:         return "FUNCTION_X";
        case FUNCTION_Y:         return "FUNCTION_Y";
        case FUNCTION_Z:         return "FUNCTION_Z";
        case FUNCTION_OPERATOR:  return "FUNCTION_OPERATOR";
        case FUNCTION_INVERT:    return "FUNCTION_INVERT";
        case SWITCH_OUTPUT_MODE: return "SWITCH_OUTPUT_MODE";
        case SWITCH_PWM_VALUE_0: return "SWITCH_PWM_VALUE_0";
        case SWITCH_PWM_VALUE_1: return "SWITCH_PWM_VALUE_1";
        case SWITCH_FLUX:        return "SWITCH_FLUX";
        case COMPUTER_ASSIST:    return "COMPUTER_ASSIST";
        case MAP_SLOT:           return "MAP_SLOT";
        case XYZ_MODE_X:         return "XYZ_MODE_X";
        case XYZ_MODE_Y:         return "XYZ_MODE_Y";
        case XYZ_MODE_Z:         return "XYZ_MODE_Z";
        case SWITCH_OPCA:        return "SWITCH_OPCA";
        default:                 return "?";
    }
}

bool saveMatrixFile() {    
    printf("[saveMatrixFile] Attempting to save matrix file.\n");

    FILE* f = sd_fopen(SatIOFileData.matix_filepaths[SatIOFileData.i_current_matrix_file_path], "w");
    if (f == NULL) {printf("[saveMatrixFile] Failed to open matrix file.\n"); return false;}
    
    char lineBuf[256];
    const char *tag_switch_port = getMatrixTag(SWITCH_PORT);
    const char *tag_switch_func = getMatrixTag(SWITCH_FUNCTION);
    const char *tag_func_x      = getMatrixTag(FUNCTION_X);
    const char *tag_func_y      = getMatrixTag(FUNCTION_Y);
    const char *tag_func_z      = getMatrixTag(FUNCTION_Z);
    const char *tag_func_op     = getMatrixTag(FUNCTION_OPERATOR);
    const char *tag_func_inv    = getMatrixTag(FUNCTION_INVERT);
    const char *tag_out_mode    = getMatrixTag(SWITCH_OUTPUT_MODE);
    const char *tag_pwm_0       = getMatrixTag(SWITCH_PWM_VALUE_0);
    const char *tag_pwm_1       = getMatrixTag(SWITCH_PWM_VALUE_1);
    const char *tag_flux        = getMatrixTag(SWITCH_FLUX);
    const char *tag_comp_assist = getMatrixTag(COMPUTER_ASSIST);
    const char *tag_map_slot    = getMatrixTag(MAP_SLOT);
    const char *tag_xyz_mode_x  = getMatrixTag(XYZ_MODE_X);
    const char *tag_xyz_mode_y  = getMatrixTag(XYZ_MODE_Y);
    const char *tag_xyz_mode_z  = getMatrixTag(XYZ_MODE_Z);
    const char *tag_opca        = getMatrixTag(SWITCH_OPCA);

    // SWITCH_PORT
    for (int i_switch=0; i_switch<MAX_MATRIX_SWITCHES; i_switch++) {
        snprintf(lineBuf, sizeof(lineBuf), "%s,%d,%d", tag_switch_port, i_switch, (int)matrixData.matrix_port_map[0][i_switch]);
        printLine(f, lineBuf);
    }

    // SWITCH_OPCA
    for (int i_switch=0; i_switch<MAX_MATRIX_SWITCHES; i_switch++) {
        snprintf(lineBuf, sizeof(lineBuf), "%s,%d,%d", tag_opca, i_switch, (int)matrixData.output_portcontroller_address[0][i_switch]);
        printLine(f, lineBuf);
    }

    // SWITCH_FUNCTION
    for (int i_switch=0; i_switch<MAX_MATRIX_SWITCHES; i_switch++) {
        for (int i_func=0; i_func<MAX_MATRIX_SWITCH_FUNCTIONS; i_func++) {
            snprintf(lineBuf, sizeof(lineBuf), "%s,%d,%d,%d", tag_switch_func, i_switch, i_func, (int)matrixData.matrix_function[0][i_switch][i_func]);
            printLine(f, lineBuf);
        }
    }

    // FUNCTION_X
    for (int i_switch=0; i_switch<MAX_MATRIX_SWITCHES; i_switch++) {
        for (int i_func=0; i_func<MAX_MATRIX_SWITCH_FUNCTIONS; i_func++) {
            snprintf(lineBuf, sizeof(lineBuf), "%s,%d,%d,%.10f", tag_func_x, i_switch, i_func, matrixData.matrix_function_xyz[0][i_switch][i_func][INDEX_MATRIX_FUNTION_X]);
            printLine(f, lineBuf);
        }
    }

    // FUNCTION_Y
    for (int i_switch=0; i_switch<MAX_MATRIX_SWITCHES; i_switch++) {
        for (int i_func=0; i_func<MAX_MATRIX_SWITCH_FUNCTIONS; i_func++) {
            snprintf(lineBuf, sizeof(lineBuf), "%s,%d,%d,%.10f", tag_func_y, i_switch, i_func, matrixData.matrix_function_xyz[0][i_switch][i_func][INDEX_MATRIX_FUNTION_Y]);
            printLine(f, lineBuf);
        }
    }

    // FUNCTION_Z
    for (int i_switch=0; i_switch<MAX_MATRIX_SWITCHES; i_switch++) {
        for (int i_func=0; i_func<MAX_MATRIX_SWITCH_FUNCTIONS; i_func++) {
            snprintf(lineBuf, sizeof(lineBuf), "%s,%d,%d,%.10f", tag_func_z, i_switch, i_func, matrixData.matrix_function_xyz[0][i_switch][i_func][INDEX_MATRIX_FUNTION_Z]);
            printLine(f, lineBuf);
        }
    }

    // FUNCTION_OPERATOR
    for (int i_switch=0; i_switch<MAX_MATRIX_SWITCHES; i_switch++) {
        for (int i_func=0; i_func<MAX_MATRIX_SWITCH_FUNCTIONS; i_func++) {
            snprintf(lineBuf, sizeof(lineBuf), "%s,%d,%d,%d", tag_func_op, i_switch, i_func, (int)matrixData.matrix_switch_operator_index[0][i_switch][i_func]);
            printLine(f, lineBuf);
        }
    }

    // FUNCTION_INVERT
    for (int i_switch=0; i_switch<MAX_MATRIX_SWITCHES; i_switch++) {
        for (int i_func=0; i_func<MAX_MATRIX_SWITCH_FUNCTIONS; i_func++) {
            snprintf(lineBuf, sizeof(lineBuf), "%s,%d,%d,%d", tag_func_inv, i_switch, i_func, (int)matrixData.matrix_switch_inverted_logic[0][i_switch][i_func]);
            printLine(f, lineBuf);
        }
    }

    // SWITCH_OUTPUT_MODE
    for (int i_switch=0; i_switch<MAX_MATRIX_SWITCHES; i_switch++) {
        snprintf(lineBuf, sizeof(lineBuf), "%s,%d,%d", tag_out_mode, i_switch, (int)matrixData.output_mode[0][i_switch]);
        printLine(f, lineBuf);
    }

    // SWITCH_PWM_VALUE_0
    for (int i_switch=0; i_switch<MAX_MATRIX_SWITCHES; i_switch++) {
        snprintf(lineBuf, sizeof(lineBuf), "%s,%d,%lu", tag_pwm_0, i_switch, (unsigned long)matrixData.output_pwm[0][i_switch][0]);
        printLine(f, lineBuf);
    }

    // SWITCH_PWM_VALUE_1
    for (int i_switch=0; i_switch<MAX_MATRIX_SWITCHES; i_switch++) {
        snprintf(lineBuf, sizeof(lineBuf), "%s,%d,%lu", tag_pwm_1, i_switch, (unsigned long)matrixData.output_pwm[0][i_switch][1]);
        printLine(f, lineBuf);
    }

    // SWITCH_FLUX
    for (int i_switch=0; i_switch<MAX_MATRIX_SWITCHES; i_switch++) {
        snprintf(lineBuf, sizeof(lineBuf), "%s,%d,%ld", tag_flux, i_switch, (long)matrixData.flux_value[0][i_switch]);
        printLine(f, lineBuf);
    }

    // COMPUTER_ASSIST
    for (int i_switch=0; i_switch<MAX_MATRIX_SWITCHES; i_switch++) {
        snprintf(lineBuf, sizeof(lineBuf), "%s,%d,%d", tag_comp_assist, i_switch, (int)matrixData.computer_assist[0][i_switch]);
        printLine(f, lineBuf);
    }

    // MAP_SLOT
    for (int i_switch=0; i_switch<MAX_MATRIX_SWITCHES; i_switch++) {
        snprintf(lineBuf, sizeof(lineBuf), "%s,%d,%d", tag_map_slot, i_switch, (int)matrixData.index_mapped_value[0][i_switch]);
        printLine(f, lineBuf);
    }

    // XYZ_MODE_X
    for (int i_switch=0; i_switch<MAX_MATRIX_SWITCHES; i_switch++) {
        for (int i_func=0; i_func<MAX_MATRIX_SWITCH_FUNCTIONS; i_func++) {
            snprintf(lineBuf, sizeof(lineBuf), "%s,%d,%d,%d", tag_xyz_mode_x, i_switch, i_func, (int)matrixData.matrix_function_mode_xyz[0][i_switch][i_func][INDEX_MATRIX_FUNTION_X]);
            printLine(f, lineBuf);
        }
    }

    // XYZ_MODE_Y
    for (int i_switch=0; i_switch<MAX_MATRIX_SWITCHES; i_switch++) {
        for (int i_func=0; i_func<MAX_MATRIX_SWITCH_FUNCTIONS; i_func++) {
            snprintf(lineBuf, sizeof(lineBuf), "%s,%d,%d,%d", tag_xyz_mode_y, i_switch, i_func, (int)matrixData.matrix_function_mode_xyz[0][i_switch][i_func][INDEX_MATRIX_FUNTION_Y]);
            printLine(f, lineBuf);
        }
    }

    // XYZ_MODE_Z
    for (int i_switch=0; i_switch<MAX_MATRIX_SWITCHES; i_switch++) {
        for (int i_func=0; i_func<MAX_MATRIX_SWITCH_FUNCTIONS; i_func++) {
            snprintf(lineBuf, sizeof(lineBuf), "%s,%d,%d,%d", tag_xyz_mode_z, i_switch, i_func, (int)matrixData.matrix_function_mode_xyz[0][i_switch][i_func][INDEX_MATRIX_FUNTION_Z]);
            printLine(f, lineBuf);
        }
    }
    
    fclose(f);
    printf("[saveMatrixFile] done.\n");
    return true;
}

bool loadMatrixFile() {
    printf("[loadMatrixFile] Attempting to load matrix file...\n");

    if (sd_exists(SatIOFileData.matix_filepaths[SatIOFileData.i_current_matrix_file_path]) == false) { printf("[loadMatrixFile] Could not find matrix file.\n"); return false; }
    
    FILE* f = sd_fopen(SatIOFileData.matix_filepaths[SatIOFileData.i_current_matrix_file_path], "r");
    if (f == NULL) { printf("[loadMatrixFile] Failed to open matrix file.\n"); return false; }
    
    printf("[loadMatrixFile] attempting to clear matrix...");
    override_all_computer_assists();
    set_all_matrix_default(); // avoid mixing current values with loaded values

    char lineBuffer[1024];
    int currentTag = 0;
    while (fgets(lineBuffer, sizeof(lineBuffer), f) != NULL) {
        size_t len = strlen(lineBuffer);
        while (len > 0 && (lineBuffer[len-1] == '\n' || lineBuffer[len-1] == '\r')) { lineBuffer[--len] = '\0'; }
        if (len == 0) continue;
        // printf("Processing Tag Token Number: %d (data: %s)\n", currentTag, lineBuffer); // uncomment to debug
        
        char *token = strtok(lineBuffer, ",");
        int tokenCount = 0;
        signed int tag_index = -1;
        for (int i_find_tag=0; i_find_tag<MAX_MATRIX_TAGS; i_find_tag++) {if (strcmp(getMatrixTag(i_find_tag), token)==0) {tag_index=i_find_tag; break;}}
        if (tag_index==-1) {printf("Unrecognized tag found in matrix file: %s\n", token); continue;}

        String data_0; String data_1; String data_2;
        token = strtok(NULL, ","); // remove tag
        while (token != NULL) {if (tokenCount==0) {data_0=token;} else if (tokenCount==1) {data_1=token;} else if (tokenCount==2) {data_2=token;} token = strtok(NULL, ","); tokenCount++;}

        if      (tag_index==SWITCH_PORT) {if (str_is_int8(data_0.c_str()) && str_is_int8(data_1.c_str())) {matrixData.matrix_port_map[0][atoi(data_0.c_str())]=atoi(data_1.c_str());} matrixData.matrix_switch_write_required[0][atoi(data_0.c_str())]=true;}
        else if (tag_index==SWITCH_OPCA) {if (str_is_int8(data_0.c_str()) && str_is_uint8(data_1.c_str())) {matrixData.output_portcontroller_address[0][atoi(data_0.c_str())]=(uint8_t)atoi(data_1.c_str());} matrixData.matrix_switch_write_required[0][atoi(data_0.c_str())]=true;}
        else if (tag_index==SWITCH_FUNCTION) {if (str_is_int8(data_0.c_str()) && str_is_int8(data_1.c_str()) && str_is_int8(data_2.c_str())) {matrixData.matrix_function[0][atoi(data_0.c_str())][atoi(data_1.c_str())]=atoi(data_2.c_str());} matrixData.matrix_switch_write_required[0][atoi(data_0.c_str())]=true;}
        else if (tag_index==FUNCTION_X) {if (str_is_int8(data_0.c_str()) && str_is_int8(data_1.c_str()) && str_is_double(data_2.c_str())) {matrixData.matrix_function_xyz[0][atoi(data_0.c_str())][atoi(data_1.c_str())][INDEX_MATRIX_FUNTION_X]=strtod(data_2.c_str(), NULL);} matrixData.matrix_switch_write_required[0][atoi(data_0.c_str())]=true;}
        else if (tag_index==FUNCTION_Y) {if (str_is_int8(data_0.c_str()) && str_is_int8(data_1.c_str()) && str_is_double(data_2.c_str())) {matrixData.matrix_function_xyz[0][atoi(data_0.c_str())][atoi(data_1.c_str())][INDEX_MATRIX_FUNTION_Y]=strtod(data_2.c_str(), NULL);} matrixData.matrix_switch_write_required[0][atoi(data_0.c_str())]=true;}
        else if (tag_index==FUNCTION_Z) {if (str_is_int8(data_0.c_str()) && str_is_int8(data_1.c_str()) && str_is_double(data_2.c_str())) {matrixData.matrix_function_xyz[0][atoi(data_0.c_str())][atoi(data_1.c_str())][INDEX_MATRIX_FUNTION_Z]=strtod(data_2.c_str(), NULL);} matrixData.matrix_switch_write_required[0][atoi(data_0.c_str())]=true;}
        else if (tag_index==FUNCTION_OPERATOR) {if (str_is_int8(data_0.c_str()) && str_is_int8(data_1.c_str()) && str_is_int8(data_2.c_str())) {matrixData.matrix_switch_operator_index[0][atoi(data_0.c_str())][atoi(data_1.c_str())]=atoi(data_2.c_str());} matrixData.matrix_switch_write_required[0][atoi(data_0.c_str())]=true;}
        else if (tag_index==FUNCTION_INVERT) {if (str_is_int8(data_0.c_str()) && str_is_int8(data_1.c_str()) && str_is_int8(data_2.c_str())) {matrixData.matrix_switch_inverted_logic[0][atoi(data_0.c_str())][atoi(data_1.c_str())]=atoi(data_2.c_str());} matrixData.matrix_switch_write_required[0][atoi(data_0.c_str())]=true;}
        else if (tag_index==SWITCH_OUTPUT_MODE) {if (str_is_int8(data_0.c_str()) && str_is_int8(data_1.c_str())) {matrixData.output_mode[0][atoi(data_0.c_str())]=atoi(data_1.c_str());} matrixData.matrix_switch_write_required[0][atoi(data_0.c_str())]=true;}
        else if (tag_index==SWITCH_PWM_VALUE_0) {if (str_is_int8(data_0.c_str()) && str_is_uint32(data_1.c_str())) {matrixData.output_pwm[0][atoi(data_0.c_str())][INDEX_MATRIX_SWITCH_PWM_OFF]=strtoul(data_1.c_str(), NULL, 10);} matrixData.matrix_switch_write_required[0][atoi(data_0.c_str())]=true;}
        else if (tag_index==SWITCH_PWM_VALUE_1) {if (str_is_int8(data_0.c_str()) && str_is_uint32(data_1.c_str())) {matrixData.output_pwm[0][atoi(data_0.c_str())][INDEX_MATRIX_SWITCH_PWM_ON]=strtoul(data_1.c_str(), NULL, 10);} matrixData.matrix_switch_write_required[0][atoi(data_0.c_str())]=true;}
        else if (tag_index==SWITCH_FLUX) {if (str_is_int8(data_0.c_str()) && str_is_long(data_1.c_str())) {matrixData.flux_value[0][atoi(data_0.c_str())]=strtol(data_1.c_str(), NULL, 10);} matrixData.matrix_switch_write_required[0][atoi(data_0.c_str())]=true;}
        else if (tag_index==COMPUTER_ASSIST) {if (str_is_int8(data_0.c_str()) && str_is_bool(data_1.c_str())) {matrixData.computer_assist[0][atoi(data_0.c_str())]=atoi(data_1.c_str());} matrixData.matrix_switch_write_required[0][atoi(data_0.c_str())]=true;}
        else if (tag_index==MAP_SLOT) {if (str_is_int8(data_0.c_str()) && str_is_int8(data_1.c_str())) {matrixData.index_mapped_value[0][atoi(data_0.c_str())]=atoi(data_1.c_str());}}
        else if (tag_index==XYZ_MODE_X) {if (str_is_int8(data_0.c_str()) && str_is_int8(data_1.c_str()) && str_is_int8(data_2.c_str())) {matrixData.matrix_function_mode_xyz[0][atoi(data_0.c_str())][atoi(data_1.c_str())][INDEX_MATRIX_FUNTION_X]=atoi(data_2.c_str());} matrixData.matrix_switch_write_required[0][atoi(data_0.c_str())]=true;}
        else if (tag_index==XYZ_MODE_Y) {if (str_is_int8(data_0.c_str()) && str_is_int8(data_1.c_str()) && str_is_int8(data_2.c_str())) {matrixData.matrix_function_mode_xyz[0][atoi(data_0.c_str())][atoi(data_1.c_str())][INDEX_MATRIX_FUNTION_Y]=atoi(data_2.c_str());} matrixData.matrix_switch_write_required[0][atoi(data_0.c_str())]=true;}
        else if (tag_index==XYZ_MODE_Z) {if (str_is_int8(data_0.c_str()) && str_is_int8(data_1.c_str()) && str_is_int8(data_2.c_str())) {matrixData.matrix_function_mode_xyz[0][atoi(data_0.c_str())][atoi(data_1.c_str())][INDEX_MATRIX_FUNTION_Z]=atoi(data_2.c_str());} matrixData.matrix_switch_write_required[0][atoi(data_0.c_str())]=true;}
        currentTag++;
    }
    fclose(f);
    if (currentTag == 0) {printf("[loadMatrixFile] No tags found in matrix file.\n"); return false;}
    printf("[loadMatrixFile] done.\n");
    return true;
}

bool deleteMatrixFile() {
    if (sd_exists(SatIOFileData.matix_filepaths[SatIOFileData.i_current_matrix_file_path])) {if (sd_remove(SatIOFileData.matix_filepaths[SatIOFileData.i_current_matrix_file_path])) {printf("[deleteMatrixFile] done.\n"); return true;}}
    printf("[deleteMatrixFile] Failed.\n");
    return false;
}

// ---------------------------------------------------------------------------------------------------------------------------
// SYSTEM
// ---------------------------------------------------------------------------------------------------------------------------

/**
 * Enum and get tag are for order agnostic tag+data reads/writes for ease of maintenance/ammendments.
 * Symmetry is only required between the enum typedef and getTag() function.
 */
typedef enum {
    SYSTEM_FILE_MATRIX_FILE = 0,
    SYSTEM_FILE_LOAD_MATRIX_ON_STARTUP,
    SYSTEM_FILE_LOGGING,

    SYSTEM_FILE_SERIAL_COMMAND,
    SYSTEM_FILE_OUTPUT_ALL,
    SYSTEM_FILE_OUTPUT_SatIO,
    SYSTEM_FILE_OUTPUT_INS,
    SYSTEM_FILE_OUTPUT_GNGGA,
    SYSTEM_FILE_OUTPUT_GNRMC,
    SYSTEM_FILE_OUTPUT_GPATT,
    SYSTEM_FILE_OUTPUT_MATRIX,
    SYSTEM_FILE_OUTPUT_ADMPLEX0,
    SYSTEM_FILE_OUTPUT_ADMPLEX1,
    SYSTEM_FILE_OUTPUT_GYRO0,

    SYSTEM_FILE_OUTPUT_SUN,
    SYSTEM_FILE_OUTPUT_MERCURY,
    SYSTEM_FILE_OUTPUT_VENUS,
    SYSTEM_FILE_OUTPUT_EARTH,
    SYSTEM_FILE_OUTPUT_LUNA,
    SYSTEM_FILE_OUTPUT_MARS,
    SYSTEM_FILE_OUTPUT_JUPITER,
    SYSTEM_FILE_OUTPUT_SATURN,
    SYSTEM_FILE_OUTPUT_URANUS,
    SYSTEM_FILE_OUTPUT_NEPTUNE,
    SYSTEM_FILE_OUTPUT_METEORS,

    SYSTEM_FILE_UTC_SECOND_OFFSET,
    SYSTEM_FILE_UTC_AUTO_OFFSET_FLAG,
    SYSTEM_FILE_SET_DATETIME_AUTOMATICALLY,

    SYSTEM_FILE_INS_REQ_GPS_PRECISION,
    SYSTEM_FILE_INS_REQ_MIN_SPEED,
    SYSTEM_FILE_INS_REQ_HEADING_RANGE_DIFF,
    SYSTEM_FILE_INS_MODE,
    SYSTEM_FILE_INS_USE_GYRO_HEADING,

    SYSTEM_FILE_USER_LATITUDE,
    SYSTEM_FILE_USER_LONGITUDE,
    SYSTEM_FILE_USER_SPEED,
    SYSTEM_FILE_USER_GROUND_HEADING,
    SYSTEM_FILE_USER_ALTITUDE,

    SYSTEM_FILE_SatIO_LOCATION_VALUE_MODE,
    SYSTEM_FILE_SatIO_ALTITUDE_VALUE_MODE,
    SYSTEM_FILE_SatIO_SPEED_VALUE_MODE,
    SYSTEM_FILE_SatIO_GROUND_HEADING_VALUE_MODE,

    SYSTEM_FILE_ADMPLEX0_CH_ENABLED,
    SYSTEM_FILE_ADMPLEX1_CH_ENABLED,
    SYSTEM_FILE_ADMPLEX0_CH_FREQ,
    SYSTEM_FILE_ADMPLEX1_CH_FREQ,
    SYSTEM_FILE_PCI_0_CH_ENABLED,
    SYSTEM_FILE_PCI_0_CH_FREQ,
    SYSTEM_FILE_PCI_1_CH_ENABLED,
    SYSTEM_FILE_PCI_1_CH_FREQ,
    SYSTEM_FILE_PCI_2_CH_ENABLED,
    SYSTEM_FILE_PCI_2_CH_FREQ,
    SYSTEM_FILE_PCI_3_CH_ENABLED,
    SYSTEM_FILE_PCI_3_CH_FREQ,
    SYSTEM_FILE_PCI_4_CH_ENABLED,
    SYSTEM_FILE_PCI_4_CH_FREQ,
    SYSTEM_FILE_PCI_5_CH_ENABLED,
    SYSTEM_FILE_PCI_5_CH_FREQ,
    SYSTEM_FILE_PCI_6_CH_ENABLED,
    SYSTEM_FILE_PCI_6_CH_FREQ,
    SYSTEM_FILE_PCI_7_CH_ENABLED,
    SYSTEM_FILE_PCI_7_CH_FREQ,
    SYSTEM_FILE_PCI_8_CH_ENABLED,
    SYSTEM_FILE_PCI_8_CH_FREQ,
    SYSTEM_FILE_PCI_9_CH_ENABLED,
    SYSTEM_FILE_PCI_9_CH_FREQ,
    SYSTEM_FILE_PCI_10_CH_ENABLED,
    SYSTEM_FILE_PCI_10_CH_FREQ,
    SYSTEM_FILE_PCI_11_CH_ENABLED,
    SYSTEM_FILE_PCI_11_CH_FREQ,
    SYSTEM_FILE_PCI_12_CH_ENABLED,
    SYSTEM_FILE_PCI_12_CH_FREQ,
    SYSTEM_FILE_PCI_13_CH_ENABLED,
    SYSTEM_FILE_PCI_13_CH_FREQ,
    SYSTEM_FILE_PCI_14_CH_ENABLED,
    SYSTEM_FILE_PCI_14_CH_FREQ,
    SYSTEM_FILE_PCI_15_CH_ENABLED,
    SYSTEM_FILE_PCI_15_CH_FREQ,
    SYSTEM_FILE_PCI_16_CH_ENABLED,
    SYSTEM_FILE_PCI_16_CH_FREQ,
    SYSTEM_FILE_PCI_17_CH_ENABLED,
    SYSTEM_FILE_PCI_17_CH_FREQ,
    SYSTEM_FILE_PCI_18_CH_ENABLED,
    SYSTEM_FILE_PCI_18_CH_FREQ,
    SYSTEM_FILE_PCI_19_CH_ENABLED,
    SYSTEM_FILE_PCI_19_CH_FREQ,
    SYSTEM_FILE_PCI_20_CH_ENABLED,
    SYSTEM_FILE_PCI_20_CH_FREQ,
    SYSTEM_FILE_PCI_21_CH_ENABLED,
    SYSTEM_FILE_PCI_21_CH_FREQ,
    SYSTEM_FILE_PCI_22_CH_ENABLED,
    SYSTEM_FILE_PCI_22_CH_FREQ,
    SYSTEM_FILE_PCI_23_CH_ENABLED,
    SYSTEM_FILE_PCI_23_CH_FREQ,
    SYSTEM_FILE_PCI_24_CH_ENABLED,
    SYSTEM_FILE_PCI_24_CH_FREQ,
    SYSTEM_FILE_PCI_25_CH_ENABLED,
    SYSTEM_FILE_PCI_25_CH_FREQ,
    SYSTEM_FILE_PCI_26_CH_ENABLED,
    SYSTEM_FILE_PCI_26_CH_FREQ,
    SYSTEM_FILE_PCI_27_CH_ENABLED,
    SYSTEM_FILE_PCI_27_CH_FREQ,
    SYSTEM_FILE_PCI_28_CH_ENABLED,
    SYSTEM_FILE_PCI_28_CH_FREQ,
    SYSTEM_FILE_PCI_29_CH_ENABLED,
    SYSTEM_FILE_PCI_29_CH_FREQ,
    SYSTEM_FILE_PCI_30_CH_ENABLED,
    SYSTEM_FILE_PCI_30_CH_FREQ,
    SYSTEM_FILE_PCI_31_CH_ENABLED,
    SYSTEM_FILE_PCI_31_CH_FREQ,
    SYSTEM_FILE_PCI_32_CH_ENABLED,
    SYSTEM_FILE_PCI_32_CH_FREQ,
    SYSTEM_FILE_PCI_33_CH_ENABLED,
    SYSTEM_FILE_PCI_33_CH_FREQ,
    SYSTEM_FILE_PCI_34_CH_ENABLED,
    SYSTEM_FILE_PCI_34_CH_FREQ,
    SYSTEM_FILE_PCI_35_CH_ENABLED,
    SYSTEM_FILE_PCI_35_CH_FREQ,
    SYSTEM_FILE_PCI_36_CH_ENABLED,
    SYSTEM_FILE_PCI_36_CH_FREQ,
    SYSTEM_FILE_PCI_37_CH_ENABLED,
    SYSTEM_FILE_PCI_37_CH_FREQ,
    SYSTEM_FILE_PCI_38_CH_ENABLED,
    SYSTEM_FILE_PCI_38_CH_FREQ,
    SYSTEM_FILE_PCI_39_CH_ENABLED,
    SYSTEM_FILE_PCI_39_CH_FREQ,
    SYSTEM_FILE_PCI_40_CH_ENABLED,
    SYSTEM_FILE_PCI_40_CH_FREQ,
    SYSTEM_FILE_PCI_41_CH_ENABLED,
    SYSTEM_FILE_PCI_41_CH_FREQ,
    SYSTEM_FILE_PCI_42_CH_ENABLED,
    SYSTEM_FILE_PCI_42_CH_FREQ,
    SYSTEM_FILE_PCI_43_CH_ENABLED,
    SYSTEM_FILE_PCI_43_CH_FREQ,
    SYSTEM_FILE_PCI_44_CH_ENABLED,
    SYSTEM_FILE_PCI_44_CH_FREQ,
    SYSTEM_FILE_PCI_45_CH_ENABLED,
    SYSTEM_FILE_PCI_45_CH_FREQ,
    SYSTEM_FILE_PCI_46_CH_ENABLED,
    SYSTEM_FILE_PCI_46_CH_FREQ,
    SYSTEM_FILE_PCI_47_CH_ENABLED,
    SYSTEM_FILE_PCI_47_CH_FREQ,
    SYSTEM_FILE_PCI_48_CH_ENABLED,
    SYSTEM_FILE_PCI_48_CH_FREQ,
    SYSTEM_FILE_PCI_49_CH_ENABLED,
    SYSTEM_FILE_PCI_49_CH_FREQ,
    SYSTEM_FILE_PCI_50_CH_ENABLED,
    SYSTEM_FILE_PCI_50_CH_FREQ,
    SYSTEM_FILE_PCI_51_CH_ENABLED,
    SYSTEM_FILE_PCI_51_CH_FREQ,
    SYSTEM_FILE_PCI_52_CH_ENABLED,
    SYSTEM_FILE_PCI_52_CH_FREQ,
    SYSTEM_FILE_PCI_53_CH_ENABLED,
    SYSTEM_FILE_PCI_53_CH_FREQ,
    SYSTEM_FILE_PCI_54_CH_ENABLED,
    SYSTEM_FILE_PCI_54_CH_FREQ,
    SYSTEM_FILE_PCI_55_CH_ENABLED,
    SYSTEM_FILE_PCI_55_CH_FREQ,
    SYSTEM_FILE_PCI_56_CH_ENABLED,
    SYSTEM_FILE_PCI_56_CH_FREQ,
    SYSTEM_FILE_PCI_57_CH_ENABLED,
    SYSTEM_FILE_PCI_57_CH_FREQ,
    SYSTEM_FILE_PCI_58_CH_ENABLED,
    SYSTEM_FILE_PCI_58_CH_FREQ,
    SYSTEM_FILE_PCI_59_CH_ENABLED,
    SYSTEM_FILE_PCI_59_CH_FREQ,
    SYSTEM_FILE_PCI_60_CH_ENABLED,
    SYSTEM_FILE_PCI_60_CH_FREQ,
    SYSTEM_FILE_PCI_61_CH_ENABLED,
    SYSTEM_FILE_PCI_61_CH_FREQ,
    SYSTEM_FILE_PCI_62_CH_ENABLED,
    SYSTEM_FILE_PCI_62_CH_FREQ,
    SYSTEM_FILE_PCI_63_CH_ENABLED,
    SYSTEM_FILE_PCI_63_CH_FREQ,
    SYSTEM_FILE_PCI_64_CH_ENABLED,
    SYSTEM_FILE_PCI_64_CH_FREQ,
    SYSTEM_FILE_PCI_65_CH_ENABLED,
    SYSTEM_FILE_PCI_65_CH_FREQ,
    SYSTEM_FILE_PCI_66_CH_ENABLED,
    SYSTEM_FILE_PCI_66_CH_FREQ,
    SYSTEM_FILE_PCI_67_CH_ENABLED,
    SYSTEM_FILE_PCI_67_CH_FREQ,
    SYSTEM_FILE_PCI_68_CH_ENABLED,
    SYSTEM_FILE_PCI_68_CH_FREQ,
    SYSTEM_FILE_PCI_69_CH_ENABLED,
    SYSTEM_FILE_PCI_69_CH_FREQ,
    SYSTEM_FILE_PCI_70_CH_ENABLED,
    SYSTEM_FILE_PCI_70_CH_FREQ,
    SYSTEM_FILE_PCI_71_CH_ENABLED,
    SYSTEM_FILE_PCI_71_CH_FREQ,
    SYSTEM_FILE_PCI_72_CH_ENABLED,
    SYSTEM_FILE_PCI_72_CH_FREQ,
    SYSTEM_FILE_PCI_73_CH_ENABLED,
    SYSTEM_FILE_PCI_73_CH_FREQ,
    SYSTEM_FILE_PCI_74_CH_ENABLED,
    SYSTEM_FILE_PCI_74_CH_FREQ,
    SYSTEM_FILE_PCI_75_CH_ENABLED,
    SYSTEM_FILE_PCI_75_CH_FREQ,
    SYSTEM_FILE_PCI_76_CH_ENABLED,
    SYSTEM_FILE_PCI_76_CH_FREQ,
    SYSTEM_FILE_PCI_77_CH_ENABLED,
    SYSTEM_FILE_PCI_77_CH_FREQ,
    SYSTEM_FILE_PCI_78_CH_ENABLED,
    SYSTEM_FILE_PCI_78_CH_FREQ,
    SYSTEM_FILE_PCI_79_CH_ENABLED,
    SYSTEM_FILE_PCI_79_CH_FREQ,
    SYSTEM_FILE_PCI_80_CH_ENABLED,
    SYSTEM_FILE_PCI_80_CH_FREQ,
    SYSTEM_FILE_PCI_81_CH_ENABLED,
    SYSTEM_FILE_PCI_81_CH_FREQ,
    SYSTEM_FILE_PCI_82_CH_ENABLED,
    SYSTEM_FILE_PCI_82_CH_FREQ,
    SYSTEM_FILE_PCI_83_CH_ENABLED,
    SYSTEM_FILE_PCI_83_CH_FREQ,
    SYSTEM_FILE_PCI_84_CH_ENABLED,
    SYSTEM_FILE_PCI_84_CH_FREQ,
    SYSTEM_FILE_PCI_85_CH_ENABLED,
    SYSTEM_FILE_PCI_85_CH_FREQ,
    SYSTEM_FILE_PCI_86_CH_ENABLED,
    SYSTEM_FILE_PCI_86_CH_FREQ,
    SYSTEM_FILE_PCI_87_CH_ENABLED,
    SYSTEM_FILE_PCI_87_CH_FREQ,
    SYSTEM_FILE_PCI_88_CH_ENABLED,
    SYSTEM_FILE_PCI_88_CH_FREQ,
    SYSTEM_FILE_PCI_89_CH_ENABLED,
    SYSTEM_FILE_PCI_89_CH_FREQ,
    SYSTEM_FILE_PCI_90_CH_ENABLED,
    SYSTEM_FILE_PCI_90_CH_FREQ,
    SYSTEM_FILE_PCI_91_CH_ENABLED,
    SYSTEM_FILE_PCI_91_CH_FREQ,
    SYSTEM_FILE_PCI_92_CH_ENABLED,
    SYSTEM_FILE_PCI_92_CH_FREQ,
    SYSTEM_FILE_PCI_93_CH_ENABLED,
    SYSTEM_FILE_PCI_93_CH_FREQ,
    SYSTEM_FILE_PCI_94_CH_ENABLED,
    SYSTEM_FILE_PCI_94_CH_FREQ,
    SYSTEM_FILE_PCI_95_CH_ENABLED,
    SYSTEM_FILE_PCI_95_CH_FREQ,
    SYSTEM_FILE_PCI_96_CH_ENABLED,
    SYSTEM_FILE_PCI_96_CH_FREQ,
    SYSTEM_FILE_PCI_97_CH_ENABLED,
    SYSTEM_FILE_PCI_97_CH_FREQ,
    SYSTEM_FILE_PCI_98_CH_ENABLED,
    SYSTEM_FILE_PCI_98_CH_FREQ,
    SYSTEM_FILE_PCI_99_CH_ENABLED,
    SYSTEM_FILE_PCI_99_CH_FREQ,
    SYSTEM_FILE_PCI_100_CH_ENABLED,
    SYSTEM_FILE_PCI_100_CH_FREQ,
    SYSTEM_FILE_PCI_101_CH_ENABLED,
    SYSTEM_FILE_PCI_101_CH_FREQ,
    SYSTEM_FILE_PCI_102_CH_ENABLED,
    SYSTEM_FILE_PCI_102_CH_FREQ,
    SYSTEM_FILE_PCI_103_CH_ENABLED,
    SYSTEM_FILE_PCI_103_CH_FREQ,
    SYSTEM_FILE_PCI_104_CH_ENABLED,
    SYSTEM_FILE_PCI_104_CH_FREQ,
    SYSTEM_FILE_PCI_105_CH_ENABLED,
    SYSTEM_FILE_PCI_105_CH_FREQ,
    SYSTEM_FILE_PCI_106_CH_ENABLED,
    SYSTEM_FILE_PCI_106_CH_FREQ,
    SYSTEM_FILE_PCI_107_CH_ENABLED,
    SYSTEM_FILE_PCI_107_CH_FREQ,
    SYSTEM_FILE_PCI_108_CH_ENABLED,
    SYSTEM_FILE_PCI_108_CH_FREQ,
    SYSTEM_FILE_PCI_109_CH_ENABLED,
    SYSTEM_FILE_PCI_109_CH_FREQ,
    SYSTEM_FILE_PCI_110_CH_ENABLED,
    SYSTEM_FILE_PCI_110_CH_FREQ,
    SYSTEM_FILE_PCI_111_CH_ENABLED,
    SYSTEM_FILE_PCI_111_CH_FREQ,
    SYSTEM_FILE_PCI_112_CH_ENABLED,
    SYSTEM_FILE_PCI_112_CH_FREQ,
    SYSTEM_FILE_PCI_113_CH_ENABLED,
    SYSTEM_FILE_PCI_113_CH_FREQ,
    SYSTEM_FILE_PCI_114_CH_ENABLED,
    SYSTEM_FILE_PCI_114_CH_FREQ,
    SYSTEM_FILE_PCI_115_CH_ENABLED,
    SYSTEM_FILE_PCI_115_CH_FREQ,
    SYSTEM_FILE_PCI_116_CH_ENABLED,
    SYSTEM_FILE_PCI_116_CH_FREQ,
    SYSTEM_FILE_PCI_117_CH_ENABLED,
    SYSTEM_FILE_PCI_117_CH_FREQ,
    SYSTEM_FILE_PCI_118_CH_ENABLED,
    SYSTEM_FILE_PCI_118_CH_FREQ,
    SYSTEM_FILE_PCI_119_CH_ENABLED,
    SYSTEM_FILE_PCI_119_CH_FREQ,
    SYSTEM_FILE_PCI_120_CH_ENABLED,
    SYSTEM_FILE_PCI_120_CH_FREQ,
    SYSTEM_FILE_PCI_121_CH_ENABLED,
    SYSTEM_FILE_PCI_121_CH_FREQ,
    SYSTEM_FILE_PCI_122_CH_ENABLED,
    SYSTEM_FILE_PCI_122_CH_FREQ,
    SYSTEM_FILE_PCI_123_CH_ENABLED,
    SYSTEM_FILE_PCI_123_CH_FREQ,
    SYSTEM_FILE_PCI_124_CH_ENABLED,
    SYSTEM_FILE_PCI_124_CH_FREQ,
    SYSTEM_FILE_PCI_125_CH_ENABLED,
    SYSTEM_FILE_PCI_125_CH_FREQ,
    SYSTEM_FILE_PCI_126_CH_ENABLED,
    SYSTEM_FILE_PCI_126_CH_FREQ,
    SYSTEM_FILE_PCI_127_CH_ENABLED,
    SYSTEM_FILE_PCI_127_CH_FREQ,

    SYSTEM_FILE_PWRCFG_NAME,
    SYSTEM_FILE_PWRCFG_GPS,
    SYSTEM_FILE_PWRCFG_ADMPLEX0,
    SYSTEM_FILE_PWRCFG_ADMPLEX1,
    SYSTEM_FILE_PWRCFG_GYRO,
    SYSTEM_FILE_PWRCFG_UNIVERSE,
    SYSTEM_FILE_PWRCFG_SWITCHES,
    SYSTEM_FILE_PWRCFG_PORTCONTROLLER_INPUT,
    SYSTEM_FILE_PWRCFG_STORAGE,
    SYSTEM_FILE_PWRCFG_DISPLAY,
    SYSTEM_FILE_PWRCFG_SatIO_SERIAL_TX,
} system_tag_t;

/* Rule 7.4: a string literal's type is "array of const char", so the
   function returning one must return const char* rather than char*.
   Rule 8.7: internal linkage; only used within this file. */
static const char * getSystemTag(int t) {
    switch (t) {
        case SYSTEM_FILE_MATRIX_FILE:                    return "MATRIX_FILE";
        case SYSTEM_FILE_LOAD_MATRIX_ON_STARTUP:         return "LOAD_MATRIX_ON_STARTUP";
        case SYSTEM_FILE_LOGGING:                        return "LOGGING";

        case SYSTEM_FILE_SERIAL_COMMAND:                 return "SERIAL_COMMAND";
        case SYSTEM_FILE_OUTPUT_ALL:                     return "OUTPUT_ALL";
        case SYSTEM_FILE_OUTPUT_SatIO:                   return "OUTPUT_SatIO";
        case SYSTEM_FILE_OUTPUT_INS:                     return "OUTPUT_INS";
        case SYSTEM_FILE_OUTPUT_GNGGA:                   return "OUTPUT_GNGGA";
        case SYSTEM_FILE_OUTPUT_GNRMC:                   return "OUTPUT_GNRMC";
        case SYSTEM_FILE_OUTPUT_GPATT:                   return "OUTPUT_GPATT";
        case SYSTEM_FILE_OUTPUT_MATRIX:                  return "OUTPUT_MATRIX";
        case SYSTEM_FILE_OUTPUT_ADMPLEX0:                return "OUTPUT_ADMPLEX0";
        case SYSTEM_FILE_OUTPUT_ADMPLEX1:                return "OUTPUT_ADMPLEX1";
        case SYSTEM_FILE_OUTPUT_GYRO0:                   return "OUTPUT_GYRO0";

        case SYSTEM_FILE_OUTPUT_SUN:                     return "OUTPUT_SUN";
        case SYSTEM_FILE_OUTPUT_MERCURY:                 return "OUTPUT_MERCURY";
        case SYSTEM_FILE_OUTPUT_VENUS:                   return "OUTPUT_VENUS";
        case SYSTEM_FILE_OUTPUT_EARTH:                   return "OUTPUT_EARTH";
        case SYSTEM_FILE_OUTPUT_LUNA:                    return "OUTPUT_LUNA";
        case SYSTEM_FILE_OUTPUT_MARS:                    return "OUTPUT_MARS";
        case SYSTEM_FILE_OUTPUT_JUPITER:                 return "OUTPUT_JUPITER";
        case SYSTEM_FILE_OUTPUT_SATURN:                  return "OUTPUT_SATURN";
        case SYSTEM_FILE_OUTPUT_URANUS:                  return "OUTPUT_URANUS";
        case SYSTEM_FILE_OUTPUT_NEPTUNE:                 return "OUTPUT_NEPTUNE";
        case SYSTEM_FILE_OUTPUT_METEORS:                 return "OUTPUT_METEORS";

        case SYSTEM_FILE_UTC_SECOND_OFFSET:              return "UTC_SECOND_OFFSET";
        case SYSTEM_FILE_UTC_AUTO_OFFSET_FLAG:           return "UTC_AUTO_OFFSET_FLAG";
        case SYSTEM_FILE_SET_DATETIME_AUTOMATICALLY:     return "SET_DATETIME_AUTOMATICALLY";

        case SYSTEM_FILE_INS_REQ_GPS_PRECISION:          return "INS_REQ_GPS_PRECISION";
        case SYSTEM_FILE_INS_REQ_MIN_SPEED:              return "INS_REQ_MIN_SPEED";
        case SYSTEM_FILE_INS_REQ_HEADING_RANGE_DIFF:     return "INS_REQ_HEADING_RANGE_DIFF";
        case SYSTEM_FILE_INS_MODE:                       return "INS_MODE";
        case SYSTEM_FILE_INS_USE_GYRO_HEADING:           return "INS_USE_GYRO_HEADING";

        case SYSTEM_FILE_USER_LATITUDE:                  return "USER_LATITUDE";
        case SYSTEM_FILE_USER_LONGITUDE:                 return "USER_LONGITUDE";
        case SYSTEM_FILE_USER_SPEED:                     return "USER_SPEED";
        case SYSTEM_FILE_USER_GROUND_HEADING:            return "USER_HEADING";
        case SYSTEM_FILE_USER_ALTITUDE:                  return "USER_ALTITUDE";

        case SYSTEM_FILE_SatIO_LOCATION_VALUE_MODE:      return "SatIO_LOCATION_VALUE_MODE";
        case SYSTEM_FILE_SatIO_ALTITUDE_VALUE_MODE:      return "SatIO_ALTITUDE_VALUE_MODE";
        case SYSTEM_FILE_SatIO_SPEED_VALUE_MODE:         return "SatIO_SPEED_VALUE_MODE";
        case SYSTEM_FILE_SatIO_GROUND_HEADING_VALUE_MODE:return "SatIO_GROUND_HEADING_VALUE_MODE";

        case SYSTEM_FILE_ADMPLEX0_CH_ENABLED:            return "ADMPLEX0_CH_ENABLED";
        case SYSTEM_FILE_ADMPLEX1_CH_ENABLED:            return "ADMPLEX1_CH_ENABLED";
        case SYSTEM_FILE_ADMPLEX0_CH_FREQ:               return "ADMPLEX0_CH_FREQ";
        case SYSTEM_FILE_ADMPLEX1_CH_FREQ:               return "ADMPLEX1_CH_FREQ";
        case SYSTEM_FILE_PCI_0_CH_ENABLED:               return "PCI_0_CH_ENABLED";
        case SYSTEM_FILE_PCI_0_CH_FREQ:                  return "PCI_0_CH_FREQ";
        case SYSTEM_FILE_PCI_1_CH_ENABLED:               return "PCI_1_CH_ENABLED";
        case SYSTEM_FILE_PCI_1_CH_FREQ:                  return "PCI_1_CH_FREQ";
        case SYSTEM_FILE_PCI_2_CH_ENABLED:               return "PCI_2_CH_ENABLED";
        case SYSTEM_FILE_PCI_2_CH_FREQ:                  return "PCI_2_CH_FREQ";
        case SYSTEM_FILE_PCI_3_CH_ENABLED:               return "PCI_3_CH_ENABLED";
        case SYSTEM_FILE_PCI_3_CH_FREQ:                  return "PCI_3_CH_FREQ";
        case SYSTEM_FILE_PCI_4_CH_ENABLED:               return "PCI_4_CH_ENABLED";
        case SYSTEM_FILE_PCI_4_CH_FREQ:                  return "PCI_4_CH_FREQ";
        case SYSTEM_FILE_PCI_5_CH_ENABLED:               return "PCI_5_CH_ENABLED";
        case SYSTEM_FILE_PCI_5_CH_FREQ:                  return "PCI_5_CH_FREQ";
        case SYSTEM_FILE_PCI_6_CH_ENABLED:               return "PCI_6_CH_ENABLED";
        case SYSTEM_FILE_PCI_6_CH_FREQ:                  return "PCI_6_CH_FREQ";
        case SYSTEM_FILE_PCI_7_CH_ENABLED:               return "PCI_7_CH_ENABLED";
        case SYSTEM_FILE_PCI_7_CH_FREQ:                  return "PCI_7_CH_FREQ";
        case SYSTEM_FILE_PCI_8_CH_ENABLED:               return "PCI_8_CH_ENABLED";
        case SYSTEM_FILE_PCI_8_CH_FREQ:                  return "PCI_8_CH_FREQ";
        case SYSTEM_FILE_PCI_9_CH_ENABLED:               return "PCI_9_CH_ENABLED";
        case SYSTEM_FILE_PCI_9_CH_FREQ:                  return "PCI_9_CH_FREQ";
        case SYSTEM_FILE_PCI_10_CH_ENABLED:               return "PCI_10_CH_ENABLED";
        case SYSTEM_FILE_PCI_10_CH_FREQ:                  return "PCI_10_CH_FREQ";
        case SYSTEM_FILE_PCI_11_CH_ENABLED:               return "PCI_11_CH_ENABLED";
        case SYSTEM_FILE_PCI_11_CH_FREQ:                  return "PCI_11_CH_FREQ";
        case SYSTEM_FILE_PCI_12_CH_ENABLED:               return "PCI_12_CH_ENABLED";
        case SYSTEM_FILE_PCI_12_CH_FREQ:                  return "PCI_12_CH_FREQ";
        case SYSTEM_FILE_PCI_13_CH_ENABLED:               return "PCI_13_CH_ENABLED";
        case SYSTEM_FILE_PCI_13_CH_FREQ:                  return "PCI_13_CH_FREQ";
        case SYSTEM_FILE_PCI_14_CH_ENABLED:               return "PCI_14_CH_ENABLED";
        case SYSTEM_FILE_PCI_14_CH_FREQ:                  return "PCI_14_CH_FREQ";
        case SYSTEM_FILE_PCI_15_CH_ENABLED:               return "PCI_15_CH_ENABLED";
        case SYSTEM_FILE_PCI_15_CH_FREQ:                  return "PCI_15_CH_FREQ";
        case SYSTEM_FILE_PCI_16_CH_ENABLED:               return "PCI_16_CH_ENABLED";
        case SYSTEM_FILE_PCI_16_CH_FREQ:                  return "PCI_16_CH_FREQ";
        case SYSTEM_FILE_PCI_17_CH_ENABLED:               return "PCI_17_CH_ENABLED";
        case SYSTEM_FILE_PCI_17_CH_FREQ:                  return "PCI_17_CH_FREQ";
        case SYSTEM_FILE_PCI_18_CH_ENABLED:               return "PCI_18_CH_ENABLED";
        case SYSTEM_FILE_PCI_18_CH_FREQ:                  return "PCI_18_CH_FREQ";
        case SYSTEM_FILE_PCI_19_CH_ENABLED:               return "PCI_19_CH_ENABLED";
        case SYSTEM_FILE_PCI_19_CH_FREQ:                  return "PCI_19_CH_FREQ";
        case SYSTEM_FILE_PCI_20_CH_ENABLED:               return "PCI_20_CH_ENABLED";
        case SYSTEM_FILE_PCI_20_CH_FREQ:                  return "PCI_20_CH_FREQ";
        case SYSTEM_FILE_PCI_21_CH_ENABLED:               return "PCI_21_CH_ENABLED";
        case SYSTEM_FILE_PCI_21_CH_FREQ:                  return "PCI_21_CH_FREQ";
        case SYSTEM_FILE_PCI_22_CH_ENABLED:               return "PCI_22_CH_ENABLED";
        case SYSTEM_FILE_PCI_22_CH_FREQ:                  return "PCI_22_CH_FREQ";
        case SYSTEM_FILE_PCI_23_CH_ENABLED:               return "PCI_23_CH_ENABLED";
        case SYSTEM_FILE_PCI_23_CH_FREQ:                  return "PCI_23_CH_FREQ";
        case SYSTEM_FILE_PCI_24_CH_ENABLED:               return "PCI_24_CH_ENABLED";
        case SYSTEM_FILE_PCI_24_CH_FREQ:                  return "PCI_24_CH_FREQ";
        case SYSTEM_FILE_PCI_25_CH_ENABLED:               return "PCI_25_CH_ENABLED";
        case SYSTEM_FILE_PCI_25_CH_FREQ:                  return "PCI_25_CH_FREQ";
        case SYSTEM_FILE_PCI_26_CH_ENABLED:               return "PCI_26_CH_ENABLED";
        case SYSTEM_FILE_PCI_26_CH_FREQ:                  return "PCI_26_CH_FREQ";
        case SYSTEM_FILE_PCI_27_CH_ENABLED:               return "PCI_27_CH_ENABLED";
        case SYSTEM_FILE_PCI_27_CH_FREQ:                  return "PCI_27_CH_FREQ";
        case SYSTEM_FILE_PCI_28_CH_ENABLED:               return "PCI_28_CH_ENABLED";
        case SYSTEM_FILE_PCI_28_CH_FREQ:                  return "PCI_28_CH_FREQ";
        case SYSTEM_FILE_PCI_29_CH_ENABLED:               return "PCI_29_CH_ENABLED";
        case SYSTEM_FILE_PCI_29_CH_FREQ:                  return "PCI_29_CH_FREQ";
        case SYSTEM_FILE_PCI_30_CH_ENABLED:               return "PCI_30_CH_ENABLED";
        case SYSTEM_FILE_PCI_30_CH_FREQ:                  return "PCI_30_CH_FREQ";
        case SYSTEM_FILE_PCI_31_CH_ENABLED:               return "PCI_31_CH_ENABLED";
        case SYSTEM_FILE_PCI_31_CH_FREQ:                  return "PCI_31_CH_FREQ";
        case SYSTEM_FILE_PCI_32_CH_ENABLED:               return "PCI_32_CH_ENABLED";
        case SYSTEM_FILE_PCI_32_CH_FREQ:                  return "PCI_32_CH_FREQ";
        case SYSTEM_FILE_PCI_33_CH_ENABLED:               return "PCI_33_CH_ENABLED";
        case SYSTEM_FILE_PCI_33_CH_FREQ:                  return "PCI_33_CH_FREQ";
        case SYSTEM_FILE_PCI_34_CH_ENABLED:               return "PCI_34_CH_ENABLED";
        case SYSTEM_FILE_PCI_34_CH_FREQ:                  return "PCI_34_CH_FREQ";
        case SYSTEM_FILE_PCI_35_CH_ENABLED:               return "PCI_35_CH_ENABLED";
        case SYSTEM_FILE_PCI_35_CH_FREQ:                  return "PCI_35_CH_FREQ";
        case SYSTEM_FILE_PCI_36_CH_ENABLED:               return "PCI_36_CH_ENABLED";
        case SYSTEM_FILE_PCI_36_CH_FREQ:                  return "PCI_36_CH_FREQ";
        case SYSTEM_FILE_PCI_37_CH_ENABLED:               return "PCI_37_CH_ENABLED";
        case SYSTEM_FILE_PCI_37_CH_FREQ:                  return "PCI_37_CH_FREQ";
        case SYSTEM_FILE_PCI_38_CH_ENABLED:               return "PCI_38_CH_ENABLED";
        case SYSTEM_FILE_PCI_38_CH_FREQ:                  return "PCI_38_CH_FREQ";
        case SYSTEM_FILE_PCI_39_CH_ENABLED:               return "PCI_39_CH_ENABLED";
        case SYSTEM_FILE_PCI_39_CH_FREQ:                  return "PCI_39_CH_FREQ";
        case SYSTEM_FILE_PCI_40_CH_ENABLED:               return "PCI_40_CH_ENABLED";
        case SYSTEM_FILE_PCI_40_CH_FREQ:                  return "PCI_40_CH_FREQ";
        case SYSTEM_FILE_PCI_41_CH_ENABLED:               return "PCI_41_CH_ENABLED";
        case SYSTEM_FILE_PCI_41_CH_FREQ:                  return "PCI_41_CH_FREQ";
        case SYSTEM_FILE_PCI_42_CH_ENABLED:               return "PCI_42_CH_ENABLED";
        case SYSTEM_FILE_PCI_42_CH_FREQ:                  return "PCI_42_CH_FREQ";
        case SYSTEM_FILE_PCI_43_CH_ENABLED:               return "PCI_43_CH_ENABLED";
        case SYSTEM_FILE_PCI_43_CH_FREQ:                  return "PCI_43_CH_FREQ";
        case SYSTEM_FILE_PCI_44_CH_ENABLED:               return "PCI_44_CH_ENABLED";
        case SYSTEM_FILE_PCI_44_CH_FREQ:                  return "PCI_44_CH_FREQ";
        case SYSTEM_FILE_PCI_45_CH_ENABLED:               return "PCI_45_CH_ENABLED";
        case SYSTEM_FILE_PCI_45_CH_FREQ:                  return "PCI_45_CH_FREQ";
        case SYSTEM_FILE_PCI_46_CH_ENABLED:               return "PCI_46_CH_ENABLED";
        case SYSTEM_FILE_PCI_46_CH_FREQ:                  return "PCI_46_CH_FREQ";
        case SYSTEM_FILE_PCI_47_CH_ENABLED:               return "PCI_47_CH_ENABLED";
        case SYSTEM_FILE_PCI_47_CH_FREQ:                  return "PCI_47_CH_FREQ";
        case SYSTEM_FILE_PCI_48_CH_ENABLED:               return "PCI_48_CH_ENABLED";
        case SYSTEM_FILE_PCI_48_CH_FREQ:                  return "PCI_48_CH_FREQ";
        case SYSTEM_FILE_PCI_49_CH_ENABLED:               return "PCI_49_CH_ENABLED";
        case SYSTEM_FILE_PCI_49_CH_FREQ:                  return "PCI_49_CH_FREQ";
        case SYSTEM_FILE_PCI_50_CH_ENABLED:               return "PCI_50_CH_ENABLED";
        case SYSTEM_FILE_PCI_50_CH_FREQ:                  return "PCI_50_CH_FREQ";
        case SYSTEM_FILE_PCI_51_CH_ENABLED:               return "PCI_51_CH_ENABLED";
        case SYSTEM_FILE_PCI_51_CH_FREQ:                  return "PCI_51_CH_FREQ";
        case SYSTEM_FILE_PCI_52_CH_ENABLED:               return "PCI_52_CH_ENABLED";
        case SYSTEM_FILE_PCI_52_CH_FREQ:                  return "PCI_52_CH_FREQ";
        case SYSTEM_FILE_PCI_53_CH_ENABLED:               return "PCI_53_CH_ENABLED";
        case SYSTEM_FILE_PCI_53_CH_FREQ:                  return "PCI_53_CH_FREQ";
        case SYSTEM_FILE_PCI_54_CH_ENABLED:               return "PCI_54_CH_ENABLED";
        case SYSTEM_FILE_PCI_54_CH_FREQ:                  return "PCI_54_CH_FREQ";
        case SYSTEM_FILE_PCI_55_CH_ENABLED:               return "PCI_55_CH_ENABLED";
        case SYSTEM_FILE_PCI_55_CH_FREQ:                  return "PCI_55_CH_FREQ";
        case SYSTEM_FILE_PCI_56_CH_ENABLED:               return "PCI_56_CH_ENABLED";
        case SYSTEM_FILE_PCI_56_CH_FREQ:                  return "PCI_56_CH_FREQ";
        case SYSTEM_FILE_PCI_57_CH_ENABLED:               return "PCI_57_CH_ENABLED";
        case SYSTEM_FILE_PCI_57_CH_FREQ:                  return "PCI_57_CH_FREQ";
        case SYSTEM_FILE_PCI_58_CH_ENABLED:               return "PCI_58_CH_ENABLED";
        case SYSTEM_FILE_PCI_58_CH_FREQ:                  return "PCI_58_CH_FREQ";
        case SYSTEM_FILE_PCI_59_CH_ENABLED:               return "PCI_59_CH_ENABLED";
        case SYSTEM_FILE_PCI_59_CH_FREQ:                  return "PCI_59_CH_FREQ";
        case SYSTEM_FILE_PCI_60_CH_ENABLED:               return "PCI_60_CH_ENABLED";
        case SYSTEM_FILE_PCI_60_CH_FREQ:                  return "PCI_60_CH_FREQ";
        case SYSTEM_FILE_PCI_61_CH_ENABLED:               return "PCI_61_CH_ENABLED";
        case SYSTEM_FILE_PCI_61_CH_FREQ:                  return "PCI_61_CH_FREQ";
        case SYSTEM_FILE_PCI_62_CH_ENABLED:               return "PCI_62_CH_ENABLED";
        case SYSTEM_FILE_PCI_62_CH_FREQ:                  return "PCI_62_CH_FREQ";
        case SYSTEM_FILE_PCI_63_CH_ENABLED:               return "PCI_63_CH_ENABLED";
        case SYSTEM_FILE_PCI_63_CH_FREQ:                  return "PCI_63_CH_FREQ";
        case SYSTEM_FILE_PCI_64_CH_ENABLED:               return "PCI_64_CH_ENABLED";
        case SYSTEM_FILE_PCI_64_CH_FREQ:                  return "PCI_64_CH_FREQ";
        case SYSTEM_FILE_PCI_65_CH_ENABLED:               return "PCI_65_CH_ENABLED";
        case SYSTEM_FILE_PCI_65_CH_FREQ:                  return "PCI_65_CH_FREQ";
        case SYSTEM_FILE_PCI_66_CH_ENABLED:               return "PCI_66_CH_ENABLED";
        case SYSTEM_FILE_PCI_66_CH_FREQ:                  return "PCI_66_CH_FREQ";
        case SYSTEM_FILE_PCI_67_CH_ENABLED:               return "PCI_67_CH_ENABLED";
        case SYSTEM_FILE_PCI_67_CH_FREQ:                  return "PCI_67_CH_FREQ";
        case SYSTEM_FILE_PCI_68_CH_ENABLED:               return "PCI_68_CH_ENABLED";
        case SYSTEM_FILE_PCI_68_CH_FREQ:                  return "PCI_68_CH_FREQ";
        case SYSTEM_FILE_PCI_69_CH_ENABLED:               return "PCI_69_CH_ENABLED";
        case SYSTEM_FILE_PCI_69_CH_FREQ:                  return "PCI_69_CH_FREQ";
        case SYSTEM_FILE_PCI_70_CH_ENABLED:               return "PCI_70_CH_ENABLED";
        case SYSTEM_FILE_PCI_70_CH_FREQ:                  return "PCI_70_CH_FREQ";
        case SYSTEM_FILE_PCI_71_CH_ENABLED:               return "PCI_71_CH_ENABLED";
        case SYSTEM_FILE_PCI_71_CH_FREQ:                  return "PCI_71_CH_FREQ";
        case SYSTEM_FILE_PCI_72_CH_ENABLED:               return "PCI_72_CH_ENABLED";
        case SYSTEM_FILE_PCI_72_CH_FREQ:                  return "PCI_72_CH_FREQ";
        case SYSTEM_FILE_PCI_73_CH_ENABLED:               return "PCI_73_CH_ENABLED";
        case SYSTEM_FILE_PCI_73_CH_FREQ:                  return "PCI_73_CH_FREQ";
        case SYSTEM_FILE_PCI_74_CH_ENABLED:               return "PCI_74_CH_ENABLED";
        case SYSTEM_FILE_PCI_74_CH_FREQ:                  return "PCI_74_CH_FREQ";
        case SYSTEM_FILE_PCI_75_CH_ENABLED:               return "PCI_75_CH_ENABLED";
        case SYSTEM_FILE_PCI_75_CH_FREQ:                  return "PCI_75_CH_FREQ";
        case SYSTEM_FILE_PCI_76_CH_ENABLED:               return "PCI_76_CH_ENABLED";
        case SYSTEM_FILE_PCI_76_CH_FREQ:                  return "PCI_76_CH_FREQ";
        case SYSTEM_FILE_PCI_77_CH_ENABLED:               return "PCI_77_CH_ENABLED";
        case SYSTEM_FILE_PCI_77_CH_FREQ:                  return "PCI_77_CH_FREQ";
        case SYSTEM_FILE_PCI_78_CH_ENABLED:               return "PCI_78_CH_ENABLED";
        case SYSTEM_FILE_PCI_78_CH_FREQ:                  return "PCI_78_CH_FREQ";
        case SYSTEM_FILE_PCI_79_CH_ENABLED:               return "PCI_79_CH_ENABLED";
        case SYSTEM_FILE_PCI_79_CH_FREQ:                  return "PCI_79_CH_FREQ";
        case SYSTEM_FILE_PCI_80_CH_ENABLED:               return "PCI_80_CH_ENABLED";
        case SYSTEM_FILE_PCI_80_CH_FREQ:                  return "PCI_80_CH_FREQ";
        case SYSTEM_FILE_PCI_81_CH_ENABLED:               return "PCI_81_CH_ENABLED";
        case SYSTEM_FILE_PCI_81_CH_FREQ:                  return "PCI_81_CH_FREQ";
        case SYSTEM_FILE_PCI_82_CH_ENABLED:               return "PCI_82_CH_ENABLED";
        case SYSTEM_FILE_PCI_82_CH_FREQ:                  return "PCI_82_CH_FREQ";
        case SYSTEM_FILE_PCI_83_CH_ENABLED:               return "PCI_83_CH_ENABLED";
        case SYSTEM_FILE_PCI_83_CH_FREQ:                  return "PCI_83_CH_FREQ";
        case SYSTEM_FILE_PCI_84_CH_ENABLED:               return "PCI_84_CH_ENABLED";
        case SYSTEM_FILE_PCI_84_CH_FREQ:                  return "PCI_84_CH_FREQ";
        case SYSTEM_FILE_PCI_85_CH_ENABLED:               return "PCI_85_CH_ENABLED";
        case SYSTEM_FILE_PCI_85_CH_FREQ:                  return "PCI_85_CH_FREQ";
        case SYSTEM_FILE_PCI_86_CH_ENABLED:               return "PCI_86_CH_ENABLED";
        case SYSTEM_FILE_PCI_86_CH_FREQ:                  return "PCI_86_CH_FREQ";
        case SYSTEM_FILE_PCI_87_CH_ENABLED:               return "PCI_87_CH_ENABLED";
        case SYSTEM_FILE_PCI_87_CH_FREQ:                  return "PCI_87_CH_FREQ";
        case SYSTEM_FILE_PCI_88_CH_ENABLED:               return "PCI_88_CH_ENABLED";
        case SYSTEM_FILE_PCI_88_CH_FREQ:                  return "PCI_88_CH_FREQ";
        case SYSTEM_FILE_PCI_89_CH_ENABLED:               return "PCI_89_CH_ENABLED";
        case SYSTEM_FILE_PCI_89_CH_FREQ:                  return "PCI_89_CH_FREQ";
        case SYSTEM_FILE_PCI_90_CH_ENABLED:               return "PCI_90_CH_ENABLED";
        case SYSTEM_FILE_PCI_90_CH_FREQ:                  return "PCI_90_CH_FREQ";
        case SYSTEM_FILE_PCI_91_CH_ENABLED:               return "PCI_91_CH_ENABLED";
        case SYSTEM_FILE_PCI_91_CH_FREQ:                  return "PCI_91_CH_FREQ";
        case SYSTEM_FILE_PCI_92_CH_ENABLED:               return "PCI_92_CH_ENABLED";
        case SYSTEM_FILE_PCI_92_CH_FREQ:                  return "PCI_92_CH_FREQ";
        case SYSTEM_FILE_PCI_93_CH_ENABLED:               return "PCI_93_CH_ENABLED";
        case SYSTEM_FILE_PCI_93_CH_FREQ:                  return "PCI_93_CH_FREQ";
        case SYSTEM_FILE_PCI_94_CH_ENABLED:               return "PCI_94_CH_ENABLED";
        case SYSTEM_FILE_PCI_94_CH_FREQ:                  return "PCI_94_CH_FREQ";
        case SYSTEM_FILE_PCI_95_CH_ENABLED:               return "PCI_95_CH_ENABLED";
        case SYSTEM_FILE_PCI_95_CH_FREQ:                  return "PCI_95_CH_FREQ";
        case SYSTEM_FILE_PCI_96_CH_ENABLED:               return "PCI_96_CH_ENABLED";
        case SYSTEM_FILE_PCI_96_CH_FREQ:                  return "PCI_96_CH_FREQ";
        case SYSTEM_FILE_PCI_97_CH_ENABLED:               return "PCI_97_CH_ENABLED";
        case SYSTEM_FILE_PCI_97_CH_FREQ:                  return "PCI_97_CH_FREQ";
        case SYSTEM_FILE_PCI_98_CH_ENABLED:               return "PCI_98_CH_ENABLED";
        case SYSTEM_FILE_PCI_98_CH_FREQ:                  return "PCI_98_CH_FREQ";
        case SYSTEM_FILE_PCI_99_CH_ENABLED:               return "PCI_99_CH_ENABLED";
        case SYSTEM_FILE_PCI_99_CH_FREQ:                  return "PCI_99_CH_FREQ";
        case SYSTEM_FILE_PCI_100_CH_ENABLED:               return "PCI_100_CH_ENABLED";
        case SYSTEM_FILE_PCI_100_CH_FREQ:                  return "PCI_100_CH_FREQ";
        case SYSTEM_FILE_PCI_101_CH_ENABLED:               return "PCI_101_CH_ENABLED";
        case SYSTEM_FILE_PCI_101_CH_FREQ:                  return "PCI_101_CH_FREQ";
        case SYSTEM_FILE_PCI_102_CH_ENABLED:               return "PCI_102_CH_ENABLED";
        case SYSTEM_FILE_PCI_102_CH_FREQ:                  return "PCI_102_CH_FREQ";
        case SYSTEM_FILE_PCI_103_CH_ENABLED:               return "PCI_103_CH_ENABLED";
        case SYSTEM_FILE_PCI_103_CH_FREQ:                  return "PCI_103_CH_FREQ";
        case SYSTEM_FILE_PCI_104_CH_ENABLED:               return "PCI_104_CH_ENABLED";
        case SYSTEM_FILE_PCI_104_CH_FREQ:                  return "PCI_104_CH_FREQ";
        case SYSTEM_FILE_PCI_105_CH_ENABLED:               return "PCI_105_CH_ENABLED";
        case SYSTEM_FILE_PCI_105_CH_FREQ:                  return "PCI_105_CH_FREQ";
        case SYSTEM_FILE_PCI_106_CH_ENABLED:               return "PCI_106_CH_ENABLED";
        case SYSTEM_FILE_PCI_106_CH_FREQ:                  return "PCI_106_CH_FREQ";
        case SYSTEM_FILE_PCI_107_CH_ENABLED:               return "PCI_107_CH_ENABLED";
        case SYSTEM_FILE_PCI_107_CH_FREQ:                  return "PCI_107_CH_FREQ";
        case SYSTEM_FILE_PCI_108_CH_ENABLED:               return "PCI_108_CH_ENABLED";
        case SYSTEM_FILE_PCI_108_CH_FREQ:                  return "PCI_108_CH_FREQ";
        case SYSTEM_FILE_PCI_109_CH_ENABLED:               return "PCI_109_CH_ENABLED";
        case SYSTEM_FILE_PCI_109_CH_FREQ:                  return "PCI_109_CH_FREQ";
        case SYSTEM_FILE_PCI_110_CH_ENABLED:               return "PCI_110_CH_ENABLED";
        case SYSTEM_FILE_PCI_110_CH_FREQ:                  return "PCI_110_CH_FREQ";
        case SYSTEM_FILE_PCI_111_CH_ENABLED:               return "PCI_111_CH_ENABLED";
        case SYSTEM_FILE_PCI_111_CH_FREQ:                  return "PCI_111_CH_FREQ";
        case SYSTEM_FILE_PCI_112_CH_ENABLED:               return "PCI_112_CH_ENABLED";
        case SYSTEM_FILE_PCI_112_CH_FREQ:                  return "PCI_112_CH_FREQ";
        case SYSTEM_FILE_PCI_113_CH_ENABLED:               return "PCI_113_CH_ENABLED";
        case SYSTEM_FILE_PCI_113_CH_FREQ:                  return "PCI_113_CH_FREQ";
        case SYSTEM_FILE_PCI_114_CH_ENABLED:               return "PCI_114_CH_ENABLED";
        case SYSTEM_FILE_PCI_114_CH_FREQ:                  return "PCI_114_CH_FREQ";
        case SYSTEM_FILE_PCI_115_CH_ENABLED:               return "PCI_115_CH_ENABLED";
        case SYSTEM_FILE_PCI_115_CH_FREQ:                  return "PCI_115_CH_FREQ";
        case SYSTEM_FILE_PCI_116_CH_ENABLED:               return "PCI_116_CH_ENABLED";
        case SYSTEM_FILE_PCI_116_CH_FREQ:                  return "PCI_116_CH_FREQ";
        case SYSTEM_FILE_PCI_117_CH_ENABLED:               return "PCI_117_CH_ENABLED";
        case SYSTEM_FILE_PCI_117_CH_FREQ:                  return "PCI_117_CH_FREQ";
        case SYSTEM_FILE_PCI_118_CH_ENABLED:               return "PCI_118_CH_ENABLED";
        case SYSTEM_FILE_PCI_118_CH_FREQ:                  return "PCI_118_CH_FREQ";
        case SYSTEM_FILE_PCI_119_CH_ENABLED:               return "PCI_119_CH_ENABLED";
        case SYSTEM_FILE_PCI_119_CH_FREQ:                  return "PCI_119_CH_FREQ";
        case SYSTEM_FILE_PCI_120_CH_ENABLED:               return "PCI_120_CH_ENABLED";
        case SYSTEM_FILE_PCI_120_CH_FREQ:                  return "PCI_120_CH_FREQ";
        case SYSTEM_FILE_PCI_121_CH_ENABLED:               return "PCI_121_CH_ENABLED";
        case SYSTEM_FILE_PCI_121_CH_FREQ:                  return "PCI_121_CH_FREQ";
        case SYSTEM_FILE_PCI_122_CH_ENABLED:               return "PCI_122_CH_ENABLED";
        case SYSTEM_FILE_PCI_122_CH_FREQ:                  return "PCI_122_CH_FREQ";
        case SYSTEM_FILE_PCI_123_CH_ENABLED:               return "PCI_123_CH_ENABLED";
        case SYSTEM_FILE_PCI_123_CH_FREQ:                  return "PCI_123_CH_FREQ";
        case SYSTEM_FILE_PCI_124_CH_ENABLED:               return "PCI_124_CH_ENABLED";
        case SYSTEM_FILE_PCI_124_CH_FREQ:                  return "PCI_124_CH_FREQ";
        case SYSTEM_FILE_PCI_125_CH_ENABLED:               return "PCI_125_CH_ENABLED";
        case SYSTEM_FILE_PCI_125_CH_FREQ:                  return "PCI_125_CH_FREQ";
        case SYSTEM_FILE_PCI_126_CH_ENABLED:               return "PCI_126_CH_ENABLED";
        case SYSTEM_FILE_PCI_126_CH_FREQ:                  return "PCI_126_CH_FREQ";
        case SYSTEM_FILE_PCI_127_CH_ENABLED:               return "PCI_127_CH_ENABLED";
        case SYSTEM_FILE_PCI_127_CH_FREQ:                  return "PCI_127_CH_FREQ";

        case SYSTEM_FILE_PWRCFG_NAME:                    return "PWRCFG_NAME";
        case SYSTEM_FILE_PWRCFG_GPS:                     return "PWRCFG_GPS";
        case SYSTEM_FILE_PWRCFG_ADMPLEX0:                return "PWRCFG_ADMPLEX0";
        case SYSTEM_FILE_PWRCFG_ADMPLEX1:                return "PWRCFG_ADMPLEX1";
        case SYSTEM_FILE_PWRCFG_GYRO:                    return "PWRCFG_GYRO";
        case SYSTEM_FILE_PWRCFG_UNIVERSE:                return "PWRCFG_UNIVERSE";
        case SYSTEM_FILE_PWRCFG_SWITCHES:                return "PWRCFG_SWITCHES";
        case SYSTEM_FILE_PWRCFG_PORTCONTROLLER_INPUT:    return "PWRCFG_PORTCONTROLLER_INPUT";
        case SYSTEM_FILE_PWRCFG_STORAGE:                 return "PWRCFG_STORAGE";
        case SYSTEM_FILE_PWRCFG_DISPLAY:                 return "PWRCFG_DISPLAY";
        case SYSTEM_FILE_PWRCFG_SatIO_SERIAL_TX:         return "PWRCFG_SatIO_SERIAL_TX";

        default:                                         return "?";
    }
}

bool saveSystemFile(const char *filepath) {
    printf("[saveSystemFile] Attempting to save system file...\n");

    FILE* f = sd_fopen(filepath, "w");
    if (f == NULL) { printf("[saveSystemFile] fopen failed, errno: %d (%s)\n", errno, strerror(errno)); return false; }
    
    // Use heap-allocated buffer to avoid stack overflow
    char* lineBuf = (char*)malloc(256);
    if (lineBuf == NULL) {fclose(f); printf("[saveSystemFile] Failed to allocate memory.\n"); return false;}
    
    #define WRITE_INT_TAG(idx, val)    snprintf(lineBuf, 256, "%s,%d",   getSystemTag(idx), (int)(val));           printLine(f, lineBuf)
    #define WRITE_LONG_TAG(idx, val)   snprintf(lineBuf, 256, "%s,%ld",  getSystemTag(idx), (long)(val));          printLine(f, lineBuf)
    #define WRITE_DBL_TAG(idx, val)    snprintf(lineBuf, 256, "%s,%.6f", getSystemTag(idx), (double)(val));        printLine(f, lineBuf)
    #define WRITE_STR_TAG(idx, val)    snprintf(lineBuf, 256, "%s,%s",   getSystemTag(idx), (val));                printLine(f, lineBuf)
    #define WRITE_UINT32_TAG(idx, val) snprintf(lineBuf, 256, "%s,%lu",  getSystemTag(idx), (unsigned long)(val)); printLine(f, lineBuf)
    
    WRITE_INT_TAG(SYSTEM_FILE_MATRIX_FILE, SatIOFileData.i_current_matrix_file_path);
    WRITE_INT_TAG(SYSTEM_FILE_LOAD_MATRIX_ON_STARTUP, matrixData.load_matrix_on_startup);
    WRITE_INT_TAG(SYSTEM_FILE_LOGGING, systemData.logging_enabled);
 
    WRITE_INT_TAG(SYSTEM_FILE_SERIAL_COMMAND, systemData.serial_command);
    WRITE_INT_TAG(SYSTEM_FILE_OUTPUT_ALL, systemData.output_SatIO_all);
    WRITE_INT_TAG(SYSTEM_FILE_OUTPUT_SatIO, systemData.output_SatIO_enabled);
    WRITE_INT_TAG(SYSTEM_FILE_OUTPUT_INS, systemData.output_ins_enabled);
    WRITE_INT_TAG(SYSTEM_FILE_OUTPUT_GNGGA, systemData.output_gngga_enabled);
    WRITE_INT_TAG(SYSTEM_FILE_OUTPUT_GNRMC, systemData.output_gnrmc_enabled);
    WRITE_INT_TAG(SYSTEM_FILE_OUTPUT_GPATT, systemData.output_gpatt_enabled);
    WRITE_INT_TAG(SYSTEM_FILE_OUTPUT_MATRIX, systemData.output_matrix_enabled);
    WRITE_INT_TAG(SYSTEM_FILE_OUTPUT_ADMPLEX0, systemData.output_admplex0_enabled);
    WRITE_INT_TAG(SYSTEM_FILE_OUTPUT_ADMPLEX1, systemData.output_admplex1_enabled);
    WRITE_INT_TAG(SYSTEM_FILE_OUTPUT_GYRO0, systemData.output_gyro_0_enabled);

    WRITE_INT_TAG(SYSTEM_FILE_OUTPUT_SUN, systemData.output_sun_enabled);
    WRITE_INT_TAG(SYSTEM_FILE_OUTPUT_MERCURY, systemData.output_mercury_enabled);
    WRITE_INT_TAG(SYSTEM_FILE_OUTPUT_VENUS, systemData.output_venus_enabled);
    WRITE_INT_TAG(SYSTEM_FILE_OUTPUT_EARTH, systemData.output_earth_enabled);
    WRITE_INT_TAG(SYSTEM_FILE_OUTPUT_LUNA, systemData.output_luna_enabled);
    WRITE_INT_TAG(SYSTEM_FILE_OUTPUT_MARS, systemData.output_mars_enabled);
    WRITE_INT_TAG(SYSTEM_FILE_OUTPUT_JUPITER, systemData.output_jupiter_enabled);
    WRITE_INT_TAG(SYSTEM_FILE_OUTPUT_SATURN, systemData.output_saturn_enabled);
    WRITE_INT_TAG(SYSTEM_FILE_OUTPUT_URANUS, systemData.output_uranus_enabled);
    WRITE_INT_TAG(SYSTEM_FILE_OUTPUT_NEPTUNE, systemData.output_neptune_enabled);
    WRITE_INT_TAG(SYSTEM_FILE_OUTPUT_METEORS, systemData.output_meteors_enabled);

    WRITE_LONG_TAG(SYSTEM_FILE_UTC_SECOND_OFFSET, SatIOData.localTime.second_offset);
    WRITE_INT_TAG(SYSTEM_FILE_UTC_AUTO_OFFSET_FLAG, SatIOData.localTime.auto_offset_flag);
    // local set_time_automatically (from systemTime)
    WRITE_INT_TAG(SYSTEM_FILE_SET_DATETIME_AUTOMATICALLY, SatIOData.systemTime.set_time_automatically); // systemTime set_time_automatically (from gps)

    WRITE_DBL_TAG(SYSTEM_FILE_INS_REQ_GPS_PRECISION, insData.INS_REQ_GPS_PRECISION);
    WRITE_DBL_TAG(SYSTEM_FILE_INS_REQ_MIN_SPEED, insData.INS_REQ_MIN_SPEED);
    WRITE_DBL_TAG(SYSTEM_FILE_INS_REQ_HEADING_RANGE_DIFF, insData.INS_REQ_HEADING_RANGE_DIFF);
    WRITE_DBL_TAG(SYSTEM_FILE_INS_MODE, insData.INS_MODE);
    WRITE_INT_TAG(SYSTEM_FILE_INS_USE_GYRO_HEADING, insData.INS_USE_GYRO_HEADING);

    WRITE_DBL_TAG(SYSTEM_FILE_USER_LATITUDE, SatIOData.user_degrees_latitude);
    WRITE_DBL_TAG(SYSTEM_FILE_USER_LONGITUDE, SatIOData.user_degrees_longitude);
    WRITE_DBL_TAG(SYSTEM_FILE_USER_SPEED, SatIOData.user_speed);
    WRITE_DBL_TAG(SYSTEM_FILE_USER_GROUND_HEADING, SatIOData.user_ground_heading);
    WRITE_DBL_TAG(SYSTEM_FILE_USER_ALTITUDE, SatIOData.user_altitude);

    WRITE_INT_TAG(SYSTEM_FILE_SatIO_LOCATION_VALUE_MODE, SatIOData.location_value_mode);
    WRITE_INT_TAG(SYSTEM_FILE_SatIO_ALTITUDE_VALUE_MODE, SatIOData.altitude_value_mode);
    WRITE_INT_TAG(SYSTEM_FILE_SatIO_SPEED_VALUE_MODE, SatIOData.speed_value_mode);
    WRITE_INT_TAG(SYSTEM_FILE_SatIO_GROUND_HEADING_VALUE_MODE, SatIOData.ground_heading_value_mode);

    // ADMPLEX0_CH_ENABLED / ADMPLEX1_CH_ENABLED
    for (int i_ch=0; i_ch<MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_ADMPLEX0_CH_ENABLED), i_ch, (int)ad_mux_0.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_ADMPLEX1_CH_ENABLED), i_ch, (int)ad_mux_1.enabled[i_ch]);
        printLine(f, lineBuf);
    }

    // ADMPLEX0_CH_FREQ / ADMPLEX1_CH_FREQ
    for (int i_ch=0; i_ch<MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_ADMPLEX0_CH_FREQ), i_ch, (unsigned long long)ad_mux_0.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_ADMPLEX1_CH_FREQ), i_ch, (unsigned long long)ad_mux_1.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }

    // PCI_0: PCI_0_CH_ENABLED / PCI_0_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_0
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_0.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_0_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_0.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_0.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_0_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_0.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_0

    // PCI_1: PCI_1_CH_ENABLED / PCI_1_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_1
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_1.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_1_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_1.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_1.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_1_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_1.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_1

    // PCI_2: PCI_2_CH_ENABLED / PCI_2_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_2
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_2.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_2_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_2.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_2.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_2_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_2.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_2

    // PCI_3: PCI_3_CH_ENABLED / PCI_3_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_3
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_3.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_3_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_3.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_3.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_3_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_3.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_3

    // PCI_4: PCI_4_CH_ENABLED / PCI_4_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_4
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_4.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_4_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_4.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_4.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_4_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_4.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_4

    // PCI_5: PCI_5_CH_ENABLED / PCI_5_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_5
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_5.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_5_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_5.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_5.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_5_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_5.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_5

    // PCI_6: PCI_6_CH_ENABLED / PCI_6_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_6
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_6.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_6_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_6.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_6.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_6_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_6.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_6

    // PCI_7: PCI_7_CH_ENABLED / PCI_7_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_7
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_7.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_7_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_7.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_7.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_7_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_7.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_7

    // PCI_8: PCI_8_CH_ENABLED / PCI_8_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_8
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_8.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_8_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_8.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_8.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_8_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_8.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_8

    // PCI_9: PCI_9_CH_ENABLED / PCI_9_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_9
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_9.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_9_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_9.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_9.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_9_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_9.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_9

    // PCI_10: PCI_10_CH_ENABLED / PCI_10_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_10
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_10.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_10_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_10.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_10.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_10_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_10.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_10

    // PCI_11: PCI_11_CH_ENABLED / PCI_11_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_11
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_11.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_11_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_11.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_11.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_11_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_11.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_11

    // PCI_12: PCI_12_CH_ENABLED / PCI_12_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_12
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_12.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_12_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_12.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_12.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_12_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_12.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_12

    // PCI_13: PCI_13_CH_ENABLED / PCI_13_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_13
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_13.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_13_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_13.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_13.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_13_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_13.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_13

    // PCI_14: PCI_14_CH_ENABLED / PCI_14_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_14
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_14.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_14_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_14.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_14.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_14_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_14.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_14

    // PCI_15: PCI_15_CH_ENABLED / PCI_15_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_15
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_15.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_15_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_15.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_15.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_15_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_15.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_15

    // PCI_16: PCI_16_CH_ENABLED / PCI_16_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_16
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_16.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_16_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_16.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_16.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_16_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_16.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_16

    // PCI_17: PCI_17_CH_ENABLED / PCI_17_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_17
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_17.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_17_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_17.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_17.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_17_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_17.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_17

    // PCI_18: PCI_18_CH_ENABLED / PCI_18_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_18
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_18.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_18_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_18.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_18.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_18_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_18.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_18

    // PCI_19: PCI_19_CH_ENABLED / PCI_19_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_19
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_19.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_19_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_19.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_19.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_19_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_19.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_19

    // PCI_20: PCI_20_CH_ENABLED / PCI_20_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_20
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_20.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_20_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_20.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_20.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_20_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_20.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_20

    // PCI_21: PCI_21_CH_ENABLED / PCI_21_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_21
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_21.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_21_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_21.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_21.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_21_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_21.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_21

    // PCI_22: PCI_22_CH_ENABLED / PCI_22_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_22
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_22.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_22_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_22.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_22.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_22_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_22.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_22

    // PCI_23: PCI_23_CH_ENABLED / PCI_23_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_23
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_23.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_23_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_23.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_23.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_23_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_23.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_23

    // PCI_24: PCI_24_CH_ENABLED / PCI_24_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_24
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_24.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_24_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_24.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_24.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_24_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_24.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_24

    // PCI_25: PCI_25_CH_ENABLED / PCI_25_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_25
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_25.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_25_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_25.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_25.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_25_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_25.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_25

    // PCI_26: PCI_26_CH_ENABLED / PCI_26_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_26
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_26.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_26_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_26.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_26.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_26_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_26.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_26

    // PCI_27: PCI_27_CH_ENABLED / PCI_27_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_27
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_27.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_27_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_27.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_27.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_27_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_27.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_27

    // PCI_28: PCI_28_CH_ENABLED / PCI_28_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_28
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_28.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_28_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_28.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_28.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_28_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_28.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_28

    // PCI_29: PCI_29_CH_ENABLED / PCI_29_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_29
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_29.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_29_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_29.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_29.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_29_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_29.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_29

    // PCI_30: PCI_30_CH_ENABLED / PCI_30_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_30
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_30.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_30_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_30.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_30.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_30_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_30.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_30

    // PCI_31: PCI_31_CH_ENABLED / PCI_31_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_31
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_31.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_31_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_31.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_31.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_31_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_31.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_31

    // PCI_32: PCI_32_CH_ENABLED / PCI_32_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_32
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_32.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_32_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_32.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_32.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_32_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_32.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_32

    // PCI_33: PCI_33_CH_ENABLED / PCI_33_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_33
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_33.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_33_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_33.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_33.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_33_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_33.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_33

    // PCI_34: PCI_34_CH_ENABLED / PCI_34_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_34
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_34.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_34_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_34.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_34.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_34_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_34.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_34

    // PCI_35: PCI_35_CH_ENABLED / PCI_35_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_35
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_35.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_35_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_35.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_35.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_35_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_35.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_35

    // PCI_36: PCI_36_CH_ENABLED / PCI_36_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_36
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_36.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_36_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_36.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_36.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_36_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_36.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_36

    // PCI_37: PCI_37_CH_ENABLED / PCI_37_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_37
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_37.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_37_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_37.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_37.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_37_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_37.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_37

    // PCI_38: PCI_38_CH_ENABLED / PCI_38_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_38
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_38.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_38_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_38.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_38.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_38_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_38.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_38

    // PCI_39: PCI_39_CH_ENABLED / PCI_39_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_39
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_39.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_39_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_39.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_39.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_39_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_39.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_39

    // PCI_40: PCI_40_CH_ENABLED / PCI_40_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_40
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_40.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_40_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_40.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_40.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_40_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_40.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_40

    // PCI_41: PCI_41_CH_ENABLED / PCI_41_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_41
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_41.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_41_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_41.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_41.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_41_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_41.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_41

    // PCI_42: PCI_42_CH_ENABLED / PCI_42_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_42
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_42.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_42_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_42.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_42.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_42_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_42.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_42

    // PCI_43: PCI_43_CH_ENABLED / PCI_43_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_43
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_43.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_43_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_43.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_43.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_43_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_43.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_43

    // PCI_44: PCI_44_CH_ENABLED / PCI_44_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_44
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_44.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_44_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_44.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_44.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_44_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_44.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_44

    // PCI_45: PCI_45_CH_ENABLED / PCI_45_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_45
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_45.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_45_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_45.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_45.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_45_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_45.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_45

    // PCI_46: PCI_46_CH_ENABLED / PCI_46_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_46
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_46.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_46_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_46.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_46.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_46_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_46.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_46

    // PCI_47: PCI_47_CH_ENABLED / PCI_47_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_47
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_47.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_47_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_47.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_47.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_47_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_47.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_47

    // PCI_48: PCI_48_CH_ENABLED / PCI_48_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_48
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_48.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_48_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_48.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_48.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_48_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_48.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_48

    // PCI_49: PCI_49_CH_ENABLED / PCI_49_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_49
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_49.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_49_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_49.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_49.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_49_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_49.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_49

    // PCI_50: PCI_50_CH_ENABLED / PCI_50_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_50
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_50.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_50_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_50.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_50.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_50_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_50.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_50

    // PCI_51: PCI_51_CH_ENABLED / PCI_51_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_51
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_51.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_51_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_51.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_51.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_51_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_51.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_51

    // PCI_52: PCI_52_CH_ENABLED / PCI_52_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_52
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_52.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_52_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_52.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_52.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_52_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_52.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_52

    // PCI_53: PCI_53_CH_ENABLED / PCI_53_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_53
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_53.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_53_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_53.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_53.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_53_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_53.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_53

    // PCI_54: PCI_54_CH_ENABLED / PCI_54_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_54
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_54.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_54_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_54.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_54.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_54_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_54.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_54

    // PCI_55: PCI_55_CH_ENABLED / PCI_55_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_55
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_55.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_55_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_55.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_55.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_55_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_55.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_55

    // PCI_56: PCI_56_CH_ENABLED / PCI_56_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_56
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_56.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_56_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_56.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_56.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_56_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_56.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_56

    // PCI_57: PCI_57_CH_ENABLED / PCI_57_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_57
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_57.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_57_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_57.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_57.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_57_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_57.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_57

    // PCI_58: PCI_58_CH_ENABLED / PCI_58_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_58
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_58.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_58_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_58.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_58.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_58_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_58.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_58

    // PCI_59: PCI_59_CH_ENABLED / PCI_59_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_59
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_59.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_59_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_59.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_59.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_59_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_59.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_59

    // PCI_60: PCI_60_CH_ENABLED / PCI_60_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_60
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_60.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_60_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_60.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_60.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_60_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_60.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_60

    // PCI_61: PCI_61_CH_ENABLED / PCI_61_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_61
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_61.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_61_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_61.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_61.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_61_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_61.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_61

    // PCI_62: PCI_62_CH_ENABLED / PCI_62_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_62
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_62.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_62_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_62.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_62.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_62_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_62.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_62

    // PCI_63: PCI_63_CH_ENABLED / PCI_63_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_63
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_63.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_63_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_63.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_63.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_63_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_63.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_63

    // PCI_64: PCI_64_CH_ENABLED / PCI_64_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_64
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_64.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_64_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_64.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_64.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_64_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_64.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_64

    // PCI_65: PCI_65_CH_ENABLED / PCI_65_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_65
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_65.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_65_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_65.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_65.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_65_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_65.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_65

    // PCI_66: PCI_66_CH_ENABLED / PCI_66_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_66
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_66.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_66_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_66.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_66.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_66_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_66.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_66

    // PCI_67: PCI_67_CH_ENABLED / PCI_67_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_67
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_67.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_67_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_67.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_67.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_67_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_67.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_67

    // PCI_68: PCI_68_CH_ENABLED / PCI_68_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_68
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_68.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_68_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_68.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_68.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_68_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_68.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_68

    // PCI_69: PCI_69_CH_ENABLED / PCI_69_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_69
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_69.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_69_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_69.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_69.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_69_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_69.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_69

    // PCI_70: PCI_70_CH_ENABLED / PCI_70_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_70
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_70.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_70_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_70.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_70.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_70_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_70.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_70

    // PCI_71: PCI_71_CH_ENABLED / PCI_71_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_71
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_71.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_71_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_71.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_71.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_71_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_71.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_71

    // PCI_72: PCI_72_CH_ENABLED / PCI_72_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_72
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_72.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_72_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_72.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_72.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_72_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_72.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_72

    // PCI_73: PCI_73_CH_ENABLED / PCI_73_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_73
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_73.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_73_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_73.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_73.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_73_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_73.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_73

    // PCI_74: PCI_74_CH_ENABLED / PCI_74_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_74
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_74.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_74_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_74.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_74.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_74_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_74.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_74

    // PCI_75: PCI_75_CH_ENABLED / PCI_75_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_75
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_75.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_75_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_75.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_75.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_75_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_75.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_75

    // PCI_76: PCI_76_CH_ENABLED / PCI_76_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_76
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_76.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_76_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_76.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_76.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_76_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_76.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_76

    // PCI_77: PCI_77_CH_ENABLED / PCI_77_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_77
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_77.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_77_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_77.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_77.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_77_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_77.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_77

    // PCI_78: PCI_78_CH_ENABLED / PCI_78_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_78
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_78.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_78_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_78.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_78.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_78_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_78.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_78

    // PCI_79: PCI_79_CH_ENABLED / PCI_79_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_79
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_79.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_79_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_79.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_79.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_79_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_79.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_79

    // PCI_80: PCI_80_CH_ENABLED / PCI_80_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_80
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_80.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_80_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_80.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_80.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_80_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_80.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_80

    // PCI_81: PCI_81_CH_ENABLED / PCI_81_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_81
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_81.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_81_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_81.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_81.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_81_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_81.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_81

    // PCI_82: PCI_82_CH_ENABLED / PCI_82_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_82
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_82.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_82_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_82.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_82.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_82_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_82.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_82

    // PCI_83: PCI_83_CH_ENABLED / PCI_83_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_83
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_83.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_83_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_83.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_83.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_83_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_83.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_83

    // PCI_84: PCI_84_CH_ENABLED / PCI_84_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_84
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_84.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_84_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_84.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_84.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_84_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_84.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_84

    // PCI_85: PCI_85_CH_ENABLED / PCI_85_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_85
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_85.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_85_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_85.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_85.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_85_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_85.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_85

    // PCI_86: PCI_86_CH_ENABLED / PCI_86_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_86
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_86.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_86_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_86.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_86.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_86_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_86.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_86

    // PCI_87: PCI_87_CH_ENABLED / PCI_87_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_87
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_87.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_87_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_87.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_87.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_87_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_87.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_87

    // PCI_88: PCI_88_CH_ENABLED / PCI_88_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_88
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_88.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_88_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_88.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_88.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_88_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_88.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_88

    // PCI_89: PCI_89_CH_ENABLED / PCI_89_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_89
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_89.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_89_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_89.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_89.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_89_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_89.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_89

    // PCI_90: PCI_90_CH_ENABLED / PCI_90_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_90
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_90.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_90_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_90.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_90.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_90_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_90.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_90

    // PCI_91: PCI_91_CH_ENABLED / PCI_91_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_91
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_91.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_91_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_91.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_91.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_91_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_91.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_91

    // PCI_92: PCI_92_CH_ENABLED / PCI_92_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_92
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_92.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_92_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_92.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_92.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_92_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_92.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_92

    // PCI_93: PCI_93_CH_ENABLED / PCI_93_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_93
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_93.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_93_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_93.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_93.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_93_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_93.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_93

    // PCI_94: PCI_94_CH_ENABLED / PCI_94_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_94
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_94.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_94_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_94.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_94.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_94_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_94.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_94

    // PCI_95: PCI_95_CH_ENABLED / PCI_95_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_95
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_95.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_95_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_95.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_95.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_95_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_95.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_95

    // PCI_96: PCI_96_CH_ENABLED / PCI_96_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_96
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_96.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_96_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_96.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_96.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_96_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_96.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_96

    // PCI_97: PCI_97_CH_ENABLED / PCI_97_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_97
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_97.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_97_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_97.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_97.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_97_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_97.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_97

    // PCI_98: PCI_98_CH_ENABLED / PCI_98_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_98
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_98.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_98_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_98.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_98.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_98_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_98.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_98

    // PCI_99: PCI_99_CH_ENABLED / PCI_99_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_99
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_99.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_99_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_99.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_99.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_99_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_99.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_99

    // PCI_100: PCI_100_CH_ENABLED / PCI_100_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_100
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_100.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_100_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_100.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_100.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_100_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_100.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_100

    // PCI_101: PCI_101_CH_ENABLED / PCI_101_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_101
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_101.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_101_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_101.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_101.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_101_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_101.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_101

    // PCI_102: PCI_102_CH_ENABLED / PCI_102_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_102
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_102.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_102_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_102.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_102.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_102_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_102.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_102

    // PCI_103: PCI_103_CH_ENABLED / PCI_103_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_103
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_103.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_103_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_103.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_103.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_103_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_103.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_103

    // PCI_104: PCI_104_CH_ENABLED / PCI_104_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_104
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_104.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_104_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_104.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_104.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_104_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_104.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_104

    // PCI_105: PCI_105_CH_ENABLED / PCI_105_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_105
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_105.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_105_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_105.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_105.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_105_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_105.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_105

    // PCI_106: PCI_106_CH_ENABLED / PCI_106_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_106
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_106.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_106_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_106.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_106.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_106_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_106.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_106

    // PCI_107: PCI_107_CH_ENABLED / PCI_107_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_107
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_107.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_107_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_107.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_107.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_107_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_107.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_107

    // PCI_108: PCI_108_CH_ENABLED / PCI_108_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_108
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_108.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_108_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_108.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_108.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_108_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_108.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_108

    // PCI_109: PCI_109_CH_ENABLED / PCI_109_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_109
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_109.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_109_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_109.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_109.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_109_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_109.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_109

    // PCI_110: PCI_110_CH_ENABLED / PCI_110_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_110
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_110.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_110_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_110.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_110.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_110_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_110.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_110

    // PCI_111: PCI_111_CH_ENABLED / PCI_111_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_111
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_111.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_111_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_111.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_111.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_111_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_111.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_111

    // PCI_112: PCI_112_CH_ENABLED / PCI_112_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_112
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_112.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_112_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_112.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_112.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_112_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_112.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_112

    // PCI_113: PCI_113_CH_ENABLED / PCI_113_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_113
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_113.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_113_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_113.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_113.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_113_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_113.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_113

    // PCI_114: PCI_114_CH_ENABLED / PCI_114_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_114
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_114.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_114_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_114.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_114.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_114_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_114.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_114

    // PCI_115: PCI_115_CH_ENABLED / PCI_115_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_115
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_115.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_115_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_115.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_115.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_115_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_115.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_115

    // PCI_116: PCI_116_CH_ENABLED / PCI_116_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_116
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_116.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_116_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_116.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_116.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_116_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_116.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_116

    // PCI_117: PCI_117_CH_ENABLED / PCI_117_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_117
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_117.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_117_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_117.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_117.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_117_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_117.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_117

    // PCI_118: PCI_118_CH_ENABLED / PCI_118_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_118
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_118.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_118_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_118.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_118.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_118_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_118.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_118

    // PCI_119: PCI_119_CH_ENABLED / PCI_119_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_119
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_119.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_119_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_119.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_119.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_119_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_119.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_119

    // PCI_120: PCI_120_CH_ENABLED / PCI_120_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_120
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_120.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_120_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_120.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_120.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_120_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_120.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_120

    // PCI_121: PCI_121_CH_ENABLED / PCI_121_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_121
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_121.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_121_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_121.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_121.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_121_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_121.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_121

    // PCI_122: PCI_122_CH_ENABLED / PCI_122_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_122
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_122.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_122_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_122.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_122.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_122_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_122.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_122

    // PCI_123: PCI_123_CH_ENABLED / PCI_123_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_123
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_123.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_123_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_123.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_123.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_123_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_123.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_123

    // PCI_124: PCI_124_CH_ENABLED / PCI_124_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_124
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_124.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_124_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_124.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_124.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_124_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_124.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_124

    // PCI_125: PCI_125_CH_ENABLED / PCI_125_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_125
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_125.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_125_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_125.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_125.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_125_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_125.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_125

    // PCI_126: PCI_126_CH_ENABLED / PCI_126_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_126
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_126.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_126_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_126.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_126.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_126_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_126.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_126

    // PCI_127: PCI_127_CH_ENABLED / PCI_127_CH_FREQ
    #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_127
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_127.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_127_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_127.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_127.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_127_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_127.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_127


    // Power config: task max-frequency values (uS) currently in effect, plus the
    // active preset's display name (see PwrConfig in UnidentifiedStudios_Config.h).
    WRITE_STR_TAG(SYSTEM_FILE_PWRCFG_NAME, pwrConfigCurrent.name);
    WRITE_UINT32_TAG(SYSTEM_FILE_PWRCFG_GPS, pwrConfigCurrent.TASK_MAX_FREQ_GPS);
    #ifdef SatIO_CD74HC4067_OPTION_USE_0
    WRITE_UINT32_TAG(SYSTEM_FILE_PWRCFG_ADMPLEX0, pwrConfigCurrent.TASK_MAX_FREQ_ADMPLEX0);
    #endif
    #ifdef SatIO_CD74HC4067_OPTION_USE_1
    WRITE_UINT32_TAG(SYSTEM_FILE_PWRCFG_ADMPLEX1, pwrConfigCurrent.TASK_MAX_FREQ_ADMPLEX1);
    #endif
    WRITE_UINT32_TAG(SYSTEM_FILE_PWRCFG_GYRO, pwrConfigCurrent.TASK_MAX_FREQ_GYRO);
    WRITE_UINT32_TAG(SYSTEM_FILE_PWRCFG_UNIVERSE, pwrConfigCurrent.TASK_MAX_FREQ_UNIVERSE);
    WRITE_UINT32_TAG(SYSTEM_FILE_PWRCFG_SWITCHES, pwrConfigCurrent.TASK_MAX_FREQ_SWITCHES);
    WRITE_UINT32_TAG(SYSTEM_FILE_PWRCFG_PORTCONTROLLER_INPUT, pwrConfigCurrent.TASK_MAX_FREQ_PORTCONTROLLER_INPUT);
    WRITE_UINT32_TAG(SYSTEM_FILE_PWRCFG_STORAGE, pwrConfigCurrent.TASK_MAX_FREQ_STORAGE);
    WRITE_UINT32_TAG(SYSTEM_FILE_PWRCFG_DISPLAY, pwrConfigCurrent.TASK_MAX_FREQ_DISPLAY);
    WRITE_UINT32_TAG(SYSTEM_FILE_PWRCFG_SatIO_SERIAL_TX, pwrConfigCurrent.TASK_MAX_FREQ_SatIO_SERIAL_TX);

    #undef WRITE_INT_TAG
    #undef WRITE_LONG_TAG
    #undef WRITE_DBL_TAG
    #undef WRITE_STR_TAG
    #undef WRITE_UINT32_TAG
    
    free(lineBuf);
    fclose(f);
    printf("[saveSystemFile] done.\n");
    return true;
}

bool loadSystemFile(const char *filepath) {
    printf("[loadSystemFile] Attempting to load system file..\n");

    if (sd_exists(filepath) == false) {printf("[loadSystemFile] Could not find system file.\n"); return false;}

    FILE* f = sd_fopen(filepath, "r");
    if (f == NULL) {printf("[loadSystemFile] Could not open system file.\n"); return false;}

    char lineBuffer[256];

    #define READ_BOOL_TAG(idx, var)      if (tag_index == idx) { if (str_is_bool(val)) { var = atoi(val); } }
    #define READ_INT8_TAG(idx, var)      if (tag_index == idx) { if (str_is_int8(val)) { var = atoi(val); } }
    #define READ_LONG_TAG(idx, var)      if (tag_index == idx) { if (str_is_long(val)) { var = strtol(val, NULL, 10); } }
    #define READ_DBL_TAG(idx, var)       if (tag_index == idx) { if (str_is_double(val)) { var = strtod(val, NULL); } }
    #define READ_UINT32_TAG(idx, var)    if (tag_index == idx) { if (str_is_uint32(val)) { var = strtoul(val, NULL, 10); } }
    #define READ_STR_TAG(idx, var, size) if (tag_index == idx) { strncpy(var, val, (size)-1); var[(size)-1] = '\0'; }

    while (fgets(lineBuffer, sizeof(lineBuffer), f) != NULL) {

        size_t len = strlen(lineBuffer);
        while (len > 0 && (lineBuffer[len-1] == '\n' || lineBuffer[len-1] == '\r')) lineBuffer[--len] = '\0';
        if (len == 0) continue;

        char *token = strtok(lineBuffer, ",");
        if (token == NULL) continue;

        int tag_index = -1;
        for (int i = 0; i < MAX_SYSTEM_TAGS; i++) {if (strcmp(getSystemTag(i), token) == 0) {tag_index = i; break;}}

        if (tag_index == -1) {printf("Unrecognized tag: %s\n", token); continue;}

        char *val = strtok(NULL, ",");
        if (val == NULL) continue;

        // ADMPLEX0/1_CH_ENABLED and ADMPLEX0/1_CH_FREQ: "TAG,channel,value" (channel-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_ADMPLEX0_CH_ENABLED || tag_index == SYSTEM_FILE_ADMPLEX1_CH_ENABLED ||
            tag_index == SYSTEM_FILE_ADMPLEX0_CH_FREQ    || tag_index == SYSTEM_FILE_ADMPLEX1_CH_FREQ) {
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS) {
                    if (tag_index == SYSTEM_FILE_ADMPLEX0_CH_ENABLED && str_is_bool(val2)) {setADMultiplexerChannelEnabled(ad_mux_0, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_ADMPLEX1_CH_ENABLED && str_is_bool(val2)) {setADMultiplexerChannelEnabled(ad_mux_1, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_ADMPLEX0_CH_FREQ && str_is_uint64(val2)) {setADMultiplexerChannelFreq(ad_mux_0, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else if (tag_index == SYSTEM_FILE_ADMPLEX1_CH_FREQ && str_is_uint64(val2)) {setADMultiplexerChannelFreq(ad_mux_1, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            continue;
        }

        // PCI_0: PCI_0_CH_ENABLED and PCI_0_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_0_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_0_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_0
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_0.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_0_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_0, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_0_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_0, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_0
            continue;
        }

        // PCI_1: PCI_1_CH_ENABLED and PCI_1_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_1_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_1_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_1
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_1.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_1_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_1, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_1_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_1, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_1
            continue;
        }

        // PCI_2: PCI_2_CH_ENABLED and PCI_2_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_2_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_2_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_2
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_2.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_2_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_2, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_2_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_2, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_2
            continue;
        }

        // PCI_3: PCI_3_CH_ENABLED and PCI_3_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_3_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_3_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_3
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_3.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_3_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_3, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_3_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_3, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_3
            continue;
        }

        // PCI_4: PCI_4_CH_ENABLED and PCI_4_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_4_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_4_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_4
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_4.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_4_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_4, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_4_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_4, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_4
            continue;
        }

        // PCI_5: PCI_5_CH_ENABLED and PCI_5_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_5_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_5_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_5
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_5.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_5_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_5, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_5_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_5, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_5
            continue;
        }

        // PCI_6: PCI_6_CH_ENABLED and PCI_6_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_6_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_6_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_6
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_6.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_6_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_6, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_6_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_6, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_6
            continue;
        }

        // PCI_7: PCI_7_CH_ENABLED and PCI_7_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_7_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_7_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_7
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_7.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_7_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_7, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_7_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_7, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_7
            continue;
        }

        // PCI_8: PCI_8_CH_ENABLED and PCI_8_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_8_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_8_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_8
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_8.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_8_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_8, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_8_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_8, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_8
            continue;
        }

        // PCI_9: PCI_9_CH_ENABLED and PCI_9_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_9_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_9_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_9
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_9.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_9_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_9, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_9_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_9, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_9
            continue;
        }

        // PCI_10: PCI_10_CH_ENABLED and PCI_10_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_10_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_10_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_10
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_10.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_10_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_10, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_10_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_10, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_10
            continue;
        }

        // PCI_11: PCI_11_CH_ENABLED and PCI_11_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_11_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_11_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_11
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_11.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_11_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_11, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_11_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_11, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_11
            continue;
        }

        // PCI_12: PCI_12_CH_ENABLED and PCI_12_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_12_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_12_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_12
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_12.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_12_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_12, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_12_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_12, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_12
            continue;
        }

        // PCI_13: PCI_13_CH_ENABLED and PCI_13_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_13_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_13_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_13
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_13.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_13_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_13, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_13_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_13, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_13
            continue;
        }

        // PCI_14: PCI_14_CH_ENABLED and PCI_14_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_14_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_14_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_14
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_14.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_14_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_14, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_14_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_14, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_14
            continue;
        }

        // PCI_15: PCI_15_CH_ENABLED and PCI_15_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_15_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_15_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_15
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_15.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_15_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_15, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_15_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_15, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_15
            continue;
        }

        // PCI_16: PCI_16_CH_ENABLED and PCI_16_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_16_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_16_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_16
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_16.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_16_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_16, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_16_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_16, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_16
            continue;
        }

        // PCI_17: PCI_17_CH_ENABLED and PCI_17_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_17_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_17_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_17
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_17.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_17_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_17, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_17_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_17, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_17
            continue;
        }

        // PCI_18: PCI_18_CH_ENABLED and PCI_18_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_18_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_18_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_18
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_18.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_18_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_18, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_18_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_18, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_18
            continue;
        }

        // PCI_19: PCI_19_CH_ENABLED and PCI_19_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_19_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_19_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_19
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_19.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_19_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_19, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_19_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_19, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_19
            continue;
        }

        // PCI_20: PCI_20_CH_ENABLED and PCI_20_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_20_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_20_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_20
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_20.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_20_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_20, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_20_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_20, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_20
            continue;
        }

        // PCI_21: PCI_21_CH_ENABLED and PCI_21_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_21_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_21_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_21
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_21.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_21_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_21, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_21_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_21, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_21
            continue;
        }

        // PCI_22: PCI_22_CH_ENABLED and PCI_22_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_22_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_22_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_22
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_22.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_22_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_22, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_22_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_22, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_22
            continue;
        }

        // PCI_23: PCI_23_CH_ENABLED and PCI_23_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_23_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_23_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_23
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_23.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_23_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_23, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_23_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_23, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_23
            continue;
        }

        // PCI_24: PCI_24_CH_ENABLED and PCI_24_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_24_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_24_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_24
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_24.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_24_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_24, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_24_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_24, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_24
            continue;
        }

        // PCI_25: PCI_25_CH_ENABLED and PCI_25_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_25_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_25_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_25
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_25.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_25_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_25, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_25_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_25, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_25
            continue;
        }

        // PCI_26: PCI_26_CH_ENABLED and PCI_26_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_26_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_26_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_26
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_26.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_26_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_26, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_26_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_26, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_26
            continue;
        }

        // PCI_27: PCI_27_CH_ENABLED and PCI_27_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_27_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_27_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_27
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_27.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_27_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_27, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_27_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_27, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_27
            continue;
        }

        // PCI_28: PCI_28_CH_ENABLED and PCI_28_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_28_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_28_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_28
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_28.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_28_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_28, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_28_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_28, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_28
            continue;
        }

        // PCI_29: PCI_29_CH_ENABLED and PCI_29_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_29_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_29_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_29
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_29.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_29_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_29, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_29_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_29, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_29
            continue;
        }

        // PCI_30: PCI_30_CH_ENABLED and PCI_30_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_30_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_30_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_30
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_30.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_30_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_30, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_30_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_30, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_30
            continue;
        }

        // PCI_31: PCI_31_CH_ENABLED and PCI_31_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_31_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_31_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_31
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_31.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_31_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_31, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_31_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_31, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_31
            continue;
        }

        // PCI_32: PCI_32_CH_ENABLED and PCI_32_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_32_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_32_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_32
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_32.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_32_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_32, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_32_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_32, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_32
            continue;
        }

        // PCI_33: PCI_33_CH_ENABLED and PCI_33_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_33_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_33_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_33
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_33.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_33_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_33, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_33_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_33, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_33
            continue;
        }

        // PCI_34: PCI_34_CH_ENABLED and PCI_34_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_34_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_34_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_34
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_34.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_34_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_34, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_34_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_34, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_34
            continue;
        }

        // PCI_35: PCI_35_CH_ENABLED and PCI_35_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_35_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_35_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_35
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_35.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_35_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_35, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_35_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_35, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_35
            continue;
        }

        // PCI_36: PCI_36_CH_ENABLED and PCI_36_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_36_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_36_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_36
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_36.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_36_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_36, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_36_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_36, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_36
            continue;
        }

        // PCI_37: PCI_37_CH_ENABLED and PCI_37_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_37_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_37_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_37
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_37.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_37_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_37, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_37_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_37, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_37
            continue;
        }

        // PCI_38: PCI_38_CH_ENABLED and PCI_38_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_38_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_38_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_38
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_38.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_38_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_38, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_38_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_38, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_38
            continue;
        }

        // PCI_39: PCI_39_CH_ENABLED and PCI_39_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_39_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_39_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_39
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_39.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_39_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_39, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_39_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_39, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_39
            continue;
        }

        // PCI_40: PCI_40_CH_ENABLED and PCI_40_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_40_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_40_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_40
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_40.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_40_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_40, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_40_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_40, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_40
            continue;
        }

        // PCI_41: PCI_41_CH_ENABLED and PCI_41_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_41_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_41_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_41
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_41.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_41_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_41, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_41_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_41, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_41
            continue;
        }

        // PCI_42: PCI_42_CH_ENABLED and PCI_42_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_42_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_42_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_42
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_42.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_42_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_42, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_42_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_42, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_42
            continue;
        }

        // PCI_43: PCI_43_CH_ENABLED and PCI_43_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_43_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_43_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_43
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_43.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_43_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_43, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_43_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_43, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_43
            continue;
        }

        // PCI_44: PCI_44_CH_ENABLED and PCI_44_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_44_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_44_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_44
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_44.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_44_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_44, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_44_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_44, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_44
            continue;
        }

        // PCI_45: PCI_45_CH_ENABLED and PCI_45_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_45_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_45_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_45
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_45.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_45_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_45, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_45_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_45, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_45
            continue;
        }

        // PCI_46: PCI_46_CH_ENABLED and PCI_46_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_46_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_46_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_46
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_46.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_46_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_46, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_46_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_46, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_46
            continue;
        }

        // PCI_47: PCI_47_CH_ENABLED and PCI_47_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_47_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_47_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_47
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_47.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_47_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_47, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_47_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_47, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_47
            continue;
        }

        // PCI_48: PCI_48_CH_ENABLED and PCI_48_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_48_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_48_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_48
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_48.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_48_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_48, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_48_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_48, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_48
            continue;
        }

        // PCI_49: PCI_49_CH_ENABLED and PCI_49_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_49_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_49_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_49
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_49.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_49_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_49, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_49_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_49, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_49
            continue;
        }

        // PCI_50: PCI_50_CH_ENABLED and PCI_50_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_50_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_50_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_50
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_50.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_50_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_50, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_50_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_50, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_50
            continue;
        }

        // PCI_51: PCI_51_CH_ENABLED and PCI_51_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_51_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_51_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_51
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_51.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_51_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_51, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_51_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_51, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_51
            continue;
        }

        // PCI_52: PCI_52_CH_ENABLED and PCI_52_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_52_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_52_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_52
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_52.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_52_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_52, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_52_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_52, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_52
            continue;
        }

        // PCI_53: PCI_53_CH_ENABLED and PCI_53_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_53_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_53_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_53
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_53.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_53_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_53, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_53_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_53, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_53
            continue;
        }

        // PCI_54: PCI_54_CH_ENABLED and PCI_54_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_54_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_54_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_54
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_54.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_54_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_54, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_54_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_54, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_54
            continue;
        }

        // PCI_55: PCI_55_CH_ENABLED and PCI_55_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_55_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_55_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_55
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_55.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_55_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_55, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_55_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_55, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_55
            continue;
        }

        // PCI_56: PCI_56_CH_ENABLED and PCI_56_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_56_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_56_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_56
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_56.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_56_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_56, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_56_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_56, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_56
            continue;
        }

        // PCI_57: PCI_57_CH_ENABLED and PCI_57_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_57_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_57_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_57
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_57.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_57_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_57, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_57_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_57, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_57
            continue;
        }

        // PCI_58: PCI_58_CH_ENABLED and PCI_58_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_58_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_58_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_58
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_58.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_58_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_58, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_58_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_58, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_58
            continue;
        }

        // PCI_59: PCI_59_CH_ENABLED and PCI_59_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_59_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_59_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_59
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_59.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_59_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_59, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_59_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_59, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_59
            continue;
        }

        // PCI_60: PCI_60_CH_ENABLED and PCI_60_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_60_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_60_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_60
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_60.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_60_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_60, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_60_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_60, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_60
            continue;
        }

        // PCI_61: PCI_61_CH_ENABLED and PCI_61_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_61_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_61_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_61
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_61.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_61_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_61, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_61_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_61, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_61
            continue;
        }

        // PCI_62: PCI_62_CH_ENABLED and PCI_62_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_62_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_62_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_62
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_62.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_62_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_62, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_62_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_62, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_62
            continue;
        }

        // PCI_63: PCI_63_CH_ENABLED and PCI_63_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_63_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_63_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_63
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_63.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_63_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_63, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_63_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_63, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_63
            continue;
        }

        // PCI_64: PCI_64_CH_ENABLED and PCI_64_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_64_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_64_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_64
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_64.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_64_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_64, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_64_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_64, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_64
            continue;
        }

        // PCI_65: PCI_65_CH_ENABLED and PCI_65_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_65_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_65_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_65
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_65.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_65_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_65, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_65_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_65, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_65
            continue;
        }

        // PCI_66: PCI_66_CH_ENABLED and PCI_66_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_66_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_66_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_66
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_66.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_66_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_66, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_66_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_66, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_66
            continue;
        }

        // PCI_67: PCI_67_CH_ENABLED and PCI_67_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_67_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_67_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_67
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_67.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_67_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_67, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_67_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_67, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_67
            continue;
        }

        // PCI_68: PCI_68_CH_ENABLED and PCI_68_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_68_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_68_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_68
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_68.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_68_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_68, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_68_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_68, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_68
            continue;
        }

        // PCI_69: PCI_69_CH_ENABLED and PCI_69_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_69_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_69_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_69
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_69.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_69_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_69, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_69_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_69, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_69
            continue;
        }

        // PCI_70: PCI_70_CH_ENABLED and PCI_70_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_70_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_70_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_70
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_70.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_70_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_70, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_70_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_70, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_70
            continue;
        }

        // PCI_71: PCI_71_CH_ENABLED and PCI_71_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_71_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_71_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_71
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_71.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_71_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_71, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_71_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_71, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_71
            continue;
        }

        // PCI_72: PCI_72_CH_ENABLED and PCI_72_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_72_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_72_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_72
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_72.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_72_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_72, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_72_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_72, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_72
            continue;
        }

        // PCI_73: PCI_73_CH_ENABLED and PCI_73_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_73_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_73_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_73
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_73.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_73_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_73, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_73_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_73, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_73
            continue;
        }

        // PCI_74: PCI_74_CH_ENABLED and PCI_74_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_74_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_74_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_74
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_74.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_74_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_74, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_74_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_74, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_74
            continue;
        }

        // PCI_75: PCI_75_CH_ENABLED and PCI_75_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_75_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_75_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_75
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_75.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_75_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_75, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_75_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_75, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_75
            continue;
        }

        // PCI_76: PCI_76_CH_ENABLED and PCI_76_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_76_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_76_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_76
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_76.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_76_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_76, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_76_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_76, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_76
            continue;
        }

        // PCI_77: PCI_77_CH_ENABLED and PCI_77_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_77_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_77_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_77
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_77.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_77_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_77, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_77_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_77, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_77
            continue;
        }

        // PCI_78: PCI_78_CH_ENABLED and PCI_78_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_78_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_78_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_78
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_78.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_78_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_78, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_78_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_78, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_78
            continue;
        }

        // PCI_79: PCI_79_CH_ENABLED and PCI_79_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_79_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_79_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_79
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_79.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_79_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_79, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_79_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_79, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_79
            continue;
        }

        // PCI_80: PCI_80_CH_ENABLED and PCI_80_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_80_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_80_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_80
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_80.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_80_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_80, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_80_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_80, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_80
            continue;
        }

        // PCI_81: PCI_81_CH_ENABLED and PCI_81_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_81_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_81_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_81
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_81.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_81_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_81, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_81_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_81, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_81
            continue;
        }

        // PCI_82: PCI_82_CH_ENABLED and PCI_82_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_82_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_82_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_82
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_82.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_82_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_82, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_82_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_82, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_82
            continue;
        }

        // PCI_83: PCI_83_CH_ENABLED and PCI_83_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_83_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_83_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_83
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_83.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_83_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_83, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_83_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_83, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_83
            continue;
        }

        // PCI_84: PCI_84_CH_ENABLED and PCI_84_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_84_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_84_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_84
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_84.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_84_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_84, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_84_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_84, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_84
            continue;
        }

        // PCI_85: PCI_85_CH_ENABLED and PCI_85_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_85_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_85_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_85
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_85.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_85_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_85, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_85_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_85, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_85
            continue;
        }

        // PCI_86: PCI_86_CH_ENABLED and PCI_86_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_86_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_86_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_86
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_86.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_86_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_86, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_86_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_86, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_86
            continue;
        }

        // PCI_87: PCI_87_CH_ENABLED and PCI_87_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_87_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_87_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_87
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_87.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_87_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_87, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_87_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_87, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_87
            continue;
        }

        // PCI_88: PCI_88_CH_ENABLED and PCI_88_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_88_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_88_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_88
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_88.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_88_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_88, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_88_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_88, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_88
            continue;
        }

        // PCI_89: PCI_89_CH_ENABLED and PCI_89_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_89_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_89_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_89
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_89.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_89_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_89, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_89_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_89, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_89
            continue;
        }

        // PCI_90: PCI_90_CH_ENABLED and PCI_90_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_90_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_90_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_90
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_90.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_90_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_90, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_90_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_90, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_90
            continue;
        }

        // PCI_91: PCI_91_CH_ENABLED and PCI_91_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_91_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_91_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_91
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_91.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_91_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_91, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_91_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_91, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_91
            continue;
        }

        // PCI_92: PCI_92_CH_ENABLED and PCI_92_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_92_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_92_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_92
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_92.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_92_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_92, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_92_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_92, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_92
            continue;
        }

        // PCI_93: PCI_93_CH_ENABLED and PCI_93_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_93_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_93_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_93
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_93.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_93_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_93, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_93_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_93, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_93
            continue;
        }

        // PCI_94: PCI_94_CH_ENABLED and PCI_94_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_94_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_94_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_94
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_94.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_94_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_94, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_94_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_94, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_94
            continue;
        }

        // PCI_95: PCI_95_CH_ENABLED and PCI_95_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_95_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_95_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_95
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_95.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_95_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_95, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_95_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_95, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_95
            continue;
        }

        // PCI_96: PCI_96_CH_ENABLED and PCI_96_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_96_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_96_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_96
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_96.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_96_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_96, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_96_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_96, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_96
            continue;
        }

        // PCI_97: PCI_97_CH_ENABLED and PCI_97_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_97_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_97_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_97
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_97.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_97_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_97, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_97_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_97, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_97
            continue;
        }

        // PCI_98: PCI_98_CH_ENABLED and PCI_98_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_98_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_98_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_98
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_98.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_98_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_98, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_98_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_98, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_98
            continue;
        }

        // PCI_99: PCI_99_CH_ENABLED and PCI_99_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_99_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_99_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_99
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_99.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_99_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_99, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_99_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_99, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_99
            continue;
        }

        // PCI_100: PCI_100_CH_ENABLED and PCI_100_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_100_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_100_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_100
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_100.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_100_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_100, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_100_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_100, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_100
            continue;
        }

        // PCI_101: PCI_101_CH_ENABLED and PCI_101_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_101_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_101_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_101
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_101.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_101_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_101, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_101_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_101, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_101
            continue;
        }

        // PCI_102: PCI_102_CH_ENABLED and PCI_102_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_102_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_102_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_102
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_102.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_102_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_102, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_102_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_102, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_102
            continue;
        }

        // PCI_103: PCI_103_CH_ENABLED and PCI_103_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_103_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_103_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_103
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_103.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_103_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_103, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_103_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_103, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_103
            continue;
        }

        // PCI_104: PCI_104_CH_ENABLED and PCI_104_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_104_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_104_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_104
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_104.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_104_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_104, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_104_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_104, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_104
            continue;
        }

        // PCI_105: PCI_105_CH_ENABLED and PCI_105_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_105_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_105_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_105
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_105.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_105_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_105, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_105_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_105, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_105
            continue;
        }

        // PCI_106: PCI_106_CH_ENABLED and PCI_106_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_106_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_106_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_106
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_106.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_106_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_106, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_106_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_106, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_106
            continue;
        }

        // PCI_107: PCI_107_CH_ENABLED and PCI_107_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_107_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_107_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_107
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_107.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_107_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_107, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_107_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_107, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_107
            continue;
        }

        // PCI_108: PCI_108_CH_ENABLED and PCI_108_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_108_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_108_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_108
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_108.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_108_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_108, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_108_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_108, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_108
            continue;
        }

        // PCI_109: PCI_109_CH_ENABLED and PCI_109_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_109_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_109_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_109
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_109.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_109_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_109, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_109_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_109, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_109
            continue;
        }

        // PCI_110: PCI_110_CH_ENABLED and PCI_110_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_110_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_110_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_110
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_110.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_110_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_110, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_110_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_110, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_110
            continue;
        }

        // PCI_111: PCI_111_CH_ENABLED and PCI_111_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_111_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_111_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_111
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_111.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_111_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_111, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_111_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_111, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_111
            continue;
        }

        // PCI_112: PCI_112_CH_ENABLED and PCI_112_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_112_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_112_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_112
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_112.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_112_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_112, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_112_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_112, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_112
            continue;
        }

        // PCI_113: PCI_113_CH_ENABLED and PCI_113_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_113_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_113_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_113
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_113.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_113_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_113, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_113_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_113, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_113
            continue;
        }

        // PCI_114: PCI_114_CH_ENABLED and PCI_114_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_114_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_114_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_114
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_114.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_114_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_114, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_114_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_114, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_114
            continue;
        }

        // PCI_115: PCI_115_CH_ENABLED and PCI_115_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_115_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_115_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_115
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_115.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_115_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_115, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_115_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_115, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_115
            continue;
        }

        // PCI_116: PCI_116_CH_ENABLED and PCI_116_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_116_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_116_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_116
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_116.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_116_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_116, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_116_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_116, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_116
            continue;
        }

        // PCI_117: PCI_117_CH_ENABLED and PCI_117_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_117_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_117_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_117
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_117.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_117_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_117, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_117_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_117, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_117
            continue;
        }

        // PCI_118: PCI_118_CH_ENABLED and PCI_118_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_118_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_118_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_118
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_118.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_118_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_118, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_118_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_118, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_118
            continue;
        }

        // PCI_119: PCI_119_CH_ENABLED and PCI_119_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_119_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_119_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_119
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_119.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_119_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_119, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_119_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_119, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_119
            continue;
        }

        // PCI_120: PCI_120_CH_ENABLED and PCI_120_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_120_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_120_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_120
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_120.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_120_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_120, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_120_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_120, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_120
            continue;
        }

        // PCI_121: PCI_121_CH_ENABLED and PCI_121_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_121_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_121_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_121
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_121.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_121_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_121, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_121_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_121, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_121
            continue;
        }

        // PCI_122: PCI_122_CH_ENABLED and PCI_122_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_122_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_122_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_122
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_122.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_122_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_122, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_122_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_122, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_122
            continue;
        }

        // PCI_123: PCI_123_CH_ENABLED and PCI_123_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_123_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_123_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_123
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_123.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_123_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_123, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_123_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_123, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_123
            continue;
        }

        // PCI_124: PCI_124_CH_ENABLED and PCI_124_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_124_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_124_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_124
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_124.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_124_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_124, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_124_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_124, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_124
            continue;
        }

        // PCI_125: PCI_125_CH_ENABLED and PCI_125_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_125_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_125_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_125
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_125.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_125_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_125, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_125_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_125, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_125
            continue;
        }

        // PCI_126: PCI_126_CH_ENABLED and PCI_126_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_126_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_126_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_126
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_126.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_126_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_126, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_126_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_126, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_126
            continue;
        }

        // PCI_127: PCI_127_CH_ENABLED and PCI_127_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_127_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_127_CH_FREQ) {
            #ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_127
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_127.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_127_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_127, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_127_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_127, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_127
            continue;
        }


        READ_INT8_TAG(SYSTEM_FILE_MATRIX_FILE, SatIOFileData.i_current_matrix_file_path);
        READ_BOOL_TAG(SYSTEM_FILE_LOAD_MATRIX_ON_STARTUP, matrixData.load_matrix_on_startup);
        READ_BOOL_TAG(SYSTEM_FILE_LOGGING, systemData.logging_enabled);

        READ_BOOL_TAG(SYSTEM_FILE_SERIAL_COMMAND, systemData.serial_command);
        READ_BOOL_TAG(SYSTEM_FILE_OUTPUT_ALL, systemData.output_SatIO_all);
        READ_BOOL_TAG(SYSTEM_FILE_OUTPUT_SatIO, systemData.output_SatIO_enabled);
        READ_BOOL_TAG(SYSTEM_FILE_OUTPUT_INS, systemData.output_ins_enabled);
        READ_BOOL_TAG(SYSTEM_FILE_OUTPUT_GNGGA, systemData.output_gngga_enabled);
        READ_BOOL_TAG(SYSTEM_FILE_OUTPUT_GNRMC, systemData.output_gnrmc_enabled);
        READ_BOOL_TAG(SYSTEM_FILE_OUTPUT_GPATT, systemData.output_gpatt_enabled);
        READ_BOOL_TAG(SYSTEM_FILE_OUTPUT_MATRIX, systemData.output_matrix_enabled);
        READ_BOOL_TAG(SYSTEM_FILE_OUTPUT_ADMPLEX0, systemData.output_admplex0_enabled);
        READ_BOOL_TAG(SYSTEM_FILE_OUTPUT_ADMPLEX1, systemData.output_admplex1_enabled);
        READ_BOOL_TAG(SYSTEM_FILE_OUTPUT_GYRO0, systemData.output_gyro_0_enabled);

        READ_BOOL_TAG(SYSTEM_FILE_OUTPUT_SUN, systemData.output_sun_enabled);
        READ_BOOL_TAG(SYSTEM_FILE_OUTPUT_MERCURY, systemData.output_mercury_enabled);
        READ_BOOL_TAG(SYSTEM_FILE_OUTPUT_VENUS, systemData.output_venus_enabled);
        READ_BOOL_TAG(SYSTEM_FILE_OUTPUT_EARTH, systemData.output_earth_enabled);
        READ_BOOL_TAG(SYSTEM_FILE_OUTPUT_LUNA, systemData.output_luna_enabled);
        READ_BOOL_TAG(SYSTEM_FILE_OUTPUT_MARS, systemData.output_mars_enabled);
        READ_BOOL_TAG(SYSTEM_FILE_OUTPUT_JUPITER, systemData.output_jupiter_enabled);
        READ_BOOL_TAG(SYSTEM_FILE_OUTPUT_SATURN, systemData.output_saturn_enabled);
        READ_BOOL_TAG(SYSTEM_FILE_OUTPUT_URANUS, systemData.output_uranus_enabled);
        READ_BOOL_TAG(SYSTEM_FILE_OUTPUT_NEPTUNE, systemData.output_neptune_enabled);
        READ_BOOL_TAG(SYSTEM_FILE_OUTPUT_METEORS, systemData.output_meteors_enabled);

        READ_LONG_TAG(SYSTEM_FILE_UTC_SECOND_OFFSET, SatIOData.localTime.second_offset);
        READ_BOOL_TAG(SYSTEM_FILE_UTC_AUTO_OFFSET_FLAG, SatIOData.localTime.auto_offset_flag);
        READ_BOOL_TAG(SYSTEM_FILE_SET_DATETIME_AUTOMATICALLY, SatIOData.systemTime.set_time_automatically);

        READ_DBL_TAG(SYSTEM_FILE_INS_REQ_GPS_PRECISION, insData.INS_REQ_GPS_PRECISION);
        READ_DBL_TAG(SYSTEM_FILE_INS_REQ_MIN_SPEED, insData.INS_REQ_MIN_SPEED);
        READ_DBL_TAG(SYSTEM_FILE_INS_REQ_HEADING_RANGE_DIFF, insData.INS_REQ_HEADING_RANGE_DIFF);
        READ_DBL_TAG(SYSTEM_FILE_INS_MODE, insData.INS_MODE);
        READ_BOOL_TAG(SYSTEM_FILE_INS_USE_GYRO_HEADING, insData.INS_USE_GYRO_HEADING);

        READ_DBL_TAG(SYSTEM_FILE_USER_LATITUDE, SatIOData.user_degrees_latitude);
        READ_DBL_TAG(SYSTEM_FILE_USER_LONGITUDE, SatIOData.user_degrees_longitude);
        READ_DBL_TAG(SYSTEM_FILE_USER_SPEED, SatIOData.user_speed);
        READ_DBL_TAG(SYSTEM_FILE_USER_GROUND_HEADING, SatIOData.user_ground_heading);
        READ_DBL_TAG(SYSTEM_FILE_USER_ALTITUDE, SatIOData.user_altitude);

        READ_INT8_TAG(SYSTEM_FILE_SatIO_LOCATION_VALUE_MODE, SatIOData.location_value_mode);
        READ_INT8_TAG(SYSTEM_FILE_SatIO_ALTITUDE_VALUE_MODE, SatIOData.altitude_value_mode);
        READ_INT8_TAG(SYSTEM_FILE_SatIO_SPEED_VALUE_MODE, SatIOData.speed_value_mode);
        READ_INT8_TAG(SYSTEM_FILE_SatIO_GROUND_HEADING_VALUE_MODE, SatIOData.ground_heading_value_mode);

        READ_STR_TAG(SYSTEM_FILE_PWRCFG_NAME, pwrConfigCurrent.name, sizeof(pwrConfigCurrent.name));
        READ_UINT32_TAG(SYSTEM_FILE_PWRCFG_GPS, pwrConfigCurrent.TASK_MAX_FREQ_GPS);
        #ifdef SatIO_CD74HC4067_OPTION_USE_0
        READ_UINT32_TAG(SYSTEM_FILE_PWRCFG_ADMPLEX0, pwrConfigCurrent.TASK_MAX_FREQ_ADMPLEX0);
        #endif
        #ifdef SatIO_CD74HC4067_OPTION_USE_1
        READ_UINT32_TAG(SYSTEM_FILE_PWRCFG_ADMPLEX1, pwrConfigCurrent.TASK_MAX_FREQ_ADMPLEX1);
        #endif
        READ_UINT32_TAG(SYSTEM_FILE_PWRCFG_GYRO, pwrConfigCurrent.TASK_MAX_FREQ_GYRO);
        READ_UINT32_TAG(SYSTEM_FILE_PWRCFG_UNIVERSE, pwrConfigCurrent.TASK_MAX_FREQ_UNIVERSE);
        READ_UINT32_TAG(SYSTEM_FILE_PWRCFG_SWITCHES, pwrConfigCurrent.TASK_MAX_FREQ_SWITCHES);
        READ_UINT32_TAG(SYSTEM_FILE_PWRCFG_PORTCONTROLLER_INPUT, pwrConfigCurrent.TASK_MAX_FREQ_PORTCONTROLLER_INPUT);
        READ_UINT32_TAG(SYSTEM_FILE_PWRCFG_STORAGE, pwrConfigCurrent.TASK_MAX_FREQ_STORAGE);
        READ_UINT32_TAG(SYSTEM_FILE_PWRCFG_DISPLAY, pwrConfigCurrent.TASK_MAX_FREQ_DISPLAY);
        READ_UINT32_TAG(SYSTEM_FILE_PWRCFG_SatIO_SERIAL_TX, pwrConfigCurrent.TASK_MAX_FREQ_SatIO_SERIAL_TX);
    }

    #undef READ_BOOL_TAG
    #undef READ_INT8_TAG
    #undef READ_LONG_TAG
    #undef READ_DBL_TAG
    #undef READ_UINT32_TAG
    #undef READ_STR_TAG

    fclose(f);

    printf("[loadSystemFile] done.\n");
    return true;
}

bool deleteSystemFile(const char *filepath) {
    if (sd_exists(filepath)) {if (sd_remove(filepath)) {printf("[deleteSystemFile] done.\n"); return true;}}
    printf("[deleteSystemFile] Failed.\n");
    return false;
}

bool ran_startup_file_operations = false;

void sdcardFlagHandler() {

  // ---- MOUNT ----

//   if (sdcardFlagData.unmount_sdcard_flag==true) {
//     printf("card unmount flag detected\n");
//     sdcardData.allow_mount=false; // override allow_mount (prevent remounting on unmount flag).
//     sdcardFlagData.no_delay_flag=false; // reset flag
//     sdcardFlagData.unmount_sdcard_flag=false; // reset flag
//     sdcard_unmount();
//   }
//   else if (sdcardFlagData.mount_sdcard_flag==true) {
//     printf("card mount flag detected\n");
//     sdcardData.allow_mount=true; // override allow_mount on mount flag.
//     sdcardFlagData.no_delay_flag=false;
//     sdcardFlagData.mount_sdcard_flag=false;
//     sdcard_mount();
//   }

  if (sdcardData.sdcard_mounted==true) {

    // ---- MAPPING ----

    if (sdcardFlagData.save_mapping) {
      printf("saving mapping ...\n");
      if (saveMappingFile(SatIOFileData.mapping_filepath))
        {printf("saved mapping successfully.\n"); set_storage_success_flag(true);}
      else {printf("save mapping failed.\n"); set_storage_success_flag(false);}
      sdcardFlagData.no_delay_flag=false;
      sdcardFlagData.save_mapping=false;
    }
    else if (sdcardFlagData.load_mapping) {
      printf("loading mapping...\n");
      if (loadMappingFile(SatIOFileData.mapping_filepath))
        {printf("loaded mapping successfully.\n"); set_storage_success_flag(true);}
      else {printf("load mapping failed.\n"); set_storage_success_flag(false);}
      sdcardFlagData.no_delay_flag=false;
      sdcardFlagData.load_mapping=false;
    }
    else if (sdcardFlagData.delete_mapping) {
      printf("deleting mapping...\n");
      if (deleteMappingFile(SatIOFileData.mapping_filepath))
        {printf("deleted mapping successfully.\n"); set_storage_success_flag(true);}
      else {printf("delete mapping failed.\n"); set_storage_success_flag(false);}
      sdcardFlagData.no_delay_flag=false;
      sdcardFlagData.delete_mapping=false;
    }

    // ---- MATRIX ----

    else if (sdcardFlagData.save_matrix) {
      printf("saving matrix ...\n");
      if (saveMatrixFile())
        {printf("saved matrix successfully.\n"); set_storage_success_flag(true);}
      else {printf("save matrix failed.\n"); set_storage_success_flag(false);}
      sdcardFlagData.no_delay_flag=false;
      sdcardFlagData.save_matrix=false;
    }
    else if (sdcardFlagData.load_matrix) {
      printf("loading matrix...\n");
      if (loadMatrixFile())
        {printf("loaded matrix successfully.\n"); set_storage_success_flag(true);}
      else {printf("load matrix failed.\n"); set_storage_success_flag(false);}
      sdcardFlagData.no_delay_flag=false;
      sdcardFlagData.load_matrix=false;
    }
    else if (sdcardFlagData.delete_matrix) {
      printf("deleting matrix...\n");
      if (deleteMatrixFile())
        {printf("deleted matrix successfully.\n"); set_storage_success_flag(true);}
      else {printf("delete matrix failed.\n"); set_storage_success_flag(false);}
      sdcardFlagData.no_delay_flag=false;
      sdcardFlagData.delete_matrix=false;
    }

    // ---- SYSTEM ----

    else if (sdcardFlagData.save_system) {
      printf("saving system...\n");
      if (saveSystemFile(SatIOFileData.system_filepath))
        {printf("saved system successfully.\n"); set_storage_success_flag(true);}
      else {printf("save system failed.\n"); set_storage_success_flag(false);}
      sdcardFlagData.no_delay_flag=false;
      sdcardFlagData.save_system=false;
    }
    else if (sdcardFlagData.delete_system) {
      printf("deleting system...\n");
      if (deleteSystemFile(SatIOFileData.system_filepath))
        {printf("deleted system successfully.\n"); set_storage_success_flag(true);}
      else {printf("delete system failed.\n"); set_storage_success_flag(false);}
      sdcardFlagData.no_delay_flag=false;
      sdcardFlagData.delete_system=false;
    }

    else if (sdcardFlagData.load_system) {
      // load system
      printf("loading system...\n");
      if (loadSystemFile(SatIOFileData.system_filepath))
        {printf("loaded system successfully.\n"); set_storage_success_flag(true);}
      else {printf("load system failed.\n"); set_storage_success_flag(false);}
      sdcardFlagData.load_system=false;

      if (ran_startup_file_operations==false && matrixData.load_matrix_on_startup==true) {
        // load mapping
        printf("loading mapping...\n");
        if (loadMappingFile(SatIOFileData.mapping_filepath))
            {printf("loaded mapping successfully.\n"); set_storage_success_flag(true);}
        else {printf("load mapping failed.\n"); set_storage_success_flag(false);}
        sdcardFlagData.load_mapping=false;

        // load matrix
        printf("loading matrix on startup...\n");
        if (loadMatrixFile())
          {printf("loaded matrix successfully.\n"); set_storage_success_flag(true);}
        else {printf("load matrix failed.\n"); set_storage_success_flag(false);}
        sdcardFlagData.load_matrix=false;

        // end
        ran_startup_file_operations = true;
      }
      sdcardFlagData.no_delay_flag=false;
    }

    // ---- LOG ----

    else if (sdcardFlagData.write_log) {
    //   if (writeLog()==true)
    //     {printf("[log] successfull.\n"); set_storage_success_flag(true);}
    //   else {printf("[log] failed.\n"); set_storage_success_flag(false);}
      writeLog();
      sdcardFlagData.no_delay_flag=false;
      sdcardFlagData.write_log=false;
    }
  }
//   clearSDMMCArgStruct();
}
