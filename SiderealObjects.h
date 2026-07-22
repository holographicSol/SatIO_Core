/******************************************************************************
SiderealObjects.h
Sidereal Objects Arduino Library Header File
David Armstrong
Version 1.1.0 - September 3, 2021
https://github.com/DavidArmstrong/SiderealObjects

This file prototypes the SiderealObjects class, as implemented in SiderealObjects.cpp

Resources:
Uses math.h for math functions

Development environment specifics:
Arduino IDE 1.8.15

This code is released under the [MIT License](http://opensource.org/licenses/MIT)
Please review the LICENSE.md file included with this example.
Distributed as-is; no warranty is given.

******************************************************************************/

/******************************************************************************
 * Modified by Benjain Jack Cullen:
 *  Constellation names
 *  Object Types
 *  Legacy Object Types
 *  Star Types
 *  More Star Names
 *  More Distances
 *  All new data can be accessed by index, for efficiency, to return any available
 *  above data, for an identified object, along with original object names.
******************************************************************************/

// ensure this library description is only included once
#ifndef __SiderealObjects_h
#define __SiderealObjects_h

//Uncomment the following line for debugging output
//#define debug_sidereal_objects

#include <stdint.h>
//#include <math.h>
#include <string.h>
#include "SiderealObjectsTables.h"

#if defined(ARDUINO) && ARDUINO >= 100
  #include "Arduino.h"
#else
  #include "WProgram.h"
#endif

// Structure to hold data
// We need to populate this when we calculate data
struct SiderealObjectsData {
  public:
    double RightAscension;
    double Declination;
    int tableNumber;
	int objectNumber;
};

// Sidereal_Planets library description
class SiderealObjects {
  // user-accessible "public" interface
  public:
    const int NSTARS = 609; // Number of stars in table
    const int NGCNUM = 7840; // Number of NGC objects
    const int ICNUM = 5386; // Number of IC objects
    const int OTHERNUM = 567; // Number of "other" objects in table
    SiderealObjectsData spData;
    boolean begin(void);
	double decimalDegrees(int degrees, int minutes, float seconds);
	void printDegMinSecs(double n);
	boolean setRAdec(double RightAscension, double Declination);
	double getRAdec(void);
    double getDeclinationDec(void);
	float getStarMagnitude(void);
	boolean selectStarTable(int n);

	const char* printStarName(int n);
	const char* printMessierName(int n);
	const char* printCaldwellName(int n);

	const char* printStarType(int n);
	const char* printNGCType(int n);
	const char* printICType(int n);
	const char* printMessierType(int n);
	const char* printCaldwellType(int n);
	const char* printHerschel400Type(int n);

	const char* printNGCCon(int n);
	const char* printICCon(int n);
	const char* printMessierCon(int n);
	const char* printCaldwellCon(int n);
	const char* printHerschel400Con(int n);

	double printStarDist(int n);
	double printMessierDist(int n);
	double printCaldwellDist(int n);

	const char* printStarDesc(int n);

	boolean selectNGCTable(int n);
	boolean selectICTable(int n);
    boolean selectMessierTable(int n);
    boolean selectCaldwellTable(int n);
	boolean selectHershel400Table(int n);
	boolean selectOtherObjectsTable(int n);
	boolean identifyObject(void);
	int getIdentifiedObjectTable(void);
	int getIdentifiedObjectNumber(void);
	int getAltIdentifiedObjectTable(void);
	int getAltIdentifiedObjectNumber(void);

	// Cone/range queries: unlike identifyObject() (single nearest match,
	// always finds *something*), these return every object in the table
	// within radiusDeg of (centerRAhours, centerDecDeg) -- the exact,
	// complete set, not a coarse sample. Output is object numbers as
	// expected by select*Table(n). Returns the number of matches written
	// (capped at maxOut). ESP32-only, like the RA-sorted indices they use.
	#if !defined(__AVR_ATmega2560__)
	int findStarsInRadius(double centerRAhours, double centerDecDeg, double radiusDeg, int *outNumbers, int maxOut);
	int findNGCInRadius(double centerRAhours, double centerDecDeg, double radiusDeg, int *outNumbers, int maxOut);
	int findICInRadius(double centerRAhours, double centerDecDeg, double radiusDeg, int *outNumbers, int maxOut);
	int findOtherInRadius(double centerRAhours, double centerDecDeg, double radiusDeg, int *outNumbers, int maxOut);
	#endif

	// Cross-references the current tablenum/objectnum (as left by
	// identifyObject() or select*Table()) against the Messier/Caldwell/
	// Herschel400 tables, setting alttablenum/altobjnum. Factored out of
	// identifyObject() so the cone-search path (which selects each match
	// via select*Table() instead of identifyObject()) can reuse it.
	void checkAltCatalogs(void);

  // library-accessible "private" interface
  private:
    const double F2PI = 2.0 * M_PI;
    const double F2to16 = 65536.0;
    const double F2to15minus1 = 32767.0;

	double RAdec, DeclinationDec;
	double RArad, DeclinationRad;
	double sinRA, sinDec;
	double cosRA, cosDec;
	float magnitude;
	uint16_t rawRA;
	int16_t rawDec;

	double inRange24(double d);
	double inRange360(double d);
	double inRange2PI(double d);
	double deg2rad(double n);
	double rad2deg(double n);

    int tablenum, alttablenum, objectnum, altobjnum;

    // identifyObject() nearest-match search: rather than scanning every
    // catalog entry (NSTARS+NGCNUM+ICNUM+OTHERNUM ~= 14,400 entries) per
    // call, each table's entries are indexed once by byte offset sorted
    // ascending by raw RA, then searched via binary search + a bounded
    // expansion (see identifyObject() in the .cpp for why that gives the
    // exact same nearest match as the brute-force scan, just much faster).
    // Fixed-size (not dynamically allocated): NSTARS/NGCNUM/ICNUM/OTHERNUM
    // are compile-time constants, so there is no allocation-failure path
    // to guard against.
    bool raIndexBuilt = false;
    uint16_t starRAOrder[609];
    uint16_t ngcRAOrder[7840];
    uint16_t icRAOrder[5386];
    uint16_t otherRAOrder[567];
    void buildRAIndex(void);

    union FourByte {
      unsigned long bit32;
      unsigned int bit16[2];
      unsigned char bit8[4];
    };
};
#endif
