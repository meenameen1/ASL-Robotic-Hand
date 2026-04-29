#ifndef MIC_H
#define MIC_H

#include <stdbool.h>

void init_uart(void);
void mic_uart_poll(void);
bool mic_pop_letter(char *letter);
bool mic_has_letters(void);

#endif