#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "usb_control.h"
#include "oled.h"
#include "pico/stdlib.h"

#include "pico/stdio_usb.h"

#include "letters.h"

#include "pico/time.h"


int main(void) {
   
   // init_servo_positions();
   // char target_letter;

   

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
      // char letters_to_test[] = {'P', 'Q'};
      while(1) {
         init_servo_positions();
         sleep_ms(2000);
         for (int i = 0; i < sizeof(letters_to_test); i++) {
            move_to_letter(letters_to_test[i]);
            sleep_ms(2000);
         }
      }
      // while(1) {
         // init_servo_positions();
         // sleep_ms(2000);
         // move_to_letter('A');
         // sleep_ms(2000);

      // }
   }  
}
