
#include <stdio.h>
#include "pico/stdlib.h"
#include "usb_control.h"
#include "oled.h"

int main(void)
{
   stdio_init_all();

   usb_init();

   oled_init();

   while(1)
   {
      usb_task();
      printf("Cycle");
   }
}
