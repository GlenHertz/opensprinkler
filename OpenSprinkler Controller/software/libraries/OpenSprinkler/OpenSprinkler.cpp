// Arduino library code for OpenSprinkler

/* OpenSprinkler Class Implementation
   Creative Commons Attribution-ShareAlike 3.0 license
   Sep 2012 @ Rayshobby.net
*/

#include "OpenSprinkler.h"

// BufferFiller is from the enc28j60 and ip code (which is GPL v2)
//      Author: Pascal Stang 
//      Modified by: Guido Socher
//      DHCP code: Andrew Lindsay
//
// 2010-05-19 <jc@wippler.nl>
// 2012-10-11 <glen.hertz@gmail.com>
void BufferFiller::emit_p(PGM_P fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    for (;;) {
        char c = pgm_read_byte(fmt++);
        if (c == 0)
            break;
        if (c != '$') {
            client.write(c);
            continue;
        }
        c = pgm_read_byte(fmt++);
        switch (c) {
            case 'D':
                //wtoa(va_arg(ap, word), (char*) ptr);  //ray
                itoa(va_arg(ap, word), (char*) ptr, 10);
                client.print(ptr);
                break;
            case 'L':
                ultoa(va_arg(ap, long), (char*) ptr, 10);
                client.print(ptr);
                break;
            case 'S':
                strcpy((char*) ptr, va_arg(ap, const char*));
                client.print(ptr);
                break;
            case 'F': {
                PGM_P s = va_arg(ap, PGM_P);
                char d;
                while ((d = pgm_read_byte(s++)) != 0)
                    client.write(d);
                continue;
            }
            case 'E': {
                byte* s = va_arg(ap, byte*);
                char d;
                while ((d = eeprom_read_byte(s++)) != 0)
                    client.write(d);
                continue;
            }
            default:
                client.write(c);
                continue;
        }
    }
    va_end(ap);
}
void BufferFiller::emit_raw (const char* s, uint16_t n) { 
  strncpy(ptr, s, n); 
  client.print(ptr); 
}
void BufferFiller::emit_raw_p (PGM_P p, uint16_t n) { 
  strncpy_P(ptr, p, n); 
  client.print(ptr); 
}

char buffer_filler[TMP_BUFFER_SIZE+1];
BufferFiller bfill = buffer_filler;

// Pins on Arduino to control relays for sprinkler values:
// Note: don't use pins 50 to 53 (used Ethernet Shield)
int zone_pins[] = {33, 35, 37, 39, 41, 43, 45, 47};  
int numZones = 8;
StatusBits OpenSprinkler::status;
byte OpenSprinkler::nboards;
byte OpenSprinkler::nstations;
byte OpenSprinkler::station_bits[MAX_EXT_BOARDS+1];
byte OpenSprinkler::masop_bits[MAX_EXT_BOARDS+1];
unsigned long OpenSprinkler::raindelay_stop_time;

// Option names
prog_char _str_fwv [] PROGMEM = "Firmware ver.";
prog_char _str_tz  [] PROGMEM = "Time zone:";
prog_char _str_ntp [] PROGMEM = "NTP Sync:";
prog_char _str_dhcp[] PROGMEM = "Use DHCP:";
prog_char _str_ip1 [] PROGMEM = "Static.ip1:";
prog_char _str_ip2 [] PROGMEM = "ip2:";
prog_char _str_ip3 [] PROGMEM = "ip3:";
prog_char _str_ip4 [] PROGMEM = "ip4:";
prog_char _str_gw1 [] PROGMEM = "Gateway.ip1:";
prog_char _str_gw2 [] PROGMEM = "ip2:";
prog_char _str_gw3 [] PROGMEM = "ip3:";
prog_char _str_gw4 [] PROGMEM = "ip4:";
prog_char _str_hp0 [] PROGMEM = "HTTP port:";
prog_char _str_hp1 [] PROGMEM = "";
prog_char _str_ar  [] PROGMEM = "Auto reconnect:";
prog_char _str_ext [] PROGMEM = "Ext. boards:";
prog_char _str_seq [] PROGMEM = "Sequential:";
prog_char _str_sdt [] PROGMEM = "Station delay:";
prog_char _str_mas [] PROGMEM = "Master station:";
prog_char _str_mton[] PROGMEM = "Mas. on adj.:";
prog_char _str_mtof[] PROGMEM = "Mas. off adj.:";
prog_char _str_urs [] PROGMEM = "Use rain sensor:";
prog_char _str_rso [] PROGMEM = "Normally open:";
prog_char _str_wl  [] PROGMEM = "Water level (%):";
prog_char _str_stt [] PROGMEM = "Selftest time:";
prog_char _str_ipas[] PROGMEM = "Ignore password:";
prog_char _str_reset[] PROGMEM = "Reset all?";


OptionStruct OpenSprinkler::options[NUM_OPTIONS] = {
  {SVC_FW_VERSION, 0, _str_fwv, OPFLAG_NONE}, // firmware version
  {32,  108, _str_tz,   OPFLAG_WEB_EDIT | OPFLAG_SETUP_EDIT},     // default time zone: GMT-4
  {1,   1,   _str_ntp,  OPFLAG_SETUP_EDIT},   // use NTP sync
  {1,   1,   _str_dhcp, OPFLAG_SETUP_EDIT},   // 0: use static ip, 1: use dhcp
  {192, 255, _str_ip1,  OPFLAG_SETUP_EDIT},   // this and next 3 bytes define static ip
  {168, 255, _str_ip2,  OPFLAG_SETUP_EDIT},
  {1,   255, _str_ip3,  OPFLAG_SETUP_EDIT},
  {22,  255, _str_ip4,  OPFLAG_SETUP_EDIT},
  {192, 255, _str_gw1,  OPFLAG_SETUP_EDIT},   // this and next 3 bytes define static gateway ip
  {168, 255, _str_gw2,  OPFLAG_SETUP_EDIT},
  {1,   255, _str_gw3,  OPFLAG_SETUP_EDIT},
  {1,   255, _str_gw4,  OPFLAG_SETUP_EDIT},
  {80,  255, _str_hp0,  OPFLAG_WEB_EDIT},     // this and next byte define http port number
  {0,   255, _str_hp1,  OPFLAG_WEB_EDIT},
  {1,   1,   _str_ar,   OPFLAG_SETUP_EDIT},   // network auto reconnect
  {0,   MAX_EXT_BOARDS, _str_ext, OPFLAG_SETUP_EDIT | OPFLAG_WEB_EDIT}, // number of extension board. 0: no extension boards
  {1,   1,   _str_seq,  OPFLAG_NONE}, // sequential mode (obsolete, non-editable)
  {0,   240, _str_sdt,  OPFLAG_SETUP_EDIT | OPFLAG_WEB_EDIT}, // station delay time (0 to 240 seconds).
  {0,   8,   _str_mas,  OPFLAG_SETUP_EDIT | OPFLAG_WEB_EDIT}, // index of master station. 0: no master station
  {0,   60,  _str_mton, OPFLAG_SETUP_EDIT | OPFLAG_WEB_EDIT}, // master on time [0,60] seconds
  {60,  120, _str_mtof, OPFLAG_SETUP_EDIT | OPFLAG_WEB_EDIT}, // master off time [-60,60] seconds
  {0,   1,   _str_urs,  OPFLAG_SETUP_EDIT | OPFLAG_WEB_EDIT}, // rain sensor control bit. 1: use rain sensor input; 0: ignore
  {1,   1,   _str_rso,  OPFLAG_SETUP_EDIT | OPFLAG_WEB_EDIT}, // rain sensor type. 0: normally closed; 1: normally open.
  {100, 250, _str_wl,   OPFLAG_SETUP_EDIT | OPFLAG_WEB_EDIT}, // water level (default 100%),
  {10,  240, _str_stt,  OPFLAG_SETUP_EDIT},                   // self-test time (in seconds)
  {0,   1,   _str_ipas, OPFLAG_SETUP_EDIT | OPFLAG_WEB_EDIT}, // 1: ignore password; 0: use password
  {0,   1,   _str_reset,OPFLAG_SETUP_EDIT}
};

// Weekday display strings
prog_char str_day0[] PROGMEM = "Mon";
prog_char str_day1[] PROGMEM = "Tue";
prog_char str_day2[] PROGMEM = "Wed";
prog_char str_day3[] PROGMEM = "Thu";
prog_char str_day4[] PROGMEM = "Fri";
prog_char str_day5[] PROGMEM = "Sat";
prog_char str_day6[] PROGMEM = "Sun";

char* OpenSprinkler::days_str[7] = {
  str_day0,
  str_day1,
  str_day2,
  str_day3,
  str_day4,
  str_day5,
  str_day6
};

// ===============
// Setup Functions
// ===============

// Arduino software reset function
void(* resetFunc) (void) = 0;

// Initialize network with the given mac address and http port
byte OpenSprinkler::start_network(byte mymac[], int http_port) {

  if (options[OPTION_USE_DHCP].value) {
    // register with domain name "opensprinkler"
    Serial.print("   - Trying to get IP address through DHCP...");
    if (!Ethernet.begin(mymac)) {
      Serial.println(" [FAIL]");
      return 0;
    }
    for (byte thisByte = 0; thisByte < 4; thisByte++) {
      // print the value of each byte of the IP address:
      Serial.print(Ethernet.localIP()[thisByte], DEC);
      Serial.print("."); 
    }
    Serial.println(" [PASS]");
  } else {
    Serial.print("   - Configuring static IP address ");
    Serial.print(options[OPTION_STATIC_IP1].value);
    Serial.print(".");
    Serial.print(options[OPTION_STATIC_IP2].value);
    Serial.print(".");
    Serial.print(options[OPTION_STATIC_IP3].value);
    Serial.print(".");
    Serial.print(options[OPTION_STATIC_IP4].value);
    Serial.print(" ...");
    byte staticip[] = {
      options[OPTION_STATIC_IP1].value,
      options[OPTION_STATIC_IP2].value,
      options[OPTION_STATIC_IP3].value,
      options[OPTION_STATIC_IP4].value};

    byte gateway[] = {
      options[OPTION_GATEWAY_IP1].value,
      options[OPTION_GATEWAY_IP2].value,
      options[OPTION_GATEWAY_IP3].value,
      options[OPTION_GATEWAY_IP4].value};
    byte dns[] = {8,8,8,8};  // Google by default
    if (!Ethernet.begin(mymac, staticip, dns, gateway)) {
      Serial.println(" [FAIL]");
      return 0;
    }
    for (byte thisByte = 0; thisByte < 4; thisByte++) {
      // print the value of each byte of the IP address:
      Serial.print(Ethernet.localIP()[thisByte], DEC);
      Serial.print("."); 
    }
    Serial.println(" [PASS]");
  }
  ether.begin();  // Start listening for clients
  Udp.begin(ntpclientportL);  // Start UPD listener
  return 1;
}

// Reboot controller
void OpenSprinkler::reboot() {
  resetFunc();
}

// OpenSprinkler init function
void OpenSprinkler::begin() {

/////////////////

  // Reset all stations
  clear_all_station_bits();
  apply_all_station_bits();
  
#ifdef PIN_RAINSENSOR  
  // Rain sensor port set up
  pinMode(PIN_RAINSENSOR, INPUT);
  digitalWrite(PIN_RAINSENSOR, HIGH); // enabled internal pullup
#endif
  
  // Reset status variables
  status.enabled = 1;
  status.rain_delayed = 0;
  status.rain_sensed = 0;
  status.program_busy = 0;
  status.manual_mode = 0;
  status.has_rtc = 0;
  status.display_board = 0;
  status.network_fails = 0;

  nboards = 1;
  nstations = 8;
  raindelay_stop_time = 0;
  
}

// Self_test function
void OpenSprinkler::self_test() {
  byte sid;
  while(1) {
    for(sid=0; sid<nstations; sid++) {
      clear_all_station_bits();
      set_station_bit(sid, 1);
      apply_all_station_bits();
      // run each station for designated amount of time
      delay((unsigned long)options[OPTION_SELFTEST_TIME].value*1000);
      //delay(3000);  // 3 seconds delay between every two stations      
    }
  }
}

// Get station name from eeprom
void OpenSprinkler::get_station_name(byte sid, char tmp[]) {
  int i=0;
  int start = ADDR_EEPROM_STN_NAMES + (int)sid * STATION_NAME_SIZE;
  tmp[STATION_NAME_SIZE]=0;
  while(1) {
    tmp[i] = eeprom_read_byte((unsigned char *)(start+i));
    if (tmp[i]==0 || i==(STATION_NAME_SIZE-1)) break;
    i++;
  }
  return;
}

// Set station name to eeprom
void OpenSprinkler::set_station_name(byte sid, char tmp[]) {
  int i=0;
  int start = ADDR_EEPROM_STN_NAMES + (int)sid * STATION_NAME_SIZE;
  tmp[STATION_NAME_SIZE]=0;
  while(1) {
    eeprom_write_byte((unsigned char *)(start+i), tmp[i]);
    if (tmp[i]==0 || i==(STATION_NAME_SIZE-1)) break;
    i++;
  }
  return;  
}

// Save station master operation bits to eeprom
void OpenSprinkler::masop_save() {
  byte i;
  for(i=0;i<=MAX_EXT_BOARDS;i++) {
    eeprom_write_byte((unsigned char *)ADDR_EEPROM_MAS_OP+i, masop_bits[i]);
  }
}

// Load station master operation bits from eeprom
void OpenSprinkler::masop_load() {
  byte i;
  for(i=0;i<=MAX_EXT_BOARDS;i++) {
    masop_bits[i] = eeprom_read_byte((unsigned char *)ADDR_EEPROM_MAS_OP+i);
  }
}

// ==================
// Schedule Functions
// ==================

// Index of today's weekday (Monday is 0)
byte OpenSprinkler::weekday_today() {
  return ((byte)weekday()+5)%7; // Time::weekday() assumes Sunday is 1
}

// Set station bit
void OpenSprinkler::set_station_bit(byte sid, byte value) {
  byte bid = (sid>>3);  // board index
  byte s = sid % 8;     // station bit index
  if (value) {
    station_bits[bid] = station_bits[bid] | ((byte)1<<s);
  } 
  else {
    station_bits[bid] = station_bits[bid] &~((byte)1<<s);
  }
}  

// Clear all station bits
void OpenSprinkler::clear_all_station_bits() {
  byte bid;
  for(bid=0;bid<=MAX_EXT_BOARDS;bid++) {
    station_bits[bid] = 0;
  }
}

// Apply all station bits
// !!! This will activate/deactivate valves !!!
void OpenSprinkler::apply_all_station_bits() {
  byte bid, s;
  byte bitvalue;

  for(bid=0;bid<=MAX_EXT_BOARDS;bid++) {
    bitvalue = 0;
    if (status.enabled && (!status.rain_delayed) && !(options[OPTION_USE_RAINSENSOR].value && status.rain_sensed))
      bitvalue = station_bits[MAX_EXT_BOARDS-bid];
    for(s=0;s<8;s++) {
      digitalWrite(zone_pins[s], bitvalue ? HIGH : LOW );
    }
  }
}    

// =================
// Options Functions
// =================

void OpenSprinkler::options_setup() {

  // add 0.5 second delay to allow EEPROM to stablize
  delay(500);
  
  // check reset condition: either firmware version has changed, or reset flag is up
  byte curr_ver = eeprom_read_byte((unsigned char*)(ADDR_EEPROM_OPTIONS+OPTION_FW_VERSION));
  if (curr_ver<100) curr_ver = curr_ver*10; // adding a default 0 if version number is the old type
  if (curr_ver != SVC_FW_VERSION || eeprom_read_byte((unsigned char*)(ADDR_EEPROM_OPTIONS+OPTION_RESET))==0xAA) {
      
    //======== Reset EEPROM data ========
    options_save(); // write default option values
    eeprom_string_set(ADDR_EEPROM_PASSWORD, DEFAULT_PASSWORD);  // write default password
    eeprom_string_set(ADDR_EEPROM_LOCATION, DEFAULT_LOCATION);  // write default location
    
    Serial.println(F("Resetting EEPROM"));
    Serial.println(F("Please Wait..."));  
    int i, sn;
    for(i=ADDR_EEPROM_STN_NAMES; i<INT_EEPROM_SIZE; i++) {
      eeprom_write_byte((unsigned char *) i, 0);      
    }

    // reset station names
    for(i=ADDR_EEPROM_STN_NAMES, sn=1; i<ADDR_EEPROM_RUNONCE; i+=STATION_NAME_SIZE, sn++) {
      eeprom_write_byte((unsigned char *)i    ,'S');
      eeprom_write_byte((unsigned char *)(i+1),'0'+(sn/10));
      eeprom_write_byte((unsigned char *)(i+2),'0'+(sn%10)); 
    }
    
    // reset master operation bits
    for(i=ADDR_EEPROM_MAS_OP; i<ADDR_EEPROM_MAS_OP+(MAX_EXT_BOARDS+1); i++) {
      // default master operation bits on
      eeprom_write_byte((unsigned char *)i, 0xff);
    }
    //======== END OF EEPROM RESET CODE ========
    
    // restart after resetting EEPROM.
    delay(500);
    reboot();
  } 
  else {
    options_load(); // load option values
    masop_load();   // load master operation bits
  }

}

// Load options from internal eeprom
void OpenSprinkler::options_load() {
  for (byte i=0; i<NUM_OPTIONS; i++) {
    options[i].value = eeprom_read_byte((unsigned char *)(ADDR_EEPROM_OPTIONS + i));
  }
  nboards = options[OPTION_EXT_BOARDS].value+1;
  nstations = nboards * 8;
}

// Save options to internal eeprom
void OpenSprinkler::options_save() {
  // save options in reverse order so version number is saved the last
  for (int i=NUM_OPTIONS-1; i>=0; i--) {
    eeprom_write_byte((unsigned char *) (ADDR_EEPROM_OPTIONS + i), options[i].value);
  }
  nboards = options[OPTION_EXT_BOARDS].value+1;
  nstations = nboards * 8;
}

// ==============================
// Controller Operation Functions
// ==============================

// Enable controller operation
void OpenSprinkler::enable() {
  status.enabled = 1;
  apply_all_station_bits();
  // write enable bit to eeprom
  options_save();
}

// Disable controller operation
void OpenSprinkler::disable() {
  status.enabled = 0;
  apply_all_station_bits();
  // write enable bit to eeprom
  options_save();
}

void OpenSprinkler::raindelay_start(byte rd) {
  if(rd == 0) return;
  raindelay_stop_time = now() + (unsigned long) rd * 3600;
  status.rain_delayed = 1;
  apply_all_station_bits();
}

void OpenSprinkler::raindelay_stop() {
  status.rain_delayed = 0;
  apply_all_station_bits();
}

void OpenSprinkler::rainsensor_status() {
  // options[OPTION_RS_TYPE]: 0 if normally closed, 1 if normally open
  status.rain_sensed = (digitalRead(PIN_RAINSENSOR) == options[OPTION_RAINSENSOR_TYPE].value ? 0 : 1);
}

// Print time to a given line
void OpenSprinkler::serial_print_time()
{
  time_t t=now();
  serial_print_2digit(hour(t));
  Serial.print((":"));
  serial_print_2digit(minute(t));
  Serial.print(("  "));
  Serial.print(days_str[weekday_today()]);
  Serial.print((" "));
  serial_print_2digit(month(t));
  Serial.print(("-"));
  serial_print_2digit(day(t));
  Serial.println("");
}
void OpenSprinkler::serial_print_2digit(int v)
{
  Serial.print((int)(v/10));
  Serial.print((int)(v%10));
}

// Print time to a given line
void OpenSprinkler::lcd_print_time(byte line)
{
  time_t t=now();
  lcd.setCursor(0, line);
  lcd_print_2digit(hour(t));
  lcd_print_pgm(PSTR(":"));
  lcd_print_2digit(minute(t));
  lcd_print_pgm(PSTR("  "));
  lcd_print_pgm(days_str[weekday_today()]);
  lcd_print_pgm(PSTR(" "));
  lcd_print_2digit(month(t));
  lcd_print_pgm(PSTR("-"));
  lcd_print_2digit(day(t));
}

// print ip address and port
void OpenSprinkler::serial_print_ip(const byte *ip, int http_port) {
  for (byte i=0; i<3; i++) {
    Serial.print((int)ip[i]); 
    Serial.print(".");
  }   
  Serial.print((int)ip[3]);
  Serial.print(":");
  Serial.print(http_port);
  Serial.print("");
}

void OpenSprinkler::serial_print_status() {
  if (status.network_fails > 0) 
  {
    Serial.print(F("Warning: "));
    Serial.print(status.network_fails);
    Serial.print(F("network failures!"));
  } else {
    Serial.print(F("Network is OK"));
  } 
}

// Print station bits
void OpenSprinkler::serial_print_station(byte line, char c) {
  if (status.display_board == 0) {
    Serial.print(F("MC:"));  // Master controller is display as 'MC'
  } else {
    Serial.print(F("E"));
    Serial.print((int)status.display_board);
    Serial.print(F(":"));   // extension boards are displayed as E1, E2...
  }
  
  if (!status.enabled) {
    Serial.print(F("-Disabled!-"));
  }
  else if (status.rain_delayed || (status.rain_sensed && options[OPTION_USE_RAINSENSOR].value)) {
    Serial.print(F("-Rain Stop-"));
  }
  else {
    byte bitvalue = station_bits[status.display_board];
    for (byte s=0; s<8; s++) {
      if (status.display_board == 0 &&(s+1) == options[OPTION_MASTER_STATION].value) {
        lcd.print((bitvalue&1) ? (char)c : 'M'); // print master station
      } else {
        lcd.print((bitvalue&1) ? (char)c : '_');
      }
      bitvalue >>= 1;
    }
  }
  Serial.print(F("    "));
  serial_print_status();
}

// Print an option value
void OpenSprinkler::serial_print_option(int i) {
  Serial.print(options[i].str, 0);  
  Serial.print(F(": "));
  int tz;
  switch(i) {
  case OPTION_TIMEZONE: // if this is the time zone option, do some conversion
    tz = (int)options[i].value-48;
    if (tz>=0) Serial.print(F("+"));
    else {Serial.print(F("-")); tz=-tz;}
    Serial.print(tz/4); // print integer portion
    Serial.print(F(":"));
    tz = (tz%4)*15;
    if (tz==0)  Serial.print(F("00"));
    else {
      Serial.print(tz);  // print fractional portion
    }
    Serial.print(F(" GMT"));    
    break;
  case OPTION_MASTER_ON_ADJ:
    Serial.print(F("+"));
    Serial.print((int)options[i].value);
    break;
  case OPTION_MASTER_OFF_ADJ:
    if(options[i].value>=60)  Serial.print(F("+"));
    Serial.print((int)options[i].value-60);
    break;
  case OPTION_HTTPPORT_0:
    Serial.print((int)(options[i+1].value<<8)+options[i].value);
    break;
  default:
    // if this is a boolean option
    if (options[i].max==1)
      Serial.print(options[i].value ? F("Yes") : F("No"));
    else
      Serial.print((int)options[i].value);
    break;
  }
  if (i==OPTION_WATER_LEVEL)  Serial.print(F("%"));
  else if (i==OPTION_MASTER_ON_ADJ || i==OPTION_MASTER_OFF_ADJ ||
      i==OPTION_SELFTEST_TIME || i==OPTION_STATION_DELAY_TIME)
    Serial.print(F(" sec"));
  Serial.println("");
}


// ==================
// String Functions
// ==================
void OpenSprinkler::eeprom_string_set(int start_addr, char* buf) {
  byte i=0;
  for (; (*buf)!=0; buf++, i++) {
    eeprom_write_byte((unsigned char*)(start_addr+i), *(buf));
  }
  eeprom_write_byte((unsigned char*)(start_addr+i), 0);  
}

void OpenSprinkler::eeprom_string_get(int start_addr, char *buf) {
  byte c;
  byte i = 0;
  do {
    c = eeprom_read_byte((unsigned char*)(start_addr+i));
    //if (c==' ') c='+';
    *(buf++) = c;
    i ++;
  } while (c != 0);
}

// verify if a string matches password
byte OpenSprinkler::password_verify(char *pw) { 
  byte i = 0;
  byte c1, c2;
  while(1) {
    c1 = eeprom_read_byte((unsigned char*)(ADDR_EEPROM_PASSWORD+i));
    c2 = *pw;
    if (c1==0 || c2==0)
      break;
    if (c1!=c2) {
      return 0;
    }
    i++;
    pw++;
  }
  return (c1==c2) ? 1 : 0;
}

