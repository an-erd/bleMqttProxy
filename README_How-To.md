## A Little How-To on Daily and Not-so-daily commands and tasks

This How-To is intended to give an overview of the tasks to be done in maintaining the repository, rebasing changes, and all that commands and tasks you need but easily forget

## Git

### Providing a Pull Request

#### 1) Start a new branch



#### 2) Create a Pull Request (PR)



#### 3) Delete branch



#### 4) Rework a PR



#### 5) Updating a local fork

You should use `rebase` to avoid those merge commits.

```
git rebase upstream/master
git push -f
```

### Remove a submodule

To remove a submodule you need to:

1. Delete the relevant section from the `.gitmodules`file.
2. Stage the `.gitmodules` changes `git add .gitmodules`.
3. Delete the relevant section from `.git/config`.
4. Run `git rm --cached path_to_submodule` (no trailing slash).
5. Run `rm -rf .git/modules/path_to_submodule` (no trailing slash).
6. Commit `git commit -m "Removed submodule"`.
7. Delete the now untracked submodule files `rm -rf path_to_submodule`.




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
drop database sensordata
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

##### Schema Exploration

[Schema Exploration ](https://docs.influxdata.com/influxdb/v1.7/query_language/schema_exploration/)


##### List Databases

```bash
$ influx -execute 'SHOW DATABASES'
```

##### Get Data

```
$ SHOW MEASUREMENTS
$ select * from "/beac/0x0007/x0006/reboot"
```



## Raspberry Pi https Server for OTA update process

On the Raspberry Pi you can use `openssl s_server` to provide OTA images to the devices. This can be achieved by starting the server in the background and ignoring the `HUP` signal, which is send e.g. if you log off, with the following command.

```
nohup openssl s_server -WWW -key ca_key.pem -cert ca_cert.pem -port 8070 > s_server.log 2> s_server.err &
```

The directory under which the server is started is `/`, thus for the sdkconfig entries

```
#
# OTA
#
CONFIG_OTA_FIRMWARE_UPG_URL="https://192.168.2.137:8070/blemqttproxy.bin"
CONFIG_OTA_SKIP_COMMON_NAME_CHECK=y
CONFIG_OTA_SKIP_VERSION_CHECK=y
CONFIG_OTA_RECV_TIMEOUT=5000
# end of OTA
```

you have to put the file `blemqttproxy.bin` in this directory. Please ensure that the `ca_key.pem` and `ca_cert.pem` correspond to the files used for compilation, and that the port (here `8070`)  match.