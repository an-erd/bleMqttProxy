#include "helperfunctions.h"

uint16_t uint16_decode(const uint8_t * p_encoded_data)
{
        return ( (((uint16_t)((uint8_t *)p_encoded_data)[0])) |
                 (((uint16_t)((uint8_t *)p_encoded_data)[1]) << 8 ));
}

uint16_t uint16_decode_r(const uint8_t * p_encoded_data)
{
        return ( (((uint16_t)((uint8_t *)p_encoded_data)[0]) << 8 ) |
                 (((uint16_t)((uint8_t *)p_encoded_data)[1])));
}

uint32_t uint32_decode(const uint8_t * p_encoded_data)
{
    return ( (((uint32_t)((uint8_t *)p_encoded_data)[0]) << 0)  |
             (((uint32_t)((uint8_t *)p_encoded_data)[1]) << 8)  |
             (((uint32_t)((uint8_t *)p_encoded_data)[2]) << 16) |
             (((uint32_t)((uint8_t *)p_encoded_data)[3]) << 24 ));
}

void convert_s_ddhhmmss(uint32_t sec, uint16_t *d, uint8_t *h, uint8_t *m, uint8_t *s)
{
    uint32_t tmp_sec = sec;

    *d = tmp_sec / 86400;
    tmp_sec -= *d*86400;

    *h = tmp_sec / 3600;
    tmp_sec -= *h*3600;

    *m = tmp_sec / 60;
    tmp_sec -= *m * 60;

    *s = tmp_sec;
}

void convert_s_hhmmss(uint32_t sec, uint8_t *h, uint8_t *m, uint8_t *s)
{
    uint32_t tmp_sec = sec;

    *h = tmp_sec / 3600;
    tmp_sec -= *h*3600;

    *m = tmp_sec / 60;
    tmp_sec -= *m * 60;

    *s = tmp_sec;
}

void convert_s_mmss(uint32_t sec, uint8_t *m, uint8_t *s)
{
    uint32_t tmp_sec = sec;

    *m = tmp_sec / 60;
    tmp_sec -= *m * 60;

    *s = tmp_sec;
}
