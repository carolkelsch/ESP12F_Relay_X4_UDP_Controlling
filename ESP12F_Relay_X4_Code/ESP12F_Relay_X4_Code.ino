#include <WiFiUdp.h>
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
#define INVALID_DATA ""

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
  RUNNING,
  DISCONNECTED,
};

WiFi_connection_state connection_state = CONFIGURING;

// WiFi default credentials
char WIFI_SSID[255] = "AAA";
char WIFI_PASS[255] = "AAA";
char static_ip[16] = "10.0.1.56";
char static_gw[16] = "10.0.1.1";
char static_sn[16] = "255.255.255.0";

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
  connection_state = RUNNING;
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

/*
Connects to the wifi network.
*/
void connect_to_wifi_network()
{
  // Begin WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
    
  // Connecting to WiFi...
  Serial.print("Connecting to ");
  Serial.print(WIFI_SSID);
  // Loop continuously while WiFi is not connected
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
    Serial.print(".");
  }
  
  // Connected to WiFi
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
  
  // Update the connection status
  connection_state = CONNECTED;
}

/*
Disable all outputs for safety reason, in case the connection is lost.
*/
void disable_outputs()
{
  digitalWrite(RELAY_1, LOW);
  components[0].value = OPEN;
  digitalWrite(RELAY_2, LOW);
  components[1].value = OPEN;
  digitalWrite(RELAY_3, LOW);
  components[2].value = OPEN;
  digitalWrite(RELAY_4, LOW);
  components[3].value = OPEN;
}

/*
Function that reconnect to the WiFi after disconnection.
*/

void wifi_reconnect()
{
  disable_outputs();
  WiFi.disconnect();
  delay(10);
  // Begin WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
    
  // Connecting to WiFi...
  Serial.print("Connecting to ");
  Serial.print(WIFI_SSID);
  Serial.print(".");
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
  disable_outputs();
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
  else{
    // Try to get previous data from flash, if the data is valid, then connect to WiFi
    String saved_SSID = wm.getWiFiSSID();
    String saved_PASS = wm.getWiFiPass();
    if(saved_SSID != INVALID_DATA){
      saved_SSID.toCharArray(WIFI_SSID, saved_SSID.length()+1);
    
      if(saved_PASS != INVALID_DATA){
        saved_PASS.toCharArray(WIFI_PASS, saved_PASS.length()+1);

        connection_state = CONFIGURED;
      }
    }
  }

  // If previous data is ok, then connect to WiFi
  if(connection_state == CONFIGURED){
    connect_to_wifi_network();
  }else{
    // If not, it starts an access point with the specified name "ESP-NET" and "password" as password
    bool res;
    
    wm.setBreakAfterConfig(true);

    //set static ip
    IPAddress _ip, _gw, _sn;
    _ip.fromString(static_ip);
    _gw.fromString(static_gw);
    _sn.fromString(static_sn);
  
    wm.setSTAStaticIPConfig(_ip, _gw, _sn);
    
    res = wm.autoConnect("EPS-NET", "password");
  
    if(!res) {
      Serial.println("Failed to connect");
      ESP.restart();
    } 
    else {
      // If you get here you are connected to the WiFi    
      Serial.println("Connected!");
      String saved_SSID = wm.getWiFiSSID();
      String saved_PASS = wm.getWiFiPass();

      // Store new WiFi SSID and password in case of disconnection
      saved_SSID.toCharArray(WIFI_SSID, saved_SSID.length()+1);
      saved_PASS.toCharArray(WIFI_PASS, saved_PASS.length()+1);

      // TODO: save static IP config
      Serial.print("WiFi IP");
      Serial.println(WiFi.localIP());
      Serial.print("WiFi Gateway");
      Serial.println(WiFi.gatewayIP());
      Serial.print("WiFi Subnet Mask");
      Serial.println(WiFi.subnetMask());
      
      connection_state == CONNECTED;
    }
  }
  
  // Starts de UDP server
  configure_UDP_server();
}

void loop() {
  // If is connected on WiFi
  while (WiFi.status() == WL_CONNECTED)
  {
    // If UDP server is UP, wait for UPD packages
    if(connection_state == RUNNING){
      int packetSize = UDP.parsePacket();
      
      if (packetSize) {
        int len = UDP.read(packet, 255);
  
        if(check_sum(packet, len) == SUCCESS){
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
    // If WIFI_CONF pin is low, then restart the ESP to erase previous configurations
    if(digitalRead(WIFI_CONF) == LOW){
      ESP.reset();  
    }
    // If came back from a disconnection, update the connection state and inform new IP
    if(connection_state == DISCONNECTED){
      connection_state = RUNNING;
      Serial.println();
      Serial.print("Connected! IP address: ");
      Serial.println(WiFi.localIP());
    }
  }
  // If just disconnected, update the connection state and start reconnection routine
  if(connection_state != DISCONNECTED){
    connection_state = DISCONNECTED;
    wifi_reconnect();
  }else{
    // While has not reconnected print dots
    delay(100);
    Serial.print(".");
  }
}
