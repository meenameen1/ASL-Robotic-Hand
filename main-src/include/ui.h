#ifndef UI_H
#define UI_H

#include "usb_control.h"
#include "lcd.h"
#include "oled.h"

typedef enum
{
   STATE_IDLE,
   STATE_TYPING,
   STATE_TRANSLATING,
   STATE_ERROR,
   STATE_KEYBOARD_DISCONNECTED,
} UI_State_t;

typedef struct
{
   UI_State_t state;
   Keyboard_Device_t* keyboard;
   LCD_t* lcd;
   OLED_Mode_t oled_mode;
} UI_Context_t;

void ui_state_machine(UI_Context_t* ui);

#endif
