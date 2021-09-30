//#include <ESP8266WiFi.h>
//#include <EEPROM.h>
#include <WiFiUdp.h>
//#include <ESPAsyncTCP.h>
//#include <ESPAsyncWebServer.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

/************************* Defines *************************/

// Pins configuration
#define RELAY_1 15
#define RELAY_2 14
#define RELAY_3 12
#define RELAY_4 13

// Pins for switchs
#define FUNC_MODE_PIN     5
#define TOP_SWITCH_PIN    4
#define BOTTOM_SWITCH_PIN 4

#define ACTIVE 2
#define WIFI_CONF 16

// Invalid data from flash
#define INVALID_DATA "AAA"

// EEPROM reference address
#define STARTING_ADDR 1

// Commands to comute the relays
#define ACTUATE_SIMPLE    0xA0
#define ACTUATE_MULTIPLE  0x0A

#define OPEN              0x01
#define CLOSE             0x00

#define SIMPLE_REQUEST    0xB0
#define MULTIPLE_REQUEST  0x0B

#define RELAY1           0x01
#define RELAY2           0x02
#define RELAY3           0x03
#define RELAY4           0x04
#define FUNC_MODE        0x05
#define TOP_SWITCH       0x06
#define BOTTOM_SWITCH    0x07

#define SUCCESS          0x00
#define FAILURE          0x01

/************************* Global Variables *************************/

// Component status variables
struct components_s{
  int pin;
  int value;
};
components_s components[7];

// UDP server port
int UDP_PORT = 4210;

// State of network connection
enum WiFi_connection_state{
  CONFIGURING = 0,
  CONFIGURED,
  INVALID,
  CONNECTED,
};

WiFi_connection_state connection_state = CONFIGURING;

// UDP variables
WiFiUDP UDP;
char packet[255];
char reply[20] = "Packet received!";
int reply_len = 0;

/************************* Custom Functions *************************/

/*
Configured UDP server.
*/
void configure_UDP_server()
{
  // Connected to WiFi
  Serial.println();
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
   
  // Begin listening to UDP port
  UDP.begin(UDP_PORT);
  Serial.print("Listening on UDP port ");
  Serial.println(UDP_PORT);

  // Update the connection status
  connection_state = CONNECTED;
}

/*
Comute the relay.
Possible values are defined by RELAY_#.
action -> open(1)/close(0)
*/
void comute_relay(int relay_pin, int action)
{
  if(action == OPEN){
    digitalWrite(relay_pin, LOW);
  }
  else if(action == CLOSE){
    digitalWrite(relay_pin, HIGH);
  }
}

/*
CheckSum of values. Simple sum.
*/
uint8 check_sum(char *value, int len)
{
  int index = 0;
  int check = 0;

  // Sum the received bytes and compare to the last byte received
  if(len > 1){
    for(index = 0; index < (len-1); index++){
      check = check + *(value+index);
    }
    if(check == *(value+(len-1))){
      return SUCCESS;
    }else{
      return FAILURE;
    }   
  }
  else{
    return FAILURE;
  }
}

/*
Parse UDP package received.
*/
void parse_packet(int package_len)
{
  int ind = 0;
  int packet_count = 0;
  int comp = 0;

  if(package_len == 4)
  {
    comp = packet[0] + 0;
    if(comp == ACTUATE_SIMPLE){ // Open or close command for only one relay
      for(ind = 0; ind < 4; ind++)
      {
        comp = packet[1] + 0;
        if(ind == (comp-1)){
          comp = packet[2] + 0;
          comute_relay(components[ind].pin, comp);
          if(comp == CLOSE){
            components[ind].value = CLOSE;
          }
          else if(comp == OPEN){
            components[ind].value = OPEN;
          }         
          reply[0] = packet[0];
          reply[1] = ind + 1;
          reply[2] = packet[2];
          reply[3] = 0x06;
          reply[4] = reply[0] +  reply[1] +  reply[2] +  reply[3];
          reply_len = 5;
          break;
        }
      }
    }
    else if(comp == ACTUATE_MULTIPLE){ // Open or close command for multiple relays
      for(ind = 0; ind < 4; ind++)
      {
        comp = packet[1] + 0;
        if(bitRead(comp, ind)){
          comp = packet[2] + 0;
          comute_relay(components[ind].pin, bitRead(comp, ind));
          if(comp == CLOSE){
            components[ind].value = CLOSE;
          }
          else if(comp == OPEN){
            components[ind].value = OPEN;
          }
        }
      }
      reply[0] = packet[0];
      reply[1] = (packet[1] & 0x0F);
      reply[2] = (packet[2] & 0x0F);
      reply[3] = 0x06;
      reply[4] = reply[0] +  reply[1] +  reply[2] +  reply[3];
      reply_len = 5;
    }
    else if(comp == MULTIPLE_REQUEST){ // Request for multiple status on relays and switches
      reply[0] = packet[0];
      reply[1] = (packet[1] & 0x0F);
      reply[2] = (packet[2] & 0x07);

      // Get relays status
      reply[3] = 0;
      comp = packet[1] + 0;
      
      for(ind = 0; ind < 4; ind++)
      {
        reply[3] = (reply[3] | (components[ind].value << ind));
      }
      
      // Get switches status
      reply[4] = 0;
      comp = packet[2] + 0;
      for(ind = 0; ind < 3; ind++)
      {
        reply[4] = (reply[4] | (components[ind+4].value << ind));
      }
      
      reply[5] = 0x06; // ACK
      reply[6] = reply[0] + reply[1] + reply[2] + reply[3] + reply[4] + reply[5]; // Simple CS
      reply_len = 7;
    }
  }
  else if(package_len == 3)
  {
    comp = packet[0] + 0;
    if(comp == SIMPLE_REQUEST){ // Request for only a component status
      comp = packet[1] + 0;
      switch(comp){
        case RELAY1: // Get relay1 status
        reply[0] = packet[0];
        reply[1] = RELAY1;
        reply[2] = components[0].value;
        reply[3] = reply[0] +  reply[1] +  reply[2];
        reply_len = 4;
        break;
        case RELAY2: // Get relay2 status
        reply[0] = packet[0];
        reply[1] = RELAY2;
        reply[2] = components[1].value;
        reply[3] = reply[0] +  reply[1] +  reply[2];
        reply_len = 4;
        break;
        case RELAY3: // Get relay3 status
        reply[0] = packet[0];
        reply[1] = RELAY3;
        reply[2] = components[2].value;
        reply[3] = reply[0] +  reply[1] +  reply[2];
        reply_len = 4;
        break;
        case RELAY4: // Get relay4 status
        reply[0] = packet[0];
        reply[1] = RELAY4;
        reply[2] = components[3].value;
        reply[3] = reply[0] +  reply[1] +  reply[2];
        reply_len = 4;
        break;
        case FUNC_MODE: // Get functional mode status
        reply[0] = packet[0];
        reply[1] = FUNC_MODE;
        components[4].value = (digitalRead(FUNC_MODE_PIN) ? CLOSE : OPEN);
        reply[2] = components[4].value;
        reply[3] = reply[0] +  reply[1] +  reply[2];
        reply_len = 4;
        break;
        case TOP_SWITCH: // Get top end switch status
        reply[0] = packet[0];
        reply[1] = TOP_SWITCH;
        components[5].value = (digitalRead(TOP_SWITCH_PIN) ? CLOSE : OPEN);
        reply[2] = components[5].value;
        reply[3] = reply[0] +  reply[1] +  reply[2];
        reply_len = 4;
        break;
        case BOTTOM_SWITCH: // Get bottom end switch status
        reply[0] = packet[0];
        reply[1] = BOTTOM_SWITCH;
        components[6].value = (digitalRead(BOTTOM_SWITCH_PIN) ? CLOSE : OPEN);
        reply[2] = components[6].value;
        reply[3] = reply[0] +  reply[1] +  reply[2];
        reply_len = 4;
        break;
      }
    }
  }
}

void setup() {
  // Configure the components of the system as relays and switches
  components[0].pin = RELAY_1;
  components[1].pin = RELAY_2;
  components[2].pin = RELAY_3;
  components[3].pin = RELAY_4;
  components[4].pin = FUNC_MODE_PIN;
  components[5].pin = TOP_SWITCH_PIN;
  components[6].pin = BOTTOM_SWITCH_PIN;
  
  // Configure pin modes as input or output
  pinMode(RELAY_1, OUTPUT);
  pinMode(RELAY_2, OUTPUT);
  pinMode(RELAY_3, OUTPUT);
  pinMode(RELAY_4, OUTPUT);
  pinMode(ACTIVE, OUTPUT);
  pinMode(WIFI_CONF, INPUT);
  pinMode(FUNC_MODE_PIN, INPUT);
  pinMode(TOP_SWITCH_PIN, INPUT);
//  pinMode(BOTTOM_SWITCH_PIN, INPUT);

  // Set starting values for each pin and update the last setted values
  digitalWrite(RELAY_1, LOW);
  components[0].value = OPEN;
  digitalWrite(RELAY_2, LOW);
  components[1].value = OPEN;
  digitalWrite(RELAY_3, LOW);
  components[2].value = OPEN;
  digitalWrite(RELAY_4, LOW);
  components[3].value = OPEN;
  digitalWrite(ACTIVE, HIGH);

  // Store initial status for switches
  components[4].value = (digitalRead(FUNC_MODE_PIN) ? CLOSE : OPEN);
  components[5].value = (digitalRead(TOP_SWITCH_PIN) ? CLOSE : OPEN);
  components[6].value = (digitalRead(BOTTOM_SWITCH_PIN) ? CLOSE : OPEN);

  // Starts serial port for debug
  Serial.begin(115200);
  Serial.println();

  // Configure wifi network
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

  // Uses WiFiManager library
  WiFiManager wm;

  // If pin for wifi_conf is high, should enter access point mode
  // to configure new network
  if(digitalRead(WIFI_CONF) == LOW){
    // Reset settings - wipe stored credentials and configure again
    wm.resetSettings();
  }

  // Automatically connect using saved credentials,
  // If connection fails, it starts an access point with the specified name ( "EPS-NET")
  bool res;
  res = wm.autoConnect("EPS-NET","password"); // password protected ap

  if(!res) {
    Serial.println("Failed to connect");
    ESP.restart();
  } 
  else {
    //if you get here you are connected to the WiFi    
    Serial.println("Connected!");
    connection_state == CONFIGURED;
  }
  configure_UDP_server();
}

void loop() {
  // If it's already connected to the network, wait for UPD packages
  if(connection_state == CONNECTED){
    int packetSize = UDP.parsePacket();
    
    if (packetSize) {
//      Serial.print("Received packet!");
      int len = UDP.read(packet, 255);

      if(check_sum(packet, len) == SUCCESS){
//        Serial.print("Packet received, check sum is ok!");
        parse_packet(len);

        // Send response packet
        UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
        UDP.write(reply, reply_len);
        UDP.endPacket();
      }else{
        Serial.print("Packet received, but check sum not ok!");
      }
    }
  }
}
