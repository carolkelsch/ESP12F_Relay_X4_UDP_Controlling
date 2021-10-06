#include "defines.h"

/************************* Global Variables *************************/

// EEPROM data structure
struct eeprom_data{ 
  uint val = 0;
  char str[35] = "";
} data;

// EEPROM address management
int eeprom_address = STARTING_ADDR;

// Component status variables
struct components_s{
  int pin;
  int value;
};
components_s components[7];

// UDP server port
int UDP_PORT = 4210;

// State of running code
enum running_code_state{
  GENERIC = 0,
  OPTIMIZED,
};

running_code_state running_state = OPTIMIZED;

// State of test stand
enum test_stand_moving_state{
  STOPPED = 0,
  GOING_UP,
  GOING_DOWN,
};

test_stand_moving_state moving_state = STOPPED;

// State of network connection
enum WiFi_connection_state{
  CONFIGURING = 0,
  CONFIGURED,
  INVALID,
  CONNECTED,
  RUNNING,
  DISCONNECTED,
};

WiFi_connection_state connection_state = CONFIGURING;

// WiFi default credentials
char WIFI_SSID[255] = "AAA";
char WIFI_PASS[255] = "AAA";
char static_ip[16] = "192.168.137.20";
char static_gw[16] = "192.168.137.1";
char static_sn[16] = "255.255.255.0";

// UDP variables
char packet[255];
char reply[20] = "Packet received!";
int reply_len = 0;

// Delay for fixed time mode
int time_delay = 1000;