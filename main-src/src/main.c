#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "usb_control.h"
#include "oled.h"
#include "lcd.h"

#include <math.h>


int main(void)
{
   stdio_init_all();
   sleep_ms(500);

   usb_init();
   oled_init();
   init_spi_lcd();

   while(1)
   {
      usb_task();
      // sleep_ms(1000);
   }
}
