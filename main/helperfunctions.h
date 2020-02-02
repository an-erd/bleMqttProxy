#ifndef __HELPERFUNCTIONS_H__
#define __HELPERFUNCTIONS_H__

#include <stdint.h>

#define ENDIAN_CHANGE_U16(x) ((((x)&0xFF00)>>8) + (((x)&0xFF)<<8))
#define MSB_16(a) (((a) & 0xFF00) >> 8)
#define LSB_16(a) ((a) & 0x00FF)
#define UNUSED(expr) do { (void)(expr); } while (0)

#define SHT3_GET_TEMPERATURE_VALUE(temp_msb, temp_lsb) \
    (-45+(((int16_t)temp_msb << 8) | ((int16_t)temp_lsb ))*175/(float)0xFFFF)

#define SHT3_GET_HUMIDITY_VALUE(humidity_msb, humidity_lsb) \
    ((((int16_t)humidity_msb << 8) | ((int16_t)humidity_lsb))*100/(float)0xFFFF)


uint16_t uint16_decode(const uint8_t * p_encoded_data);
uint32_t uint32_decode(const uint8_t * p_encoded_data);
void convert_s_hhmmss(uint16_t sec, uint8_t *h, uint8_t *m, uint8_t *s);
void convert_s_mmss(uint16_t sec, uint8_t *m, uint8_t *s);

#endif // __HELPERFUNCTIONS_H__