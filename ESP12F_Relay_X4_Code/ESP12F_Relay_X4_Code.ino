#include <WiFiUdp.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <EEPROM.h>
#include "variables.h"
#include "ESP8266TimerInterrupt.h"
/************************* Global Variables *************************/
// UDP handler
WiFiUDP UDP;

// Timer for fixed time operating mode
ESP8266Timer ITimer;
/************************* Custom Functions *************************/

/*
Timer callback for fixed time operating mode.
*/
void IRAM_ATTR TimerHandler(void)
{
  disable_outputs();
  ITimer.disableTimer();
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
Switch relays to go up.
*/
void go_up()
{
  if(moving_state == GOING_DOWN){
    disable_outputs();
    delay(1000);
  }
  // Relays 1 and 3 are closed
  digitalWrite(RELAY_1, HIGH);
  components[0].value = CLOSE;
  digitalWrite(RELAY_3, HIGH);
  components[2].value = CLOSE;
  moving_state = GOING_UP;
}

/*
Switch relays to go down.
*/
void go_down()
{
  if(moving_state == GOING_UP){
    disable_outputs();
    delay(1000);
  }
  // Relays 2 and 4 are closed
  digitalWrite(RELAY_2, HIGH);
  components[1].value = CLOSE;
  digitalWrite(RELAY_4, HIGH);
  components[3].value = CLOSE;
  moving_state = GOING_DOWN;
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
  moving_state = STOPPED;
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
  int resp_status = 0;
  
  if(package_len == 4)
  {
    comp = packet[0] + 0;
    if(comp == ACTUATE_SIMPLE) // Open or close command for only one relay
    {
      if(running_state == GENERIC) // Only work if it's in generic mode
      {
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
      else{ // If it's not in generic mode, rensponse is NACK
        reply[0] = NACK;
        reply_len = 1;
      }
    }
    else if(comp == ACTUATE_MULTIPLE) // Open or close command for multiple relays
    {
      if(running_state == GENERIC) // Only works if it's in generic mode
      {
        for(ind = 0; ind < 4; ind++)
        {
          comp = packet[1] + 0;
          if(bitRead(comp, ind)){
            comp = packet[2] + 0;
            comute_relay(components[ind].pin, bitRead(comp, ind));
            if(bitRead(comp, ind) == CLOSE){
              components[ind].value = CLOSE;
            }
            else if(bitRead(comp, ind) == OPEN){
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
      else{ // If it's not in generic mode, rensponse is NACK
        reply[0] = NACK;
        reply_len = 1;
      }
    }
    else if(comp == MULTIPLE_REQUEST) // Request for multiple status on relays and switches
    {
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
      
      reply[5] = ACK;
      reply[6] = reply[0] + reply[1] + reply[2] + reply[3] + reply[4] + reply[5]; // Simple CS
      reply_len = 7;
    }
    else if(comp == PROGRAM_SETTINGS)
    {
      comp = packet[1] + 0;
      if(comp == SET_RELAYS_DELAY) // Configure a new fixed time for optimized mode
      {
        int new_delay = (packet[2] << 0x08) | packet[3];
        if(new_delay > 26000){ // 26 seconds is the maximum time for timer configuration
          new_delay = 26000;
        }
        disable_outputs();
        time_delay = new_delay; // Update fixed timing
        if(time_delay == 0){ // If time was set to 0s, it's because the continuous mode is selected, no timer is used in this mode
          disable_outputs();
          ITimer.disableTimer();
        }
        // Send response
        reply[0] = packet[0];
        reply[1] = SET_RELAYS_DELAY;
        reply[2] = ((time_delay >> 0x08) & (0xFF));
        reply[3] = (time_delay & 0xFF);
        reply[4] = ACK;
        reply_len = 5;
      }
      else if(comp == CHANGE_CODE) // Change working mode
      {
        comp = packet[2] + 0;
        switch(comp){
          case GENERIC_CODE: // Run generic code
          running_state = GENERIC;
          disable_outputs();
          // Send response
          reply[0] = packet[0];
          reply[1] = CHANGE_CODE;
          reply[2] = GENERIC_CODE;
          reply[3] = ACK;
          reply[4] = reply[0] +  reply[1] +  reply[2] + reply[3];
          reply_len = 5;
          break;
          case OPTIMIZED_CODE: // Run optimized code
          running_state = OPTIMIZED;
          disable_outputs();
          // Send response
          reply[0] = packet[0];
          reply[1] = CHANGE_CODE;
          reply[2] = OPTIMIZED_CODE;
          reply[3] = ACK;
          reply[4] = reply[0] +  reply[1] +  reply[2] + reply[3];
          reply_len = 5;
          break;
          default: // Not recognized mode
          reply[0] = NACK;
          reply_len = 1;
          break;
        }
      }
      else{ // If the command was not recognized, send NACK
        reply[0] = NACK;
        reply_len = 1;
      }
    }
  }
  else if(package_len == 3)
  {
    comp = packet[0] + 0;
    if(comp == ACTUATE_SIMPLE) // Command to change relays status
    {
      if(running_state == OPTIMIZED) // These only work in optimized mode
      {
        comp = packet[1] + 0;
        switch(comp){
          case STOP: // Stop motor
          disable_outputs();
          reply[0] = packet[0];
          reply[1] = STOP;
          reply[2] = ACK;
          reply[3] = reply[0] +  reply[1] +  reply[2];
          reply_len = 4;
          break;
          case GO_DOWN: // Move stand down
          if(time_delay != 0){
            if(ITimer.setInterval((long int)time_delay * 1000, TimerHandler) == true){ // Configure timer to stop after specified time
              go_down();
              resp_status = 1;
            }
            else{ // If timer could not be configured, then send a NACK response
              resp_status = 0;
            }
          }
          else{ // If specified time is 0s, then go down until receive stop or up command
            go_down();
            resp_status = 1;
          }
          if(resp_status != 0){ // Send ACK response
            reply[0] = packet[0];
            reply[1] = GO_DOWN;
            reply[2] = ACK;
            reply[3] = reply[0] +  reply[1] +  reply[2];
            reply_len = 4;
          }
          else{ // Send NACK response
            reply[0] = packet[0];
            reply[1] = GO_DOWN;
            reply[2] = NACK;
            reply[3] = reply[0] +  reply[1] +  reply[2];
            reply_len = 4;
          }
          break;
          case GO_UP: // Move stand up
          if(time_delay != 0){
            if(ITimer.setInterval((long int)time_delay * 1000, TimerHandler) == true){ // Configure timer to stop after specified time
              go_up();
              resp_status = 1;
            }
            else{ // If timer could not be configured, then send a NACK response
              resp_status = 0;
            }
          }
          else{ // If specified time is 0s, then go up until receive stop or down command
            go_up();
            resp_status = 1;
          }
          if(resp_status != 0){ // Send ACK response
            reply[0] = packet[0];
            reply[1] = GO_UP;
            reply[2] = ACK;
            reply[3] = reply[0] +  reply[1] +  reply[2];
            reply_len = 4;
          }
          else{ // Send NACK response
            reply[0] = packet[0];
            reply[1] = GO_UP;
            reply[2] = NACK;
            reply[3] = reply[0] +  reply[1] +  reply[2];
            reply_len = 4;
          }
          break;
          default: // If the command was not recognized, send NACK response
          reply[0] = NACK;
          reply_len = 1;
          break;
        }
      }
      else{ // If code is working in generic mode, send NACK response
        reply[0] = NACK;
        reply_len = 1;
      }
    }
    else if(comp == SIMPLE_REQUEST) // Request for only a component status
    {
      comp = packet[1] + 0;
      switch(comp){
        case CONNECTION: // Connection status
        reply[0] = packet[0];
        reply[1] = CONNECTION;
        reply[2] = CONNECTION;
        reply[3] = ACK;
        reply[4] = reply[0] +  reply[1] +  reply[2] + reply[3];
        reply_len = 5;
        break;
        case RELAY1: // Get relay1 status
        reply[0] = packet[0];
        reply[1] = RELAY1;
        reply[2] = components[0].value;
        reply[3] = ACK;
        reply[4] = reply[0] +  reply[1] +  reply[2] + reply[3];
        reply_len = 5;
        break;
        case RELAY2: // Get relay2 status
        reply[0] = packet[0];
        reply[1] = RELAY2;
        reply[2] = components[1].value;
        reply[3] = ACK;
        reply[4] = reply[0] +  reply[1] +  reply[2] + reply[3];
        reply_len = 5;
        break;
        case RELAY3: // Get relay3 status
        reply[0] = packet[0];
        reply[1] = RELAY3;
        reply[2] = components[2].value;
        reply[3] = ACK;
        reply[4] = reply[0] +  reply[1] +  reply[2] + reply[3];
        reply_len = 5;
        break;
        case RELAY4: // Get relay4 status
        reply[0] = packet[0];
        reply[1] = RELAY4;
        reply[2] = components[3].value;
        reply[3] = ACK;
        reply[4] = reply[0] +  reply[1] +  reply[2] + reply[3];
        reply_len = 5;
        break;
        case FUNC_MODE: // Get functional mode status
        reply[0] = packet[0];
        reply[1] = FUNC_MODE;
        reply[2] = components[4].value;
        reply[3] = ACK;
        reply[4] = reply[0] +  reply[1] +  reply[2] + reply[3];
        reply_len = 5;
        break;
        case TOP_SWITCH: // Get top end switch status
        reply[0] = packet[0];
        reply[1] = TOP_SWITCH;
        reply[2] = components[5].value;
        reply[3] = ACK;
        reply[4] = reply[0] +  reply[1] +  reply[2] + reply[3];
        reply_len = 5;
        break;
        case BOTTOM_SWITCH: // Get bottom end switch status
        reply[0] = packet[0];
        reply[1] = BOTTOM_SWITCH;
        reply[2] = components[6].value;
        reply[3] = ACK;
        reply[4] = reply[0] +  reply[1] +  reply[2] + reply[3];
        reply_len = 5;
        break;
        default: // If component was not recognized, send NACK
        reply[0] = NACK;
        reply_len = 1;
        break;
      }
    }
  }
  else{ // If invalid size of packet, send NACK
    reply[0] = NACK;
    reply_len = 1;
  }
}

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
Connects to the wifi network.
*/
void connect_to_wifi_network(IPAddress ip, IPAddress gateway, IPAddress submask)
{
  if((ip != IPAddress(0,0,0,0)) && (gateway != IPAddress(0,0,0,0)) && (submask != IPAddress(0,0,0,0))){
    // Set static IP
    IPAddress dns(8,8,8,8);
    WiFi.config(ip, dns, gateway, submask);
  }
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
Function that reconnect to the WiFi after disconnection.
*/

void wifi_reconnect()
{
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
  EEPROM.begin(512);

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

    IPAddress IP(0,0,0,0);
    IPAddress gw(0,0,0,0);
    IPAddress sm(0,0,0,0);

    // Read static IP from flash
    EEPROM.get(STARTING_ADDR, data);
    IP.fromString(data.str);
    Serial.print("WiFi IP ");
    Serial.print(IP);

    // Write gateway on flash
    eeprom_address = STARTING_ADDR + sizeof(eeprom_data);
    EEPROM.get(eeprom_address, data);
    gw.fromString(data.str);
    Serial.print("WiFi gateway ");
    Serial.print(gw);

    // Write subnet mask on flash
    eeprom_address = eeprom_address + sizeof(eeprom_data);
    EEPROM.get(eeprom_address, data);
    sm.fromString(data.str);
    Serial.print("WiFi Subnet Mask ");
    Serial.print(sm);
      
    connect_to_wifi_network(IP, gw, sm);
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
      String IP_config;

      // Store new WiFi SSID and password in case of disconnection
      saved_SSID.toCharArray(WIFI_SSID, saved_SSID.length()+1);
      saved_PASS.toCharArray(WIFI_PASS, saved_PASS.length()+1);

      // TODO: save static IP config

      // Write static IP on flash
      IP_config = WiFi.localIP().toString();
      IP_config.concat('\0');
      IP_config.toCharArray(data.str, IP_config.length());
      Serial.print("WiFi IP ");
      Serial.print(IP_config);
      Serial.print(" IP length ");
      Serial.println(IP_config.length());
      EEPROM.put(STARTING_ADDR, data);
      EEPROM.commit();

      // Write gateway on flash
      eeprom_address = STARTING_ADDR + sizeof(eeprom_data);
      IP_config = WiFi.gatewayIP().toString();
      IP_config.concat('\0');
      IP_config.toCharArray(data.str, IP_config.length());
      Serial.print("WiFi Gateway ");
      Serial.print(IP_config);
      Serial.print(" Gateway length ");
      Serial.println(IP_config.length());
      EEPROM.put(eeprom_address, data);
      EEPROM.commit();

      // Write subnet mask on flash
      eeprom_address = eeprom_address + sizeof(eeprom_data);
      IP_config = WiFi.subnetMask().toString();
      IP_config.concat('\0');
      IP_config.toCharArray(data.str, IP_config.length());
      Serial.print("WiFi Subnet Mask ");
      Serial.print(IP_config);
      Serial.print(" Subnet Mask length ");
      Serial.println(IP_config.length());
      EEPROM.put(eeprom_address, data);
      EEPROM.commit();
      
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
        }
        else if((len == 4) && (packet[0] == PROGRAM_SETTINGS) && (packet[1] == SET_RELAYS_DELAY)){
          // If checksum failed, it might be a set time command for fixed time mode
          parse_packet(len);

          // Send response packet
          UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
          UDP.write(reply, reply_len);
          UDP.endPacket();
        }else{
          Serial.print("Packet received, but check sum not ok!");
          // Send response packet
          reply[0] = NACK;
          reply_len = 1;
          UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
          UDP.write(reply, reply_len);
          UDP.endPacket();
        }
      }
    }
    // If WIFI_CONF pin is low, then restart the ESP to erase previous configurations
    if(digitalRead(WIFI_CONF) == LOW){
      disable_outputs();
      ESP.reset();  
    }
    // If came back from a disconnection, update the connection state and inform new IP
    if(connection_state == DISCONNECTED){
      connection_state = RUNNING;
      Serial.println();
      Serial.print("Connected! IP address: ");
      Serial.println(WiFi.localIP());
    }
    components[4].value = (digitalRead(FUNC_MODE_PIN) ? CLOSE : OPEN);
    components[5].value = (digitalRead(TOP_SWITCH_PIN) ? CLOSE : OPEN);
    components[6].value = (digitalRead(BOTTOM_SWITCH_PIN) ? CLOSE : OPEN);
  }
  // If just disconnected, update the connection state and start reconnection routine
  if(connection_state != DISCONNECTED){
    connection_state = DISCONNECTED;
    disable_outputs();
    wifi_reconnect();
  }else{
    // While has not reconnected print dots
    delay(100);
    Serial.print(".");
  }
}
