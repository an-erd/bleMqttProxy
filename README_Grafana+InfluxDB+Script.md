## MQTT-InfluxDB Proxy using WLAN on Raspberry Pi

The Raspberry pi is used to receive the MQTT packages using WLAN and to add them to the InfluxDB database in the table `sensordata`. Currently, it's just a very simple Python3 script, with almost no error checks etc. Fortunately, the Raspberry Pi with InfluxDB and this Python3 script is running stable for more than 8 month. After that, the data stopped but - fingers crossed - only because the IoT WLAN was (probably by accident) switched of. 

Thus, starting IoT WLAN again and restarting the script (which was probably not necessary) everything is working fine again. BTW: I needed 3 weeks to realize that's not running due to the holidays.

Start the script to forward MQTT messages to the InfluxDB database:

```
pi@raspberrypi:~ $ nohup python3 ./mosq-influx-task.py > /dev/null 2> textpy.err.log & 
```

## InfluxDB

### Backup the InfluxDB database

#### Database Backup

```
sudo influxd backup -portable -database sensordata -host 127.0.0.1:8088 /home/pi/influxdb_sensordata_backup_DATE
```

#### Database Restore

```
sudo influxd restore -portable -db sensordata /home/pi/influxdb_sensordata_backup_DATE
```

The database must not exist. If it already exist, use the InfluxDB CLI to drop the database:

```bash
$ influx
drop database solaranzeige
quit
```

If you want to use a different database name for the restored database, use the option `-newdb newname` 

```bash
$ sudo influxd restore -portable -db sensordata -newdb newsensordata  /home/pi/influxdb_sensordata_backup_DATE
```



### Links and Information

[InfluxDB CLI](https://docs.influxdata.com/influxdb/v1.7/tools/shell/)

[InbluxDB Backup/Restore](https://docs.influxdata.com/influxdb/v1.7/administration/backup_and_restore/)

[Information on how to backup the DB](https://solaranzeige.de/phpBB3/viewtopic.php?t=310)

#### Prerequisites

Ensure that you can access the DB from localhost by:

```
sudo vi /etc/influxdb/influxdb.conf
```

Remove the `#` at the beginning of the following line:

```
# bind-address = "127.0.0.1:88"
```



#### Useful Commands

##### List Databases

```bash
$ influx -execute 'SHOW DATABASES'
```

