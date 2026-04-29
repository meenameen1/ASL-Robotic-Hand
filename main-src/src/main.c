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

#include "mic.h"

void core1_operation(void);

#define DEFAULT_STEP_SIZE_US 50
#define DEFAULT_TICK_TIME_MS 50

int main(void)
{

   stdio_init_all();
   sleep_ms(500);

   //Running UI stuff on core 1
   multicore_launch_core1(core1_operation);

   gpio_init(22);
   gpio_set_dir(22, GPIO_OUT);
   gpio_put(22, 0); // Start with power off

   
   

   bool calibrate_new_motor = false;
   int init_port = 21;
   // char letters[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'};
   if(calibrate_new_motor) {
      // init_servo_positions();
      while(1) {
         secondpwmtest(init_port, 800);
         sleep_ms(2000);
         secondpwmtest(init_port, 2300);
         sleep_ms(2000);
         secondpwmtest(init_port, 1600);
         sleep_ms(8000);
      }
   }
   else {
      char letters_to_test[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'};
      // char letters_to_test[] = {'I', 'J', 'K'};
      while(1) {
         init_servo_positions();
         sleep_ms(2000);
         for (int i = 0; i < sizeof(letters_to_test); i++) {
            // move_to_letter(letters_to_test[i]);
            move_to_letter_smoothly(letters_to_test[i], DEFAULT_STEP_SIZE_US, DEFAULT_TICK_TIME_MS);
            u16 x = 200;
            u16 y = 200;
            u16 fc = 0xFFFF; // White color
            u16 bc = 0x0000; // Black color
            // u16 bc = letters_to_test[i];
            char num = letters_to_test[i];
            u8 size = 64; // Size of the character
            u8 mode = 0; // Normal mode
            LCD_DrawChar(x, y, fc, bc, num, size, mode);
            sleep_ms(2000);
         }
      }
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

   OLED_Display_t oled = {
      .current_mode = OLED_MODE_DEFAULT
   };
   oled_init(&oled);

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
      .oled = &oled
   };

   usb_initPeripherals(&keyboard);
   setUsbPowerOutput(1);
   while(1)
   {
      usb_task();
      ui_state_machine(&ui_context);
   }
}
