#include "pico/stdlib.h"
#include "hardware/uart.h"
#include <stdio.h>
#include <stdint.h>
#include "mic.h"

#define pin_TX 20
#define pin_RX 21

#define UART_ID uart1

void init_uart() {
    gpio_set_function(pin_TX, GPIO_FUNC_UART);
    gpio_set_function(pin_RX, GPIO_FUNC_UART);

    uart_init(UART_ID, 115200);
    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
}

void blocking_read_uart()
{
    init_uart();
    for (;;) {
        char buf[2];
        uart_read_blocking(UART_ID, (uint8_t*)buf, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0'; // Ensure null-termination
        uart_puts(UART_ID, "Character: ");
        uart_puts(UART_ID, buf);
        uart_puts(UART_ID, "\n");
    }
}
