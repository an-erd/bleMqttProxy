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

## Build Process

### Prerequisites

### Updating to a specific ESP-IDF version

Use git checkout version to checkout a specific version.

Update submodules: `git submodule update --recursive`

Call `~/esp-idf/install.sh` to get tools downloaded and installed

#### menuconfig

Start with calling `menuconfig` to make the basic configuration

Important: under MQTT you can set MQTT to 0 to disable or to 1 to enable MQTT messages, which will be put in the database by the script afterwards.

#### Using the correct Python version

According to the ESP-IDF documentation, currently only Phython 2.7 is supported. So, if on your systems python3 is the default Python, you need to implement
the steps as shown in [Build System CMake - phython](https://docs.espressif.com/projects/esp-idf/en/stable/api-guides/build-system-cmake.html).

### Flashing the app

#### Step 1 (optional): Erase all ESP32 flash memory if necessary

`make erase_flash`


#### Step 2: Flash the Partition, Application, ... (TBD)

`make flash`

#### Step 3: Use Monitor

Use `make monitor` to get the console messages to check that everything is fine.

### OTA Webserver setup

On a Rapsberry Pi start an server using with the binary file `blemqttproxy.bin` and the certificates in the same directory.

```
openssl s_server -WWW -key ca_key.pem -cert ca_cert.pem -port 8070 .
```

To run in the background use 

```
pi@raspberrypi:~/ota_server $ nohup openssl s_server -WWW -key ca_key.pem -cert ca_cert.pem -port 8070 > s_server.log 2> s_server.err &
```

Using the openssl command on a MSYS32 system I experienced that the download was not completed. Other user report similar issues, thus the alternative on a Raspberry Pi is used.

## Logfile

### Beacons

#### ble_beacon v3

D (10444) BLEMQTTPROXY: 0x3ffba598   02 01 04 1a ff 59 00 02  15 01 12 23 34 00 07 00  |.....Y.....#4...|
D (10454) BLEMQTTPROXY: 0x3ffba5a8   02 c3 5f c9 62 bd 56 3d  bc fe 3c 09 0b ac        |.._.b.V=..<...|

    Flags:      02 01 04
    Length:     1A
    Type:       FF
    Comp.Id:    59 00
    Beac.Type:  02 15
    ...         01 12 23 34 00 07 00 02
    ...         c3
    Temperature 5f c9
    Humidity    62 bd
    X           56 3d
    Y           bc fe
    Z           3c 09
    Battery     0b ac



#### ble_beacon v4

D (35714) BLEMQTTPROXY: 0x3ffba594   02 01 06 13 ff 59 00 00  07 00 06 5f e9 68 f9 c2  |.....Y....._.h..|
D (35724) BLEMQTTPROXY: 0x3ffba5a4   00 d2 00 ba 40 0b ca                              |....@..|

    Flags:      02 01 06
    Length:     13
    Type:       FF
    Comp.Id:    59 00
    Beac.Type:  00 [07]     (for the sake of simplicity just take the 07 as Beac.Type, aargh)
    ...         00 06
    Temperature 5f e9
    Humidity    68 f9
    X           c2 00
    Y           d2 00
    Z           ba 40
    Battery     0b ca

