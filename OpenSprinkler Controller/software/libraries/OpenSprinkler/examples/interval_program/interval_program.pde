// Example code for OpenSprinkler

/* This is a program-based sprinkler schedule algorithm.
 Programs are set similar to calendar schedule.
 Each program specifies the days, stations,
 start time, end time, interval and duration.
 The number of programs you can create are subject to EEPROM size.
 
 Creative Commons Attribution-ShareAlike 3.0 license
 Sep 2012 @ Rayshobby.net
 */

#include <limits.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <OpenSprinkler.h>
#include "program.h"

// ================================================================================
// This is the path to which external Javascripst are stored
// To create custom Javascripts, you need to make a copy of these scripts
// and put them to your own server, or github, or any available file hosting service

#define JAVASCRIPT_PATH  "http://rayshobby.net/scripts/java/svc1.8" 
//"https://github.com/rayshobby/opensprinkler/raw/master/scripts/java/svc1.8"
// ================================================================================

// NTP sync interval (in seconds)
#define NTP_SYNC_INTERVAL       86400L  // 24 hours default
#define RTC_SYNC_INTERVAL       60     // 1 minute default
// Interval for checking network connection (in seconds)
#define CHECK_NETWORK_INTERVAL  60     // 1 minute default
// Ping test time out (in milliseconds)
#define PING_TIMEOUT            200     // 0.2 second default


// ====== Ethernet defines ======
EthernetServer server = EthernetServer(80);
EthernetClient client = EthernetClient(80);
char weather_host[] = "weather.yahooapis.com";
char weather_query[] = "/forecastrss?w=29375164&u=c";

byte mymac[] = { 0x00,0x69,0x69,0x2D,0x30,0x30 }; // mac address
IPAddress ntpip(204,9,54,119);                    // Default NTP server ip
uint8_t ntpclientportL = 123;                     // Default NTP client port (to listen to UDP packets)
const int NTP_PACKET_SIZE = 48;                   // time is in first few bytes
byte packetBuffer[NTP_PACKET_SIZE];               // to hold incoming/outgoing packets
EthernetUDP Udp;
int myport;

char tmp_buffer[TMP_BUFFER_SIZE+1];       // scratch buffer
BufferFiller bfill;                       // buffer filler
char buffer[ETHER_BUFFER_SIZE+1];

// ====== Object defines ======
OpenSprinkler svc;    // OpenSprinkler object
ProgramData pd;       // ProgramdData object 

// ====== UI defines ======
static char ui_anim_chars[3] = {'.', 'o', 'O'};

// ======================
// Arduino Setup Function
// ======================
void setup() { 

  Serial.begin(9600);
  Serial.println("Starting OpenSprinkler:");
  Serial.println("  - reseting hardware...");
  svc.begin();          // OpenSprinkler init
  Serial.println("  - restore options setup...");
  svc.options_setup();  // Setup options
  Serial.println("  - reset program data...");
  pd.init();            // ProgramData init
  // calculate http port number
  myport = (int)(svc.options[OPTION_HTTPPORT_1].value<<8) + (int)svc.options[OPTION_HTTPPORT_0].value;

  Serial.print("Connecting to the network...");
    
  if (svc.start_network(mymac, myport)) {  // initialize network
    svc.status.network_fails = 0;
    Serial.println(Ethernet.localIP());
  } else {
    svc.status.network_fails = 1;
  }
  delay(500);
  
  svc.apply_all_station_bits(); // reset station bits
  
  perform_ntp_sync(now());
  
  svc.serial_print_time();  // display time
}

// =================
// Arduino Main Loop
// =================
void loop()
{
  static unsigned long last_time = 0;
  static unsigned long last_minute = 0;
  static uint16_t pos;

  byte bid, sid, s, pid, bitvalue, mas;
  ProgramStruct prog;

  mas = svc.options[OPTION_MASTER_STATION].value;


  EthernetClient client = server.available();  // to respond to incoming requests
  char *ptr = buffer; 
  if (client) {
    while (client.connected()) {
      if (client.available()) {
        memset(buffer, 0, sizeof(buffer));  // clear the buffer
        client.readBytesUntil('\n', buffer, ETHER_BUFFER_SIZE);  // get first line of request
        analyze_get_url(buffer);
        break;
      }
    }
    // give the web browser time to receive the data
    // close the connection:
    client.stop();
  }

  // if 1 second has passed
  time_t curr_time = now();
  if (last_time != curr_time) {

    last_time = curr_time;
    svc.serial_print_time();       // print time

    // ====== Check raindelay status ======
    if (svc.status.rain_delayed) {
      if (curr_time >= svc.raindelay_stop_time) {
        // raindelay time is over      
        svc.raindelay_stop();
      }
    }
    
    // ====== Check rain sensor status ======
    svc.rainsensor_status();    

    // ====== Schedule program data ======
    // Check if we are cleared to schedule a new program. The conditions are:
    // 1) the controller is in program mode (manual_mode == 0), and if
    // 2) the cotroller is not busy
    if (svc.status.manual_mode==0 && svc.status.program_busy==0) {
      unsigned long curr_minute = curr_time / 60;
      boolean match_found = false;
      // since the granularity of start time is minute
      // we only need to check once every minute
      if (curr_minute != last_minute) {
        last_minute = curr_minute;
        // check through all programs
        for(pid=0; pid<pd.nprograms; pid++) {
          pd.read(pid, &prog);
          if(prog.check_match(curr_time) && prog.duration != 0) {
            // program match found
            // process all selected stations
            for(bid=0; bid<svc.nboards; bid++) {
              for(s=0;s<8;s++) {
                sid=bid*8+s;
                // ignore master station because it's not scheduled independently
                if (mas == sid+1)  continue;
                // if the station is current running, skip it
                if (svc.station_bits[bid]>>s) continue;
                
                // if station bits match
                if(prog.stations[bid]&(1<<s)) {
                  // initialize schedule data
                  // store duration temporarily in stop_time variable
                  // duration is scaled by water level
                  pd.scheduled_stop_time[sid] = (unsigned long)prog.duration * svc.options[OPTION_WATER_LEVEL].value / 100;
                  pd.scheduled_program_index[sid] = pid+1;
                  match_found = true;
                }
              }
            }
          }
        }
        
        // calculate start and end time
        if (match_found) {
          schedule_all_stations(curr_time);
        }
      }//if_check_current_minute
    } //if_cleared_for_scheduling
    
    // ====== Run program data ======
    // Check if a program is running currently
    if (svc.status.program_busy){
      for(bid=0;bid<svc.nboards; bid++) {
        bitvalue = svc.station_bits[bid];
        for(s=0;s<8;s++) {
          byte sid = bid*8+s;
          
          // check if the current station is already running
          if(((bitvalue>>s)&1)) {
            // if so, check if we should turn it off
            if (curr_time >= pd.scheduled_stop_time[sid])
            {
              svc.set_station_bit(sid, 0);

              // record lastrun log (only for non-master stations)
              if(mas != sid+1)
              {
                pd.lastrun.station = sid;
                pd.lastrun.program = pd.scheduled_program_index[sid];
                pd.lastrun.duration = curr_time - pd.scheduled_start_time[sid];
                pd.lastrun.endtime = curr_time;
              }      
              
              // reset program data variables
              //pd.remaining_time[sid] = 0;
              pd.scheduled_start_time[sid] = 0;
              pd.scheduled_stop_time[sid] = 0;
              pd.scheduled_program_index[sid] = 0;            
            }
          }
          else {
            // if not running, check if we should turn it on
            if (curr_time >= pd.scheduled_start_time[sid] && curr_time < pd.scheduled_stop_time[sid]) {
              svc.set_station_bit(sid, 1);
              
              // schedule master station here if
              // 1) master station is defined
              // 2) the station is non-master and is set to activate master
              // 3) program is not running in manual mode
              if ((mas>0) && (mas!=sid+1) && (svc.masop_bits[bid]&(1<<s)) && svc.status.manual_mode==0) {
                byte masid=mas-1;
                // master will turn on when a station opens,
                // adjusted by the master on and off time
                pd.scheduled_start_time[masid] = pd.scheduled_start_time[sid]+svc.options[OPTION_MASTER_ON_ADJ].value;
                pd.scheduled_stop_time[masid] = pd.scheduled_stop_time[sid]+svc.options[OPTION_MASTER_OFF_ADJ].value-60;
                pd.scheduled_program_index[masid] = pd.scheduled_program_index[sid];
                // check if we should turn master on now
                if (curr_time >= pd.scheduled_start_time[masid] && curr_time < pd.scheduled_stop_time[masid])
                {
                  svc.set_station_bit(masid, 1);
                }
              }
            }
          }
        }//end_s
      }//end_bid
      
      // activate/deactivate valves
      svc.apply_all_station_bits();

      boolean program_still_busy = false;
      for(sid=0;sid<svc.nstations;sid++) {
        // check if any station has a non-zero and non-infinity stop time
        if (pd.scheduled_stop_time[sid] > 0 && pd.scheduled_stop_time[sid] < ULONG_MAX) {
          program_still_busy = true;
          break;
        }
      }
      // if the program is finished, reset program busy bit
      if (program_still_busy == false) {
        // turn off all stations
        svc.clear_all_station_bits();
        
        svc.status.program_busy = 0;
        
        // in case some options have changed while executing the program        
        mas = svc.options[OPTION_MASTER_STATION].value; // update master station
      }
      
    }//if_some_program_is_running

    // handle master station for manual or parallel mode
    if ((mas>0) && svc.status.manual_mode==1) {
      // in parallel mode or manual mode
      // master will remain on until the end of program
      byte masbit = 0;
      for(sid=0;sid<svc.nstations;sid++) {
        bid = sid>>3;
        s = sid&0x07;
        // check there is any non-master station that activates master and is currently turned on
        if ((mas!=sid+1) && (svc.station_bits[bid]&(1<<s)) && (svc.masop_bits[bid]&(1<<s))) {
          masbit = 1;
          break;
        }
      }
      svc.set_station_bit(mas-1, masbit);
    }    
    
    // activate/deactivate valves
    svc.apply_all_station_bits();
    
    svc.serial_print_station((char)ui_anim_chars[curr_time%3]);
    
    // check network connection
    check_network(curr_time);
    
    Serial.println(Ethernet.localIP());
    // perform ntp sync
    perform_ntp_sync(curr_time);
  }
}

void manual_station_off(byte sid) {
  unsigned long curr_time = now();

  // set station stop time (now)
  pd.scheduled_stop_time[sid] = curr_time;  
}

void manual_station_on(byte sid, int ontimer) {
  unsigned long curr_time = now();
  // set station start time (now)
  pd.scheduled_start_time[sid] = curr_time + 1;
  if (ontimer == 0) {
    pd.scheduled_stop_time[sid] = pd.scheduled_start_time[sid] + 43200; // maximum running time is 8 hours
  } else { 
    pd.scheduled_stop_time[sid] = pd.scheduled_start_time[sid] + ontimer;
  }
  // set program index
  pd.scheduled_program_index[sid] = 255;
  svc.status.program_busy = 1;
}

void perform_ntp_sync(time_t curr_time) {
  static unsigned long last_sync_time = 0;
  // do not perform sync if this option is disabled, or if network is not available
  if (svc.options[OPTION_USE_NTP].value==0 || svc.status.network_fails>0) return;   
  // sync every 1 hour
  if (last_sync_time == 0 || (curr_time - last_sync_time > NTP_SYNC_INTERVAL)) {
    last_sync_time = curr_time;
    unsigned long t = getNtpTime();   
    if (t>0) {    
      setTime(t);
    }
  }
}

void check_network(time_t curr_time) {
  static unsigned long last_check_time = 0;

  if (last_check_time == 0) {last_check_time = curr_time; return;}
  // check network condition periodically
  if (curr_time - last_check_time > CHECK_NETWORK_INTERVAL) {
    last_check_time = curr_time;

    Serial.print(F("Checking network connection..."));
    unsigned long start = millis();
    boolean failed = true;
    // wait at most PING_TIMEOUT milliseconds for ping result
    do {
      if (client.connect("google.com", 80)) 
      {
         client.stop();
         failed = false;
         Serial.println(F("[PASS]"));
         break;
      }
    } while(millis() - start < PING_TIMEOUT);
    if (failed) 
    { 
      svc.status.network_fails++;
      Serial.println(F("[PASS]"));
    } else {
      svc.status.network_fails=0;
    }
    // if failed more than 4 times in a row, reconnect
    if (svc.status.network_fails>4&&svc.options[OPTION_NETFAIL_RECONNECT].value) {
      Serial.print(F("Reconnecting..."));
      svc.start_network(mymac, myport);
      //svc.status.network_fails=0;
    }
  } 
}

void schedule_all_stations(unsigned long curr_time)
{
  unsigned long accumulate_time = curr_time + 1;
  byte sid;
  // calculate start time of each station
  // tations run one after another
  // separated by station delay time
  for(sid=0;sid<svc.nstations;sid++) {
    if(pd.scheduled_stop_time[sid]) {
      pd.scheduled_start_time[sid] = accumulate_time;
      accumulate_time += pd.scheduled_stop_time[sid];
      pd.scheduled_stop_time[sid] = accumulate_time;
      accumulate_time += svc.options[OPTION_STATION_DELAY_TIME].value; // add station delay time
      svc.status.program_busy = 1;  // set program busy bit
    }
  }
}

void reset_all_stations() {
  svc.clear_all_station_bits();
  svc.apply_all_station_bits();
  pd.reset_runtime();
}
