/*
 * st7735_mini - tiny text-only driver for ST7735S 128x160 (black tab)
 * Target: ATmega88PA, hardware SPI, direct register access.
 * No Arduino SPI / Print / digitalWrite / Serial needed.
 *
 * Pin map (read from VI XU LY schematic):
 *   PB0 = RST   PB1 = BL   PB2 = CS (SS)   PB3 = MOSI   PB5 = SCK   PD7 = DC
 */
#ifndef ST7735_MINI_H
#define ST7735_MINI_H

#include <stdint.h>
#include <avr/pgmspace.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================= USER CONFIG ================= */
#ifndef TFT_LANDSCAPE
#define TFT_LANDSCAPE 1          /* 0 = portrait 128x160, 1 = landscape 160x128 */
#endif
#ifndef TFT_COL_OFFSET
#define TFT_COL_OFFSET 2         
#endif
#ifndef TFT_ROW_OFFSET
#define TFT_ROW_OFFSET 1
#endif
#ifndef TFT_BL_ACTIVE_HIGH
#define TFT_BL_ACTIVE_HIGH 1     /* set 0 if your module's backlight is active-low */
#endif
/* ============================================== */

#if TFT_LANDSCAPE
  #define TFT_W      160
  #define TFT_H      128
  #define TFT_MADCTL 0xA0        /* MY | MV | RGB  (black tab, 90 deg) */
  #define TFT_X_OFF  TFT_ROW_OFFSET
  #define TFT_Y_OFF  TFT_COL_OFFSET
#else
  #define TFT_W      128
  #define TFT_H      160
  #define TFT_MADCTL 0xC0        /* MX | MY | RGB  (black tab, 0 deg) */
  #define TFT_X_OFF  TFT_COL_OFFSET
  #define TFT_Y_OFF  TFT_ROW_OFFSET
#endif

/* RGB565 colours */
#define TFT_RGB(r,g,b) ((uint16_t)((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3)))
#define TFT_BLACK   0x0000
#define TFT_WHITE   0xFFFF
#define TFT_RED     0xF800
#define TFT_GREEN   0x07E0
#define TFT_BLUE    0x001F
#define TFT_YELLOW  0xFFE0
#define TFT_CYAN    0x07FF
#define TFT_ORANGE  0xFC00

/* Character cell = 6*size wide, 8*size tall (5x7 glyph + 1px gap, bg included) */

void    tft_init(void);
void    tft_backlight(uint8_t on);
void    tft_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t color);
#define tft_clear(c) tft_fill_rect(0, 0, TFT_W, TFT_H, (c))

/* Text in RAM string. Returns x position after the last char drawn. */
uint8_t tft_text(uint8_t x, uint8_t y, const char *s,
                 uint16_t fg, uint16_t bg, uint8_t size);
/* Text in flash (PROGMEM) - saves SRAM. */
uint8_t tft_text_P(uint8_t x, uint8_t y, const char *s,
                   uint16_t fg, uint16_t bg, uint8_t size);
/* Convenience: tft_puts_P(x, y, "literal", fg, bg, size) */
#define tft_puts_P(x, y, s, fg, bg, sz) tft_text_P((x), (y), PSTR(s), (fg), (bg), (sz))

/* Signed integer, left-aligned, padded with spaces up to 'width' chars (max 7)
 * so shorter numbers overwrite old digits without clearing. */
uint8_t tft_int(uint8_t x, uint8_t y, int16_t v, uint8_t width,
                uint16_t fg, uint16_t bg, uint8_t size);

#ifdef __cplusplus
}
#endif
#endif /* ST7735_MINI_H */