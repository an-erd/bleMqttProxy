# bleMqttProxy

A Proxy or Gateway service for BLE beacons or local attached sensors to a configurable MQTT broker. The code is designed for an ESP32 board with OLED display. (It can be purchased using search for "Wemos Lolin ESP32 OLED Module"). Beside an USB connector there are two buttons (reset and a usable button). The display is based on the SSD1306 controller and has a resolution of 128x64 pixesls.


## Screens

1. empty screen
2. beacon details screen (one for each announced beacon)
3. app version information screen
4. last seen screen
5. local sensor screen (one for each attached sensor)
6. optional (to be compiled): display test screen

## Button and BLE beacon denouncement functionality

Since there is only one button available, the usability is restricted. Thus, we define the following functions depending on the screen shown.

**1. Empty screen, n=0**
- **push** moves to next screen
- **long push** ...

**2. beac information screens**

- **push** moves to next screen
- **long push** remove (=denounce) current beacon from active beacons after confirmation (with a **push** in between 2 secs)

**3. app version information screen**
- **push** moves to next screen
- **long push** ...

**4. last seen screen**
- **push** moves to next screen
- **long push** ...

**5. local sensor screen**
- **push** moves to next screen
- **long push** ...

**at the end** of the list start over with **1.** again


## BLE beacon announcement functionality

To add a not yet active beacon to this proxy **move the beacon close to the proxy or vice versa**.
After a confirmation with a **push** in between 2 secs the device will be added. Only configured beacons can be made active.
The current configuration of active beacons will be stored in NVS.
