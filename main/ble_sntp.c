#include "blemqttproxy.h"
#include <lwip/apps/sntp.h>
#include "ble_sntp.h"

static const char* TAG = "BLE_SNTP";

bool sntp_time_available = false;

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
    sntp_time_available = true;
}

void initialize_sntp(void)
{
    static bool sntp_initialized = false;
    if(sntp_initialized)
        return;

    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);  // see: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
    tzset();
    sntp_initialized = true;
}
