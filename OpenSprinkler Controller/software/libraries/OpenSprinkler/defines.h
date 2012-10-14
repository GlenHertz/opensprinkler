// Arduino library code for OpenSprinkler

/* Macro definitions and Arduino pin assignments
   Creative Commons Attribution-ShareAlike 3.0 license
   Sep 2012 @ Rayshobby.net
*/

#ifndef _Defines_h
#define _Defines_h

// Firmware version
#define SVC_FW_VERSION  256 // firmware version (e.g. 1.8.1 etc)
                            // if this number is different from stored in EEPROM,
                            // an EEPROM reset will be automatically triggered

#define MAX_EXT_BOARDS  3   // maximum number of ext. boards (each consists of 8 stations)
                            // total number of stations: (1+MAX_EXT_BOARDS) * 8
                            // increasing this number will consume more memory and EEPROM space

#define STATION_NAME_SIZE  12 // size of each station name, default is 12 letters max

// Internal EEPROM Defines
#define INT_EEPROM_SIZE         1024    // ATmega328 eeprom size
#define ADDR_EEPROM_OPTIONS     0x0000  // address where options are stored, 32 bytes reserved
#define ADDR_EEPROM_PASSWORD    0x0020	// address where password is stored, 16 bytes reserved
#define ADDR_EEPROM_LOCATION    0x0030  // address where location is stored, 32 bytes reserved
#define ADDR_EEPROM_STN_NAMES   0x0050  // address where station names are stored
#define ADDR_EEPROM_RUNONCE     (ADDR_EEPROM_STN_NAMES+(MAX_EXT_BOARDS+1)*8*STATION_NAME_SIZE)
                                        // address where run-once data is stored
#define ADDR_EEPROM_MAS_OP      (ADDR_EEPROM_RUNONCE+(MAX_EXT_BOARDS+1)*8*2)
                                        // address where master operation bits are stored
#define ADDR_EEPROM_USER        (ADDR_EEPROM_MAS_OP+(MAX_EXT_BOARDS+1))
                                        // address where program schedule data is stored

#define DEFAULT_PASSWORD        "opendoor"
#define DEFAULT_LOCATION        "Boston,MA" // zip code, city name or any google supported location strings
                                            // IMPORTANT: use , or + in place of space
                                            // So instead of 'New York', use 'New,York' or 'New+York'
// macro define of each option
// See OpenSprinkler.cpp for details on each option
typedef enum {
  OPTION_FW_VERSION = 0,
  OPTION_TIMEZONE,
  OPTION_USE_NTP,
  OPTION_USE_DHCP,
  OPTION_STATIC_IP1,
  OPTION_STATIC_IP2,
  OPTION_STATIC_IP3,
  OPTION_STATIC_IP4,
  OPTION_GATEWAY_IP1,
  OPTION_GATEWAY_IP2,
  OPTION_GATEWAY_IP3,
  OPTION_GATEWAY_IP4,
  OPTION_HTTPPORT_0,
  OPTION_HTTPPORT_1,
  OPTION_NETFAIL_RECONNECT,
  OPTION_EXT_BOARDS,
  OPTION_SEQUENTIAL,
  OPTION_STATION_DELAY_TIME,
  OPTION_MASTER_STATION,
  OPTION_MASTER_ON_ADJ,
  OPTION_MASTER_OFF_ADJ,
  OPTION_USE_RAINSENSOR,
  OPTION_RAINSENSOR_TYPE,
  OPTION_WATER_LEVEL,
  OPTION_SELFTEST_TIME,
  OPTION_IGNORE_PASSWORD,
  OPTION_RESET,
  NUM_OPTIONS	// total number of options
} OS_OPTION_t;

// Option Flags
#define OPFLAG_NONE        0x00  // default flag, this option is not editable
#define OPFLAG_SETUP_EDIT  0x01  // this option is editable during startup
#define OPFLAG_WEB_EDIT    0x02  // this option is editable on the Options webpage


// =====================================
// ====== Arduino Pin Assignments ======
// =====================================

// ------ Define hardware version here ------
#define SVC_HW_VERSION 2560

#ifndef SVC_HW_VERSION
#error "==This error is intentional==: you must define SVC_HW_VERSION in arduino-xxxx/libraries/OpenSprnikler/defines.h"
#endif

//#define PIN_RAINSENSOR     3    // rain sensor is connected to pin D3


// ====== Ethernet Defines ======
#define ETHER_BUFFER_SIZE     720 // if buffer size is increased, you must check the total RAM consumption
                                  // otherwise it may cause the program to crash
#define TMP_BUFFER_SIZE        32 // scratch buffer size

#endif


