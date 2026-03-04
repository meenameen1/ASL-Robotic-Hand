#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include <stdio.h>
#include "oled.h"

const int SPI_DISP_SCK = 10;
const int SPI_DISP_CSn = 9;
const int SPI_DISP_TX = 11;

spi_inst_t *OLED_SPI = spi1; // Use SPI1 for the LCD

void init_chardisp_pins() {
    gpio_set_function(SPI_DISP_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_DISP_CSn, GPIO_FUNC_SPI);
    gpio_set_function(SPI_DISP_TX, GPIO_FUNC_SPI);

    // initialize SPI
    spi_init(spi1, 2000000); // 2 MHz = 220 us for both lines, 4 MHz = 130 us for both lines, 12 MHz doesnt work
    spi_set_format(
        spi1,
        10,             // data bits
        0, 0,          // CPOL, CPHA not used
        SPI_MSB_FIRST  // MSB first
    );
}

void send_spi_cmd(spi_inst_t* spi, uint16_t value) {
    while(spi_is_busy(spi));
    spi_write16_blocking(spi, &value, 1);
}

void send_spi_data(spi_inst_t* spi, uint16_t value) {
    send_spi_cmd(spi, 0x200 | value);
}

void cd_init() {
    spi_inst_t* spi = spi1; // Use spi1 for the display

    sleep_ms(1);
    // Perform a Function Set command
    send_spi_cmd(spi, 0x38);
    // Perform a Display On/Off command
    send_spi_cmd(spi, 0x0c);
    // Perform a Clear Display command
    send_spi_cmd(spi, 0x01);
    sleep_ms(2);
    // Perform an Entry Mode Set command
    send_spi_cmd(spi, 0x06);
    // Perform a Return Home command
    send_spi_cmd(spi, 0x02);
}

void cd_display1(const char *str) {
    spi_inst_t* spi = spi1;
    send_spi_cmd(spi, 0x02);
    while(*str != '\0') {
        send_spi_data(spi, *str);
        str++;
    }
}
void cd_display2(const char *str) {
    spi_inst_t* spi = spi1;
    send_spi_cmd(spi, 0xc0);
    while(*str != '\0') {
        send_spi_data(spi, *str);
        str++;
    }
}

void oled_init()
{
    init_chardisp_pins();
    cd_init();
    // uint32_t initialTime = timer_hw->timerawl;
    cd_display1("ASL Robotic Hand");
    cd_display2("Translator");

    // uint32_t finalTime = timer_hw->timerawl;
    // uint32_t timeDiff = finalTime - initialTime; // 131us
}
/***************************************************************** */
