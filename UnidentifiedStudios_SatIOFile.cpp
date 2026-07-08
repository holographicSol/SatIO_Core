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
    #ifdef SatIO_CD74HC4067_OPTION_USE_0
    line="$MPLEX0,";
    for (int i=0; i<MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS; i++) {line=line+String(ad_mux_0.data[i])+",";}
    printLogLine(line.c_str());
    #endif

    #ifdef SatIO_CD74HC4067_OPTION_USE_1
    line="$MPLEX1,";
    for (int i=0; i<MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS; i++) {line=line+String(ad_mux_1.data[i])+",";}
    printLogLine(line.c_str());
    #endif

    // --------------------------------
    // Log Line: Port Controller Input
    // --------------------------------
    #ifdef SatIO_USE_GPIOPE_INPUT_0
    line="$GPIOEI0,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_0.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_0


    #ifdef SatIO_USE_GPIOPE_INPUT_1
    line="$GPIOEI1,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_1.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_1


    #ifdef SatIO_USE_GPIOPE_INPUT_2
    line="$GPIOEI2,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_2.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_2


    #ifdef SatIO_USE_GPIOPE_INPUT_3
    line="$GPIOEI3,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_3.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_3


    #ifdef SatIO_USE_GPIOPE_INPUT_4
    line="$GPIOEI4,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_4.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_4


    #ifdef SatIO_USE_GPIOPE_INPUT_5
    line="$GPIOEI5,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_5.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_5


    #ifdef SatIO_USE_GPIOPE_INPUT_6
    line="$GPIOEI6,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_6.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_6


    #ifdef SatIO_USE_GPIOPE_INPUT_7
    line="$GPIOEI7,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_7.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_7


    #ifdef SatIO_USE_GPIOPE_INPUT_8
    line="$GPIOEI8,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_8.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_8


    #ifdef SatIO_USE_GPIOPE_INPUT_9
    line="$GPIOEI9,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_9.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_9


    #ifdef SatIO_USE_GPIOPE_INPUT_10
    line="$GPIOEI10,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_10.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_10


    #ifdef SatIO_USE_GPIOPE_INPUT_11
    line="$GPIOEI11,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_11.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_11


    #ifdef SatIO_USE_GPIOPE_INPUT_12
    line="$GPIOEI12,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_12.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_12


    #ifdef SatIO_USE_GPIOPE_INPUT_13
    line="$GPIOEI13,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_13.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_13


    #ifdef SatIO_USE_GPIOPE_INPUT_14
    line="$GPIOEI14,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_14.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_14


    #ifdef SatIO_USE_GPIOPE_INPUT_15
    line="$GPIOEI15,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_15.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_15


    #ifdef SatIO_USE_GPIOPE_INPUT_16
    line="$GPIOEI16,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_16.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_16


    #ifdef SatIO_USE_GPIOPE_INPUT_17
    line="$GPIOEI17,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_17.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_17


    #ifdef SatIO_USE_GPIOPE_INPUT_18
    line="$GPIOEI18,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_18.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_18


    #ifdef SatIO_USE_GPIOPE_INPUT_19
    line="$GPIOEI19,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_19.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_19


    #ifdef SatIO_USE_GPIOPE_INPUT_20
    line="$GPIOEI20,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_20.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_20


    #ifdef SatIO_USE_GPIOPE_INPUT_21
    line="$GPIOEI21,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_21.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_21


    #ifdef SatIO_USE_GPIOPE_INPUT_22
    line="$GPIOEI22,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_22.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_22


    #ifdef SatIO_USE_GPIOPE_INPUT_23
    line="$GPIOEI23,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_23.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_23


    #ifdef SatIO_USE_GPIOPE_INPUT_24
    line="$GPIOEI24,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_24.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_24


    #ifdef SatIO_USE_GPIOPE_INPUT_25
    line="$GPIOEI25,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_25.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_25


    #ifdef SatIO_USE_GPIOPE_INPUT_26
    line="$GPIOEI26,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_26.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_26


    #ifdef SatIO_USE_GPIOPE_INPUT_27
    line="$GPIOEI27,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_27.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_27


    #ifdef SatIO_USE_GPIOPE_INPUT_28
    line="$GPIOEI28,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_28.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_28


    #ifdef SatIO_USE_GPIOPE_INPUT_29
    line="$GPIOEI29,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_29.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_29


    #ifdef SatIO_USE_GPIOPE_INPUT_30
    line="$GPIOEI30,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_30.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_30


    #ifdef SatIO_USE_GPIOPE_INPUT_31
    line="$GPIOEI31,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_31.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_31


    #ifdef SatIO_USE_GPIOPE_INPUT_32
    line="$GPIOEI32,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_32.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_32


    #ifdef SatIO_USE_GPIOPE_INPUT_33
    line="$GPIOEI33,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_33.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_33


    #ifdef SatIO_USE_GPIOPE_INPUT_34
    line="$GPIOEI34,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_34.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_34


    #ifdef SatIO_USE_GPIOPE_INPUT_35
    line="$GPIOEI35,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_35.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_35


    #ifdef SatIO_USE_GPIOPE_INPUT_36
    line="$GPIOEI36,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_36.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_36


    #ifdef SatIO_USE_GPIOPE_INPUT_37
    line="$GPIOEI37,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_37.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_37


    #ifdef SatIO_USE_GPIOPE_INPUT_38
    line="$GPIOEI38,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_38.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_38


    #ifdef SatIO_USE_GPIOPE_INPUT_39
    line="$GPIOEI39,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_39.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_39


    #ifdef SatIO_USE_GPIOPE_INPUT_40
    line="$GPIOEI40,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_40.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_40


    #ifdef SatIO_USE_GPIOPE_INPUT_41
    line="$GPIOEI41,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_41.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_41


    #ifdef SatIO_USE_GPIOPE_INPUT_42
    line="$GPIOEI42,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_42.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_42


    #ifdef SatIO_USE_GPIOPE_INPUT_43
    line="$GPIOEI43,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_43.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_43


    #ifdef SatIO_USE_GPIOPE_INPUT_44
    line="$GPIOEI44,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_44.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_44


    #ifdef SatIO_USE_GPIOPE_INPUT_45
    line="$GPIOEI45,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_45.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_45


    #ifdef SatIO_USE_GPIOPE_INPUT_46
    line="$GPIOEI46,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_46.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_46


    #ifdef SatIO_USE_GPIOPE_INPUT_47
    line="$GPIOEI47,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_47.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_47


    #ifdef SatIO_USE_GPIOPE_INPUT_48
    line="$GPIOEI48,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_48.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_48


    #ifdef SatIO_USE_GPIOPE_INPUT_49
    line="$GPIOEI49,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_49.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_49


    #ifdef SatIO_USE_GPIOPE_INPUT_50
    line="$GPIOEI50,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_50.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_50


    #ifdef SatIO_USE_GPIOPE_INPUT_51
    line="$GPIOEI51,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_51.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_51


    #ifdef SatIO_USE_GPIOPE_INPUT_52
    line="$GPIOEI52,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_52.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_52


    #ifdef SatIO_USE_GPIOPE_INPUT_53
    line="$GPIOEI53,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_53.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_53


    #ifdef SatIO_USE_GPIOPE_INPUT_54
    line="$GPIOEI54,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_54.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_54


    #ifdef SatIO_USE_GPIOPE_INPUT_55
    line="$GPIOEI55,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_55.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_55


    #ifdef SatIO_USE_GPIOPE_INPUT_56
    line="$GPIOEI56,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_56.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_56


    #ifdef SatIO_USE_GPIOPE_INPUT_57
    line="$GPIOEI57,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_57.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_57


    #ifdef SatIO_USE_GPIOPE_INPUT_58
    line="$GPIOEI58,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_58.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_58


    #ifdef SatIO_USE_GPIOPE_INPUT_59
    line="$GPIOEI59,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_59.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_59


    #ifdef SatIO_USE_GPIOPE_INPUT_60
    line="$GPIOEI60,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_60.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_60


    #ifdef SatIO_USE_GPIOPE_INPUT_61
    line="$GPIOEI61,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_61.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_61


    #ifdef SatIO_USE_GPIOPE_INPUT_62
    line="$GPIOEI62,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_62.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_62


    #ifdef SatIO_USE_GPIOPE_INPUT_63
    line="$GPIOEI63,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_63.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_63


    #ifdef SatIO_USE_GPIOPE_INPUT_64
    line="$GPIOEI64,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_64.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_64


    #ifdef SatIO_USE_GPIOPE_INPUT_65
    line="$GPIOEI65,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_65.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_65


    #ifdef SatIO_USE_GPIOPE_INPUT_66
    line="$GPIOEI66,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_66.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_66


    #ifdef SatIO_USE_GPIOPE_INPUT_67
    line="$GPIOEI67,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_67.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_67


    #ifdef SatIO_USE_GPIOPE_INPUT_68
    line="$GPIOEI68,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_68.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_68


    #ifdef SatIO_USE_GPIOPE_INPUT_69
    line="$GPIOEI69,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_69.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_69


    #ifdef SatIO_USE_GPIOPE_INPUT_70
    line="$GPIOEI70,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_70.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_70


    #ifdef SatIO_USE_GPIOPE_INPUT_71
    line="$GPIOEI71,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_71.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_71


    #ifdef SatIO_USE_GPIOPE_INPUT_72
    line="$GPIOEI72,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_72.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_72


    #ifdef SatIO_USE_GPIOPE_INPUT_73
    line="$GPIOEI73,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_73.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_73


    #ifdef SatIO_USE_GPIOPE_INPUT_74
    line="$GPIOEI74,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_74.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_74


    #ifdef SatIO_USE_GPIOPE_INPUT_75
    line="$GPIOEI75,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_75.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_75


    #ifdef SatIO_USE_GPIOPE_INPUT_76
    line="$GPIOEI76,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_76.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_76


    #ifdef SatIO_USE_GPIOPE_INPUT_77
    line="$GPIOEI77,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_77.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_77


    #ifdef SatIO_USE_GPIOPE_INPUT_78
    line="$GPIOEI78,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_78.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_78


    #ifdef SatIO_USE_GPIOPE_INPUT_79
    line="$GPIOEI79,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_79.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_79


    #ifdef SatIO_USE_GPIOPE_INPUT_80
    line="$GPIOEI80,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_80.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_80


    #ifdef SatIO_USE_GPIOPE_INPUT_81
    line="$GPIOEI81,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_81.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_81


    #ifdef SatIO_USE_GPIOPE_INPUT_82
    line="$GPIOEI82,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_82.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_82


    #ifdef SatIO_USE_GPIOPE_INPUT_83
    line="$GPIOEI83,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_83.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_83


    #ifdef SatIO_USE_GPIOPE_INPUT_84
    line="$GPIOEI84,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_84.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_84


    #ifdef SatIO_USE_GPIOPE_INPUT_85
    line="$GPIOEI85,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_85.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_85


    #ifdef SatIO_USE_GPIOPE_INPUT_86
    line="$GPIOEI86,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_86.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_86


    #ifdef SatIO_USE_GPIOPE_INPUT_87
    line="$GPIOEI87,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_87.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_87


    #ifdef SatIO_USE_GPIOPE_INPUT_88
    line="$GPIOEI88,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_88.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_88


    #ifdef SatIO_USE_GPIOPE_INPUT_89
    line="$GPIOEI89,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_89.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_89


    #ifdef SatIO_USE_GPIOPE_INPUT_90
    line="$GPIOEI90,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_90.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_90


    #ifdef SatIO_USE_GPIOPE_INPUT_91
    line="$GPIOEI91,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_91.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_91


    #ifdef SatIO_USE_GPIOPE_INPUT_92
    line="$GPIOEI92,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_92.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_92


    #ifdef SatIO_USE_GPIOPE_INPUT_93
    line="$GPIOEI93,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_93.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_93


    #ifdef SatIO_USE_GPIOPE_INPUT_94
    line="$GPIOEI94,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_94.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_94


    #ifdef SatIO_USE_GPIOPE_INPUT_95
    line="$GPIOEI95,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_95.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_95


    #ifdef SatIO_USE_GPIOPE_INPUT_96
    line="$GPIOEI96,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_96.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_96


    #ifdef SatIO_USE_GPIOPE_INPUT_97
    line="$GPIOEI97,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_97.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_97


    #ifdef SatIO_USE_GPIOPE_INPUT_98
    line="$GPIOEI98,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_98.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_98


    #ifdef SatIO_USE_GPIOPE_INPUT_99
    line="$GPIOEI99,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_99.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_99


    #ifdef SatIO_USE_GPIOPE_INPUT_100
    line="$GPIOEI100,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_100.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_100


    #ifdef SatIO_USE_GPIOPE_INPUT_101
    line="$GPIOEI101,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_101.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_101


    #ifdef SatIO_USE_GPIOPE_INPUT_102
    line="$GPIOEI102,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_102.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_102


    #ifdef SatIO_USE_GPIOPE_INPUT_103
    line="$GPIOEI103,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_103.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_103


    #ifdef SatIO_USE_GPIOPE_INPUT_104
    line="$GPIOEI104,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_104.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_104


    #ifdef SatIO_USE_GPIOPE_INPUT_105
    line="$GPIOEI105,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_105.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_105


    #ifdef SatIO_USE_GPIOPE_INPUT_106
    line="$GPIOEI106,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_106.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_106


    #ifdef SatIO_USE_GPIOPE_INPUT_107
    line="$GPIOEI107,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_107.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_107


    #ifdef SatIO_USE_GPIOPE_INPUT_108
    line="$GPIOEI108,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_108.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_108


    #ifdef SatIO_USE_GPIOPE_INPUT_109
    line="$GPIOEI109,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_109.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_109


    #ifdef SatIO_USE_GPIOPE_INPUT_110
    line="$GPIOEI110,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_110.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_110


    #ifdef SatIO_USE_GPIOPE_INPUT_111
    line="$GPIOEI111,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_111.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_111


    #ifdef SatIO_USE_GPIOPE_INPUT_112
    line="$GPIOEI112,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_112.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_112


    #ifdef SatIO_USE_GPIOPE_INPUT_113
    line="$GPIOEI113,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_113.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_113


    #ifdef SatIO_USE_GPIOPE_INPUT_114
    line="$GPIOEI114,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_114.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_114


    #ifdef SatIO_USE_GPIOPE_INPUT_115
    line="$GPIOEI115,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_115.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_115


    #ifdef SatIO_USE_GPIOPE_INPUT_116
    line="$GPIOEI116,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_116.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_116


    #ifdef SatIO_USE_GPIOPE_INPUT_117
    line="$GPIOEI117,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_117.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_117


    #ifdef SatIO_USE_GPIOPE_INPUT_118
    line="$GPIOEI118,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_118.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_118


    #ifdef SatIO_USE_GPIOPE_INPUT_119
    line="$GPIOEI119,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_119.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_119


    #ifdef SatIO_USE_GPIOPE_INPUT_120
    line="$GPIOEI120,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_120.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_120


    #ifdef SatIO_USE_GPIOPE_INPUT_121
    line="$GPIOEI121,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_121.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_121


    #ifdef SatIO_USE_GPIOPE_INPUT_122
    line="$GPIOEI122,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_122.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_122


    #ifdef SatIO_USE_GPIOPE_INPUT_123
    line="$GPIOEI123,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_123.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_123


    #ifdef SatIO_USE_GPIOPE_INPUT_124
    line="$GPIOEI124,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_124.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_124


    #ifdef SatIO_USE_GPIOPE_INPUT_125
    line="$GPIOEI125,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_125.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_125


    #ifdef SatIO_USE_GPIOPE_INPUT_126
    line="$GPIOEI126,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_126.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_126


    #ifdef SatIO_USE_GPIOPE_INPUT_127
    line="$GPIOEI127,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPE_INPUT_127.input_value[i])+",";}
    printLogLine(line.c_str());
    #endif // SatIO_USE_GPIOPE_INPUT_127

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
    SYSTEM_FILE_GPIOE_0_CH_ENABLED,
    SYSTEM_FILE_GPIOE_0_CH_FREQ,
    SYSTEM_FILE_GPIOE_1_CH_ENABLED,
    SYSTEM_FILE_GPIOE_1_CH_FREQ,
    SYSTEM_FILE_GPIOE_2_CH_ENABLED,
    SYSTEM_FILE_GPIOE_2_CH_FREQ,
    SYSTEM_FILE_GPIOE_3_CH_ENABLED,
    SYSTEM_FILE_GPIOE_3_CH_FREQ,
    SYSTEM_FILE_GPIOE_4_CH_ENABLED,
    SYSTEM_FILE_GPIOE_4_CH_FREQ,
    SYSTEM_FILE_GPIOE_5_CH_ENABLED,
    SYSTEM_FILE_GPIOE_5_CH_FREQ,
    SYSTEM_FILE_GPIOE_6_CH_ENABLED,
    SYSTEM_FILE_GPIOE_6_CH_FREQ,
    SYSTEM_FILE_GPIOE_7_CH_ENABLED,
    SYSTEM_FILE_GPIOE_7_CH_FREQ,
    SYSTEM_FILE_GPIOE_8_CH_ENABLED,
    SYSTEM_FILE_GPIOE_8_CH_FREQ,
    SYSTEM_FILE_GPIOE_9_CH_ENABLED,
    SYSTEM_FILE_GPIOE_9_CH_FREQ,
    SYSTEM_FILE_GPIOE_10_CH_ENABLED,
    SYSTEM_FILE_GPIOE_10_CH_FREQ,
    SYSTEM_FILE_GPIOE_11_CH_ENABLED,
    SYSTEM_FILE_GPIOE_11_CH_FREQ,
    SYSTEM_FILE_GPIOE_12_CH_ENABLED,
    SYSTEM_FILE_GPIOE_12_CH_FREQ,
    SYSTEM_FILE_GPIOE_13_CH_ENABLED,
    SYSTEM_FILE_GPIOE_13_CH_FREQ,
    SYSTEM_FILE_GPIOE_14_CH_ENABLED,
    SYSTEM_FILE_GPIOE_14_CH_FREQ,
    SYSTEM_FILE_GPIOE_15_CH_ENABLED,
    SYSTEM_FILE_GPIOE_15_CH_FREQ,
    SYSTEM_FILE_GPIOE_16_CH_ENABLED,
    SYSTEM_FILE_GPIOE_16_CH_FREQ,
    SYSTEM_FILE_GPIOE_17_CH_ENABLED,
    SYSTEM_FILE_GPIOE_17_CH_FREQ,
    SYSTEM_FILE_GPIOE_18_CH_ENABLED,
    SYSTEM_FILE_GPIOE_18_CH_FREQ,
    SYSTEM_FILE_GPIOE_19_CH_ENABLED,
    SYSTEM_FILE_GPIOE_19_CH_FREQ,
    SYSTEM_FILE_GPIOE_20_CH_ENABLED,
    SYSTEM_FILE_GPIOE_20_CH_FREQ,
    SYSTEM_FILE_GPIOE_21_CH_ENABLED,
    SYSTEM_FILE_GPIOE_21_CH_FREQ,
    SYSTEM_FILE_GPIOE_22_CH_ENABLED,
    SYSTEM_FILE_GPIOE_22_CH_FREQ,
    SYSTEM_FILE_GPIOE_23_CH_ENABLED,
    SYSTEM_FILE_GPIOE_23_CH_FREQ,
    SYSTEM_FILE_GPIOE_24_CH_ENABLED,
    SYSTEM_FILE_GPIOE_24_CH_FREQ,
    SYSTEM_FILE_GPIOE_25_CH_ENABLED,
    SYSTEM_FILE_GPIOE_25_CH_FREQ,
    SYSTEM_FILE_GPIOE_26_CH_ENABLED,
    SYSTEM_FILE_GPIOE_26_CH_FREQ,
    SYSTEM_FILE_GPIOE_27_CH_ENABLED,
    SYSTEM_FILE_GPIOE_27_CH_FREQ,
    SYSTEM_FILE_GPIOE_28_CH_ENABLED,
    SYSTEM_FILE_GPIOE_28_CH_FREQ,
    SYSTEM_FILE_GPIOE_29_CH_ENABLED,
    SYSTEM_FILE_GPIOE_29_CH_FREQ,
    SYSTEM_FILE_GPIOE_30_CH_ENABLED,
    SYSTEM_FILE_GPIOE_30_CH_FREQ,
    SYSTEM_FILE_GPIOE_31_CH_ENABLED,
    SYSTEM_FILE_GPIOE_31_CH_FREQ,
    SYSTEM_FILE_GPIOE_32_CH_ENABLED,
    SYSTEM_FILE_GPIOE_32_CH_FREQ,
    SYSTEM_FILE_GPIOE_33_CH_ENABLED,
    SYSTEM_FILE_GPIOE_33_CH_FREQ,
    SYSTEM_FILE_GPIOE_34_CH_ENABLED,
    SYSTEM_FILE_GPIOE_34_CH_FREQ,
    SYSTEM_FILE_GPIOE_35_CH_ENABLED,
    SYSTEM_FILE_GPIOE_35_CH_FREQ,
    SYSTEM_FILE_GPIOE_36_CH_ENABLED,
    SYSTEM_FILE_GPIOE_36_CH_FREQ,
    SYSTEM_FILE_GPIOE_37_CH_ENABLED,
    SYSTEM_FILE_GPIOE_37_CH_FREQ,
    SYSTEM_FILE_GPIOE_38_CH_ENABLED,
    SYSTEM_FILE_GPIOE_38_CH_FREQ,
    SYSTEM_FILE_GPIOE_39_CH_ENABLED,
    SYSTEM_FILE_GPIOE_39_CH_FREQ,
    SYSTEM_FILE_GPIOE_40_CH_ENABLED,
    SYSTEM_FILE_GPIOE_40_CH_FREQ,
    SYSTEM_FILE_GPIOE_41_CH_ENABLED,
    SYSTEM_FILE_GPIOE_41_CH_FREQ,
    SYSTEM_FILE_GPIOE_42_CH_ENABLED,
    SYSTEM_FILE_GPIOE_42_CH_FREQ,
    SYSTEM_FILE_GPIOE_43_CH_ENABLED,
    SYSTEM_FILE_GPIOE_43_CH_FREQ,
    SYSTEM_FILE_GPIOE_44_CH_ENABLED,
    SYSTEM_FILE_GPIOE_44_CH_FREQ,
    SYSTEM_FILE_GPIOE_45_CH_ENABLED,
    SYSTEM_FILE_GPIOE_45_CH_FREQ,
    SYSTEM_FILE_GPIOE_46_CH_ENABLED,
    SYSTEM_FILE_GPIOE_46_CH_FREQ,
    SYSTEM_FILE_GPIOE_47_CH_ENABLED,
    SYSTEM_FILE_GPIOE_47_CH_FREQ,
    SYSTEM_FILE_GPIOE_48_CH_ENABLED,
    SYSTEM_FILE_GPIOE_48_CH_FREQ,
    SYSTEM_FILE_GPIOE_49_CH_ENABLED,
    SYSTEM_FILE_GPIOE_49_CH_FREQ,
    SYSTEM_FILE_GPIOE_50_CH_ENABLED,
    SYSTEM_FILE_GPIOE_50_CH_FREQ,
    SYSTEM_FILE_GPIOE_51_CH_ENABLED,
    SYSTEM_FILE_GPIOE_51_CH_FREQ,
    SYSTEM_FILE_GPIOE_52_CH_ENABLED,
    SYSTEM_FILE_GPIOE_52_CH_FREQ,
    SYSTEM_FILE_GPIOE_53_CH_ENABLED,
    SYSTEM_FILE_GPIOE_53_CH_FREQ,
    SYSTEM_FILE_GPIOE_54_CH_ENABLED,
    SYSTEM_FILE_GPIOE_54_CH_FREQ,
    SYSTEM_FILE_GPIOE_55_CH_ENABLED,
    SYSTEM_FILE_GPIOE_55_CH_FREQ,
    SYSTEM_FILE_GPIOE_56_CH_ENABLED,
    SYSTEM_FILE_GPIOE_56_CH_FREQ,
    SYSTEM_FILE_GPIOE_57_CH_ENABLED,
    SYSTEM_FILE_GPIOE_57_CH_FREQ,
    SYSTEM_FILE_GPIOE_58_CH_ENABLED,
    SYSTEM_FILE_GPIOE_58_CH_FREQ,
    SYSTEM_FILE_GPIOE_59_CH_ENABLED,
    SYSTEM_FILE_GPIOE_59_CH_FREQ,
    SYSTEM_FILE_GPIOE_60_CH_ENABLED,
    SYSTEM_FILE_GPIOE_60_CH_FREQ,
    SYSTEM_FILE_GPIOE_61_CH_ENABLED,
    SYSTEM_FILE_GPIOE_61_CH_FREQ,
    SYSTEM_FILE_GPIOE_62_CH_ENABLED,
    SYSTEM_FILE_GPIOE_62_CH_FREQ,
    SYSTEM_FILE_GPIOE_63_CH_ENABLED,
    SYSTEM_FILE_GPIOE_63_CH_FREQ,
    SYSTEM_FILE_GPIOE_64_CH_ENABLED,
    SYSTEM_FILE_GPIOE_64_CH_FREQ,
    SYSTEM_FILE_GPIOE_65_CH_ENABLED,
    SYSTEM_FILE_GPIOE_65_CH_FREQ,
    SYSTEM_FILE_GPIOE_66_CH_ENABLED,
    SYSTEM_FILE_GPIOE_66_CH_FREQ,
    SYSTEM_FILE_GPIOE_67_CH_ENABLED,
    SYSTEM_FILE_GPIOE_67_CH_FREQ,
    SYSTEM_FILE_GPIOE_68_CH_ENABLED,
    SYSTEM_FILE_GPIOE_68_CH_FREQ,
    SYSTEM_FILE_GPIOE_69_CH_ENABLED,
    SYSTEM_FILE_GPIOE_69_CH_FREQ,
    SYSTEM_FILE_GPIOE_70_CH_ENABLED,
    SYSTEM_FILE_GPIOE_70_CH_FREQ,
    SYSTEM_FILE_GPIOE_71_CH_ENABLED,
    SYSTEM_FILE_GPIOE_71_CH_FREQ,
    SYSTEM_FILE_GPIOE_72_CH_ENABLED,
    SYSTEM_FILE_GPIOE_72_CH_FREQ,
    SYSTEM_FILE_GPIOE_73_CH_ENABLED,
    SYSTEM_FILE_GPIOE_73_CH_FREQ,
    SYSTEM_FILE_GPIOE_74_CH_ENABLED,
    SYSTEM_FILE_GPIOE_74_CH_FREQ,
    SYSTEM_FILE_GPIOE_75_CH_ENABLED,
    SYSTEM_FILE_GPIOE_75_CH_FREQ,
    SYSTEM_FILE_GPIOE_76_CH_ENABLED,
    SYSTEM_FILE_GPIOE_76_CH_FREQ,
    SYSTEM_FILE_GPIOE_77_CH_ENABLED,
    SYSTEM_FILE_GPIOE_77_CH_FREQ,
    SYSTEM_FILE_GPIOE_78_CH_ENABLED,
    SYSTEM_FILE_GPIOE_78_CH_FREQ,
    SYSTEM_FILE_GPIOE_79_CH_ENABLED,
    SYSTEM_FILE_GPIOE_79_CH_FREQ,
    SYSTEM_FILE_GPIOE_80_CH_ENABLED,
    SYSTEM_FILE_GPIOE_80_CH_FREQ,
    SYSTEM_FILE_GPIOE_81_CH_ENABLED,
    SYSTEM_FILE_GPIOE_81_CH_FREQ,
    SYSTEM_FILE_GPIOE_82_CH_ENABLED,
    SYSTEM_FILE_GPIOE_82_CH_FREQ,
    SYSTEM_FILE_GPIOE_83_CH_ENABLED,
    SYSTEM_FILE_GPIOE_83_CH_FREQ,
    SYSTEM_FILE_GPIOE_84_CH_ENABLED,
    SYSTEM_FILE_GPIOE_84_CH_FREQ,
    SYSTEM_FILE_GPIOE_85_CH_ENABLED,
    SYSTEM_FILE_GPIOE_85_CH_FREQ,
    SYSTEM_FILE_GPIOE_86_CH_ENABLED,
    SYSTEM_FILE_GPIOE_86_CH_FREQ,
    SYSTEM_FILE_GPIOE_87_CH_ENABLED,
    SYSTEM_FILE_GPIOE_87_CH_FREQ,
    SYSTEM_FILE_GPIOE_88_CH_ENABLED,
    SYSTEM_FILE_GPIOE_88_CH_FREQ,
    SYSTEM_FILE_GPIOE_89_CH_ENABLED,
    SYSTEM_FILE_GPIOE_89_CH_FREQ,
    SYSTEM_FILE_GPIOE_90_CH_ENABLED,
    SYSTEM_FILE_GPIOE_90_CH_FREQ,
    SYSTEM_FILE_GPIOE_91_CH_ENABLED,
    SYSTEM_FILE_GPIOE_91_CH_FREQ,
    SYSTEM_FILE_GPIOE_92_CH_ENABLED,
    SYSTEM_FILE_GPIOE_92_CH_FREQ,
    SYSTEM_FILE_GPIOE_93_CH_ENABLED,
    SYSTEM_FILE_GPIOE_93_CH_FREQ,
    SYSTEM_FILE_GPIOE_94_CH_ENABLED,
    SYSTEM_FILE_GPIOE_94_CH_FREQ,
    SYSTEM_FILE_GPIOE_95_CH_ENABLED,
    SYSTEM_FILE_GPIOE_95_CH_FREQ,
    SYSTEM_FILE_GPIOE_96_CH_ENABLED,
    SYSTEM_FILE_GPIOE_96_CH_FREQ,
    SYSTEM_FILE_GPIOE_97_CH_ENABLED,
    SYSTEM_FILE_GPIOE_97_CH_FREQ,
    SYSTEM_FILE_GPIOE_98_CH_ENABLED,
    SYSTEM_FILE_GPIOE_98_CH_FREQ,
    SYSTEM_FILE_GPIOE_99_CH_ENABLED,
    SYSTEM_FILE_GPIOE_99_CH_FREQ,
    SYSTEM_FILE_GPIOE_100_CH_ENABLED,
    SYSTEM_FILE_GPIOE_100_CH_FREQ,
    SYSTEM_FILE_GPIOE_101_CH_ENABLED,
    SYSTEM_FILE_GPIOE_101_CH_FREQ,
    SYSTEM_FILE_GPIOE_102_CH_ENABLED,
    SYSTEM_FILE_GPIOE_102_CH_FREQ,
    SYSTEM_FILE_GPIOE_103_CH_ENABLED,
    SYSTEM_FILE_GPIOE_103_CH_FREQ,
    SYSTEM_FILE_GPIOE_104_CH_ENABLED,
    SYSTEM_FILE_GPIOE_104_CH_FREQ,
    SYSTEM_FILE_GPIOE_105_CH_ENABLED,
    SYSTEM_FILE_GPIOE_105_CH_FREQ,
    SYSTEM_FILE_GPIOE_106_CH_ENABLED,
    SYSTEM_FILE_GPIOE_106_CH_FREQ,
    SYSTEM_FILE_GPIOE_107_CH_ENABLED,
    SYSTEM_FILE_GPIOE_107_CH_FREQ,
    SYSTEM_FILE_GPIOE_108_CH_ENABLED,
    SYSTEM_FILE_GPIOE_108_CH_FREQ,
    SYSTEM_FILE_GPIOE_109_CH_ENABLED,
    SYSTEM_FILE_GPIOE_109_CH_FREQ,
    SYSTEM_FILE_GPIOE_110_CH_ENABLED,
    SYSTEM_FILE_GPIOE_110_CH_FREQ,
    SYSTEM_FILE_GPIOE_111_CH_ENABLED,
    SYSTEM_FILE_GPIOE_111_CH_FREQ,
    SYSTEM_FILE_GPIOE_112_CH_ENABLED,
    SYSTEM_FILE_GPIOE_112_CH_FREQ,
    SYSTEM_FILE_GPIOE_113_CH_ENABLED,
    SYSTEM_FILE_GPIOE_113_CH_FREQ,
    SYSTEM_FILE_GPIOE_114_CH_ENABLED,
    SYSTEM_FILE_GPIOE_114_CH_FREQ,
    SYSTEM_FILE_GPIOE_115_CH_ENABLED,
    SYSTEM_FILE_GPIOE_115_CH_FREQ,
    SYSTEM_FILE_GPIOE_116_CH_ENABLED,
    SYSTEM_FILE_GPIOE_116_CH_FREQ,
    SYSTEM_FILE_GPIOE_117_CH_ENABLED,
    SYSTEM_FILE_GPIOE_117_CH_FREQ,
    SYSTEM_FILE_GPIOE_118_CH_ENABLED,
    SYSTEM_FILE_GPIOE_118_CH_FREQ,
    SYSTEM_FILE_GPIOE_119_CH_ENABLED,
    SYSTEM_FILE_GPIOE_119_CH_FREQ,
    SYSTEM_FILE_GPIOE_120_CH_ENABLED,
    SYSTEM_FILE_GPIOE_120_CH_FREQ,
    SYSTEM_FILE_GPIOE_121_CH_ENABLED,
    SYSTEM_FILE_GPIOE_121_CH_FREQ,
    SYSTEM_FILE_GPIOE_122_CH_ENABLED,
    SYSTEM_FILE_GPIOE_122_CH_FREQ,
    SYSTEM_FILE_GPIOE_123_CH_ENABLED,
    SYSTEM_FILE_GPIOE_123_CH_FREQ,
    SYSTEM_FILE_GPIOE_124_CH_ENABLED,
    SYSTEM_FILE_GPIOE_124_CH_FREQ,
    SYSTEM_FILE_GPIOE_125_CH_ENABLED,
    SYSTEM_FILE_GPIOE_125_CH_FREQ,
    SYSTEM_FILE_GPIOE_126_CH_ENABLED,
    SYSTEM_FILE_GPIOE_126_CH_FREQ,
    SYSTEM_FILE_GPIOE_127_CH_ENABLED,
    SYSTEM_FILE_GPIOE_127_CH_FREQ,

    SYSTEM_FILE_PWRCFG_NAME,
    SYSTEM_FILE_PWRCFG_GPS,
    SYSTEM_FILE_PWRCFG_ADMPLEX0,
    SYSTEM_FILE_PWRCFG_ADMPLEX1,
    SYSTEM_FILE_PWRCFG_GYRO,
    SYSTEM_FILE_PWRCFG_UNIVERSE,
    SYSTEM_FILE_PWRCFG_SWITCHES,
    SYSTEM_FILE_PWRCFG_GPIOE_INPUT,
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
        case SYSTEM_FILE_GPIOE_0_CH_ENABLED:               return "GPIOE_0_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_0_CH_FREQ:                  return "GPIOE_0_CH_FREQ";
        case SYSTEM_FILE_GPIOE_1_CH_ENABLED:               return "GPIOE_1_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_1_CH_FREQ:                  return "GPIOE_1_CH_FREQ";
        case SYSTEM_FILE_GPIOE_2_CH_ENABLED:               return "GPIOE_2_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_2_CH_FREQ:                  return "GPIOE_2_CH_FREQ";
        case SYSTEM_FILE_GPIOE_3_CH_ENABLED:               return "GPIOE_3_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_3_CH_FREQ:                  return "GPIOE_3_CH_FREQ";
        case SYSTEM_FILE_GPIOE_4_CH_ENABLED:               return "GPIOE_4_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_4_CH_FREQ:                  return "GPIOE_4_CH_FREQ";
        case SYSTEM_FILE_GPIOE_5_CH_ENABLED:               return "GPIOE_5_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_5_CH_FREQ:                  return "GPIOE_5_CH_FREQ";
        case SYSTEM_FILE_GPIOE_6_CH_ENABLED:               return "GPIOE_6_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_6_CH_FREQ:                  return "GPIOE_6_CH_FREQ";
        case SYSTEM_FILE_GPIOE_7_CH_ENABLED:               return "GPIOE_7_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_7_CH_FREQ:                  return "GPIOE_7_CH_FREQ";
        case SYSTEM_FILE_GPIOE_8_CH_ENABLED:               return "GPIOE_8_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_8_CH_FREQ:                  return "GPIOE_8_CH_FREQ";
        case SYSTEM_FILE_GPIOE_9_CH_ENABLED:               return "GPIOE_9_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_9_CH_FREQ:                  return "GPIOE_9_CH_FREQ";
        case SYSTEM_FILE_GPIOE_10_CH_ENABLED:               return "GPIOE_10_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_10_CH_FREQ:                  return "GPIOE_10_CH_FREQ";
        case SYSTEM_FILE_GPIOE_11_CH_ENABLED:               return "GPIOE_11_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_11_CH_FREQ:                  return "GPIOE_11_CH_FREQ";
        case SYSTEM_FILE_GPIOE_12_CH_ENABLED:               return "GPIOE_12_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_12_CH_FREQ:                  return "GPIOE_12_CH_FREQ";
        case SYSTEM_FILE_GPIOE_13_CH_ENABLED:               return "GPIOE_13_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_13_CH_FREQ:                  return "GPIOE_13_CH_FREQ";
        case SYSTEM_FILE_GPIOE_14_CH_ENABLED:               return "GPIOE_14_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_14_CH_FREQ:                  return "GPIOE_14_CH_FREQ";
        case SYSTEM_FILE_GPIOE_15_CH_ENABLED:               return "GPIOE_15_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_15_CH_FREQ:                  return "GPIOE_15_CH_FREQ";
        case SYSTEM_FILE_GPIOE_16_CH_ENABLED:               return "GPIOE_16_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_16_CH_FREQ:                  return "GPIOE_16_CH_FREQ";
        case SYSTEM_FILE_GPIOE_17_CH_ENABLED:               return "GPIOE_17_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_17_CH_FREQ:                  return "GPIOE_17_CH_FREQ";
        case SYSTEM_FILE_GPIOE_18_CH_ENABLED:               return "GPIOE_18_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_18_CH_FREQ:                  return "GPIOE_18_CH_FREQ";
        case SYSTEM_FILE_GPIOE_19_CH_ENABLED:               return "GPIOE_19_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_19_CH_FREQ:                  return "GPIOE_19_CH_FREQ";
        case SYSTEM_FILE_GPIOE_20_CH_ENABLED:               return "GPIOE_20_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_20_CH_FREQ:                  return "GPIOE_20_CH_FREQ";
        case SYSTEM_FILE_GPIOE_21_CH_ENABLED:               return "GPIOE_21_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_21_CH_FREQ:                  return "GPIOE_21_CH_FREQ";
        case SYSTEM_FILE_GPIOE_22_CH_ENABLED:               return "GPIOE_22_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_22_CH_FREQ:                  return "GPIOE_22_CH_FREQ";
        case SYSTEM_FILE_GPIOE_23_CH_ENABLED:               return "GPIOE_23_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_23_CH_FREQ:                  return "GPIOE_23_CH_FREQ";
        case SYSTEM_FILE_GPIOE_24_CH_ENABLED:               return "GPIOE_24_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_24_CH_FREQ:                  return "GPIOE_24_CH_FREQ";
        case SYSTEM_FILE_GPIOE_25_CH_ENABLED:               return "GPIOE_25_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_25_CH_FREQ:                  return "GPIOE_25_CH_FREQ";
        case SYSTEM_FILE_GPIOE_26_CH_ENABLED:               return "GPIOE_26_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_26_CH_FREQ:                  return "GPIOE_26_CH_FREQ";
        case SYSTEM_FILE_GPIOE_27_CH_ENABLED:               return "GPIOE_27_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_27_CH_FREQ:                  return "GPIOE_27_CH_FREQ";
        case SYSTEM_FILE_GPIOE_28_CH_ENABLED:               return "GPIOE_28_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_28_CH_FREQ:                  return "GPIOE_28_CH_FREQ";
        case SYSTEM_FILE_GPIOE_29_CH_ENABLED:               return "GPIOE_29_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_29_CH_FREQ:                  return "GPIOE_29_CH_FREQ";
        case SYSTEM_FILE_GPIOE_30_CH_ENABLED:               return "GPIOE_30_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_30_CH_FREQ:                  return "GPIOE_30_CH_FREQ";
        case SYSTEM_FILE_GPIOE_31_CH_ENABLED:               return "GPIOE_31_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_31_CH_FREQ:                  return "GPIOE_31_CH_FREQ";
        case SYSTEM_FILE_GPIOE_32_CH_ENABLED:               return "GPIOE_32_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_32_CH_FREQ:                  return "GPIOE_32_CH_FREQ";
        case SYSTEM_FILE_GPIOE_33_CH_ENABLED:               return "GPIOE_33_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_33_CH_FREQ:                  return "GPIOE_33_CH_FREQ";
        case SYSTEM_FILE_GPIOE_34_CH_ENABLED:               return "GPIOE_34_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_34_CH_FREQ:                  return "GPIOE_34_CH_FREQ";
        case SYSTEM_FILE_GPIOE_35_CH_ENABLED:               return "GPIOE_35_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_35_CH_FREQ:                  return "GPIOE_35_CH_FREQ";
        case SYSTEM_FILE_GPIOE_36_CH_ENABLED:               return "GPIOE_36_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_36_CH_FREQ:                  return "GPIOE_36_CH_FREQ";
        case SYSTEM_FILE_GPIOE_37_CH_ENABLED:               return "GPIOE_37_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_37_CH_FREQ:                  return "GPIOE_37_CH_FREQ";
        case SYSTEM_FILE_GPIOE_38_CH_ENABLED:               return "GPIOE_38_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_38_CH_FREQ:                  return "GPIOE_38_CH_FREQ";
        case SYSTEM_FILE_GPIOE_39_CH_ENABLED:               return "GPIOE_39_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_39_CH_FREQ:                  return "GPIOE_39_CH_FREQ";
        case SYSTEM_FILE_GPIOE_40_CH_ENABLED:               return "GPIOE_40_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_40_CH_FREQ:                  return "GPIOE_40_CH_FREQ";
        case SYSTEM_FILE_GPIOE_41_CH_ENABLED:               return "GPIOE_41_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_41_CH_FREQ:                  return "GPIOE_41_CH_FREQ";
        case SYSTEM_FILE_GPIOE_42_CH_ENABLED:               return "GPIOE_42_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_42_CH_FREQ:                  return "GPIOE_42_CH_FREQ";
        case SYSTEM_FILE_GPIOE_43_CH_ENABLED:               return "GPIOE_43_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_43_CH_FREQ:                  return "GPIOE_43_CH_FREQ";
        case SYSTEM_FILE_GPIOE_44_CH_ENABLED:               return "GPIOE_44_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_44_CH_FREQ:                  return "GPIOE_44_CH_FREQ";
        case SYSTEM_FILE_GPIOE_45_CH_ENABLED:               return "GPIOE_45_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_45_CH_FREQ:                  return "GPIOE_45_CH_FREQ";
        case SYSTEM_FILE_GPIOE_46_CH_ENABLED:               return "GPIOE_46_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_46_CH_FREQ:                  return "GPIOE_46_CH_FREQ";
        case SYSTEM_FILE_GPIOE_47_CH_ENABLED:               return "GPIOE_47_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_47_CH_FREQ:                  return "GPIOE_47_CH_FREQ";
        case SYSTEM_FILE_GPIOE_48_CH_ENABLED:               return "GPIOE_48_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_48_CH_FREQ:                  return "GPIOE_48_CH_FREQ";
        case SYSTEM_FILE_GPIOE_49_CH_ENABLED:               return "GPIOE_49_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_49_CH_FREQ:                  return "GPIOE_49_CH_FREQ";
        case SYSTEM_FILE_GPIOE_50_CH_ENABLED:               return "GPIOE_50_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_50_CH_FREQ:                  return "GPIOE_50_CH_FREQ";
        case SYSTEM_FILE_GPIOE_51_CH_ENABLED:               return "GPIOE_51_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_51_CH_FREQ:                  return "GPIOE_51_CH_FREQ";
        case SYSTEM_FILE_GPIOE_52_CH_ENABLED:               return "GPIOE_52_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_52_CH_FREQ:                  return "GPIOE_52_CH_FREQ";
        case SYSTEM_FILE_GPIOE_53_CH_ENABLED:               return "GPIOE_53_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_53_CH_FREQ:                  return "GPIOE_53_CH_FREQ";
        case SYSTEM_FILE_GPIOE_54_CH_ENABLED:               return "GPIOE_54_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_54_CH_FREQ:                  return "GPIOE_54_CH_FREQ";
        case SYSTEM_FILE_GPIOE_55_CH_ENABLED:               return "GPIOE_55_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_55_CH_FREQ:                  return "GPIOE_55_CH_FREQ";
        case SYSTEM_FILE_GPIOE_56_CH_ENABLED:               return "GPIOE_56_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_56_CH_FREQ:                  return "GPIOE_56_CH_FREQ";
        case SYSTEM_FILE_GPIOE_57_CH_ENABLED:               return "GPIOE_57_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_57_CH_FREQ:                  return "GPIOE_57_CH_FREQ";
        case SYSTEM_FILE_GPIOE_58_CH_ENABLED:               return "GPIOE_58_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_58_CH_FREQ:                  return "GPIOE_58_CH_FREQ";
        case SYSTEM_FILE_GPIOE_59_CH_ENABLED:               return "GPIOE_59_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_59_CH_FREQ:                  return "GPIOE_59_CH_FREQ";
        case SYSTEM_FILE_GPIOE_60_CH_ENABLED:               return "GPIOE_60_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_60_CH_FREQ:                  return "GPIOE_60_CH_FREQ";
        case SYSTEM_FILE_GPIOE_61_CH_ENABLED:               return "GPIOE_61_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_61_CH_FREQ:                  return "GPIOE_61_CH_FREQ";
        case SYSTEM_FILE_GPIOE_62_CH_ENABLED:               return "GPIOE_62_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_62_CH_FREQ:                  return "GPIOE_62_CH_FREQ";
        case SYSTEM_FILE_GPIOE_63_CH_ENABLED:               return "GPIOE_63_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_63_CH_FREQ:                  return "GPIOE_63_CH_FREQ";
        case SYSTEM_FILE_GPIOE_64_CH_ENABLED:               return "GPIOE_64_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_64_CH_FREQ:                  return "GPIOE_64_CH_FREQ";
        case SYSTEM_FILE_GPIOE_65_CH_ENABLED:               return "GPIOE_65_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_65_CH_FREQ:                  return "GPIOE_65_CH_FREQ";
        case SYSTEM_FILE_GPIOE_66_CH_ENABLED:               return "GPIOE_66_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_66_CH_FREQ:                  return "GPIOE_66_CH_FREQ";
        case SYSTEM_FILE_GPIOE_67_CH_ENABLED:               return "GPIOE_67_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_67_CH_FREQ:                  return "GPIOE_67_CH_FREQ";
        case SYSTEM_FILE_GPIOE_68_CH_ENABLED:               return "GPIOE_68_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_68_CH_FREQ:                  return "GPIOE_68_CH_FREQ";
        case SYSTEM_FILE_GPIOE_69_CH_ENABLED:               return "GPIOE_69_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_69_CH_FREQ:                  return "GPIOE_69_CH_FREQ";
        case SYSTEM_FILE_GPIOE_70_CH_ENABLED:               return "GPIOE_70_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_70_CH_FREQ:                  return "GPIOE_70_CH_FREQ";
        case SYSTEM_FILE_GPIOE_71_CH_ENABLED:               return "GPIOE_71_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_71_CH_FREQ:                  return "GPIOE_71_CH_FREQ";
        case SYSTEM_FILE_GPIOE_72_CH_ENABLED:               return "GPIOE_72_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_72_CH_FREQ:                  return "GPIOE_72_CH_FREQ";
        case SYSTEM_FILE_GPIOE_73_CH_ENABLED:               return "GPIOE_73_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_73_CH_FREQ:                  return "GPIOE_73_CH_FREQ";
        case SYSTEM_FILE_GPIOE_74_CH_ENABLED:               return "GPIOE_74_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_74_CH_FREQ:                  return "GPIOE_74_CH_FREQ";
        case SYSTEM_FILE_GPIOE_75_CH_ENABLED:               return "GPIOE_75_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_75_CH_FREQ:                  return "GPIOE_75_CH_FREQ";
        case SYSTEM_FILE_GPIOE_76_CH_ENABLED:               return "GPIOE_76_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_76_CH_FREQ:                  return "GPIOE_76_CH_FREQ";
        case SYSTEM_FILE_GPIOE_77_CH_ENABLED:               return "GPIOE_77_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_77_CH_FREQ:                  return "GPIOE_77_CH_FREQ";
        case SYSTEM_FILE_GPIOE_78_CH_ENABLED:               return "GPIOE_78_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_78_CH_FREQ:                  return "GPIOE_78_CH_FREQ";
        case SYSTEM_FILE_GPIOE_79_CH_ENABLED:               return "GPIOE_79_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_79_CH_FREQ:                  return "GPIOE_79_CH_FREQ";
        case SYSTEM_FILE_GPIOE_80_CH_ENABLED:               return "GPIOE_80_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_80_CH_FREQ:                  return "GPIOE_80_CH_FREQ";
        case SYSTEM_FILE_GPIOE_81_CH_ENABLED:               return "GPIOE_81_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_81_CH_FREQ:                  return "GPIOE_81_CH_FREQ";
        case SYSTEM_FILE_GPIOE_82_CH_ENABLED:               return "GPIOE_82_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_82_CH_FREQ:                  return "GPIOE_82_CH_FREQ";
        case SYSTEM_FILE_GPIOE_83_CH_ENABLED:               return "GPIOE_83_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_83_CH_FREQ:                  return "GPIOE_83_CH_FREQ";
        case SYSTEM_FILE_GPIOE_84_CH_ENABLED:               return "GPIOE_84_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_84_CH_FREQ:                  return "GPIOE_84_CH_FREQ";
        case SYSTEM_FILE_GPIOE_85_CH_ENABLED:               return "GPIOE_85_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_85_CH_FREQ:                  return "GPIOE_85_CH_FREQ";
        case SYSTEM_FILE_GPIOE_86_CH_ENABLED:               return "GPIOE_86_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_86_CH_FREQ:                  return "GPIOE_86_CH_FREQ";
        case SYSTEM_FILE_GPIOE_87_CH_ENABLED:               return "GPIOE_87_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_87_CH_FREQ:                  return "GPIOE_87_CH_FREQ";
        case SYSTEM_FILE_GPIOE_88_CH_ENABLED:               return "GPIOE_88_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_88_CH_FREQ:                  return "GPIOE_88_CH_FREQ";
        case SYSTEM_FILE_GPIOE_89_CH_ENABLED:               return "GPIOE_89_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_89_CH_FREQ:                  return "GPIOE_89_CH_FREQ";
        case SYSTEM_FILE_GPIOE_90_CH_ENABLED:               return "GPIOE_90_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_90_CH_FREQ:                  return "GPIOE_90_CH_FREQ";
        case SYSTEM_FILE_GPIOE_91_CH_ENABLED:               return "GPIOE_91_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_91_CH_FREQ:                  return "GPIOE_91_CH_FREQ";
        case SYSTEM_FILE_GPIOE_92_CH_ENABLED:               return "GPIOE_92_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_92_CH_FREQ:                  return "GPIOE_92_CH_FREQ";
        case SYSTEM_FILE_GPIOE_93_CH_ENABLED:               return "GPIOE_93_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_93_CH_FREQ:                  return "GPIOE_93_CH_FREQ";
        case SYSTEM_FILE_GPIOE_94_CH_ENABLED:               return "GPIOE_94_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_94_CH_FREQ:                  return "GPIOE_94_CH_FREQ";
        case SYSTEM_FILE_GPIOE_95_CH_ENABLED:               return "GPIOE_95_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_95_CH_FREQ:                  return "GPIOE_95_CH_FREQ";
        case SYSTEM_FILE_GPIOE_96_CH_ENABLED:               return "GPIOE_96_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_96_CH_FREQ:                  return "GPIOE_96_CH_FREQ";
        case SYSTEM_FILE_GPIOE_97_CH_ENABLED:               return "GPIOE_97_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_97_CH_FREQ:                  return "GPIOE_97_CH_FREQ";
        case SYSTEM_FILE_GPIOE_98_CH_ENABLED:               return "GPIOE_98_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_98_CH_FREQ:                  return "GPIOE_98_CH_FREQ";
        case SYSTEM_FILE_GPIOE_99_CH_ENABLED:               return "GPIOE_99_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_99_CH_FREQ:                  return "GPIOE_99_CH_FREQ";
        case SYSTEM_FILE_GPIOE_100_CH_ENABLED:               return "GPIOE_100_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_100_CH_FREQ:                  return "GPIOE_100_CH_FREQ";
        case SYSTEM_FILE_GPIOE_101_CH_ENABLED:               return "GPIOE_101_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_101_CH_FREQ:                  return "GPIOE_101_CH_FREQ";
        case SYSTEM_FILE_GPIOE_102_CH_ENABLED:               return "GPIOE_102_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_102_CH_FREQ:                  return "GPIOE_102_CH_FREQ";
        case SYSTEM_FILE_GPIOE_103_CH_ENABLED:               return "GPIOE_103_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_103_CH_FREQ:                  return "GPIOE_103_CH_FREQ";
        case SYSTEM_FILE_GPIOE_104_CH_ENABLED:               return "GPIOE_104_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_104_CH_FREQ:                  return "GPIOE_104_CH_FREQ";
        case SYSTEM_FILE_GPIOE_105_CH_ENABLED:               return "GPIOE_105_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_105_CH_FREQ:                  return "GPIOE_105_CH_FREQ";
        case SYSTEM_FILE_GPIOE_106_CH_ENABLED:               return "GPIOE_106_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_106_CH_FREQ:                  return "GPIOE_106_CH_FREQ";
        case SYSTEM_FILE_GPIOE_107_CH_ENABLED:               return "GPIOE_107_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_107_CH_FREQ:                  return "GPIOE_107_CH_FREQ";
        case SYSTEM_FILE_GPIOE_108_CH_ENABLED:               return "GPIOE_108_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_108_CH_FREQ:                  return "GPIOE_108_CH_FREQ";
        case SYSTEM_FILE_GPIOE_109_CH_ENABLED:               return "GPIOE_109_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_109_CH_FREQ:                  return "GPIOE_109_CH_FREQ";
        case SYSTEM_FILE_GPIOE_110_CH_ENABLED:               return "GPIOE_110_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_110_CH_FREQ:                  return "GPIOE_110_CH_FREQ";
        case SYSTEM_FILE_GPIOE_111_CH_ENABLED:               return "GPIOE_111_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_111_CH_FREQ:                  return "GPIOE_111_CH_FREQ";
        case SYSTEM_FILE_GPIOE_112_CH_ENABLED:               return "GPIOE_112_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_112_CH_FREQ:                  return "GPIOE_112_CH_FREQ";
        case SYSTEM_FILE_GPIOE_113_CH_ENABLED:               return "GPIOE_113_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_113_CH_FREQ:                  return "GPIOE_113_CH_FREQ";
        case SYSTEM_FILE_GPIOE_114_CH_ENABLED:               return "GPIOE_114_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_114_CH_FREQ:                  return "GPIOE_114_CH_FREQ";
        case SYSTEM_FILE_GPIOE_115_CH_ENABLED:               return "GPIOE_115_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_115_CH_FREQ:                  return "GPIOE_115_CH_FREQ";
        case SYSTEM_FILE_GPIOE_116_CH_ENABLED:               return "GPIOE_116_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_116_CH_FREQ:                  return "GPIOE_116_CH_FREQ";
        case SYSTEM_FILE_GPIOE_117_CH_ENABLED:               return "GPIOE_117_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_117_CH_FREQ:                  return "GPIOE_117_CH_FREQ";
        case SYSTEM_FILE_GPIOE_118_CH_ENABLED:               return "GPIOE_118_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_118_CH_FREQ:                  return "GPIOE_118_CH_FREQ";
        case SYSTEM_FILE_GPIOE_119_CH_ENABLED:               return "GPIOE_119_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_119_CH_FREQ:                  return "GPIOE_119_CH_FREQ";
        case SYSTEM_FILE_GPIOE_120_CH_ENABLED:               return "GPIOE_120_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_120_CH_FREQ:                  return "GPIOE_120_CH_FREQ";
        case SYSTEM_FILE_GPIOE_121_CH_ENABLED:               return "GPIOE_121_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_121_CH_FREQ:                  return "GPIOE_121_CH_FREQ";
        case SYSTEM_FILE_GPIOE_122_CH_ENABLED:               return "GPIOE_122_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_122_CH_FREQ:                  return "GPIOE_122_CH_FREQ";
        case SYSTEM_FILE_GPIOE_123_CH_ENABLED:               return "GPIOE_123_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_123_CH_FREQ:                  return "GPIOE_123_CH_FREQ";
        case SYSTEM_FILE_GPIOE_124_CH_ENABLED:               return "GPIOE_124_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_124_CH_FREQ:                  return "GPIOE_124_CH_FREQ";
        case SYSTEM_FILE_GPIOE_125_CH_ENABLED:               return "GPIOE_125_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_125_CH_FREQ:                  return "GPIOE_125_CH_FREQ";
        case SYSTEM_FILE_GPIOE_126_CH_ENABLED:               return "GPIOE_126_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_126_CH_FREQ:                  return "GPIOE_126_CH_FREQ";
        case SYSTEM_FILE_GPIOE_127_CH_ENABLED:               return "GPIOE_127_CH_ENABLED";
        case SYSTEM_FILE_GPIOE_127_CH_FREQ:                  return "GPIOE_127_CH_FREQ";

        case SYSTEM_FILE_PWRCFG_NAME:                    return "PWRCFG_NAME";
        case SYSTEM_FILE_PWRCFG_GPS:                     return "PWRCFG_GPS";
        case SYSTEM_FILE_PWRCFG_ADMPLEX0:                return "PWRCFG_ADMPLEX0";
        case SYSTEM_FILE_PWRCFG_ADMPLEX1:                return "PWRCFG_ADMPLEX1";
        case SYSTEM_FILE_PWRCFG_GYRO:                    return "PWRCFG_GYRO";
        case SYSTEM_FILE_PWRCFG_UNIVERSE:                return "PWRCFG_UNIVERSE";
        case SYSTEM_FILE_PWRCFG_SWITCHES:                return "PWRCFG_SWITCHES";
        case SYSTEM_FILE_PWRCFG_GPIOE_INPUT:    return "PWRCFG_GPIOE_INPUT";
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

    // GPIOE_0: GPIOE_0_CH_ENABLED / GPIOE_0_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_0
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_0.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_0_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_0.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_0.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_0_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_0.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_0

    // GPIOE_1: GPIOE_1_CH_ENABLED / GPIOE_1_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_1
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_1.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_1_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_1.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_1.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_1_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_1.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_1

    // GPIOE_2: GPIOE_2_CH_ENABLED / GPIOE_2_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_2
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_2.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_2_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_2.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_2.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_2_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_2.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_2

    // GPIOE_3: GPIOE_3_CH_ENABLED / GPIOE_3_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_3
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_3.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_3_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_3.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_3.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_3_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_3.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_3

    // GPIOE_4: GPIOE_4_CH_ENABLED / GPIOE_4_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_4
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_4.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_4_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_4.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_4.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_4_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_4.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_4

    // GPIOE_5: GPIOE_5_CH_ENABLED / GPIOE_5_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_5
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_5.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_5_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_5.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_5.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_5_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_5.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_5

    // GPIOE_6: GPIOE_6_CH_ENABLED / GPIOE_6_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_6
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_6.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_6_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_6.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_6.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_6_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_6.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_6

    // GPIOE_7: GPIOE_7_CH_ENABLED / GPIOE_7_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_7
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_7.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_7_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_7.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_7.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_7_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_7.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_7

    // GPIOE_8: GPIOE_8_CH_ENABLED / GPIOE_8_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_8
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_8.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_8_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_8.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_8.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_8_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_8.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_8

    // GPIOE_9: GPIOE_9_CH_ENABLED / GPIOE_9_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_9
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_9.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_9_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_9.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_9.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_9_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_9.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_9

    // GPIOE_10: GPIOE_10_CH_ENABLED / GPIOE_10_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_10
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_10.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_10_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_10.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_10.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_10_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_10.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_10

    // GPIOE_11: GPIOE_11_CH_ENABLED / GPIOE_11_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_11
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_11.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_11_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_11.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_11.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_11_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_11.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_11

    // GPIOE_12: GPIOE_12_CH_ENABLED / GPIOE_12_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_12
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_12.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_12_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_12.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_12.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_12_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_12.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_12

    // GPIOE_13: GPIOE_13_CH_ENABLED / GPIOE_13_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_13
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_13.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_13_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_13.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_13.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_13_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_13.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_13

    // GPIOE_14: GPIOE_14_CH_ENABLED / GPIOE_14_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_14
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_14.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_14_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_14.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_14.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_14_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_14.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_14

    // GPIOE_15: GPIOE_15_CH_ENABLED / GPIOE_15_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_15
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_15.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_15_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_15.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_15.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_15_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_15.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_15

    // GPIOE_16: GPIOE_16_CH_ENABLED / GPIOE_16_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_16
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_16.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_16_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_16.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_16.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_16_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_16.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_16

    // GPIOE_17: GPIOE_17_CH_ENABLED / GPIOE_17_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_17
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_17.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_17_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_17.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_17.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_17_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_17.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_17

    // GPIOE_18: GPIOE_18_CH_ENABLED / GPIOE_18_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_18
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_18.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_18_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_18.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_18.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_18_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_18.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_18

    // GPIOE_19: GPIOE_19_CH_ENABLED / GPIOE_19_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_19
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_19.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_19_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_19.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_19.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_19_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_19.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_19

    // GPIOE_20: GPIOE_20_CH_ENABLED / GPIOE_20_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_20
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_20.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_20_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_20.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_20.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_20_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_20.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_20

    // GPIOE_21: GPIOE_21_CH_ENABLED / GPIOE_21_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_21
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_21.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_21_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_21.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_21.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_21_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_21.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_21

    // GPIOE_22: GPIOE_22_CH_ENABLED / GPIOE_22_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_22
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_22.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_22_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_22.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_22.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_22_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_22.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_22

    // GPIOE_23: GPIOE_23_CH_ENABLED / GPIOE_23_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_23
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_23.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_23_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_23.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_23.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_23_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_23.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_23

    // GPIOE_24: GPIOE_24_CH_ENABLED / GPIOE_24_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_24
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_24.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_24_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_24.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_24.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_24_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_24.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_24

    // GPIOE_25: GPIOE_25_CH_ENABLED / GPIOE_25_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_25
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_25.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_25_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_25.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_25.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_25_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_25.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_25

    // GPIOE_26: GPIOE_26_CH_ENABLED / GPIOE_26_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_26
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_26.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_26_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_26.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_26.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_26_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_26.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_26

    // GPIOE_27: GPIOE_27_CH_ENABLED / GPIOE_27_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_27
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_27.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_27_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_27.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_27.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_27_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_27.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_27

    // GPIOE_28: GPIOE_28_CH_ENABLED / GPIOE_28_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_28
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_28.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_28_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_28.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_28.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_28_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_28.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_28

    // GPIOE_29: GPIOE_29_CH_ENABLED / GPIOE_29_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_29
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_29.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_29_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_29.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_29.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_29_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_29.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_29

    // GPIOE_30: GPIOE_30_CH_ENABLED / GPIOE_30_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_30
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_30.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_30_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_30.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_30.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_30_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_30.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_30

    // GPIOE_31: GPIOE_31_CH_ENABLED / GPIOE_31_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_31
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_31.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_31_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_31.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_31.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_31_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_31.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_31

    // GPIOE_32: GPIOE_32_CH_ENABLED / GPIOE_32_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_32
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_32.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_32_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_32.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_32.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_32_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_32.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_32

    // GPIOE_33: GPIOE_33_CH_ENABLED / GPIOE_33_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_33
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_33.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_33_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_33.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_33.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_33_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_33.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_33

    // GPIOE_34: GPIOE_34_CH_ENABLED / GPIOE_34_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_34
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_34.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_34_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_34.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_34.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_34_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_34.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_34

    // GPIOE_35: GPIOE_35_CH_ENABLED / GPIOE_35_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_35
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_35.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_35_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_35.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_35.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_35_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_35.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_35

    // GPIOE_36: GPIOE_36_CH_ENABLED / GPIOE_36_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_36
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_36.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_36_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_36.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_36.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_36_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_36.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_36

    // GPIOE_37: GPIOE_37_CH_ENABLED / GPIOE_37_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_37
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_37.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_37_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_37.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_37.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_37_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_37.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_37

    // GPIOE_38: GPIOE_38_CH_ENABLED / GPIOE_38_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_38
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_38.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_38_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_38.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_38.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_38_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_38.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_38

    // GPIOE_39: GPIOE_39_CH_ENABLED / GPIOE_39_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_39
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_39.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_39_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_39.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_39.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_39_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_39.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_39

    // GPIOE_40: GPIOE_40_CH_ENABLED / GPIOE_40_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_40
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_40.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_40_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_40.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_40.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_40_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_40.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_40

    // GPIOE_41: GPIOE_41_CH_ENABLED / GPIOE_41_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_41
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_41.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_41_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_41.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_41.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_41_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_41.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_41

    // GPIOE_42: GPIOE_42_CH_ENABLED / GPIOE_42_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_42
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_42.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_42_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_42.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_42.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_42_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_42.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_42

    // GPIOE_43: GPIOE_43_CH_ENABLED / GPIOE_43_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_43
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_43.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_43_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_43.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_43.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_43_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_43.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_43

    // GPIOE_44: GPIOE_44_CH_ENABLED / GPIOE_44_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_44
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_44.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_44_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_44.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_44.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_44_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_44.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_44

    // GPIOE_45: GPIOE_45_CH_ENABLED / GPIOE_45_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_45
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_45.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_45_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_45.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_45.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_45_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_45.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_45

    // GPIOE_46: GPIOE_46_CH_ENABLED / GPIOE_46_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_46
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_46.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_46_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_46.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_46.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_46_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_46.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_46

    // GPIOE_47: GPIOE_47_CH_ENABLED / GPIOE_47_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_47
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_47.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_47_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_47.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_47.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_47_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_47.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_47

    // GPIOE_48: GPIOE_48_CH_ENABLED / GPIOE_48_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_48
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_48.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_48_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_48.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_48.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_48_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_48.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_48

    // GPIOE_49: GPIOE_49_CH_ENABLED / GPIOE_49_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_49
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_49.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_49_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_49.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_49.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_49_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_49.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_49

    // GPIOE_50: GPIOE_50_CH_ENABLED / GPIOE_50_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_50
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_50.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_50_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_50.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_50.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_50_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_50.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_50

    // GPIOE_51: GPIOE_51_CH_ENABLED / GPIOE_51_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_51
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_51.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_51_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_51.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_51.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_51_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_51.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_51

    // GPIOE_52: GPIOE_52_CH_ENABLED / GPIOE_52_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_52
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_52.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_52_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_52.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_52.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_52_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_52.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_52

    // GPIOE_53: GPIOE_53_CH_ENABLED / GPIOE_53_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_53
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_53.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_53_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_53.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_53.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_53_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_53.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_53

    // GPIOE_54: GPIOE_54_CH_ENABLED / GPIOE_54_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_54
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_54.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_54_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_54.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_54.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_54_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_54.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_54

    // GPIOE_55: GPIOE_55_CH_ENABLED / GPIOE_55_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_55
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_55.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_55_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_55.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_55.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_55_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_55.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_55

    // GPIOE_56: GPIOE_56_CH_ENABLED / GPIOE_56_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_56
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_56.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_56_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_56.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_56.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_56_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_56.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_56

    // GPIOE_57: GPIOE_57_CH_ENABLED / GPIOE_57_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_57
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_57.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_57_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_57.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_57.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_57_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_57.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_57

    // GPIOE_58: GPIOE_58_CH_ENABLED / GPIOE_58_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_58
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_58.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_58_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_58.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_58.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_58_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_58.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_58

    // GPIOE_59: GPIOE_59_CH_ENABLED / GPIOE_59_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_59
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_59.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_59_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_59.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_59.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_59_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_59.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_59

    // GPIOE_60: GPIOE_60_CH_ENABLED / GPIOE_60_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_60
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_60.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_60_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_60.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_60.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_60_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_60.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_60

    // GPIOE_61: GPIOE_61_CH_ENABLED / GPIOE_61_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_61
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_61.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_61_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_61.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_61.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_61_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_61.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_61

    // GPIOE_62: GPIOE_62_CH_ENABLED / GPIOE_62_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_62
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_62.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_62_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_62.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_62.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_62_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_62.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_62

    // GPIOE_63: GPIOE_63_CH_ENABLED / GPIOE_63_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_63
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_63.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_63_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_63.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_63.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_63_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_63.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_63

    // GPIOE_64: GPIOE_64_CH_ENABLED / GPIOE_64_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_64
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_64.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_64_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_64.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_64.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_64_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_64.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_64

    // GPIOE_65: GPIOE_65_CH_ENABLED / GPIOE_65_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_65
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_65.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_65_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_65.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_65.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_65_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_65.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_65

    // GPIOE_66: GPIOE_66_CH_ENABLED / GPIOE_66_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_66
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_66.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_66_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_66.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_66.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_66_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_66.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_66

    // GPIOE_67: GPIOE_67_CH_ENABLED / GPIOE_67_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_67
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_67.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_67_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_67.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_67.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_67_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_67.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_67

    // GPIOE_68: GPIOE_68_CH_ENABLED / GPIOE_68_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_68
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_68.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_68_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_68.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_68.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_68_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_68.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_68

    // GPIOE_69: GPIOE_69_CH_ENABLED / GPIOE_69_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_69
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_69.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_69_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_69.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_69.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_69_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_69.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_69

    // GPIOE_70: GPIOE_70_CH_ENABLED / GPIOE_70_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_70
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_70.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_70_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_70.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_70.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_70_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_70.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_70

    // GPIOE_71: GPIOE_71_CH_ENABLED / GPIOE_71_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_71
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_71.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_71_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_71.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_71.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_71_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_71.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_71

    // GPIOE_72: GPIOE_72_CH_ENABLED / GPIOE_72_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_72
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_72.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_72_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_72.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_72.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_72_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_72.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_72

    // GPIOE_73: GPIOE_73_CH_ENABLED / GPIOE_73_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_73
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_73.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_73_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_73.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_73.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_73_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_73.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_73

    // GPIOE_74: GPIOE_74_CH_ENABLED / GPIOE_74_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_74
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_74.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_74_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_74.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_74.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_74_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_74.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_74

    // GPIOE_75: GPIOE_75_CH_ENABLED / GPIOE_75_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_75
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_75.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_75_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_75.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_75.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_75_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_75.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_75

    // GPIOE_76: GPIOE_76_CH_ENABLED / GPIOE_76_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_76
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_76.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_76_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_76.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_76.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_76_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_76.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_76

    // GPIOE_77: GPIOE_77_CH_ENABLED / GPIOE_77_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_77
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_77.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_77_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_77.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_77.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_77_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_77.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_77

    // GPIOE_78: GPIOE_78_CH_ENABLED / GPIOE_78_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_78
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_78.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_78_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_78.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_78.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_78_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_78.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_78

    // GPIOE_79: GPIOE_79_CH_ENABLED / GPIOE_79_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_79
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_79.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_79_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_79.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_79.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_79_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_79.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_79

    // GPIOE_80: GPIOE_80_CH_ENABLED / GPIOE_80_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_80
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_80.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_80_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_80.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_80.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_80_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_80.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_80

    // GPIOE_81: GPIOE_81_CH_ENABLED / GPIOE_81_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_81
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_81.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_81_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_81.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_81.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_81_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_81.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_81

    // GPIOE_82: GPIOE_82_CH_ENABLED / GPIOE_82_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_82
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_82.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_82_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_82.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_82.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_82_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_82.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_82

    // GPIOE_83: GPIOE_83_CH_ENABLED / GPIOE_83_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_83
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_83.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_83_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_83.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_83.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_83_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_83.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_83

    // GPIOE_84: GPIOE_84_CH_ENABLED / GPIOE_84_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_84
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_84.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_84_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_84.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_84.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_84_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_84.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_84

    // GPIOE_85: GPIOE_85_CH_ENABLED / GPIOE_85_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_85
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_85.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_85_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_85.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_85.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_85_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_85.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_85

    // GPIOE_86: GPIOE_86_CH_ENABLED / GPIOE_86_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_86
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_86.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_86_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_86.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_86.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_86_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_86.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_86

    // GPIOE_87: GPIOE_87_CH_ENABLED / GPIOE_87_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_87
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_87.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_87_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_87.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_87.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_87_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_87.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_87

    // GPIOE_88: GPIOE_88_CH_ENABLED / GPIOE_88_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_88
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_88.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_88_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_88.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_88.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_88_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_88.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_88

    // GPIOE_89: GPIOE_89_CH_ENABLED / GPIOE_89_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_89
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_89.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_89_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_89.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_89.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_89_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_89.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_89

    // GPIOE_90: GPIOE_90_CH_ENABLED / GPIOE_90_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_90
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_90.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_90_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_90.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_90.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_90_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_90.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_90

    // GPIOE_91: GPIOE_91_CH_ENABLED / GPIOE_91_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_91
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_91.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_91_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_91.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_91.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_91_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_91.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_91

    // GPIOE_92: GPIOE_92_CH_ENABLED / GPIOE_92_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_92
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_92.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_92_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_92.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_92.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_92_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_92.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_92

    // GPIOE_93: GPIOE_93_CH_ENABLED / GPIOE_93_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_93
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_93.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_93_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_93.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_93.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_93_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_93.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_93

    // GPIOE_94: GPIOE_94_CH_ENABLED / GPIOE_94_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_94
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_94.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_94_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_94.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_94.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_94_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_94.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_94

    // GPIOE_95: GPIOE_95_CH_ENABLED / GPIOE_95_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_95
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_95.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_95_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_95.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_95.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_95_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_95.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_95

    // GPIOE_96: GPIOE_96_CH_ENABLED / GPIOE_96_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_96
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_96.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_96_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_96.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_96.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_96_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_96.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_96

    // GPIOE_97: GPIOE_97_CH_ENABLED / GPIOE_97_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_97
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_97.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_97_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_97.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_97.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_97_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_97.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_97

    // GPIOE_98: GPIOE_98_CH_ENABLED / GPIOE_98_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_98
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_98.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_98_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_98.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_98.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_98_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_98.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_98

    // GPIOE_99: GPIOE_99_CH_ENABLED / GPIOE_99_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_99
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_99.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_99_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_99.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_99.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_99_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_99.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_99

    // GPIOE_100: GPIOE_100_CH_ENABLED / GPIOE_100_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_100
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_100.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_100_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_100.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_100.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_100_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_100.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_100

    // GPIOE_101: GPIOE_101_CH_ENABLED / GPIOE_101_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_101
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_101.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_101_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_101.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_101.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_101_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_101.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_101

    // GPIOE_102: GPIOE_102_CH_ENABLED / GPIOE_102_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_102
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_102.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_102_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_102.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_102.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_102_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_102.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_102

    // GPIOE_103: GPIOE_103_CH_ENABLED / GPIOE_103_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_103
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_103.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_103_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_103.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_103.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_103_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_103.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_103

    // GPIOE_104: GPIOE_104_CH_ENABLED / GPIOE_104_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_104
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_104.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_104_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_104.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_104.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_104_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_104.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_104

    // GPIOE_105: GPIOE_105_CH_ENABLED / GPIOE_105_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_105
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_105.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_105_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_105.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_105.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_105_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_105.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_105

    // GPIOE_106: GPIOE_106_CH_ENABLED / GPIOE_106_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_106
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_106.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_106_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_106.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_106.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_106_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_106.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_106

    // GPIOE_107: GPIOE_107_CH_ENABLED / GPIOE_107_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_107
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_107.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_107_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_107.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_107.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_107_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_107.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_107

    // GPIOE_108: GPIOE_108_CH_ENABLED / GPIOE_108_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_108
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_108.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_108_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_108.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_108.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_108_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_108.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_108

    // GPIOE_109: GPIOE_109_CH_ENABLED / GPIOE_109_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_109
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_109.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_109_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_109.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_109.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_109_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_109.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_109

    // GPIOE_110: GPIOE_110_CH_ENABLED / GPIOE_110_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_110
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_110.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_110_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_110.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_110.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_110_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_110.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_110

    // GPIOE_111: GPIOE_111_CH_ENABLED / GPIOE_111_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_111
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_111.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_111_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_111.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_111.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_111_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_111.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_111

    // GPIOE_112: GPIOE_112_CH_ENABLED / GPIOE_112_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_112
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_112.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_112_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_112.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_112.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_112_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_112.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_112

    // GPIOE_113: GPIOE_113_CH_ENABLED / GPIOE_113_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_113
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_113.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_113_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_113.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_113.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_113_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_113.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_113

    // GPIOE_114: GPIOE_114_CH_ENABLED / GPIOE_114_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_114
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_114.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_114_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_114.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_114.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_114_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_114.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_114

    // GPIOE_115: GPIOE_115_CH_ENABLED / GPIOE_115_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_115
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_115.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_115_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_115.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_115.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_115_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_115.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_115

    // GPIOE_116: GPIOE_116_CH_ENABLED / GPIOE_116_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_116
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_116.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_116_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_116.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_116.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_116_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_116.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_116

    // GPIOE_117: GPIOE_117_CH_ENABLED / GPIOE_117_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_117
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_117.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_117_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_117.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_117.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_117_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_117.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_117

    // GPIOE_118: GPIOE_118_CH_ENABLED / GPIOE_118_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_118
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_118.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_118_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_118.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_118.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_118_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_118.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_118

    // GPIOE_119: GPIOE_119_CH_ENABLED / GPIOE_119_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_119
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_119.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_119_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_119.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_119.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_119_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_119.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_119

    // GPIOE_120: GPIOE_120_CH_ENABLED / GPIOE_120_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_120
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_120.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_120_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_120.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_120.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_120_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_120.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_120

    // GPIOE_121: GPIOE_121_CH_ENABLED / GPIOE_121_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_121
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_121.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_121_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_121.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_121.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_121_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_121.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_121

    // GPIOE_122: GPIOE_122_CH_ENABLED / GPIOE_122_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_122
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_122.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_122_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_122.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_122.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_122_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_122.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_122

    // GPIOE_123: GPIOE_123_CH_ENABLED / GPIOE_123_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_123
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_123.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_123_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_123.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_123.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_123_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_123.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_123

    // GPIOE_124: GPIOE_124_CH_ENABLED / GPIOE_124_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_124
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_124.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_124_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_124.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_124.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_124_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_124.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_124

    // GPIOE_125: GPIOE_125_CH_ENABLED / GPIOE_125_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_125
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_125.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_125_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_125.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_125.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_125_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_125.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_125

    // GPIOE_126: GPIOE_126_CH_ENABLED / GPIOE_126_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_126
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_126.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_126_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_126.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_126.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_126_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_126.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_126

    // GPIOE_127: GPIOE_127_CH_ENABLED / GPIOE_127_CH_FREQ
    #ifdef SatIO_USE_GPIOPE_INPUT_127
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_127.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_GPIOE_127_CH_ENABLED), i_ch, (int)GPIOPE_INPUT_127.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPE_INPUT_127.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_GPIOE_127_CH_FREQ), i_ch, (unsigned long long)GPIOPE_INPUT_127.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }
    #endif // SatIO_USE_GPIOPE_INPUT_127


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
    WRITE_UINT32_TAG(SYSTEM_FILE_PWRCFG_GPIOE_INPUT, pwrConfigCurrent.TASK_MAX_FREQ_GPIOE_INPUT);
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

        // GPIOE_0: GPIOE_0_CH_ENABLED and GPIOE_0_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_0_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_0_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_0
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_0.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_0_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_0, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_0_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_0, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_0
            continue;
        }

        // GPIOE_1: GPIOE_1_CH_ENABLED and GPIOE_1_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_1_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_1_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_1
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_1.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_1_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_1, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_1_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_1, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_1
            continue;
        }

        // GPIOE_2: GPIOE_2_CH_ENABLED and GPIOE_2_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_2_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_2_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_2
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_2.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_2_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_2, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_2_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_2, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_2
            continue;
        }

        // GPIOE_3: GPIOE_3_CH_ENABLED and GPIOE_3_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_3_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_3_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_3
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_3.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_3_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_3, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_3_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_3, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_3
            continue;
        }

        // GPIOE_4: GPIOE_4_CH_ENABLED and GPIOE_4_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_4_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_4_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_4
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_4.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_4_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_4, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_4_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_4, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_4
            continue;
        }

        // GPIOE_5: GPIOE_5_CH_ENABLED and GPIOE_5_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_5_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_5_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_5
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_5.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_5_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_5, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_5_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_5, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_5
            continue;
        }

        // GPIOE_6: GPIOE_6_CH_ENABLED and GPIOE_6_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_6_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_6_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_6
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_6.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_6_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_6, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_6_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_6, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_6
            continue;
        }

        // GPIOE_7: GPIOE_7_CH_ENABLED and GPIOE_7_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_7_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_7_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_7
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_7.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_7_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_7, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_7_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_7, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_7
            continue;
        }

        // GPIOE_8: GPIOE_8_CH_ENABLED and GPIOE_8_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_8_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_8_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_8
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_8.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_8_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_8, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_8_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_8, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_8
            continue;
        }

        // GPIOE_9: GPIOE_9_CH_ENABLED and GPIOE_9_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_9_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_9_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_9
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_9.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_9_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_9, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_9_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_9, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_9
            continue;
        }

        // GPIOE_10: GPIOE_10_CH_ENABLED and GPIOE_10_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_10_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_10_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_10
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_10.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_10_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_10, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_10_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_10, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_10
            continue;
        }

        // GPIOE_11: GPIOE_11_CH_ENABLED and GPIOE_11_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_11_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_11_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_11
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_11.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_11_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_11, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_11_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_11, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_11
            continue;
        }

        // GPIOE_12: GPIOE_12_CH_ENABLED and GPIOE_12_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_12_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_12_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_12
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_12.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_12_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_12, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_12_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_12, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_12
            continue;
        }

        // GPIOE_13: GPIOE_13_CH_ENABLED and GPIOE_13_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_13_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_13_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_13
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_13.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_13_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_13, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_13_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_13, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_13
            continue;
        }

        // GPIOE_14: GPIOE_14_CH_ENABLED and GPIOE_14_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_14_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_14_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_14
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_14.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_14_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_14, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_14_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_14, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_14
            continue;
        }

        // GPIOE_15: GPIOE_15_CH_ENABLED and GPIOE_15_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_15_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_15_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_15
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_15.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_15_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_15, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_15_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_15, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_15
            continue;
        }

        // GPIOE_16: GPIOE_16_CH_ENABLED and GPIOE_16_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_16_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_16_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_16
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_16.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_16_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_16, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_16_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_16, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_16
            continue;
        }

        // GPIOE_17: GPIOE_17_CH_ENABLED and GPIOE_17_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_17_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_17_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_17
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_17.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_17_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_17, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_17_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_17, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_17
            continue;
        }

        // GPIOE_18: GPIOE_18_CH_ENABLED and GPIOE_18_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_18_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_18_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_18
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_18.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_18_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_18, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_18_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_18, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_18
            continue;
        }

        // GPIOE_19: GPIOE_19_CH_ENABLED and GPIOE_19_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_19_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_19_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_19
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_19.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_19_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_19, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_19_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_19, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_19
            continue;
        }

        // GPIOE_20: GPIOE_20_CH_ENABLED and GPIOE_20_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_20_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_20_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_20
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_20.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_20_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_20, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_20_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_20, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_20
            continue;
        }

        // GPIOE_21: GPIOE_21_CH_ENABLED and GPIOE_21_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_21_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_21_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_21
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_21.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_21_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_21, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_21_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_21, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_21
            continue;
        }

        // GPIOE_22: GPIOE_22_CH_ENABLED and GPIOE_22_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_22_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_22_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_22
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_22.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_22_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_22, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_22_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_22, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_22
            continue;
        }

        // GPIOE_23: GPIOE_23_CH_ENABLED and GPIOE_23_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_23_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_23_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_23
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_23.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_23_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_23, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_23_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_23, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_23
            continue;
        }

        // GPIOE_24: GPIOE_24_CH_ENABLED and GPIOE_24_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_24_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_24_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_24
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_24.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_24_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_24, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_24_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_24, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_24
            continue;
        }

        // GPIOE_25: GPIOE_25_CH_ENABLED and GPIOE_25_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_25_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_25_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_25
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_25.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_25_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_25, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_25_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_25, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_25
            continue;
        }

        // GPIOE_26: GPIOE_26_CH_ENABLED and GPIOE_26_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_26_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_26_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_26
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_26.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_26_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_26, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_26_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_26, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_26
            continue;
        }

        // GPIOE_27: GPIOE_27_CH_ENABLED and GPIOE_27_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_27_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_27_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_27
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_27.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_27_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_27, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_27_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_27, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_27
            continue;
        }

        // GPIOE_28: GPIOE_28_CH_ENABLED and GPIOE_28_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_28_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_28_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_28
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_28.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_28_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_28, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_28_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_28, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_28
            continue;
        }

        // GPIOE_29: GPIOE_29_CH_ENABLED and GPIOE_29_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_29_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_29_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_29
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_29.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_29_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_29, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_29_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_29, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_29
            continue;
        }

        // GPIOE_30: GPIOE_30_CH_ENABLED and GPIOE_30_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_30_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_30_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_30
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_30.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_30_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_30, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_30_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_30, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_30
            continue;
        }

        // GPIOE_31: GPIOE_31_CH_ENABLED and GPIOE_31_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_31_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_31_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_31
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_31.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_31_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_31, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_31_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_31, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_31
            continue;
        }

        // GPIOE_32: GPIOE_32_CH_ENABLED and GPIOE_32_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_32_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_32_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_32
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_32.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_32_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_32, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_32_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_32, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_32
            continue;
        }

        // GPIOE_33: GPIOE_33_CH_ENABLED and GPIOE_33_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_33_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_33_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_33
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_33.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_33_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_33, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_33_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_33, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_33
            continue;
        }

        // GPIOE_34: GPIOE_34_CH_ENABLED and GPIOE_34_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_34_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_34_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_34
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_34.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_34_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_34, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_34_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_34, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_34
            continue;
        }

        // GPIOE_35: GPIOE_35_CH_ENABLED and GPIOE_35_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_35_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_35_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_35
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_35.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_35_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_35, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_35_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_35, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_35
            continue;
        }

        // GPIOE_36: GPIOE_36_CH_ENABLED and GPIOE_36_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_36_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_36_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_36
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_36.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_36_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_36, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_36_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_36, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_36
            continue;
        }

        // GPIOE_37: GPIOE_37_CH_ENABLED and GPIOE_37_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_37_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_37_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_37
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_37.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_37_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_37, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_37_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_37, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_37
            continue;
        }

        // GPIOE_38: GPIOE_38_CH_ENABLED and GPIOE_38_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_38_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_38_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_38
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_38.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_38_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_38, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_38_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_38, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_38
            continue;
        }

        // GPIOE_39: GPIOE_39_CH_ENABLED and GPIOE_39_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_39_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_39_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_39
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_39.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_39_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_39, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_39_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_39, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_39
            continue;
        }

        // GPIOE_40: GPIOE_40_CH_ENABLED and GPIOE_40_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_40_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_40_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_40
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_40.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_40_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_40, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_40_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_40, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_40
            continue;
        }

        // GPIOE_41: GPIOE_41_CH_ENABLED and GPIOE_41_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_41_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_41_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_41
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_41.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_41_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_41, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_41_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_41, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_41
            continue;
        }

        // GPIOE_42: GPIOE_42_CH_ENABLED and GPIOE_42_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_42_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_42_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_42
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_42.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_42_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_42, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_42_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_42, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_42
            continue;
        }

        // GPIOE_43: GPIOE_43_CH_ENABLED and GPIOE_43_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_43_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_43_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_43
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_43.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_43_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_43, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_43_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_43, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_43
            continue;
        }

        // GPIOE_44: GPIOE_44_CH_ENABLED and GPIOE_44_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_44_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_44_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_44
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_44.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_44_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_44, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_44_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_44, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_44
            continue;
        }

        // GPIOE_45: GPIOE_45_CH_ENABLED and GPIOE_45_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_45_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_45_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_45
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_45.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_45_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_45, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_45_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_45, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_45
            continue;
        }

        // GPIOE_46: GPIOE_46_CH_ENABLED and GPIOE_46_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_46_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_46_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_46
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_46.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_46_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_46, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_46_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_46, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_46
            continue;
        }

        // GPIOE_47: GPIOE_47_CH_ENABLED and GPIOE_47_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_47_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_47_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_47
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_47.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_47_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_47, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_47_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_47, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_47
            continue;
        }

        // GPIOE_48: GPIOE_48_CH_ENABLED and GPIOE_48_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_48_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_48_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_48
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_48.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_48_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_48, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_48_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_48, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_48
            continue;
        }

        // GPIOE_49: GPIOE_49_CH_ENABLED and GPIOE_49_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_49_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_49_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_49
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_49.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_49_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_49, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_49_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_49, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_49
            continue;
        }

        // GPIOE_50: GPIOE_50_CH_ENABLED and GPIOE_50_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_50_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_50_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_50
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_50.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_50_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_50, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_50_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_50, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_50
            continue;
        }

        // GPIOE_51: GPIOE_51_CH_ENABLED and GPIOE_51_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_51_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_51_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_51
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_51.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_51_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_51, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_51_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_51, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_51
            continue;
        }

        // GPIOE_52: GPIOE_52_CH_ENABLED and GPIOE_52_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_52_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_52_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_52
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_52.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_52_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_52, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_52_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_52, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_52
            continue;
        }

        // GPIOE_53: GPIOE_53_CH_ENABLED and GPIOE_53_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_53_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_53_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_53
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_53.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_53_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_53, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_53_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_53, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_53
            continue;
        }

        // GPIOE_54: GPIOE_54_CH_ENABLED and GPIOE_54_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_54_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_54_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_54
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_54.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_54_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_54, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_54_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_54, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_54
            continue;
        }

        // GPIOE_55: GPIOE_55_CH_ENABLED and GPIOE_55_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_55_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_55_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_55
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_55.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_55_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_55, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_55_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_55, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_55
            continue;
        }

        // GPIOE_56: GPIOE_56_CH_ENABLED and GPIOE_56_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_56_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_56_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_56
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_56.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_56_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_56, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_56_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_56, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_56
            continue;
        }

        // GPIOE_57: GPIOE_57_CH_ENABLED and GPIOE_57_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_57_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_57_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_57
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_57.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_57_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_57, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_57_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_57, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_57
            continue;
        }

        // GPIOE_58: GPIOE_58_CH_ENABLED and GPIOE_58_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_58_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_58_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_58
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_58.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_58_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_58, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_58_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_58, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_58
            continue;
        }

        // GPIOE_59: GPIOE_59_CH_ENABLED and GPIOE_59_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_59_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_59_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_59
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_59.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_59_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_59, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_59_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_59, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_59
            continue;
        }

        // GPIOE_60: GPIOE_60_CH_ENABLED and GPIOE_60_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_60_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_60_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_60
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_60.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_60_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_60, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_60_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_60, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_60
            continue;
        }

        // GPIOE_61: GPIOE_61_CH_ENABLED and GPIOE_61_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_61_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_61_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_61
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_61.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_61_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_61, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_61_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_61, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_61
            continue;
        }

        // GPIOE_62: GPIOE_62_CH_ENABLED and GPIOE_62_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_62_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_62_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_62
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_62.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_62_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_62, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_62_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_62, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_62
            continue;
        }

        // GPIOE_63: GPIOE_63_CH_ENABLED and GPIOE_63_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_63_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_63_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_63
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_63.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_63_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_63, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_63_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_63, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_63
            continue;
        }

        // GPIOE_64: GPIOE_64_CH_ENABLED and GPIOE_64_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_64_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_64_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_64
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_64.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_64_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_64, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_64_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_64, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_64
            continue;
        }

        // GPIOE_65: GPIOE_65_CH_ENABLED and GPIOE_65_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_65_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_65_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_65
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_65.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_65_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_65, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_65_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_65, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_65
            continue;
        }

        // GPIOE_66: GPIOE_66_CH_ENABLED and GPIOE_66_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_66_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_66_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_66
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_66.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_66_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_66, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_66_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_66, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_66
            continue;
        }

        // GPIOE_67: GPIOE_67_CH_ENABLED and GPIOE_67_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_67_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_67_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_67
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_67.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_67_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_67, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_67_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_67, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_67
            continue;
        }

        // GPIOE_68: GPIOE_68_CH_ENABLED and GPIOE_68_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_68_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_68_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_68
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_68.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_68_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_68, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_68_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_68, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_68
            continue;
        }

        // GPIOE_69: GPIOE_69_CH_ENABLED and GPIOE_69_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_69_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_69_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_69
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_69.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_69_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_69, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_69_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_69, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_69
            continue;
        }

        // GPIOE_70: GPIOE_70_CH_ENABLED and GPIOE_70_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_70_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_70_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_70
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_70.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_70_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_70, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_70_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_70, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_70
            continue;
        }

        // GPIOE_71: GPIOE_71_CH_ENABLED and GPIOE_71_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_71_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_71_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_71
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_71.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_71_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_71, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_71_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_71, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_71
            continue;
        }

        // GPIOE_72: GPIOE_72_CH_ENABLED and GPIOE_72_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_72_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_72_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_72
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_72.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_72_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_72, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_72_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_72, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_72
            continue;
        }

        // GPIOE_73: GPIOE_73_CH_ENABLED and GPIOE_73_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_73_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_73_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_73
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_73.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_73_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_73, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_73_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_73, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_73
            continue;
        }

        // GPIOE_74: GPIOE_74_CH_ENABLED and GPIOE_74_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_74_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_74_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_74
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_74.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_74_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_74, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_74_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_74, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_74
            continue;
        }

        // GPIOE_75: GPIOE_75_CH_ENABLED and GPIOE_75_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_75_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_75_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_75
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_75.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_75_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_75, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_75_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_75, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_75
            continue;
        }

        // GPIOE_76: GPIOE_76_CH_ENABLED and GPIOE_76_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_76_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_76_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_76
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_76.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_76_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_76, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_76_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_76, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_76
            continue;
        }

        // GPIOE_77: GPIOE_77_CH_ENABLED and GPIOE_77_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_77_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_77_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_77
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_77.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_77_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_77, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_77_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_77, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_77
            continue;
        }

        // GPIOE_78: GPIOE_78_CH_ENABLED and GPIOE_78_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_78_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_78_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_78
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_78.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_78_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_78, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_78_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_78, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_78
            continue;
        }

        // GPIOE_79: GPIOE_79_CH_ENABLED and GPIOE_79_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_79_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_79_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_79
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_79.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_79_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_79, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_79_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_79, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_79
            continue;
        }

        // GPIOE_80: GPIOE_80_CH_ENABLED and GPIOE_80_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_80_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_80_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_80
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_80.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_80_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_80, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_80_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_80, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_80
            continue;
        }

        // GPIOE_81: GPIOE_81_CH_ENABLED and GPIOE_81_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_81_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_81_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_81
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_81.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_81_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_81, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_81_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_81, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_81
            continue;
        }

        // GPIOE_82: GPIOE_82_CH_ENABLED and GPIOE_82_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_82_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_82_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_82
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_82.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_82_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_82, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_82_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_82, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_82
            continue;
        }

        // GPIOE_83: GPIOE_83_CH_ENABLED and GPIOE_83_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_83_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_83_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_83
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_83.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_83_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_83, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_83_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_83, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_83
            continue;
        }

        // GPIOE_84: GPIOE_84_CH_ENABLED and GPIOE_84_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_84_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_84_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_84
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_84.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_84_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_84, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_84_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_84, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_84
            continue;
        }

        // GPIOE_85: GPIOE_85_CH_ENABLED and GPIOE_85_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_85_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_85_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_85
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_85.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_85_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_85, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_85_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_85, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_85
            continue;
        }

        // GPIOE_86: GPIOE_86_CH_ENABLED and GPIOE_86_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_86_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_86_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_86
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_86.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_86_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_86, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_86_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_86, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_86
            continue;
        }

        // GPIOE_87: GPIOE_87_CH_ENABLED and GPIOE_87_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_87_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_87_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_87
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_87.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_87_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_87, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_87_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_87, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_87
            continue;
        }

        // GPIOE_88: GPIOE_88_CH_ENABLED and GPIOE_88_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_88_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_88_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_88
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_88.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_88_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_88, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_88_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_88, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_88
            continue;
        }

        // GPIOE_89: GPIOE_89_CH_ENABLED and GPIOE_89_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_89_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_89_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_89
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_89.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_89_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_89, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_89_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_89, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_89
            continue;
        }

        // GPIOE_90: GPIOE_90_CH_ENABLED and GPIOE_90_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_90_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_90_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_90
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_90.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_90_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_90, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_90_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_90, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_90
            continue;
        }

        // GPIOE_91: GPIOE_91_CH_ENABLED and GPIOE_91_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_91_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_91_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_91
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_91.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_91_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_91, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_91_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_91, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_91
            continue;
        }

        // GPIOE_92: GPIOE_92_CH_ENABLED and GPIOE_92_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_92_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_92_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_92
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_92.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_92_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_92, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_92_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_92, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_92
            continue;
        }

        // GPIOE_93: GPIOE_93_CH_ENABLED and GPIOE_93_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_93_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_93_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_93
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_93.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_93_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_93, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_93_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_93, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_93
            continue;
        }

        // GPIOE_94: GPIOE_94_CH_ENABLED and GPIOE_94_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_94_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_94_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_94
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_94.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_94_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_94, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_94_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_94, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_94
            continue;
        }

        // GPIOE_95: GPIOE_95_CH_ENABLED and GPIOE_95_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_95_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_95_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_95
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_95.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_95_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_95, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_95_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_95, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_95
            continue;
        }

        // GPIOE_96: GPIOE_96_CH_ENABLED and GPIOE_96_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_96_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_96_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_96
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_96.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_96_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_96, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_96_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_96, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_96
            continue;
        }

        // GPIOE_97: GPIOE_97_CH_ENABLED and GPIOE_97_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_97_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_97_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_97
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_97.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_97_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_97, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_97_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_97, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_97
            continue;
        }

        // GPIOE_98: GPIOE_98_CH_ENABLED and GPIOE_98_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_98_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_98_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_98
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_98.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_98_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_98, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_98_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_98, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_98
            continue;
        }

        // GPIOE_99: GPIOE_99_CH_ENABLED and GPIOE_99_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_99_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_99_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_99
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_99.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_99_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_99, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_99_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_99, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_99
            continue;
        }

        // GPIOE_100: GPIOE_100_CH_ENABLED and GPIOE_100_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_100_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_100_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_100
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_100.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_100_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_100, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_100_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_100, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_100
            continue;
        }

        // GPIOE_101: GPIOE_101_CH_ENABLED and GPIOE_101_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_101_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_101_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_101
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_101.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_101_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_101, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_101_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_101, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_101
            continue;
        }

        // GPIOE_102: GPIOE_102_CH_ENABLED and GPIOE_102_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_102_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_102_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_102
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_102.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_102_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_102, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_102_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_102, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_102
            continue;
        }

        // GPIOE_103: GPIOE_103_CH_ENABLED and GPIOE_103_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_103_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_103_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_103
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_103.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_103_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_103, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_103_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_103, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_103
            continue;
        }

        // GPIOE_104: GPIOE_104_CH_ENABLED and GPIOE_104_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_104_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_104_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_104
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_104.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_104_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_104, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_104_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_104, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_104
            continue;
        }

        // GPIOE_105: GPIOE_105_CH_ENABLED and GPIOE_105_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_105_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_105_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_105
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_105.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_105_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_105, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_105_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_105, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_105
            continue;
        }

        // GPIOE_106: GPIOE_106_CH_ENABLED and GPIOE_106_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_106_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_106_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_106
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_106.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_106_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_106, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_106_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_106, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_106
            continue;
        }

        // GPIOE_107: GPIOE_107_CH_ENABLED and GPIOE_107_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_107_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_107_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_107
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_107.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_107_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_107, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_107_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_107, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_107
            continue;
        }

        // GPIOE_108: GPIOE_108_CH_ENABLED and GPIOE_108_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_108_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_108_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_108
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_108.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_108_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_108, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_108_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_108, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_108
            continue;
        }

        // GPIOE_109: GPIOE_109_CH_ENABLED and GPIOE_109_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_109_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_109_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_109
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_109.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_109_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_109, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_109_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_109, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_109
            continue;
        }

        // GPIOE_110: GPIOE_110_CH_ENABLED and GPIOE_110_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_110_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_110_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_110
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_110.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_110_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_110, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_110_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_110, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_110
            continue;
        }

        // GPIOE_111: GPIOE_111_CH_ENABLED and GPIOE_111_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_111_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_111_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_111
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_111.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_111_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_111, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_111_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_111, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_111
            continue;
        }

        // GPIOE_112: GPIOE_112_CH_ENABLED and GPIOE_112_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_112_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_112_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_112
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_112.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_112_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_112, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_112_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_112, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_112
            continue;
        }

        // GPIOE_113: GPIOE_113_CH_ENABLED and GPIOE_113_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_113_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_113_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_113
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_113.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_113_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_113, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_113_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_113, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_113
            continue;
        }

        // GPIOE_114: GPIOE_114_CH_ENABLED and GPIOE_114_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_114_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_114_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_114
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_114.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_114_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_114, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_114_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_114, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_114
            continue;
        }

        // GPIOE_115: GPIOE_115_CH_ENABLED and GPIOE_115_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_115_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_115_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_115
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_115.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_115_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_115, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_115_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_115, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_115
            continue;
        }

        // GPIOE_116: GPIOE_116_CH_ENABLED and GPIOE_116_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_116_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_116_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_116
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_116.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_116_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_116, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_116_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_116, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_116
            continue;
        }

        // GPIOE_117: GPIOE_117_CH_ENABLED and GPIOE_117_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_117_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_117_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_117
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_117.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_117_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_117, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_117_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_117, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_117
            continue;
        }

        // GPIOE_118: GPIOE_118_CH_ENABLED and GPIOE_118_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_118_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_118_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_118
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_118.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_118_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_118, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_118_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_118, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_118
            continue;
        }

        // GPIOE_119: GPIOE_119_CH_ENABLED and GPIOE_119_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_119_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_119_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_119
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_119.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_119_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_119, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_119_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_119, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_119
            continue;
        }

        // GPIOE_120: GPIOE_120_CH_ENABLED and GPIOE_120_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_120_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_120_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_120
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_120.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_120_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_120, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_120_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_120, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_120
            continue;
        }

        // GPIOE_121: GPIOE_121_CH_ENABLED and GPIOE_121_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_121_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_121_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_121
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_121.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_121_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_121, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_121_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_121, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_121
            continue;
        }

        // GPIOE_122: GPIOE_122_CH_ENABLED and GPIOE_122_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_122_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_122_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_122
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_122.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_122_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_122, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_122_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_122, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_122
            continue;
        }

        // GPIOE_123: GPIOE_123_CH_ENABLED and GPIOE_123_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_123_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_123_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_123
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_123.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_123_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_123, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_123_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_123, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_123
            continue;
        }

        // GPIOE_124: GPIOE_124_CH_ENABLED and GPIOE_124_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_124_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_124_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_124
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_124.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_124_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_124, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_124_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_124, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_124
            continue;
        }

        // GPIOE_125: GPIOE_125_CH_ENABLED and GPIOE_125_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_125_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_125_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_125
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_125.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_125_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_125, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_125_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_125, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_125
            continue;
        }

        // GPIOE_126: GPIOE_126_CH_ENABLED and GPIOE_126_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_126_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_126_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_126
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_126.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_126_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_126, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_126_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_126, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_126
            continue;
        }

        // GPIOE_127: GPIOE_127_CH_ENABLED and GPIOE_127_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_GPIOE_127_CH_ENABLED || tag_index == SYSTEM_FILE_GPIOE_127_CH_FREQ) {
            #ifdef SatIO_USE_GPIOPE_INPUT_127
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPE_INPUT_127.max_pins) {
                    if (tag_index == SYSTEM_FILE_GPIOE_127_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPE_INPUT_127, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_GPIOE_127_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPE_INPUT_127, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
            #endif // SatIO_USE_GPIOPE_INPUT_127
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
        READ_UINT32_TAG(SYSTEM_FILE_PWRCFG_GPIOE_INPUT, pwrConfigCurrent.TASK_MAX_FREQ_GPIOE_INPUT);
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
