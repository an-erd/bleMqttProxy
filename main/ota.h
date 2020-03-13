#ifndef __OTA_H__
#define __OTA_H__

#include "freertos/event_groups.h"

#define OTA_START_UPDATE          (BIT0)
extern EventGroupHandle_t ota_evg;

void initialize_ota(void);
void ota_task(void* pvParameters);

#endif // __OTA_H__
