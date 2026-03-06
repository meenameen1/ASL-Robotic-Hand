#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

// -- MCU & OS Configuration --
#define CFG_TUSB_MCU                OPT_MCU_RP2040
#define CFG_TUSB_OS                 OPT_OS_PICO

// -- Port Configuration (CRITICAL FOR HOST MODE) --
#define CFG_TUSB_RHPORT0_MODE       OPT_MODE_HOST

// -- Device Stack (Turn OFF) --
#define CFG_TUD_ENABLED             0

// -- Host Stack (Turn ON) --
#define CFG_TUH_ENABLED             1
#define CFG_TUH_MAX_CHANNELS        1

// -- Host Class Driver Configuration --
#define CFG_TUH_HUB                 1
#define CFG_TUH_HID                 1
#define CFG_TUH_HID_EPIN_BUFSIZE    64

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CONFIG_H_ */
