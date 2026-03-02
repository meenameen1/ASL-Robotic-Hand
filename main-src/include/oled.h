#ifndef OLED_H
#define OLED_H

#include "hardware/spi.h"

void send_spi_cmd(spi_inst_t* spi, uint16_t value);
void send_spi_data(spi_inst_t* spi, uint16_t value);
void cd_init();
void cd_display1(const char *string);
void cd_display2(const char *string);
void oled_init();

#endif
