# ESP12F_Relay_X4_UDP_Controlling
This project provides a code for ESP12F_Relay_X4 board (https://templates.blakadder.com/ESP12F_Relay_X4.html). It enables the relays to be controlled by UDP commands.

The ESP will connect to a WiFi network, wich can be configured by accessing the ESP when it's in Access Point mode. After connected to the WLAN, the ESP can receive UDP packages with commands to controll the relays and read digital inputs states.

# Setting commands
| Command  |  Function  |
| --------- | --------- |
|  A0 01 01 A2 |  Open relay 1 |
|  A0 01 00 A1 |  Close relay 1 |
|  A0 02 01 A3 |  Open relay 2 |
|  A0 02 00 A2 |  Close relay 2 |
|  A0 03 01 A4 |  Open relay 3 |
|  A0 03 00 A3 |  Close relay 3 |
|  A0 04 01 A5 |  Open relay 4 |
|  A0 04 00 A4 |  Close relay 4 |
|  0A R# C# CS |  Open or close multiple relays |

The last command can change the state of multiple relays, the structure of the command is:
- **0A**: identificator byte;
- **R#**: relays mask byte, if bit is set to 1, then the command will be applied to the relay;


| Bit 7 | Bit 6 | Bit 5 | Bit 4 | Bit 3 | Bit 2 | Bit 1 | Bit 0 |
| ----- | ----- | ----- | ----- | ----- | ----- | ----- | ----- |
| - | - | - | - | Relay 4 | Relay 3 | Relay 2 | Relay 1 |

- **C#**: command mask byte, if bit set to 1 (Open) the respective relay will be opened, if set to 0 (Close), then relay will be closed. Note that this command only works if the relay is set in the relay mask;

| Bit 7 | Bit 6 | Bit 5 | Bit 4 | Bit 3 | Bit 2 | Bit 1 | Bit 0 |
| ----- | ----- | ----- | ----- | ----- | ----- | ----- | ----- |
| - | - | - | - | 1 (Open) / 0 (Close) | 1 (Open) / 0 (Close) | 1 (Open) / 0 (Close) | 1 (Open) / 0 (Close) |

- **CS**: simple check sum, aquired by summing all the previous bytes os the message.

The response for the setting commands are the first 3 bytes of the command, plus a 0x06 byte (ACK) and the new CheckSum.

# Getting commands
| Command  |  Function  |
| --------- | --------- |
|  B0 01 B1 |  Status of relay 1 |
|  B0 02 B1 |  Status of relay 2 |
|  B0 03 B1 |  Status of relay 3 |
|  B0 04 B1 |  Status of relay 4 |
|  B0 05 B1 |  Status of functional mode |
|  B0 06 B1 |  Status of top end switch |
|  B0 07 B1 |  Status of bottom end switch |
|  0B R# S# CS |  Request for multiple status |

The last command can aquire the state of multiple relays and inputs, the structure of the command is:
- **0B**: identificator byte;
- **R#**: relays mask byte, if bit is set to 1, then the status of the respective relay will be returned;

| Bit 7 | Bit 6 | Bit 5 | Bit 4 | Bit 3 | Bit 2 | Bit 1 | Bit 0 |
| ----- | ----- | ----- | ----- | ----- | ----- | ----- | ----- |
| - | - | - | - | Relay 4 | Relay 3 | Relay 2 | Relay 1 |

- **S#**: switch (or inputs) mask byte, if bit is set to 1, then the status of the respective switch will be returned;

| Bit 7 | Bit 6 | Bit 5 | Bit 4 | Bit 3 | Bit 2 | Bit 1 | Bit 0 |
| ----- | ----- | ----- | ----- | ----- | ----- | ----- | ----- |
| - | - | - | - | - | Bottom switch | Top switch | Functional mode |

- **CS**: simple check sum, aquired by summing all the previous bytes os the message.

The response for the simple getting commands are the first 2 bytes from the request, plus a byte for the status 0x01 if opened or 0x00 for closed, plus 0x06 byte (ACK) and the new CheckSum.

The response to the multiple status request is:
- **0B**: identificator byte;
- **R#**: relays mask byte, if bit is set to 1, then the status of the respective relay will be returned;
- **S#**: switch (or inputs) mask byte, if bit is set to 1, then the status of the respective switch will be returned;
- **RS#**: relay status mask, if bit is set to 1 then relay is opened, if bit is set to 0 then relay is closed. The bit status for each relay respect the same position as the relay mask;
- **SS#**: switch (or input) status mask, if bit is set to 1 then input is low (or opened), if bit is set to 0 then input is high (or closed). The bit status for each switch respect the same position as the switch mask;
- **CS**: simple check sum, aquired by summing all the previous bytes os the message.

# Configuring Arduino IDE, installing drivers and packages

The ESP12F_Relay_X4 board uses a EP8266, in order to use it on Arduino IDE some extra packages need to be installed.
- Install Arduino IDE (https://www.arduino.cc/en/software)
- Add package for ESP8266:

Open Arduino IDE, then go to **File -> Preferences** and add the URL "http://arduino.esp8266.com/stable/package_esp8266com_index.json" in the field **Additional Boards Manager URLs**. Go to **Tools -> Board -> Boards Manager** and search for **ESP8266**, install the latest version.
- Install package for WiFi configuring:

Download the WiFiManager library (https://github.com/tzapu/WiFiManagerESPAsyncWebServer)) in ZIP file. After this, go to **Sketch -> Include library -> Add .zip library** and select it in the file manager.

**NOTE**
You may need to install the driver for your serial-USB conversor, please check with the manufacturer.
If you are using a FTDI chip, like FT232RT, you can download and install this driver (https://oemdrivers.com/usb-ft232r-usb-uart-driver).

# Steps to run the code

To flash the ESP8266, first you need to select the board in the IDE, to do this go to **Tools -> Board -> ESP8266 Boards** and select the **ESPino (ESP-12 Module)**.
Then you can flash the board, just remind that the ESP8266 needs to be in boot mode for this. In order to put the ESP in boot mode, you should conect the GPIO 0 to the GND and reset the board by pressing the reset buttom or disconecting and connection the power. After flashing the code, just disconect the GPIO 0 from the GND, and reset again. The board should run the code. :)

To flash, make these connections between the serial-USB conversor and the board:

| Serial-USB conversor | ESP12F_Relay_X4 board |
| --------- | --------- |
|  TX  |  RX  |
|  RX  |  TX  |
|  GND  |  GND  |

**PS:** if you are using the serial-USB conversor to power the board, then connect the +5V of the conversor in the +5V from the board. If you are using another source to power the board, leave the +5V of the conversor open.

# Making the board connections

The ESP12F_Relay_X4 has no previous connections with the mounted relays, so you can configure wich pin will be connected to each relay. In this code the following connections are used:
| Pin | Function |
| --------- | --------- |
|  GPIO 15  |  Output / Relay 1  |
|  GPIO 14  |  Output / Relay 2  |
|  GPIO 12  |  Output / Relay 3  |
|  GPIO 13  |  Output / Relay 4  |
|  GPIO 05  |  Input / Functional mode  |
|  GPIO 04  |  Input / Top end switch  |
|  GPIO 0x  |  Input / Bottom end switch  |
|  GPIO 16  |  Input / WiFi conf. mode  |

# Configuring the WiFi network

The program runs as a UDP server connected to a WLAN. To configure the WLAN, in wich the ESP should be connected, the WiFiManager library from tzapu is used (https://github.com/tzapu/WiFiManager). 
It configures the ESP as an access point with a WebServer page. In this page it's possible to scan for WLAN networks and configure it.
Connect to "ESP-NET", the password is "password". You can use your smartphone or notebook to connect and access the IP 192.168.4.1 through the web browser. A page will appear, where you can configure the SSID and the Password from the net you will connect your board.

Once you set the WLAN and password, the ESP saves the informations and connects automatically to the network.
Everytime the board is energized it will try to connect to it's previous network configured. If for some reason it's not able to connect, it will launch the access point mode again.
If you want to change the network, you can connect the GPIO 16 to GND, and power the board (or reset), this will erase the previous configuration of network connection. After this disconnect the pin from GND.

After you configured the network the ESP will connect to the WLAN and the UDP server should be up. Enabling you to change relays status and respond you with informations about the inputs.

In case of disconnection all the relays will be turned to OPEN, disabling possible outputs. The ESP will try to reconnect to the same WLAN. When it's reconnected it may have another IP. So the new IP will be printed on Serial.

