#include <stdio.h>
#include "ssd1306_convert_fonts.h"
#include "ssd1306_fonts.h"

void convert_fonts_1(){
    uint8_t num_char = 95;
    uint8_t num_cols = 12;
    uint16_t twobyte;
    printf("const uint8_t c_chFont1006[%d][%d] = {\n", num_char, num_cols);
    for (int i = 0; i < num_char; i++){
        printf("  { ");
        for (int j = 0; j < num_cols/2; j++){
            twobyte = ((c_chFont1206[i][2*j]) <<8) | (c_chFont1206[i][2*j+1]);
            twobyte <<= 2;
            if(j != 0)
                printf(", ");
            printf("0x%02X, 0x%02X", (twobyte & 0xFF00)>>8, (twobyte & 0xFF));
        }
        printf(" }, // %d\n", i);
    }
    printf("};\n");
}
