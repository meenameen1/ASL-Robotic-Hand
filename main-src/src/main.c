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
   bool calibrate_new_motor_on_port_0 = false;
   if(calibrate_new_motor_on_port_0) {
      secondpwmtest(0, 500);
      sleep_ms(2000);
      secondpwmtest(0, 2700);
      sleep_ms(2000);
      secondpwmtest(0, 1600);
      sleep_ms(2000);
   }
   else {
      while(1) {
         move_to_letter('A');
         sleep_ms(2000);
         move_to_letter('B');
         sleep_ms(2000);
         move_to_letter('C');
         sleep_ms(2000);
         move_to_letter('F');
         sleep_ms(2000);
      }
   }  
}
