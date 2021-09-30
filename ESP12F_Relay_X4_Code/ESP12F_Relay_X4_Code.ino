#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <WiFiUdp.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

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

// WiFi default credentials
char WIFI_SSID[255] = "DESKTOP-3DJR285";
char WIFI_PASS[255] = "4y1Y1178";
int UDP_PORT = 4210;

// Access point configurations
IPAddress local_IP(192,168,4,22);
IPAddress gateway(192,168,4,9);
IPAddress subnet(255,255,255,0);

// EEPROM address management
int eeprom_address = STARTING_ADDR;

// State of network connection
enum WiFi_connection_state{
  CONFIGURING = 0,
  CONFIGURED,
  INVALID,
  CONNECTED,
};

WiFi_connection_state connection_state = CONFIGURING;

// EEPROM data structure
struct eeprom_data{ 
  uint val = 0;
  char str[35] = "";
} data;

// AsyncWebServer on port 80 for wifi network configuring
AsyncWebServer server(80);

// WebServer inputs
char* PARAM_INPUT_1 = "imputPORT";
char* PARAM_INPUT_2 = "inputSSID";
char* PARAM_INPUT_3 = "inputPASS";

// WebServer simple page
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>ESP WiFi Config Form</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  <form action="/configWifi" method="get">
    PORT: <input type="number" name="imputPORT" value="4210"><br>
    SSID: <input type="text" name="inputSSID" value="MySSID"><br>
    PASS: <input type="password" name="inputPASS"><br>
    <input type="submit" value="Submit">
  </form>
</body></html>)rawliteral";

// UDP variables
WiFiUDP UDP;
char packet[255];
char reply[20] = "Packet received!";
int reply_len = 0;

/************************* Custom Functions *************************/

/*
Send error message to WebServer.
*/
void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

/*
Configure WiFi as access point and configure a little webpage to get configurations
from new WiFi network.
*/
void configure_wifi_network()
{
  Serial.print("Setting soft-AP configuration ... ");
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");

  Serial.print("Setting soft-AP ... ");
  Serial.println(WiFi.softAP("ESP-Net") ? "Ready" : "Failed!");

  Serial.print("Soft-AP IP address = ");
  Serial.println(WiFi.softAPIP());
  
  Serial.println();
  
  // Send web page with input fields to client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  // Send a GET request to input parameters
  server.on("/configWifi", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputParam;
    String ssid_string;
    String pass_string;
    String inputMessage;
    String port_number;
    // GET input 1, 2 and 3 values
    if (request->hasParam(PARAM_INPUT_2) && request->hasParam(PARAM_INPUT_3) && request->hasParam(PARAM_INPUT_1)) {
      ssid_string = request->getParam(PARAM_INPUT_2)->value();
      pass_string = request->getParam(PARAM_INPUT_3)->value();
      port_number = request->getParam(PARAM_INPUT_1)->value();
      inputParam = PARAM_INPUT_1;

      // Write SSID from network on flash
      ssid_string.concat('\0');
      ssid_string.toCharArray(data.str, (ssid_string.length()+2));
      ssid_string.toCharArray(WIFI_SSID, (ssid_string.length()+2));
      EEPROM.put(STARTING_ADDR, data);
      EEPROM.commit();

      // Write Password from network on flash
      eeprom_address = STARTING_ADDR + sizeof(eeprom_data);
      pass_string.concat('\0');
      pass_string.toCharArray(data.str, (pass_string.length()+2));
      ssid_string.toCharArray(WIFI_PASS, (ssid_string.length()+2));
      EEPROM.put(eeprom_address, data);
      EEPROM.commit();

      UDP_PORT = port_number.toInt();
      // Write UDP port to flash
      eeprom_address = eeprom_address + sizeof(eeprom_data);
      data.val = UDP_PORT;
      EEPROM.put(eeprom_address, data);
      EEPROM.commit();

      inputMessage = "ok, switch to working mode and reset the board";
      connection_state = CONFIGURED;
    }
    else {
      inputMessage = "Error, reset the board and try again";
      inputParam = "none";
    }
    Serial.println(inputMessage);
    // Send a resposnse
    request->send(200, "text/html", "HTTP GET request status " 
                                     + inputMessage +
                                     "<br><a href=\"/\">Return to Home Page</a>");
  });

  server.onNotFound(notFound);
  server.begin();
}

/*
Disconnects from access point mode.
*/
void disconnect_AP_mode()
{
  WiFi.softAPdisconnect(true);
  delay(500);
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

  EEPROM.begin(512);

  // Read from flash
  EEPROM.get(STARTING_ADDR, data);

  // Test if network SSID read from Flash is valid
  if(data.str != INVALID_DATA){
    Serial.println("Has Network SSID!");
    strcpy(WIFI_SSID, data.str);
    connection_state = CONFIGURED;
  }else{
    Serial.println("No valid Network SSID!");
    connection_state = INVALID;
  }
  eeprom_address = STARTING_ADDR + sizeof(eeprom_data);
  EEPROM.get(eeprom_address, data);

  // Test if network Password read from Flash is valid
  if(data.str != INVALID_DATA){
    Serial.println("Has Network PASS!");
    strcpy(WIFI_PASS, data.str);
    connection_state = CONFIGURED;
  }else{
    Serial.println("No valid Network PASS!");
    connection_state = INVALID;
  }
  
  eeprom_address = eeprom_address + sizeof(eeprom_data);
  EEPROM.get(eeprom_address, data);
  
  // Test if UDP port read from Flash is valid
  if(data.val != 0){
    Serial.println("Has UDP Port!");
     UDP_PORT = data.val;
    connection_state = CONFIGURED;
  }else{
    Serial.println("No valid UDP Port, setting to default 4210!");
  }

  // If pin for wifi_conf is high, should enter access point mode
  // to configure new network
  if(digitalRead(WIFI_CONF) == HIGH){
    connection_state = CONFIGURING;
    configure_wifi_network();
  }
  else if(connection_state == INVALID){ // If Flash values are invalid, should configure new network
    connection_state = CONFIGURING;
    configure_wifi_network();
  }
  else{ // If has a valid pre-configured network, connects to the network
    connect_to_wifi_network();
  }
}

void loop() {
  // If it's already connected to the RaspberryPi network, wait for UPD packages
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
  else if(connection_state == CONFIGURED){ // If it was configuring the new network, and now has valid parameters,
    // change the access point mode to a sta mode
    Serial.println("Turning off the access point.");
    disconnect_AP_mode();
    connect_to_wifi_network();
  }
}
