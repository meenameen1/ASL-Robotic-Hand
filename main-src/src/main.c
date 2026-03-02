#include <stdio.h>
#include "usb_control.h"
#include "oled.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"


int main(void)
{
   stdio_init_all();

   // usb_init();

   oled_init();

   while(1)
   {
      // usb_task();
      sleep_ms(1000);
   }
}
