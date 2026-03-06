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
   init_servo_positions();
   char target;
   while(1) {
      target = 'A';
      // move_to_letter_smoothly(target, 100, 20);
      move_to_letter(target);
      sleep_ms(2000);
      target = 'B';
      move_to_letter(target);      
      sleep_ms(2000);
      target = 'X';
      move_to_letter(target);      
      sleep_ms(2000);
   }
   
   
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
/*
int main(void)
{
   stdio_init_all();
   sleep_ms(2000); 
   printf("Hello ASL Robotic Hand!\n");
   MovementInstructions instructions = calculate_delta('A', 'B');

   printf("Instructions for moving from A to B:\n");
   for (int i = 0; i < MOTOR_COUNT; i++) {
      printf("Motor %d: %d\n", i, instructions.deltas[i]);
   }

   while (true) {
      tight_loop_contents();
   }
   
}
*/