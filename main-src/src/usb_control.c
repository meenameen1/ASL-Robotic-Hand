#include "tusb_config.h"
#include "tusb.h"
#include "usb_control.h"
#include "hardware/timer.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"

#define USB320_ID_GPIO 2
#define USB_POWERSWITCH_EN_GPIO 46
#define USB_POWERSWITCH_FLG_GPIO 47

static Keyboard_Device_t* kbd;

#include "letters.h"
// --------------------------------------------------------------------+
// ASCII Lookup Table (Letters, Numbers, Space, Enter)
// First column is unshifted, second colwumn is shifted
// --------------------------------------------------------------------+
static const char keycode2ascii[128][2] = {
    [HID_KEY_A] = {'a', 'A'}, [HID_KEY_B] = {'b', 'B'}, [HID_KEY_C] = {'c', 'C'},
    [HID_KEY_D] = {'d', 'D'}, [HID_KEY_E] = {'e', 'E'}, [HID_KEY_F] = {'f', 'F'},
    [HID_KEY_G] = {'g', 'G'}, [HID_KEY_H] = {'h', 'H'}, [HID_KEY_I] = {'i', 'I'},
    [HID_KEY_J] = {'j', 'J'}, [HID_KEY_K] = {'k', 'K'}, [HID_KEY_L] = {'l', 'L'},
    [HID_KEY_M] = {'m', 'M'}, [HID_KEY_N] = {'n', 'N'}, [HID_KEY_O] = {'o', 'O'},
    [HID_KEY_P] = {'p', 'P'}, [HID_KEY_Q] = {'q', 'Q'}, [HID_KEY_R] = {'r', 'R'},
    [HID_KEY_S] = {'s', 'S'}, [HID_KEY_T] = {'t', 'T'}, [HID_KEY_U] = {'u', 'U'},
    [HID_KEY_V] = {'v', 'V'}, [HID_KEY_W] = {'w', 'W'}, [HID_KEY_X] = {'x', 'X'},
    [HID_KEY_Y] = {'y', 'Y'}, [HID_KEY_Z] = {'z', 'Z'},
    [HID_KEY_1] = {'1', '!'}, [HID_KEY_2] = {'2', '@'}, [HID_KEY_3] = {'3', '#'},
    [HID_KEY_4] = {'4', '$'}, [HID_KEY_5] = {'5', '%'}, [HID_KEY_6] = {'6', '^'},
    [HID_KEY_7] = {'7', '&'}, [HID_KEY_8] = {'8', '*'}, [HID_KEY_9] = {'9', '('},
    [HID_KEY_0] = {'0', ')'},
    [HID_KEY_ENTER] = {ENTER, ENTER}, [HID_KEY_SPACE] = {' ', ' '},
    [HID_KEY_BACKSPACE] = {'\b', '\b'},
    [HID_KEY_ARROW_DOWN] = {ARROW_UP, ARROW_UP}, [HID_KEY_ARROW_UP] = {ARROW_DOWN, ARROW_DOWN},
};

// --------------------------------------------------------------------+
// Application Logic
// --------------------------------------------------------------------+



// Helper function to check if a specific keycode is currently in a report
static inline bool is_key_held(hid_keyboard_report_t const *report, uint8_t keycode) {
    for(uint8_t i = 0; i < 6; i++) {
        if (report->keycode[i] == keycode) return true;
    }
    return false;
}

// Parses the 8-byte report and prints the characters
void process_kbd_report(hid_keyboard_report_t const *report) {
    // Keep track of the last report so we only print new key presses
    static hid_keyboard_report_t prev_report = { 0 };


    kbd->start_time_us = timer_hw->timerawl;

    // Check if Left Shift or Right Shift is being held down
    bool is_shift = report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT);

    // Loop through the 6 possible simultaneous keypress slots
    for(uint8_t i = 0; i < 6; i++) {
        uint8_t keycode = report->keycode[i];

        // If there is a keycode, AND it wasn't pressed in the previous report
        if (keycode && !is_key_held(&prev_report, keycode)) {

            // Look up the ASCII character (using shift logic)
            char ch = keycode2ascii[keycode][is_shift ? 1 : 0];

            if (ch) {
                // Give give to struct and set the ready flag
                kbd->last_key = ch;
                kbd->key_ready = true;
            }
        }
    }

    // Save the current report as the previous report for the next loop
    prev_report = *report;
}

void usb_task(void)
{
   tuh_task();
}

void usb_init(Keyboard_Device_t *keyboard)
{
    kbd = keyboard;
    kbd->connected = false;

    // setUsbPowerOutput(1);
    tusb_init();
}

void setUsbPowerOutput(bool power_on)
{
    gpio_put(USB_POWERSWITCH_EN_GPIO, power_on ? 1 : 0);
}

void USB320_ID_GPIO_callback(uint gpio, uint32_t events)
{
    //Device Attached
    if(events & (1<<GPIO_IRQ_EDGE_FALL)) {
        gpio_acknowledge_irq(USB320_ID_GPIO, GPIO_IRQ_EDGE_FALL);
        kbd->device_attached = ID_DEVICE_ATTACHED;
        setUsbPowerOutput(true);
    }
    //Host/ Nothing Attached
    else if(gpio_get_irq_event_mask(USB320_ID_GPIO) & GPIO_IRQ_EDGE_RISE) {
        gpio_acknowledge_irq(USB320_ID_GPIO, GPIO_IRQ_EDGE_RISE);
        // Host or nothing attached
        kbd->device_attached = ID_HOST_ATTACHED;
         setUsbPowerOutput(false);
    }
}

void AP2171_FLG_GPIO_callback(uint gpio, uint32_t events)
{
    //Power Switch Flag triggered (overcurrent or overtemp)
    if(events & (1<<GPIO_IRQ_EDGE_FALL)) {
        gpio_acknowledge_irq(USB_POWERSWITCH_FLG_GPIO, GPIO_IRQ_EDGE_FALL);
        setUsbPowerOutput(false); // Cut power to the USB device
    }
}

void usb_initPeripherals(Keyboard_Device_t *kbd)
{
    // Initialize GPIOs for USB power control and detection
    gpio_init(USB320_ID_GPIO);
    gpio_set_dir(USB320_ID_GPIO, GPIO_IN);
    gpio_pull_up(USB320_ID_GPIO);
    //Setup interrupt for USB ID pin to detect when a device/host is attached or detached
    gpio_set_irq_enabled_with_callback(USB320_ID_GPIO, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, USB320_ID_GPIO_callback);

    gpio_init(USB_POWERSWITCH_EN_GPIO);
    gpio_set_dir(USB_POWERSWITCH_EN_GPIO, GPIO_OUT);
    gpio_put(USB_POWERSWITCH_EN_GPIO, 0); // Start with power off

    gpio_init(USB_POWERSWITCH_FLG_GPIO);
    gpio_set_dir(USB_POWERSWITCH_FLG_GPIO, GPIO_IN);
    gpio_pull_up(USB_POWERSWITCH_FLG_GPIO);
    gpio_set_irq_enabled_with_callback(USB_POWERSWITCH_FLG_GPIO, GPIO_IRQ_EDGE_FALL, true, AP2171_FLG_GPIO_callback);


}

// --------------------------------------------------------------------+
// TinyUSB HID Callbacks
// --------------------------------------------------------------------+

// 1. Invoked when a device is plugged in
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    kbd->connected = true;
    tuh_hid_receive_report(dev_addr, instance);
}

// 2. Invoked when a device is unplugged
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    kbd->connected = false;
    printf("\n[Keyboard Disconnected]\n");
}

// 3. Invoked when the keyboard sends data
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {

    // Cast the raw byte array into the TinyUSB keyboard struct
    hid_keyboard_report_t const *kbd_report = (hid_keyboard_report_t const *) report;

    // Process the keystrokes
    process_kbd_report(kbd_report);

    // Ask the keyboard for the next report
    tuh_hid_receive_report(dev_addr, instance);
}
