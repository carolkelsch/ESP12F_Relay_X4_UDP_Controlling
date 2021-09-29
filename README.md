# ESP12F_Relay_X4_UDP_Controlling
This project provides a code for ESP12F_Relay_X4 board. It enables the relays to be controlled by UDP commands.

The ESP will connect to a WiFi network, wich can be configured by accessing the ESP when it's in Access Point mode. After connected to the WLAN, the ESP can receive UDP packages with commands to controll the relays and read digital inputs states.

+-----------------------------------------------+
|                Setting commands               |
+---------------+-------------------------------+
| Command (HEX) |            Function           |
+---------------+-------------------------------+
|  A0 01 01 A2  | Open Relay 1                  |
+---------------+-------------------------------+
|  A0 01 00 A1  | Close Relay 1                 |
+---------------+-------------------------------+
|  A0 02 01 A2  | Open Relay 2                  |
+---------------+-------------------------------+
|  A0 02 00 A2  | Close Relay 2                 |
+---------------+-------------------------------+
|  A0 03 01 A4  | Open Relay 3                  |
+---------------+-------------------------------+
|  A0 03 00 A3  | Close Relay 3                 |
+---------------+-------------------------------+
|  A0 04 01 A5  | Open Relay 4                  |
+---------------+-------------------------------+
|  A0 04 00 A4  | Close Relay 4                 |
+---------------+-------------------------------+
|  0A R# C# CS  | Open or close multiple relays |
+---------------+-------------------------------+
