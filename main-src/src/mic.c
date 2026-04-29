#include "pico/stdlib.h"
#include "hardware/uart.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "mic.h"

#define pin_TX 20
#define pin_RX 21

#define UART_ID uart1

#define LETTER_BUFFER_SIZE 64

static char letter_buffer[LETTER_BUFFER_SIZE];
static int buffer_head = 0;
static int buffer_tail = 0;

static int next_index(int index)
{
    return (index + 1) % LETTER_BUFFER_SIZE;
}

static bool buffer_is_full(void)
{
    return next_index(buffer_head) == buffer_tail;
}

static bool buffer_is_empty(void)
{
    return buffer_head == buffer_tail;
}

static void push_letter(char c)
{
    if (!buffer_is_full()) {
        letter_buffer[buffer_head] = c;
        buffer_head = next_index(buffer_head);
    } else {
        // Buffer full. Drop the new character.
        // You could also overwrite the oldest character if preferred.
        printf("Letter buffer full, dropped: %c\n", c);
    }
}

bool mic_pop_letter(char *letter)
{
    if (buffer_is_empty()) {
        return false;
    }

    *letter = letter_buffer[buffer_tail];
    buffer_tail = next_index(buffer_tail);

    return true;
}

bool mic_has_letters(void)
{
    return !buffer_is_empty();
}

void init_uart(void)
{
    gpio_set_function(pin_TX, GPIO_FUNC_UART);
    gpio_set_function(pin_RX, GPIO_FUNC_UART);

    // This must match the Raspberry Pi side.
    uart_init(UART_ID, 56000);

    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(UART_ID, true);
}

void mic_uart_poll(void)
{
    while (uart_is_readable(UART_ID)) {
        uint8_t c = uart_getc(UART_ID);

        // Accept only uppercase A-Z
        if (c >= 'A' && c <= 'Z') {
            push_letter((char)c);
        }

        // Optional: convert lowercase to uppercase
        else if (c >= 'a' && c <= 'z') {
            push_letter((char)(c - 32));
        }

        // Ignore newline, carriage return, spaces, etc.
    }
}

// Old functions
/*

void init_uart() {
    gpio_set_function(pin_TX, GPIO_FUNC_UART);
    gpio_set_function(pin_RX, GPIO_FUNC_UART);

    uart_init(UART_ID, 56000);
    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
}
void blocking_read_uart()
{
    init_uart();
    sleep_ms(2000);

    u16 x = 50;
    u16 y = 100;
    u16 fc = 0xFFFF; // White
    u16 bc = 0x0000; // Black
    u8 size = 64;
    u8 mode = 0;

    LCD_DrawChar(x, y, fc, bc, 'A', size, mode);

    for (;;) {
        uint8_t c;

        uart_read_blocking(UART_ID, &c, 1);

        // Ignore line endings from Python
        if (c < 65 || c > 90) {
            continue;
        }
        
        LCD_DrawChar(x, y, fc, bc, (char)c, size, mode);
        move_to_letter_smoothly((char)c, 50, 50);
        x = x + 60;
        if (x > 350) {
            x = 50;
            y = y + 70;
        }
    }
}

*/