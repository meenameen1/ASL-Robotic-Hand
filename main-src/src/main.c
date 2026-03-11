#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "usb_control.h"
#include "oled.h"
#include "pico/stdlib.h"
#include "usb_control.h"
#include "oled.h"
#include "lcd.h"
#include "ui.h"
#include "pico/multicore.h"

#include <math.h>
#include "pico/stdio_usb.h"
#include "letters.h"
#include "pico/time.h"

void core1_operation(void);

int main(void)
{

   stdio_init_all();
   sleep_ms(500);

   //Running UI stuff on core 1
   multicore_launch_core1(core1_operation);

   init_servo_positions();
   char target_letter;

   while(1) {
      target_letter = 'H';
      move_to_letter(target_letter);
      // move_to_letter_smoothly(target_letter, 100, 20);
      sleep_ms(2000);
      target_letter = 'I';
      move_to_letter(target_letter);
      // move_to_letter_smoothly(target_letter, 10, 10);
      sleep_ms(2000);
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
