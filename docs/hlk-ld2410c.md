# HLK-LD2410C Human Presence Motion Module User Manual

Version: V1.00  
Revised date: 2022-11-07  
Copyright: Hi-Link Electronic Co., Ltd.

---

Page 1 / 19  
Shenzhen Hi-Link Electronic Co., Ltd. HLK-LD2410C Human Presence Motion Module User Manual

Page 2 / 19  
HLK-LD2410C

Content
1. Product introduction
2. Product features and benefits
   2.1 Features
   2.2 Solution advantage
3. Application scenarios
4. Hardware description
   4.1 Dimensions
   4.2 Pin definition
5. Use and configuration
   5.1 Typical application circuit
   5.2 The role of configuration parameters
   5.3 Visual configuration tool description
   5.4 Mounting method and sensing range
   5.5 Installation conditions
6. Bluetooth instructions
   6.1 Install software
   6.2 Instructions
   6.3 Bluetooth password
   6.4 OTA upgrade
   6.5 Bluetooth communication protocol
   6.6 Turn on bluetooth again
7. Performance and electrical parameters
8. Radome design guidelines
   8.1 Effects of radomes on mm wave sensor performance
   8.2 Radome design principles
   8.3 Common materials
9. Revision records
10. Technical support and contact

---

Page 3 / 19  
HLK-LD2410C

## 1. Product Introduction

LD2410C is a high-sensitivity 24GHz human presence status sensing module developed by Hi-Link Electronics. Its working principle is to use FMCW frequency-modulated continuous wave to detect human targets in the set space. Combined with radar signal processing and accurate human body sensing algorithms, it realizes high-sensitivity human presence status sensing, and can identify human bodies in motion and stationary states. And auxiliary information such as the distance of the target can be calculated.

This product is mainly used in indoor scenes to sense whether there is a moving or micro-moving human body in the area, and output the detection results in real time. The farthest sensing distance can reach 5 meters, and the distance resolution is 0.75m. Provides a visual configuration tool, which can easily configure the sensing distance range, sensing sensitivity in different intervals and unmanned delay time, etc., to adapt to different specific application needs. Support GPIO and UART output, plug and play, and can be flexibly applied to different smart scenarios and terminal products.

Figure 1 Diagram of usage

---

Page 4 / 19  
HLK-LD2410C

## 2. Product features and benefits

### 2.1 Features
- Plug and play, easy assembly
- The longest sensing distance is up to 5 meters
- Large detection angle, coverage up to +/-60 degrees
- Accurate identification within the interval, support the division of the sensing range, and shield the interference outside the interval
- Multi-level intelligent parameter adjustment can be realized through Bluetooth or serial port to meet the needs of scene changes
- Visual debugging and configuration tools
- Small and simple, the minimum size is only 16mm x 22mm
- Supports various installation methods such as ceiling hanging and wall hanging
- 24GHz ISM band, can be certified by FCC and CE spectrum regulations
- The ultimate cost-effective choice

### 2.2 Solution advantage
The LD2410C human body sensing module adopts 24GHz millimeter wave radar sensor technology. Compared with other solutions, it has obvious advantages in human body sensing applications:
1. In addition to being sensitive to moving human bodies, it can also sensitively sense static, micro-moving, sitting and lying human bodies that cannot be identified by traditional solutions.
2. It has good environmental adaptability, and the sensing effect is not affected by the surrounding environment such as temperature, brightness, humidity and light fluctuations.
3. It has good shell penetration and can be hidden in the shell to work without opening holes on the surface of the product, which improves the aesthetics of the product.
4. It can flexibly configure the farthest sensing distance and the sensitivity on each distance door to achieve flexible and fine personalized configuration.
5. With the Bluetooth function, you can directly use the APP to debug the radar parameters without catching the serial port.

Figure 2 Comparison of millimeter wave radar scheme and other schemes

---

Page 5 / 19  
HLK-LD2410C

## 3. Application scenarios

The LD2410C human body sensing module can detect and identify the human body in motion, fretting, standing, sitting and lying down. It supports multi-level parameter adjustment and can be widely used in various AIoT scenarios. The common types are as follows:
- Human body sensor light control  
  It senses whether there is someone in the space, and automatically controls lights, such as lighting equipment in public places, various sensor lights, bulb lights, etc.
- Human body induction wake-up of advertising screen and other equipment  
  Automatically turn on when people come, and automatically sleep when no one comes to save power, information delivery is more accurate and efficient.
- Life safety protection  
  UV lamp work protection, to prevent the UV lamp from being turned on when there are people around and causing personal injury;  
  Automatic detection and alarm of dangerous places to prevent people from entering specific high risk spaces, such as high-risk places entered by personnel from coal mine blasting.
- Smart home appliances  
  When there is no one in the room for a long time, the TV, air conditioner and other electrical appliances are automatically turned off, saving energy and safety.
- Intelligent security  
  Detection and identification of people intruding, staying, etc. within the specified range.

Figure 3 Application Scenario

---

Page 6 / 19  
HLK-LD2410C

## 4. Hardware description

### 4.1 Dimensions
Figure 4 Module Real Image

---

Page 7 / 19  
HLK-LD2410C

Figure 5 Module Dimensions

Module size: 16mm x 22mm, 5 pin holes are reserved in the hardware (the factory default does not match the pins).  
The pin hole diameter is 0.9mm, and the pin spacing is 2.54mm.

### 4.2 Pin definition
Figure 6 Module pin definition diagram

Pin | Symbol | Items | Function
--- | --- | --- | ---
1 | UART_Tx | Serial Tx | Serial Tx pin
2 | UART_Rx | Serial Rx | Serial Rx pin
3 | OUT | Target status output | Human presence detected: output high level. No human presence: output low level
4 | GND | Power ground | Power ground
5 | VCC | Power Input | Power input 5V to 12V (advise 5V)

Table 1 Pin Definition Table

---

Page 8 / 19  
HLK-LD2410C

## 5. Use and configuration

### 5.1 Typical application circuit
The LD2410C module directly outputs the detected target state through an IO pin (someone is high, no one is low), and it can also output the detection result data through the serial port according to the specified protocol. The serial port output data includes: Target status and distance auxiliary information, etc., users can use it flexibly according to specific application scenarios.

The power supply voltage of the module is 5V, and the power supply capacity of the input power supply is required to be greater than 200mA. The module IO output level is 3.3V. The default baud rate of the serial port is 256000, 1 stop bit, and no parity bit.

### 5.2 The role of configuration parameters
The user can modify the configuration parameters of the module through the serial port of the LD2410C to adapt to different application requirements, and the configuration content will not be lost when the power is turned off. The configurable parameters include the following:
- Farthest detection distance  
  Set the farthest detectable distance, only human targets that appear within this farthest distance will be detected and output the result. Set in units of distance gates, and each distance gate is 0.75m. Including the farthest door for motion detection and the farthest door for static detection, the setting range is 1 to 8. For example, if the farthest door is set to 2, only if there is a human body within 1.5m will it effectively detect and output the result.
- Sensitivity  
  Only when the detected target energy value (range 0 to 100) is greater than the sensitivity value will it be determined that the target exists, otherwise it will be ignored. The sensitivity value can be set from 0 to 100. The sensitivity of each range gate can be independently set, so that the detection in different distance ranges can be precisely adjusted, local accurate detection or filtering of interference sources in specific areas. In addition, if the sensitivity of a certain distance gate is set to 100, the effect of not recognizing the target under the distance gate can be achieved. For example, if the sensitivity of distance gate 3 and distance gate 4 is set to 20, and the sensitivity of other distance gates is set to 100, it is possible to detect only the human body within the range of 2.25m to 3.75m from the distance module.
- No-one duration  
  When the radar outputs the result from man to no man, it will report man for a period of time. If there is no man in the radar test range during this time period, the radar will report no man; if the radar detects man during this time period, it will be refreshed again. This time, in seconds. It is equivalent to the unmanned delay time. After the person leaves, the output state will be unmanned only after the person has left the system for more than this duration.

---

Page 9 / 19  
HLK-LD2410C

### 5.3 Visual configuration tool description
In order to facilitate the user to test and configure the module quickly and efficiently, a PC configuration tool is provided. The user can use this tool software to connect the serial port of the module, read and configure the parameters of the module, and receive the detection results reported by the module data, and real-time visual display, which greatly facilitates the use of users.

How to use the host computer tool:
1. Use the USB to serial port tool to connect the module serial port correctly.
2. Select the corresponding serial port number in the host computer tool, set the baud rate to 256000, select the engineering mode, and click to connect the device.
3. After the connection is successful, click the Start button, and the graphical interface on the right will display the test results and data.
4. After connecting, when the start button is not clicked, or click stop after starting, the mode parameter information can be read or set.

Note: The parameters cannot be read and configured after clicking start, and configuration can only be performed after stopping.

The interface and common functions of the host computer tool are as follows:

---

Page 10 / 19  
HLK-LD2410C

### 5.4 Mounting method and sensing range
Figure 7 Schematic diagram of ceiling-mounted installation (distance unit: meters, angle unit: degrees)
Figure 8 Schematic diagram of the detection range (the ceiling height is 3 meters)

Radar Ceiling  
High 2.6 to 3m

---

Page 11 / 19  
HLK-LD2410C

Radar Wall  
5m  
High 1.5 to 2m  
(distance unit: meters, angle unit: degrees)

Figure 9 Schematic diagram of wall-mounted installation  
Figure 10 Schematic diagram of the detection range (the height of the wall is 1.5 meters)

---

Page 12 / 19  
HLK-LD2410C

### 5.5 Installation conditions
- Confirm the minimum installation clearance  
  If the radar needs to be installed with a casing, the casing must have good wave-transmitting properties at 24GHz, and cannot contain metal materials or materials that have a shielding effect on electromagnetic waves.
- Installation environment requirements  
  This product needs to be installed in a suitable environment. If it is used in the following environments, the detection effect will be affected:
  - There are non-human objects that are continuously moving in the sensing area, such as animals, continuously swinging curtains, large green plants facing the air outlet, etc.
  - There is a large area of strong reflectors in the sensing area, and the strong reflectors will cause interference to the radar antenna.
  - When installing on the wall, external interference factors such as air conditioners and electric fans on the top of the room need to be considered.
- Precautions during installation
  - Try to ensure that the radar antenna is facing the area to be detected, and the surrounding area of the antenna is open and unobstructed.
  - To ensure that the installation position of the sensor is firm and stable, the shaking of the radar itself will affect the detection effect.
  - To ensure there is no movement or vibration on the back of the radar. Due to the penetrating nature of radar waves, the back lobe of the antenna signal may detect moving objects behind the radar. A metal shield or metal backplane can be used to shield the radar back lobe and reduce the impact of objects on the back of the radar.
  - The theoretical distance accuracy of radar is the result obtained through special algorithm processing on the basis of the physical resolution of 0.75 meters. Due to the difference in the size, state, and RCS of the target, the target distance accuracy will fluctuate; at the same time, the longest distance will also fluctuate slightly.

---

Page 13 / 19  
HLK-LD2410C

## 6. Bluetooth instructions

### 6.1 Install software
Currently the APP supports Android and IOS platforms, you can download it from this link:  
https://www.pgyer.com/Lq8p (Android)  
You can also go to major app stores to search for "HLKRadarTools" and install it.

### 6.2 Instructions
Open the app, and the app searches for nearby radar devices. The broadcast name of the device is "HLK-LD2410B_xxxx" (xxxx is the last four digits of the mac address). After the module is successfully connected, you can view the radar information, or debug and save the parameters. The use distance of the APP should not exceed the Bluetooth signal range (within 4 meters).

1. Search for Bluetooth
2. View parameters
3. Modify radar parameters

The process of modifying the radar parameters of the Bluetooth APP is the same as that of the PC host computer tool.

### 6.3 Bluetooth password
You must enter a password to control the APP for the first connection. The default password is HiLink, which can be modified in Parameter Settings -> Control Password. The password is fixed at 6 bytes.

Note: Only V1.07.22091516 or newer version supports password function.

---

Page 14 / 19  
HLK-LD2410C

### 6.4 OTA upgrade
When the firmware of the device has been updated, the word "upgradeable" will appear on the firmware version, long press the version number to enter the upgrade interface; only or newer versions support the upgrade.

Long press the red circle to enter the upgrade. Enter OTA upgrade.

---

Page 15 / 19  
HLK-LD2410C

During Upgrading

The overall upgrade time takes 1 to 3 minutes. The upgrade must be performed from the module, otherwise the upgrade will fail if the Bluetooth signal is poor. Do not power off or restart the module before the upgrade is completed, and do not forcibly exit the APP, otherwise the upgrade will fail. If the upgrade fails, the 2410C's radar program will be disabled and radar detection will not be possible.

If the device upgrade fails, please restart the device and reconnect the APP, and a "waiting for upgrade" prompt will appear on the device list:

Waiting for upgrading  
Upgraded successfully

Click the device to be upgraded to re-upgrade, and the radar function can be restored only after the upgrade is successful.

---

Page 16 / 19  
HLK-LD2410C

### 6.5 Bluetooth communication protocol
2410C acts as a slave side, only allowed to be connected by one master.

Feature UUID | Operation authority | Function definition
--- | --- | ---
0000fff1-0000-1000-8000-00805f9b34fb | Read/Notify | Module send, APP receive
0000fff2-0000-1000-8000-00805f9b34fb | Write Without Response | APP send, module receive

When the app and 2410C Bluetooth connection and password verification are successful, the module will start the transparent transmission of radar data. The data transmitted by Bluetooth is exactly the same as the serial port protocol, please refer to the "LD2410C Serial Port Communication Protocol.pdf" document.

If the App is successfully connected, it will send a Bluetooth password to the module for verification. Only when the password is correct, the module will start to transparently transmit data. For details, see the chapter Obtaining Bluetooth Permissions in "LD2410C Serial Communication Protocol.pdf".

### 6.6 Turn on bluetooth again
The Bluetooth function of LD2410C is enabled by default, and Bluetooth can be turned off or turned on through the serial port protocol (see LD2410C serial port communication protocol.pdf). If the bluetooth has been turned off, or the serial port cannot be used, the bluetooth can be turned on again after the module is powered off and then powered on for more than 5 times within 2 to 3s.

---

Page 17 / 19  
HLK-LD2410C

## 7. Performance and electrical parameters

Operating frequency: 24GHz to 24.25GHz  
Compliant with FCC, CE, non-commission certification standards  
Operating Voltage: DC 5V, power supply capacity >200mA  
Average operating current: 79 mA  
Modulation: FMCW  
Interface: A GPIO, IO level 3.3V. A UART  
Target application: Human presence sensor  
Detection distance: 0.75m to 6m, adjustable  
Detection angle: +/-60 deg  
Distance resolution: 0.75m  
Sweep Bandwidth: 250MHz  
Compliant with FCC, CE, non-commission certification standards  
Ambient temperature: -40 to 85C  
Dimensions: 7mm x 35mm

Table 2 Performance and electrical parameters table  
Figure 11 Measured data of module working current

---

Page 18 / 19  
HLK-LD2410C

## 8. Radome design guidelines

### 8.1 Effects of radomes on mm wave sensor performance
- Radar waves are reflected on the radome boundary
- Losses in total radar radiated or received power
- The reflected wave enters the receiving channel, affecting the isolation between the transmitting and receiving channels
- Reflections may degrade the standing wave of the antenna, further affecting the antenna gain
- Radar waves will suffer loss when propagated in the medium. In theory, the higher the frequency, the greater the loss will be
- Electromagnetic waves undergo a certain degree of refraction as they pass through a medium
- Affects the antenna's radiation pattern, which in turn affects the sensor's coverage

### 8.2 Radome design principles

### 8.3 Common materials
- Understand the material and electrical characteristics of the radome before designing
- The table on the right is for reference only, the actual value should be confirmed with the supplier
- Height H from the antenna to the inner surface of the radome
- If there is enough space, it is preferred to recommend 1 times or 1.5 times the wavelength
- For example, 12.4 or 18.6mm is recommended for 24.125GHz
- Error control: +/-1.2mm
- Radome thickness D
- Recommended half wavelength, error control +/-20%
- If the thickness requirement of half wavelength cannot be met
- It is recommended to use low materials
- Thickness recommended 1/8 wavelength or thinner
- Influence of heterogeneous materials or multi-layer composite materials on radar performance, it is recommended to make experimental adjustments during design

Table 3 Common Material Properties of Radomes

---

Page 19 / 19  
HLK-LD2410C

## 9. Revision records

Date | Version | Modify the content
--- | --- | ---
2022-11-7 | 1.00 | Initial version

## 10. Technical support and contact

Shenzhen Hi-Link Electronic Co., Ltd.  
Phone: 0755-23152658 / 83575155  
Website: www.hlktech.net
