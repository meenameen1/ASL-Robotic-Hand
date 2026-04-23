#ifndef OLED_H
#define OLED_H

#include "hardware/spi.h"

void send_spi_cmd(spi_inst_t* spi, uint16_t value);
void send_spi_data(spi_inst_t* spi, uint16_t value);
void cd_init();
void clear_display();
void cd_display1(const char *string, bool show_arrow);
void cd_display2(const char *string, bool show_arrow);
void oled_init();

typedef uint32_t OLED_mode_t;

typedef enum
{
    OLED_MODE_DEFAULT,
    OLED_MODE_LEARN,
    OLED_MODE_TRANSATE,
    OLED_MODE_KEYBOARD_DISCONNECTED,
    OLED_MODE_TYPING,
    OLED_MODE_TRANSLATING,
    OLED_MODE_ERROR,
} OLED_Mode_t;

typedef struct {
    OLED_Mode_t current_mode;
} OLED_Display_t;

void display_screen(OLED_Display_t* oled, OLED_Mode_t oled_mode);

#endif
