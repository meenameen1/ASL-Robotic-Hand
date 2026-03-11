#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "usb_control.h"
#include "oled.h"
#include "lcd.h"
#include "ui.h"
#include "pico/multicore.h"

#include <math.h>

void core1_operation(void);


int main(void)
{
   stdio_init_all();
   sleep_ms(500);


   multicore_launch_core1(core1_operation);

   while(1)
   {

   }


}

// This function will run on the second core and handle all USB, LCD, OLED, and UI logic
void core1_operation(void)
{
   Keyboard_Device_t keyboard =
   {
       .last_key = 0,
       .key_ready = false
   };
   usb_init(&keyboard);

   oled_init();

   LCD_t lcd =
   {
         .display_buffer = {0},
         .len = 0
   };
   init_spi_lcd();


   UI_Context_t ui_context =
   {
      .state = STATE_IDLE,
      .keyboard = &keyboard,
      .lcd = &lcd,
   };

   while(1)
   {
      usb_task();
      ui_state_machine(&ui_context);
   }
}
