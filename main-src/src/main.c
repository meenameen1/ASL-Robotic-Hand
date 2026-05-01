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

#define MAX_SENTENCE_LEN 128

static UI_Context_t* ui;
static int current_index = 0;
static int translate_trigger = 0;

int main(void)
{

   stdio_init_all();
   sleep_ms(500);

   //Running UI stuff on core 1
   multicore_launch_core1(core1_operation);
   sleep_ms(500);
   gpio_init(22);
   gpio_set_dir(22, GPIO_OUT);
   gpio_put(22, 0); // Start with power off

   init_servo_positions();
   // blocking_read_uart();




      while(1) {

      if (translate_trigger && current_index < ui->lcd->len) {
         draw_highlighted_letter(ui->lcd, current_index);
         move_to_letter_smoothly(ui->lcd->display_buffer[current_index], DEFAULT_STEP_SIZE_US, DEFAULT_TICK_TIME_MS);
         sleep_ms(500);
         current_index++;
      }
      if(current_index >= ui->lcd->len) {
         translate_trigger = 0; // Reset trigger when done
         current_index = 0; // Reset index for next time
      }
    tight_loop_contents();
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
   ui = &ui_context;

   usb_initPeripherals(&keyboard);
   setUsbPowerOutput(0);


   init_uart();
   while(1)
   {
      // usb_task();
      // ui_state_machine(&ui_context);
      mic_uart_poll();
      char letter;
      // Pull all new UART letters into the sentence buffer
      switch(ui_context.state)
      {
         case(STATE_IDLE):
            while (mic_pop_letter(&letter))
            {
               if(letter == '\r')
               {
                  ui_context.state = STATE_TRANSLATING;
                  translate_trigger = 1;
               }
               else if(letter == '\n')
               {

               }
               else
               {
                  // handle_key_press(ui, 48 + ui->lcd->len%10);
                  handle_key_press(ui, letter);
               }
            }
            break;
         case(STATE_TRANSLATING):
            if(translate_trigger == 1)
            {
            }
            else
            {
               ui->state = STATE_IDLE;
               LCD_Clear(0x0000);
               LCD_DrawString(10, 10, 0x0000, 0xFFFF, " ASL Robotic Hand Translator ", 32, 0);
               ui->lcd->display_buffer[0] = '\0';
               ui->lcd->len = 0;
            }
             break;
         default:
            break;
      }
   }
}
