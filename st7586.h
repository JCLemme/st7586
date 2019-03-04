#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ST7586_DRIVER
#define ST7586_DRIVER

/* ST7586 driver 
 *
 * This is a very simple driver for displays that use the ST7586 controller,
 * specifically EastRising's 240x160 chip-on-glass display (model ERC240160FS).
 * 
 * The display communicates over SPI. You'll need to modify the SPI routines
 * to fit your platform. The routines here should work on an ESP32.
 *
 * Released under the MIT license. Written and partially reverse-engineered by
 * John Lemme (jclemme at proportionallabs dot com)
 */

typedef enum greycolor_packed
{
    GP_WHITE = 3,
    GP_LIGHTGREY = 2,
    GP_DARKGREY = 1,
    GP_BLACK = 0
} greycolor_packed;

typedef enum greycolor 
{
    GC_WHITE = 0x00,
    GC_LIGHTGREY = 0x08,
    GC_DARKGREY = 0x10,
    GC_BLACK = 0x1C
} greycolor;

int display_raw(uint8_t data, uint8_t asel);
uint8_t* display_ptr();

uint8_t* display_init();
int display_halt();

int display_refresh(int debug);
int display_progress();
int display_clear(int y1, int y2);
int display_blank();

int display_image(uint8_t* data);

int display_draw_pixel(int x, int y, greycolor color);

#endif
