#ifndef USB_H
#define USB_H

#include <stdbool.h>
#include <stdint.h>

#define ARROW_UP    128
#define ARROW_DOWN  129
#define ARROW_LEFT  130
#define ARROW_RIGHT 131
#define ENTER       132

typedef struct {
    char last_key;
    bool key_ready;
    bool connected;
    uint32_t start_time_us;
} Keyboard_Device_t;

void usb_task(void);
void usb_init(Keyboard_Device_t *kbd);


#endif
