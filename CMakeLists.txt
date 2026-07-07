idf_component_register(
    SRCS
        Adafruit_I2CDevice.cpp
        font_cobalt_alien_17.c
        font_cobalt_alien_25.c
        font_mono_bold_12.c
        font_mono_bold_14.c
        font_mono_bold_16.c
        font_unscii_12.c
        main.cpp
        SiderealObjects.cpp
        SiderealObjectsTables.cpp
        SiderealPlanets.cpp
        UnidentifiedStudios_ArgParser.cpp
        UnidentifiedStudios_AstroClock.cpp
        UnidentifiedStudios_CMD.cpp
        UnidentifiedStudios_Config.cpp
        UnidentifiedStudios_GPIOPortExpander.cpp
        UnidentifiedStudios_HexToDig.cpp
        UnidentifiedStudios_I2C.cpp
        UnidentifiedStudios_INS.cpp
        UnidentifiedStudios_LMST.cpp
        UnidentifiedStudios_Mapping.cpp
        UnidentifiedStudios_Matrix.cpp
        UnidentifiedStudios_Meteors.cpp
        UnidentifiedStudios_Multiplexers.cpp
        UnidentifiedStudios_SatIO.cpp
        UnidentifiedStudios_SatIOFile.cpp
        UnidentifiedStudios_SatIOLVGL.cpp
        UnidentifiedStudios_SdCardHelper.cpp
        UnidentifiedStudios_SiderealHelper.cpp
        UnidentifiedStudios_StrVal.cpp
        UnidentifiedStudios_SystemData.cpp
        UnidentifiedStudios_TaskHandler.cpp
        UnidentifiedStudios_WT901.cpp
        UnidentifiedStudios_WTGPS300P.cpp
        wit_c_sdk.cpp
    INCLUDE_DIRS
        .
)

# add_definitions(-DARDUINO_USB_MODE -DARDUINO_USB_CDC_ON_BOOT)

# # Include extra CMake options to relax warnings-as-errors
# include("${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt.extra")

# Disable strict warnings for SiderealPlanets library (Arduino port)
set_source_files_properties(
    SiderealPlanets.cpp
    SiderealObjects.cpp
    PROPERTIES COMPILE_FLAGS 
    "-Wno-dangling-else -Wno-unused-variable -Wno-unused-but-set-variable -Wno-maybe-uninitialized"
)