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
   
   int x;
   init_servo_positions();
   char target_letter;


   absolute_time_t start_time_A = get_absolute_time();
   target_letter = 'A';
   move_to_letter(target_letter);
   int64_t elapsed_time_start_to_A = absolute_time_diff_us(start_time_A, get_absolute_time());

   absolute_time_t start_time_B = get_absolute_time();
   target_letter = 'B';
   move_to_letter(target_letter);
   int64_t elapsed_time_start_to_B = absolute_time_diff_us(start_time_B, get_absolute_time());

   absolute_time_t start_time_X = get_absolute_time();
   target_letter = 'X';
   move_to_letter(target_letter);
   int64_t elapsed_time_start_to_X = absolute_time_diff_us(start_time_X, get_absolute_time());

   



   x=1;


   
   usb_init();
   while(1) {
      usb_task();
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