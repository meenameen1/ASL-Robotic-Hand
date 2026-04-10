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

   init_servo_positions();

   bool calibrate_new_motor = false;
   int init_port = 15;
   // char letters[2] = ['Y', 'Z'];
   if(calibrate_new_motor) {
      while(1) {
         secondpwmtest(init_port, 0);
         sleep_ms(2000);
         secondpwmtest(init_port, 2700);
         sleep_ms(2000);
         secondpwmtest(init_port, 1600);
         sleep_ms(8000);
      }
   }
   else {
      // while(1) {
      //    init_servo_positions();
      //    sleep_ms(2000);
      //    for (int i = 0; i < sizeof(letters); i++) {
      //    move_to_letter(letters[i]);
      //    sleep_ms(2000);
      //    }
      // }
      while(1) {
         init_servo_positions();
         sleep_ms(2000);
         move_to_letter('A');
         sleep_ms(2000);
         move_to_letter('B');
         sleep_ms(2000);
         move_to_letter('C');
         sleep_ms(2000);
         move_to_letter('D');
         sleep_ms(2000);
         move_to_letter('E');
         sleep_ms(2000);
         move_to_letter('F');
         sleep_ms(2000);

      }
   }  
}
