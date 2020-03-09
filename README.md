# A Bluetooth to MQTT Proxy (bleMqttProxy)

## Overview

**bleMqttProxy** is a software for ESP32 boards with display and a push button. It acts as a proxy or gateway service for BLE beacons to a configurable MQTT broker using a WiFi connection. Also, in addition or instead of the Beacons a local temperature sensor can be attached.

Using the push button you can change the information to be displayed (e.g. detailed page per Beacon, local sensors, statistics) or activate/deactivate the Beacons used with this Proxy.

The code is designed for an ESP32 board with OLED display. (It can be purchased using search for "Wemos Lolin ESP32 OLED Module"). Beside an USB connector there are two buttons (reset and a usable button). The display is based on the SSD1306 controller and has a resolution of 128x64 pixels.

### Provided Services

A web server is available providing status information of the Beacons (e.g., when the beacon was last seen, last MQTT message send, address) and of the proxy service (e.g., uptime, version). Also a reboot or start a OTA Update (over-the-air update) can be initiated.

The BLE beacons provide an offline buffer. Using web server commands a download to the Proxy can be triggered, which, if then available, can be downloads using a browser.

### Sensors to be attached

Currently the following sensor can be attached to the board using the OWB (one-wire bus). 

| Sensor  | Description                                        | Data Sheet                                  |
| ------- | -------------------------------------------------- | ------------------------------------------- |
| DS18B20 | Programmable Resolution 1-Wire Digital Thermometer | [DS18B20 Data Sheet](Documents/DS18B20.pdf) |

### Available Buttons on Board

There is one user defined tactile push button (connected to P0) and a reset button.

### Available Display on Board

On board is an OLED display with a resolution of 128x64 pixels attached to an SSD1306 controller attached using I2C.

| Controller | Description                                                  | Data Sheet                                                   | TWI Address |
| ---------- | ------------------------------------------------------------ | ------------------------------------------------------------ | ----------- |
| SSD1306    | 128 x 64 Dot Matrix OLED/PLED Segment/Common Driver with Controller | [SSD1306 Advanced Information](Documents/SSD1306%20Datasheet%20for%20096%20OLED.pdf) | 0x3C        |

### Example Data and Beacon used throughout this Document

In this Readme we use a single BLE beacon device with the following address `D7:59:9D:1D:7B:6B`. The device name for this Beacon is set to `Bx0708` and will be constructed using its major and minor id which will be set to: Major ID `0x07` and Minor ID `0x08`, preceded by `Bx` (as in Beacon).

The device IP address is set to `192.168.2.156`.

## Programming the Device

### Installing ESP-IDF

If not already done, install Epressif ESP-IDF, see [ESP-IDF Get Started](https://docs.espressif.com/projects/esp-idf/en/stable/get-started/).

### Updating to a specific ESP-IDF version

Use `git checkout version` to checkout a specific version. Afterwards, if necessary, update the submodules to the correct commit

```
git submodule update --recursive
```

If necessary, use the following command to get tools downloaded and installed

```
~/esp-idf/install.sh 
```

#### Configure using menuconfig

Start with calling `menuconfig` to make the specific bleMqttProxy configuration and the ESP-IDF basic configuration.

**Important**: For example, MQTT you can set MQTT to 0 to disable or to 1 to enable MQTT messages, which will be put in the database by the script afterwards.

#### Using the correct Python version

According to the ESP-IDF 3.3.1 documentation, currently only Python 2.7 is supported. So, if on your system Python 3 is the default Python version, you need to implement the steps as shown in [Build System CMake - phython](https://docs.espressif.com/projects/esp-idf/en/stable/api-guides/build-system-cmake.html).

### Flashing the App

#### Step 1 (optional): Erase all ESP32 flash memory if necessary

```
make erase_flash
```


#### Step 2: Make and Flash the Partition Table and Application

`make partition_table-flash`: will flash the partition table with esptool.py.

`make flash`: Will flash everything including the partition table.

`make flash-app`: Will build the application and flash everything including the partition table.

#### Step 3: Use Monitor

Use `make monitor` to get the console messages to check that everything is fine, alternatively use `putty` (on the appropriate COM port and speed as defined using `menuconfig`).

### Update the App using OTA

You can use OTA (over-the-air updates) to update the app. Use `menuconfig` to configure an secure web server and path/file name, and put the application image created during application build on this server.

An introduction is given by Espressif [here](https://github.com/espressif/esp-idf/tree/master/examples/system/ota), in particular creating the certificate and running the https server. But, I experienced problems using Windows 10 with `openssl s_server` with not completed downloads of the firmware to flash, thus moved to a Raspberry Pi server. 

This command line creates the certificate:

```
openssl req -x509 -newkey rsa:2048 -keyout ca_key.pem -out ca_cert.pem -days 365 -nodes
```

and this command line starts the server:

```
openssl s_server -WWW -key ca_key.pem -cert ca_cert.pem -port 8070
```



## Functionalities



## Usage

### Device Usage using Push Button

There is one tactile push button available for application usage (in addition to the reset button). This button can be used to switch between the different screens (e.g., showing detailed BLE Beacon information, device information, status and version screen). Since there's only one button available, short and long push is used in a context sensitive manner.

#### Different Screens and button actions

This section gives an overview on the different screens and the respective button actions.

| Screen            | Number of pages                                              | Description                                                  | Button action (short push)                                   | Button action (long push) |
| ----------------- | ------------------------------------------------------------ | ------------------------------------------------------------ | ------------------------------------------------------------ | ------------------------- |
| Splash            | one                                                          | The splash screen is shown for a limited time directly after startup | n/a                                                          | n/a                       |
| Empty             | one                                                          | The empty screen can be used as a "screen saver". After a limited time, the display is shut down. Exceptions occur for... | Turn display on, show last screen shown again.               | n/a                       |
| Beacon details    | Multiple, one page per  configured beacon                    | Information screen with detailed information, see details below. | Move to next **"Beacon details"** screen or **"last seen"** screen | toggle active/inactive    |
| Last seen         | Multiple, each page shows a list with max. 5 entries         | Shows when the beacon was last seen and when the MQTT message was last send. | Move to next **"Last seen"** screen or **"Local temperature"** screen | n/a                       |
| Local temperature | Multiple, one page per configured local temperature sensor, or none. | Shows information on the temperature from local temperature sensor. <br />**NOT IMPLEMENTED YET!** | Move to next **"App version"** screen                        | n/a                       |
| App version       | one                                                          | Information on the application and IDF version, project name, IP address, active beacon mask. | Move to next **"Statistics"** screen                         | n/a                       |
| Statistics        | one                                                          | Information on uptime, WiFi/MQTT status, WiFi connections/failures, MQTT messages send/failed. | Move to next **"Beacon details"** screen again               | Clear statistics values   |

##### Screen "Splash"

##### Screen "Beacon details"

##### Screen "Last seen"

##### Screen "Local temperature"

##### Screen "App version"

##### Screen "Statistics"



### Device Usage using Web Interface

You can reach the Web Interface using the following address in the browser, with the device's IP address `192.168.2.156`:

```
http://192.168.2.156/csv?cmd=list
```

The following figure shows a screenshot of the Web Interface:

**TDB Figure**

The Web Interface is intended for getting the status of the beacons configured for the device, to initiate commands, and status and commands of the proxy device. 

#### Beacon List of BLE Beacon devices 

The beacon list shows a table of the configured beacons, with information about name, address, some status information and commands.

##### Information and Commands per BLE Beacon

| Column              | Description                                                  | Example                                 |
| ------------------- | ------------------------------------------------------------ | --------------------------------------- |
| Name                | Name of the BLE Beacon device, build using BLE Beacons major and minor id, MAJ and MIN, set as "Bx_MAJ_MIN". | Bx0708                                  |
| Address             | Bluetooth device address                                     | D7:59:9D:1D:7B:6B                       |
| Last seen (sec ago) | The time gone since the device was last seen, or, if the beacon is not configured as active: "inactive" | 00:01:35 (for 1 Minute and 35 secs ago) |
| MQTT send (sec ago) | The time gone since the last MQTT message was send (or could be send). | 00:01:35                                |
| Status              | The status of the offline buffer download from beacon. See the list "Status" below. | Download in progress                    |
| Command             | The available commands regarding offline buffer download. See the list "Command" below. | Cancel                                  |
| Download file       | If the offline buffer download file is available, the link is shown, else its empty. | Download                                |

###### Status

| Status                  | Description                                                  |
| ----------------------- | ------------------------------------------------------------ |
| None                    | No offline buffer download requested and non available.      |
| Download Requested      | Offline buffer download is requested. Currently waiting for the next BLE (connectable) advertising packet to start a connection to the device and download. |
| Download in progress    | The connection to the device and the offline buffer download is in progress. |
| Download File Available | The download of the device's offline buffer is completed and the file is available for download. |
| Unknown, Invalid        | Should not happen.                                           |

###### Command

| Command | Description                                                  |
| ------- | ------------------------------------------------------------ |
| Request | Request to download the device's offline buffer.             |
| Cancel  | Cancel the currently requested download.                     |
| Clear   | Clear the already downloaded offline buffer and free memory again. |

#### Device Commands and Device status

##### Commands regarding Device

| Command       | Description                                                  |
| ------------- | ------------------------------------------------------------ |
| Reboot        | Reboot the proxy device                                      |
| Start OTA     | Initiate the OTA process, currently there's no additional information the status given. |
| Delete (bond) | Removes a device from the security database list of peer device. |

##### Status information regarding Device

Most of the flags are just for debug reasons and will be removed in the future is the process stable.

| Flag/Field                       | Description                                                  |
| -------------------------------- | ------------------------------------------------------------ |
| Uptime                           | Show the device uptime in "DD d HH:MM:SS"<br />e.g. "3d 11:02:34" |
| gattc_connect                    | Flag, initiate connection to device "gattc_connect_beacon_idx", is true until canceled or DISCONNECT_EVT |
| gattc_is_connected               | Flag, currently connected to peer                            |
| gattc_scanning                   | Flag, true if in scanning process                            |
| gattc_offline_buffer_downloading | Flag, true if connection is initiated                        |
| gattc_give_up_now                | Flag, true if e.g. cancel is pressed during download process. Will wait for a timeout on the current activity and stop before next try, or, if no connection initiated, executed directly. |
| gattc_connect_beacon_idx         | Number of beacon to connect to or in connection with.        |
| app_desc->version                | The git version of the app software, e.g. 0.34-21-gd86d164   |
| app_desc->project_name           | The configured project name, e.g. blemqttproxy               |
| app_desc->idf_ver                | The ESP-IDF version used, e.g. v3.3.1-205-gf3c3605fc         |
| OTA running partition            | The currently running OTA partition, e.g. <br />type 0 subtype 16 (offset 0x00010000) |
| OTA configured partition         | The newly configured OTA partition, e.g.<br />type 0 subtype 16 (offset 0x00010000) |
| Bond device num                  | Number of bond devices                                       |
| Bond device NUM                  | For each bond device NUM, the address and the IRK is given, e.g.<br />ADDR: D7 59 9D 1D 7B 6B <br />IRK: 7A 1F 1A 20 CE 24 2A A2 63 F3 1A 62 BB 69 EA CA |


## BLE beacon announcement functionality

To add a not yet active beacon to this proxy **move the beacon close to the proxy or vice versa**.
After a confirmation with a **push** in between 2 secs the device will be added. Only configured beacons can be made active.
The current configuration of active beacons will be stored in NVS.


## 

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

