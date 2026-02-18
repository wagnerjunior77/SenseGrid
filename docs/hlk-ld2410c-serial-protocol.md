# HLK-LD2410C Human Presence Sensing Module Serial Communication Protocol

Version: V1.07  
Modify date: 2024-08-05  
Copyright: Shenzhen Hi-Link Electronic Co., Ltd.

---

Page 1 / 27  
Shenzhen Hi-Link Electronic Co., Ltd. HLK-LD2410C Human Presence Sensing Module Serial Communication Protocol

Page 2 / 27  
LD2410C

Contents
1. Communication interface introduction
   1.1 Pin definition
   1.2 Use and configuration
   1.2.1 Typical application circuits
   1.2.2 The role of configuration parameters
   1.2.3 Visual configuration tool description
2. Communication protocols
   2.1 Protocol format
   2.1.1 Protocol data format
   2.1.2 Command protocol frame format
   2.2 Send command with ACK
   2.2.1 Enabling configuration commands
   2.2.2 End configuration command
   2.2.3 Maximum distance gate and unoccupied duration parameters configuration command
   2.2.4 Read parameter command
   2.2.5 Enabling engineering mode command
   2.2.6 Close project mode command
   2.2.7 Distance gate sensitivity configuration command
   2.2.8 Read firmware version command
   2.2.9 Set serial port baud rate
   2.2.10 Restore factory settings
   2.2.11 Restart module
   2.2.12 Bluetooth settings
   2.2.13 Get mac address
   2.2.14 Obtaining bluetooth permissions
   2.2.15 Setting Bluetooth password
   2.2.16 Distance resolution setting
   2.2.17 Query distance resolution setting
   2.2.18 Auxiliary control function settings
   2.2.19 Query auxiliary control function configuration
   2.2.20 Start performing background noise detection and automatic sensitivity configuration
   2.2.21 Query the execution status of bottom noise detection
   2.3 Radar data output protocol
   2.3.1 Reported data frame format
   2.3.2 Target data composition
   2.4 Radar command configuration method
   2.4.1 Radar command configuration steps
3. Revision records
4. Technical support and contact information

---

Page 4 / 27  
Chart Index
Table 1 Pin definition table  
Table 2 Send command protocol frame format  
Table 3 Data format in the sending frame  
Table 4 ACK command protocol frame format  
Table 5 ACK intra-frame data format  
Table 6 Serial port baud rate selection  
Table 7 Factory default configuration values  
Table 8 Distance resolution selection  
Table 9 Command values for auxiliary control function settings  
Table 10 Reported data frame format  
Table 11 Intra-frame data frame format  
Table 12 Data type description  
Table 13 Target basic information data composition  
Table 14 Target state value description  
Table 15 Engineering model target data composition  
Figure 1 Module pin definition diagram  
Figure 2 Radar command configuration process

---

Page 5 / 27

## 1. Communication interface introduction

### 1.1 Pin definition

Pin | Symbol | Name | Function
--- | --- | --- | ---
1 | UART_Tx | UART Tx | UART Tx pin
2 | UART_Rx | UART Rx | UART Rx pin
3 | OUT | Target state output | Human presence detected: output high level. No human presence: output low level
4 | GND | Power Ground | Power Ground
5 | VCC | Power input | Power input 5V

Table 1 Pin definition table

---

Page 6 / 27

### 1.2 Use and configuration

#### 1.2.1 Typical application circuits
LD2410C module directly outputs the detected target state through an IO pin (someone high, no one low), and also outputs detection results through the serial port. The serial output data contains target state and distance auxiliary information. The module power supply voltage is 5V and the input power supply capacity is required to be greater than 200mA. The module IO output level is 3.3V. The default baud rate of the serial port is 256000, with 1 stop bit and no parity bit.

#### 1.2.2 The role of configuration parameters
Users can modify the configuration parameters through the serial port to adapt to different application requirements. The configurable radar detection parameters include:
- The farthest detection distance. Set in units of distance gates, maximum 8 gates and configurable distance resolution (0.2m or 0.75m per gate). Includes farthest distance gate for motion detection and for stationary detection, range 1 to 8. Example: farthest gate 2 and 0.75m resolution means only within 1.5m.
- Sensitivity. Target energy (0 to 100) must be greater than the sensitivity to be considered present. Sensitivity can be set 0 to 100. Each distance gate can be set independently. If a gate sensitivity is 100, targets under that gate can be effectively ignored.
- No-one duration. After occupied to unoccupied transition, radar keeps reporting occupied for a period. If no target during this period, then unoccupied is reported. If target appears, the period resets. Unit seconds. Equivalent to no-one delay time.

#### 1.2.3 Visual configuration tool description
A PC tool is provided to connect via serial port, read and configure parameters, and receive reported data with real-time visualization. Usage:
1. Connect module serial port using USB to serial tool.
2. Select serial port number, set baud rate 256000, select engineering mode, click connect.
3. After connection, click Start to show detection results and data.
4. To read/set parameters, stop (do not keep Start active).

---

Page 8 / 27

## 2. Communication protocols

The LD2410C communicates through TTL serial. Data output and parameter configuration use this protocol. Default baud rate 256000, 1 stop bit, no parity.

### 2.1 Protocol format

#### 2.1.1 Protocol data format
LD2410C uses little-end format for serial data communication. All data in the tables are in hexadecimal.

#### 2.1.2 Command protocol frame format

Table 2 Send command protocol frame format
Frame header | Intra-frame data length | Intra-frame data | End of frame
--- | --- | --- | ---
FD FC FB FA | 2 bytes | See Table 3 | 04 03 02 01

Table 3 Data format in the sending frame
Command word (2 bytes) + Command value (N bytes)

Table 4 ACK command protocol frame format
Frame header | Intra-frame data length | Intra-frame data | End of frame
--- | --- | --- | ---
FD FC FB FA | 2 bytes | See Table 5 | 04 03 02 01

Table 5 ACK intra-frame data format
Send command word + 0x0100 (2 bytes) + Return value (N bytes)

---

### 2.2 Send command with ACK

#### 2.2.1 Enabling configuration commands
Any other commands must be executed after this command, otherwise they are invalid.  
Command word: 0x00FF  
Command value: 0x0001  
Return value: 2 bytes ACK status (0 success, 1 failure) + 2 bytes protocol version (0x0001) + 2 bytes buffer size (0x0040)

Send data:  
FD FC FB FA 04 00 FF 00 01 00 04 03 02 01

Radar ACK (success):  
FD FC FB FA 08 00 FF 01 00 00 01 00 40 00 04 03 02 01

#### 2.2.2 End configuration command
Ends configuration mode. To issue commands again, send enable configuration first.  
Command word: 0x00FE  
Command value: None  
Return value: 2-byte ACK status (0 success, 1 failure)

Send data:  
FD FC FB FA 02 00 FE 00 04 03 02 01

Radar ACK (success):  
FD FC FB FA 04 00 FE 01 00 00 04 03 02 01

#### 2.2.3 Maximum distance gate and unoccupied duration parameters configuration command
Sets maximum distance gate for motion and stationary (range 2 to 8), and unmanned duration (0 to 65535 seconds).  
Command word: 0x0060  
Command value:  
2-byte maximum motion distance gate word + 4-byte maximum motion distance gate parameter +  
2-byte maximum standstill distance gate word + 4-byte maximum standstill distance gate parameter +  
2-byte unoccupied duration word + 4-byte unoccupied duration parameter

Parameter words:
Parameter name | Parameter word
--- | ---
Maximum movement distance door | 0x0000
Maximum resting distance door | 0x0001
No one duration | 0x0002

Send data example: maximum distance door 8 (motion and stationary), no one duration 5 seconds  
FD FC FB FA 14 00 60 00 00 00 08 00 00 00 01 00 08 00 00 00 02 00 05 00 00 00 04 03 02 01

Radar ACK (success):  
FD FC FB FA 04 00 60 01 00 00 04 03 02 01

#### 2.2.4 Read parameter command
Reads current configuration parameters.  
Command word: 0x0061  
Command value: None  
Return value: 2 bytes ACK status + 0xAA header + max distance gate N + configured max motion gate + configured max rest gate + motion sensitivities + stationary sensitivities + unoccupied duration (2 bytes)

Send data:  
FD FC FB FA 02 00 61 00 04 03 02 01

ACK example (success, max gate 8, motion 8, stationary 8, motion sens 20, stationary sens 25, unoccupied 5s):  
FD FC FB FA 1C 00 61 01 00 00 AA 08 08 08 14 14 14 14 14 14 14 14 14 19 19 19 19 19 19 19 19 19 05 00 04 03 02 01

#### 2.2.5 Enabling engineering mode command
Enables engineering mode. Adds each distance gate energy value to report data. Off by default after power on.  
Command word: 0x0062  
Command value: None  
Return value: 2-byte ACK status

Send data:  
FD FC FB FA 02 00 62 00 04 03 02 01

Radar ACK (success):  
FD FC FB FA 04 00 62 01 00 00 04 03 02 01

#### 2.2.6 Close project mode command
Disables engineering mode.  
Command word: 0x0063  
Command value: None  
Return value: 2-byte ACK status

Send data:  
FD FC FB FA 02 00 63 00 04 03 02 01

Radar ACK (success):  
FD FC FB FA 04 00 63 01 00 00 04 03 02 01

#### 2.2.7 Distance gate sensitivity configuration command
Configures distance gate sensitivity. Can set a specific gate or all gates with 0xFFFF.  
Command word: 0x0064  
Command value: 2-byte distance gate word + 4-byte distance gate value + 2-byte motion sensitivity word + 4-byte motion sensitivity value + 2-byte standstill sensitivity word + 4-byte standstill sensitivity value  
Return value: 2-byte ACK status

Parameter words:
Parameter name | Parameter word
--- | ---
Distance door | 0x0000
Movement sensitivity word | 0x0001
Static sensitivity word | 0x0002

Send data example: distance door 3 motion sensitivity 40, stationary sensitivity 40  
FD FC FB FA 14 00 64 00 00 00 03 00 00 00 01 00 28 00 00 00 02 00 28 00 00 00 04 03 02 01

Radar ACK (success):  
FD FC FB FA 04 00 64 01 00 00 04 03 02 01

Send data example: set all gates motion sensitivity 40, stationary sensitivity 40  
FD FC FB FA 14 00 64 00 00 00 FF FF 00 00 01 00 28 00 00 00 02 00 28 00 00 00 04 03 02 01

Radar ACK (success):  
FD FC FB FA 04 00 64 01 00 00 04 03 02 01

#### 2.2.8 Read firmware version command
Reads radar firmware version.  
Command word: 0x00A0  
Command value: None  
Return value: 2 bytes ACK status + 2 bytes firmware type (0x0001) + 2 bytes major version + 4 bytes minor version

Send data:  
FD FC FB FA 02 00 A0 00 04 03 02 01

Radar ACK (success):  
FD FC FB FA 0C 00 00 00 00 01 07 01 16 15 09 22 04 03 02 01  
Version number: V1.02.22062416

#### 2.2.9 Set serial port baud rate
Sets baud rate (takes effect after restart).  
Command word: 0x00A1  
Command value: 2-byte baud rate selection index  
Return value: 2-byte ACK status

Table 6 Serial port baud rate selection
Index | Baud rate
--- | ---
0x0001 | 9600
0x0002 | 19200
0x0003 | 38400
0x0004 | 57600
0x0005 | 115200
0x0006 | 230400
0x0007 | 256000
0x0008 | 460800

Factory default: 0x0007 (256000)

Send data:  
FD FC FB FA 04 00 A1 00 07 00 04 03 02 01

Radar ACK (success):  
FD FC FB FA 04 00 A1 01 00 00 04 03 02 01

#### 2.2.10 Restore factory settings
Restores configuration values to factory defaults (takes effect after reboot).  
Command word: 0x00A2  
Command value: None  
Return value: 2-byte ACK status

Send data:  
FD FC FB FA 02 00 A2 00 04 03 02 01

Radar ACK (success):  
FD FC FB FA 04 00 A2 01 00 00 04 03 02 01

#### 2.2.11 Restart module
Module restarts after ACK.  
Command word: 0x00A3  
Command value: None  
Return value: 2-byte ACK status

Send data:  
FD FC FB FA 02 00 A3 00 04 03 02 01

Radar ACK (success):  
FD FC FB FA 04 00 A3 01 00 00 04 03 02 01

Factory default configuration values (excerpt):
Configuration item | Default value
--- | ---
Maximum movement distance door | 8
Maximum resting distance door | 8
No one duration | 5
Serial port baud rate | 256000
Distance resolution | 0.75m

Motion sensitivity of distance gate 0: 50  
Motion sensitivity of distance gate 1: 50  
Motion sensitivity of distance gate 2: 40  
Motion sensitivity of distance gate 3: 30  
Motion sensitivity of distance gate 4: 20  
Motion sensitivity of distance gate 5: 15  
Motion sensitivity of distance gate 6: 15  
Motion sensitivity of distance gate 7: 15  
Motion sensitivity of distance gate 8: 15  
Static sensitivity of distance gate 2: 40  
Static sensitivity of distance gate 3: 40  
Static sensitivity of distance gate 4: 30  
Static sensitivity of distance gate 5: 30  
Static sensitivity of distance gate 6: 20  
Static sensitivity of distance gate 7: 20  
Static sensitivity of distance gate 8: 20

#### 2.2.12 Bluetooth settings
Controls Bluetooth on or off (takes effect after reboot).  
Command word: 0x00A4  
Command value: 0x0100 turn on, 0x0000 turn off  
Return value: 2-byte ACK status

Send data example (turn on):  
FD FC FB FA 04 00 A4 00 01 00 04 03 02 01

Radar ACK (success):  
FD FC FB FA 04 00 A4 01 00 00 04 03 02 01

#### 2.2.13 Get mac address
Queries MAC address.  
Command word: 0x00A5  
Command value: 0x0001  
Return value: 2-byte ACK status + 1 byte fixed type (0x00) + 3 bytes MAC address (big-end)

Send data:  
FD FC FB FA 04 00 A5 00 01 00 04 03 02 01

Radar ACK (success):  
FD FC FB FA 0A 00 A5 01 00 00 8F 27 2E B8 0F 65 04 03 02 01

#### 2.2.14 Obtaining bluetooth permissions
Gets Bluetooth permission (used by app).  
Command word: 0x00A8  
Command value: 6 bytes password (each 2 bytes little-end). Default "HiLink" = 0x4869 0x4C69 0x6E6B  
Return value: 2-byte ACK status  
Note: response only answers to Bluetooth, not serial port.

Send data:  
FD FC FB FA 08 00 A8 00 48 69 4C 69 6E 6B 48 69 04 03 02 01

Radar ACK (success):  
FD FC FB FA 04 00 A8 01 00 00 04 03 02 01

#### 2.2.15 Setting Bluetooth password
Sets Bluetooth control password.  
Command word: 0x00A9  
Command value: 6 bytes password (each byte little-end)  
Return value: 2-byte ACK status

Send data:  
FD FC FB FA 08 00 A9 00 48 69 4C 69 6E 6B 48 69 04 03 02 01

Radar ACK (success):  
FD FC FB FA 04 00 A9 01 00 00 04 03 02 01

#### 2.2.16 Distance resolution setting
Sets distance resolution (0.75m or 0.2m per gate), max 8 gates. Takes effect after restart.  
Command word: 0x00AA  
Command value: 2-byte distance resolution selection index  
Return value: 2-byte ACK status

Table 8 Distance resolution selection
Index | Distance resolution
--- | ---
0x0000 | 0.75m
0x0001 | 0.2m

Factory default: 0x0001 (0.75m)

Send data:  
FD FC FB FA 04 00 AA 00 01 00 04 03 02 01

Radar ACK (success):  
FD FC FB FA 04 00 A1 01 00 00 04 03 02 01

#### 2.2.17 Query distance resolution setting
Queries current distance resolution setting.  
Command word: 0x00AB  
Command value: None  
Return value: 2-byte ACK status + 2-byte distance resolution selection index

Send data:  
FD FC FB FA 02 00 AB 00 04 03 02 01

Radar ACK (success):  
FD FC FB FA 06 00 AB 01 00 00 01 00 04 03 02 01

#### 2.2.18 Auxiliary control function settings
Light sensing auxiliary control using photo diode. When enabled, OUT pin output depends on radar detection and light sensing logic.  
Command word: 0x00AD  
Command value: 4-byte configuration value  
Return value: 2-byte ACK status

Table 9 Command values for auxiliary control function settings
First byte | Meaning
--- | ---
0x00 | Disable light sensing auxiliary control, OUT not affected by light sensing
0x01 | Enable light sensing auxiliary control, condition met when light value < threshold
0x02 | Enable light sensing auxiliary control, condition met when light value > threshold

Second byte: light sensitivity threshold (0x00 to 0xFF), default 0x80

OUT pin default level configuration:
Third byte | Meaning
--- | ---
0x00 | OUT defaults low, low when no target, high when target
0x01 | OUT defaults high, high when no target, low when target

Default value: 0x00 (OUT defaults low)

Send data example: light value < threshold, threshold 0x60, OUT default low  
FD FC FB FA 06 00 AD 00 01 60 00 00 04 03 02 01

Radar ACK (success):  
FD FC FB FA 04 00 AD 01 00 00 04 03 02 01

#### 2.2.19 Query auxiliary control function configuration
Queries current auxiliary control configuration.  
Command word: 0x00AE  
Command value: None  
Return value: 2-byte ACK status + 4-byte configuration value

Send data:  
FD FC FB FA 02 00 AE 00 04 03 02 01

Radar ACK (success):  
FD FC FB FA 08 00 AE 01 00 00 01 60 01 00 04 03 02 01

#### 2.2.20 Start performing background noise detection and automatic sensitivity configuration
Module enters background noise detection, starts after 10 seconds. Ensure no one in range. Module records energy value per distance gate and sets sensitivities automatically. Target state values in reported data indicate current state.  
Command word: 0x000B  
Command value: 2-byte duration in seconds  
Return value: 2-byte ACK status

Send data:  
FD FC FB FA 04 00 0B 00 0A 00 04 03 02 01

Radar ACK (success):  
FD FC FB FA 04 00 0B 01 00 00 04 03 02 01

#### 2.2.21 Querying the execution status of bottom noise detection
Queries status of background noise detection.  
Command word: 0x001B  
Command value: None  
Return value: 2-byte ACK status + 2-byte status value

Status value:
0x0000 not in progress  
0x0001 in progress  
0x0002 detection completed

Send data:  
FD FC FB FA 02 00 1B 00 04 03 02 01

Radar ACK (success):  
FD FC FB FA 06 00 1B 01 00 00 01 00 04 03 02 01

---

## 2.3 Radar data output protocol

LD2410C outputs detection results through serial port. Default output is basic target information: target status, motion energy, stationary energy, motion distance, stationary distance, etc. Engineering mode adds each distance gate energy value. Radar data is output in defined frame format.

### 2.3.1 Reported data frame format

Table 10 Reported data frame format
Frame header | Length of data in frame | Intra-frame data | End of frame
--- | --- | --- | ---
F4 F3 F2 F1 | 2 bytes | See Table 11 | F8 F7 F6 F5

Table 11 Intra-frame data frame format
Data type | Head | Target data | Tail | Calibration
--- | --- | --- | --- | ---
1 byte (See Table 12) | 0xAA | See Table 13, Table 15 | 0x55 | 0x00

Table 12 Data type description
Data type value | Description
--- | ---
0x01 | Engineering mode data
0x02 | Target basic information data

### 2.3.2 Target data composition

Table 13 Target basic information data composition
Field | Size
--- | ---
Target status (see Table 14) | 1 byte
Movement target distance (cm) | 2 bytes
Exercise target energy value | 1 byte
Distance to stationary target (cm) | 2 bytes
Stationary target energy value | 1 byte
Detection distance (cm) | 2 bytes

Table 14 Target state value description
Value | Description
--- | ---
0x00 | No target
0x01 | Campaign target
0x02 | Stationary target
0x03 | Campaign and stationary target
0x04 | Under background noise detection (only valid during detection)
0x05 | Bottom noise detection successful (only valid during detection)
0x06 | Bottom noise detection failed (only valid during detection)

Table 15 Engineering model target data composition
Adds after basic info:
Maximum movement distance door N  
Maximum resting distance door N  
Movement distance gate 0 energy value ... gate N energy value  
Stationary distance gate 0 energy value ... gate N energy value  
Retain data, store additional information

Example data reported in normal operating mode:
F4 F3 F2 F1 0D 00 02 AA 02 51 00 00 00 00 3B 00 00 55 00 F8 F7 F6 F5

Example data reported in engineering mode:
F4 F3 F2 F1 23 00 01 AA 03 1E 00 3C 00 00 39 00 00 08 08 3C 22 05 03 03 04 03 06 05 00 00 39 10 13 06 06 08 04 03 05 55 00 F8 F7 F6 F5

Example analysis (engineering mode):
F4 F3 F2 F1 frame head  
23 00 frame data length  
01 engineering mode  
AA frame data header  
03 target state value  
1E 00 moving target distance 30 cm  
3C moving target energy value  
00 00 static target distance  
39 static target energy value  
00 00 detection distance  
08 08 maximum motion distance door, maximum static distance door  
3C 22 05 03 03 04 03 06 05 motion distance door energy values (9)  
00 00 39 10 13 06 06 08 04 03 05 static distance door energy values (9)  
60 photosensitive  
01 out status  
55 00 tail verification  
F8 F7 F6 F5 end of frame

---

## 2.4 Radar command configuration method

### 2.4.1 Radar command configuration steps
Command execution has two parts: host sends command and radar replies with ACK. If no ACK or ACK fails, command failed. Before sending any command, send enable configuration; after each sequence, send end configuration. Example for reading parameters: enable configuration -> read parameters -> end configuration.

---

## 3. Revision records

Date | Version | Modify the content
--- | --- | ---
2022-06-24 | 1.01 | Initial version
2022-07-01 | 1.02 | Fix some incorrect descriptions, add restart and factory reset commands
2022-07-19 | 1.03 | Correct the length values of some command instances
2022-08-26 | 1.04 | Add instructions for configuring commands to increase distance resolution
2022-09-20 | 1.05 | Add protocols for Bluetooth
2023-02-21 | 1.06 | Add instructions for outputting light sensitivity values and commands for setting auxiliary control functions
2024-08-05 | 1.07 | Command instructions for adding background noise detection and sensitivity automatic configuration related functions
2024-11-22 | 1.08 | Modify some instruction reply errors and add engineering mode data parsing

---

## 4. Technical support and contact information

Shenzhen Hi-Link Electronic Co., Ltd.  
Email: sales@hlktech.com  
Website: www.hlktech.net
