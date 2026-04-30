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

int main(void)
{

   stdio_init_all();
   sleep_ms(500);

   //Running UI stuff on core 1
   multicore_launch_core1(core1_operation);

   gpio_init(22);
   gpio_set_dir(22, GPIO_OUT);
   gpio_put(22, 0); // Start with power off

   init_servo_positions();
   // blocking_read_uart();

   init_uart();

   char sentence[MAX_SENTENCE_LEN];
   int sentence_len = 0;
   int current_index = 0;


   // char letters_to_test[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'};
   while(1) {
      mic_uart_poll();
      char letter;
      // Pull all new UART letters into the sentence buffer
      while (mic_pop_letter(&letter)) {
         if (sentence_len < MAX_SENTENCE_LEN - 1) {
            sentence[sentence_len++] = letter;
            sentence[sentence_len] = '\0';
         }
      }
      if (current_index < sentence_len) {
         draw_sentence_screen(sentence, sentence_len, current_index);
         move_to_letter_smoothly(sentence[current_index], DEFAULT_STEP_SIZE_US, DEFAULT_TICK_TIME_MS);
         current_index++;
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

   usb_initPeripherals(&keyboard);
   setUsbPowerOutput(1);
   while(1)
   {
      usb_task();
      ui_state_machine(&ui_context);
   }
}
