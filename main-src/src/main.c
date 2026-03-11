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
   char target_letter;
   // while(1) {
   //    secondpwmtest(15, 500);
   //    sleep_ms(2000);
   //    secondpwmtest(15, 1600);
   //    sleep_ms(2000);

   //    secondpwmtest(16, 500);
   //    sleep_ms(2000);
   //    secondpwmtest(16, 1600);
   //    sleep_ms(2000);
   // }
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
   // }
   // while(1) {
   //    target_letter = 'A';
   //    move_to_letter(target_letter);
   //    sleep_ms(2000);
   //    target_letter = 'B';
   //    move_to_letter(target_letter);
   //    sleep_ms(2000);
   //    target_letter = 'X';
   //    move_to_letter(target_letter);
   //    sleep_ms(2000);
   // }

   /*
   usb_init();
   while(1) {
      usb_task();
   }*/
}


/*
#include "pico/stdlib.h"
bool pca9685_ack_test(void);
void init_i2c(void); // or your public init function

int main(void) {
    const uint LED = PICO_DEFAULT_LED_PIN;
    gpio_init(LED);
    gpio_set_dir(LED, GPIO_OUT);

    init_i2c();

    bool ok = pca9685_ack_test();

    while (true) {
        gpio_put(LED, 1);
        sleep_ms(ok ? 100 : 600);  // fast blink = ACK, slow blink = FAIL
        gpio_put(LED, 0);
        sleep_ms(ok ? 100 : 600);
    }
}
*/
/*
int main(void) {
   stdio_init_all();
   const uint LED = PICO_DEFAULT_LED_PIN;
   gpio_init(LED);
   gpio_set_dir(LED, GPIO_OUT);

   oled_init();

   while (true) {
      gpio_put(LED, 1);
      sleep_ms(200);
      gpio_put(LED, 0);
      sleep_ms(200);
      cd_display1("ASL Robotic Hand");
      cd_display2("Translator");
  }
}
*/