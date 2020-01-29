#include "esp_assert.h"
#include "esp_log.h"


#include "localsensor.h"

#if CONFIG_LOCAL_SENSORS_TEMPERATURE==1

int s_owb_num_devices = 0;

static OneWireBus *s_owb = NULL;
static OneWireBus_ROMCode s_device_rom_codes[OWB_MAX_DEVICES] = {0};

extern int s_owb_num_devices = 0;
static OneWireBus_SearchState s_owb_search_state = {0};
static DS18B20_Info *s_owb_devices[OWB_MAX_DEVICES] = {0};

local_temperature_data_t local_temperature_data[OWB_MAX_DEVICES] = { 0 };

void init_owb_tempsensor()
{
    // Create a 1-Wire bus, using the RMT timeslot driver
    owb_rmt_driver_info rmt_driver_info;
    s_owb = owb_rmt_initialize(&rmt_driver_info, GPIO_DS18B20_0, RMT_CHANNEL_1, RMT_CHANNEL_0);
    owb_use_crc(s_owb, true);  // enable CRC check for ROM code

    // Find all connected devices
    ESP_LOGI(TAG, "init_owb_tempsensor: find devices");

    bool found = false;
    owb_search_first(s_owb, &s_owb_search_state, &found);
    while (found)
    {
        char rom_code_s[17];
        owb_string_from_rom_code(s_owb_search_state.rom_code, rom_code_s, sizeof(rom_code_s));
        ESP_LOGI(TAG, "  %d : %s", s_owb_num_devices, rom_code_s);
        s_device_rom_codes[s_owb_num_devices] = s_owb_search_state.rom_code;
        ++s_owb_num_devices;
        owb_search_next(s_owb, &s_owb_search_state, &found);
    }
    ESP_LOGI(TAG, "Found device(s): %d", s_owb_num_devices);

    if (s_owb_num_devices == 1)
    {
        // For a single device only:
        OneWireBus_ROMCode rom_code;
        owb_status status = owb_read_rom(s_owb, &rom_code);
        if (status == OWB_STATUS_OK)
        {
            char rom_code_s[OWB_ROM_CODE_STRING_LENGTH];
            owb_string_from_rom_code(rom_code, rom_code_s, sizeof(rom_code_s));
            ESP_LOGI(TAG, "Single device %s present", rom_code_s);
        }
        else
        {
        }
    }
    else
    {
        ESP_LOGE(TAG, "init_owb_tempsensor: found %d devices, expected 1 device", s_owb_num_devices);
        // // Search for a known ROM code (LSB first):
        // // For example: 0x1502162ca5b2ee28
        // OneWireBus_ROMCode known_device = {
        //     .fields.family = { 0x28 },
        //     .fields.serial_number = { 0xee, 0xb2, 0xa5, 0x2c, 0x16, 0x02 },
        //     .fields.crc = { 0x15 },
        // };
        // char rom_code_s[OWB_ROM_CODE_STRING_LENGTH];
        // owb_string_from_rom_code(known_device, rom_code_s, sizeof(rom_code_s));
        // bool is_present = false;

        // owb_status search_status = owb_verify_rom(s_owb, known_device, &is_present);
        // if (search_status == OWB_STATUS_OK)
        // {
        //     printf("Device %s is %s", rom_code_s, is_present ? "present" : "not present");
        // }
        // else
        // {
        //     printf("An error occurred searching for known device: %d", search_status);
        // }
    }

    // Create DS18B20 devices on the 1-Wire bus
    for (int i = 0; i < s_owb_num_devices; ++i)
    {
        DS18B20_Info * ds18b20_info = ds18b20_malloc();  // heap allocation
        s_owb_devices[i] = ds18b20_info;

        if (s_owb_num_devices == 1)
        {
            ESP_LOGI(TAG,"Single device optimisations enabled");
            ds18b20_init_solo(ds18b20_info, s_owb);
        }
        else
        {
            ds18b20_init(ds18b20_info, s_owb, s_device_rom_codes[i]); // associate with bus and device
        }
        ds18b20_use_crc(ds18b20_info, true);           // enable CRC check for temperature readings
        ds18b20_set_resolution(ds18b20_info, DS18B20_RESOLUTION);
    }
}

void cleanup_owb_tempsensor(){
    // clean up dynamically allocated data
    for (int i = 0; i < s_owb_num_devices; ++i)
    {
        ds18b20_free(&s_owb_devices[i]);
    }
    owb_uninitialize(s_owb);
}

void update_local_temp_data(uint8_t num_devices, float *readings)
{
    for(int i = 0; i < num_devices; i++){
        local_temperature_data[i].temperature = readings[i];
        local_temperature_data[i].last_seen   = esp_timer_get_time();
    }

    xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);
}

void localsensor_task(void* pvParameters)
{
    int errors_count[OWB_MAX_DEVICES] = {0};
    int sample_count = 0;
    if (s_owb_num_devices > 0){
        TickType_t last_wake_time = xTaskGetTickCount();

        while (1)
        {
            last_wake_time = xTaskGetTickCount();

            ds18b20_convert_all(s_owb);

            // In this application all devices use the same resolution,
            // so use the first device to determine the delay
            ds18b20_wait_for_conversion(s_owb_devices[0]);

            // Read the results immediately after conversion otherwise it may fail
            float readings[OWB_MAX_DEVICES] = { 0 };
            DS18B20_ERROR errors[OWB_MAX_DEVICES] = { 0 };

            for (int i = 0; i < s_owb_num_devices; ++i)
            {
                errors[i] = ds18b20_read_temp(s_owb_devices[i], &readings[i]);
            }

            ESP_LOGI(TAG, "Temperature readings (degrees C): sample %d", ++sample_count);
            for (int i = 0; i < s_owb_num_devices; ++i){
                if (errors[i] != DS18B20_OK){
                    ++errors_count[i];
                }
                ESP_LOGI(TAG, "  %d: %.1f    %d errors", i, readings[i], errors_count[i]);
            }

            update_local_temp_data(s_owb_num_devices, readings);

            vTaskDelayUntil(&last_wake_time, LOCAL_SENSOR_SAMPLE_PERIOD / portTICK_PERIOD_MS);
        }
    }

    vTaskDelete(NULL);
}

#endif // CONFIG_LOCAL_SENSORS_TEMPERATURE
