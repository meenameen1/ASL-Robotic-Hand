#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "usb_control.h"
#include "oled.h"
#include "pico/stdlib.h"
#include "letters.h"

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