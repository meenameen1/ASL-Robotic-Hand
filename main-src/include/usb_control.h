#ifndef USB_H
#define USB_H

#include <stdbool.h>
#include <stdint.h>

#define ARROW_UP    128
#define ARROW_DOWN  129
#define ARROW_LEFT  130
#define ARROW_RIGHT 131
#define ENTER       132

#define ID_DEVICE_ATTACHED 0
#define ID_HOST_ATTACHED 1

typedef struct {
    uint32_t start_time_us;
    uint32_t device_attached;
    bool key_ready;
    bool connected;
    char last_key;

} Keyboard_Device_t;

void usb_task(void);
void usb_initPeripherals(Keyboard_Device_t *kbd);
void usb_init(Keyboard_Device_t *kbd);
void setUsbPowerOutput(bool on);
void getDeviceAttached();



#endif
