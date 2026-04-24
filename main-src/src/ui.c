#include "ui.h"
#include "oled.h"
#include "hardware/timer.h"
#include <stdint.h>

void handle_key_press(UI_Context_t* ui, char key);


void ui_state_machine(UI_Context_t* ui)
{
   switch(ui->state)
   {
      case STATE_IDLE:
         if (ui->keyboard->key_ready)
         {
            ui->state = STATE_TYPING;
         }
         if(!ui->keyboard->connected)
         {
            ui->state = STATE_KEYBOARD_DISCONNECTED;
            // uint32_t oledStart = timer_hw->timerawl;
            display_screen(ui->oled, OLED_MODE_KEYBOARD_DISCONNECTED);
            // uint32_t oledEnd =  timer_hw->timerawl;
            // uint32_t oledElapsed =  oledEnd - oledStart;
         }
         break;

      case STATE_TYPING:
         //
         ;
         char currentKey = ui->keyboard->last_key;

         handle_key_press(ui, currentKey);
         // After processing, go back to idle
         ui->keyboard->key_ready = false; // Reset the key ready flag
         ui->state = STATE_IDLE;
         break;

      case STATE_TRANSLATING:
         // Handle translation logic
         // ...
         break;

      case STATE_KEYBOARD_DISCONNECTED:
         if(ui->keyboard->connected)
         {
            display_screen(ui->oled, OLED_MODE_DEFAULT);

            ui->state = STATE_IDLE;
         }
         else
         {
            usb_init(ui->keyboard); // Attempt to reinitialize USB connection
         }
         break;

      case STATE_ERROR:
         // Handle error state
         // ...
         break;

      default:
         ui->state = STATE_ERROR;
         break;
   }
}

void handle_key_press(UI_Context_t* ui, char key) {
        if(key == '\b')
         {
            // Handle backspace: remove last character from display buffer
            if (ui->lcd->len > 0) {
               LCD_write_letter(ui->lcd, ' '); // Update LCD to reflect backspace
               ui->lcd->display_buffer[--ui->lcd->len] = '\0'; // Remove last char and null-terminate
            }
         }
         else {
            // Add the new character to the display buffer
            if (ui->lcd->len < sizeof(ui->lcd->display_buffer) - 1) {
               ui->lcd->display_buffer[ui->lcd->len++] = key; // Add char and increment length
               ui->lcd->display_buffer[ui->lcd->len] = '\0'; // Null-terminate
               LCD_write_letter(ui->lcd, key); // Update LCD to reflect new character

               //  uint32_t currentTime = timer_hw->timerawl;
               //  uint32_t elapsedTime =  currentTime - ui->keyboard->start_time_us  ; //600 - 800 us

            }
         }
}
