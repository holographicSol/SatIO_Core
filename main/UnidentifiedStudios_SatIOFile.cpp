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
    for (int i=0; i<MAX_AD_MUX_CHANNELS; i++) {line=line+String(ad_mux_0.data[i])+",";}
    printLogLine(line.c_str());
    // --------------------------------
    // Log Line: Port Controller Input
    // --------------------------------
    line="$PCINPT,";
    for (int i=0; i<MAX_MATRIX_SWITCHES; i++) {line=line+String(GPIOPortExpander_ATMEGA2560_Input_0.input_value[i])+",";}
    printLogLine(line.c_str());
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
    SYSTEM_FILE_PCI_CH_ENABLED,
    SYSTEM_FILE_PCI_CH_FREQ,

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
        case SYSTEM_FILE_PCI_CH_ENABLED:                 return "PCI_CH_ENABLED";
        case SYSTEM_FILE_PCI_CH_FREQ:                    return "PCI_CH_FREQ";

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

    // PCI_CH_ENABLED / PCI_CH_FREQ
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_0.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%d", getSystemTag(SYSTEM_FILE_PCI_CH_ENABLED), i_ch, (int)GPIOPortExpander_ATMEGA2560_Input_0.enabled[i_ch]);
        printLine(f, lineBuf);
    }
    for (int i_ch=0; i_ch<(int)GPIOPortExpander_ATMEGA2560_Input_0.max_pins; i_ch++) {
        snprintf(lineBuf, 256, "%s,%d,%llu", getSystemTag(SYSTEM_FILE_PCI_CH_FREQ), i_ch, (unsigned long long)GPIOPortExpander_ATMEGA2560_Input_0.chan_freq_uS[i_ch]);
        printLine(f, lineBuf);
    }

    // Power config: task max-frequency values (uS) currently in effect, plus the
    // active preset's display name (see PwrConfig in UnidentifiedStudios_Config.h).
    WRITE_STR_TAG(SYSTEM_FILE_PWRCFG_NAME, pwrConfigCurrent.name);
    WRITE_UINT32_TAG(SYSTEM_FILE_PWRCFG_GPS, pwrConfigCurrent.TASK_MAX_FREQ_GPS);
    #ifdef SatIO_CD74HC4067_OPTION_USE_1
    WRITE_UINT32_TAG(SYSTEM_FILE_PWRCFG_ADMPLEX0, pwrConfigCurrent.TASK_MAX_FREQ_ADMPLEX0);
    #endif
    #ifdef SatIO_CD74HC4067_OPTION_USE_2
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

        // PCI_CH_ENABLED and PCI_CH_FREQ: "TAG,pin,value" (pin-indexed, unlike the single-value tags below).
        if (tag_index == SYSTEM_FILE_PCI_CH_ENABLED || tag_index == SYSTEM_FILE_PCI_CH_FREQ) {
            char *val2 = strtok(NULL, ",");
            if (val2 != NULL && str_is_int8(val)) {
                int ch = atoi(val);
                if (ch >= 0 && ch < (int)GPIOPortExpander_ATMEGA2560_Input_0.max_pins) {
                    if (tag_index == SYSTEM_FILE_PCI_CH_ENABLED && str_is_bool(val2)) {setGPIOPortExpanderChannelEnabled(GPIOPortExpander_ATMEGA2560_Input_0, (uint8_t)ch, atoi(val2) != 0);}
                    else if (tag_index == SYSTEM_FILE_PCI_CH_FREQ && str_is_uint64(val2)) {setGPIOPortExpanderChannelFreq(GPIOPortExpander_ATMEGA2560_Input_0, (uint8_t)ch, strtoull(val2, NULL, 10));}
                    else { /* value failed validation for this tag: skip */ }
                }
            }
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
        #ifdef SatIO_CD74HC4067_OPTION_USE_1
        READ_UINT32_TAG(SYSTEM_FILE_PWRCFG_ADMPLEX0, pwrConfigCurrent.TASK_MAX_FREQ_ADMPLEX0);
        #endif
        #ifdef SatIO_CD74HC4067_OPTION_USE_2
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