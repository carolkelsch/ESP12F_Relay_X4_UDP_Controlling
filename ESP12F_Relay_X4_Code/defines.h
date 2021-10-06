/************************* Defines *************************/

// Pins configuration
#define RELAY_1           15
#define RELAY_2           14
#define RELAY_3           12
#define RELAY_4           13

// Pins for switchs
#define FUNC_MODE_PIN     5
#define TOP_SWITCH_PIN    4
#define BOTTOM_SWITCH_PIN 4

#define ACTIVE            2
#define WIFI_CONF         16

// Invalid data from flash
#define INVALID_DATA      ""

// EEPROM reference address
#define STARTING_ADDR     0

// Commands to comute the relays
#define ACTUATE_SIMPLE    0xA0
#define ACTUATE_MULTIPLE  0x0A

// Commands to change program settings
#define PROGRAM_SETTINGS  0xC0
#define SET_RELAYS_DELAY  0x00
#define CHANGE_CODE       0x01
#define GENERIC_CODE      0x00
#define OPTIMIZED_CODE    0x01

#define OPEN              0x01
#define CLOSE             0x00

#define SIMPLE_REQUEST    0xB0
#define MULTIPLE_REQUEST  0x0B

#define STOP              0x00
#define GO_DOWN           0x01
#define GO_UP             0x02

#define ACK               0x06
#define NACK              0x15

#define CONNECTION        0x00
#define RELAY1            0x01
#define RELAY2            0x02
#define RELAY3            0x03
#define RELAY4            0x04
#define FUNC_MODE         0x05
#define TOP_SWITCH        0x06
#define BOTTOM_SWITCH     0x07

#define SUCCESS           0x00
#define FAILURE           0x01

#define TIMER_INTERVAL_MS        10000
