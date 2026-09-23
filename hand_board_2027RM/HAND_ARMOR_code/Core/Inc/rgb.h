#ifndef RGB_H
#define RGB_H

#include <stdint.h>

typedef enum
{
    RGB_COLOR_RED = 0,
    RGB_COLOR_GREEN,
    RGB_COLOR_BLUE,
    RGB_COLOR_YELLOW,
    RGB_COLOR_CYAN,
    RGB_COLOR_MAGENTA,
    RGB_COLOR_WHITE,
    RGB_COLOR_COUNT
} RGB_Color_t;

void RGB_Init(void);

void RGB_SetColor(RGB_Color_t color);
RGB_Color_t RGB_GetColor(void);

void RGB_SetBrightness(uint8_t percent);
uint8_t RGB_GetBrightness(void);

void RGB_Off(void);

#endif /* RGB_H */
