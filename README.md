# ESP12F_Relay_X4_UDP_Controlling
This project provides a code for ESP12F_Relay_X4 board. It enables the relays to be controlled by UDP commands.

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

# Drivers and stept to run the code:

The ESP12F_Relay_X4 board uses a EP8266, in order to use it on Arduino IDE some extra packages need to be installed.
- Install Arduino IDE (https://www.arduino.cc/en/software)
- Add package for ESP8266:
Open Arduino IDE, then go to **File -> Preferences** and add the URL "http://arduino.esp8266.com/stable/package_esp8266com_index.json" in the field Additional Boards Manager URLs.
Go to **Tools -> Board -> Boards Manager** and search for **ESP8266**, install the latest version.
- Install packages for WebServer and Access Point mode:
Download the ESPAsyncWebServer (https://github.com/me-no-dev/ESPAsyncWebServer) and the ESPAsyncTCP (https://github.com/me-no-dev/ESPAsyncTCP) libraries in ZIP files. After this, go to **Sketch -> Include library -> Add .zip library** and select them in the file manager.

**NOTE**
You may need to install the driver for your serial-USB conversor, please check with the manufacturer.