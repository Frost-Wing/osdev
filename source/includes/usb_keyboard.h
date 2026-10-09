/**
 * @file usb_keyboard.h
 * @brief USB HID boot-keyboard driver interface.
 */
#ifndef USB_KEYBOARD_H
#define USB_KEYBOARD_H

/** Register the USB HID boot-keyboard class driver. */
int usb_keyboard_init(void);

/** Poll connected boot keyboards for their latest input reports. */
void usb_keyboard_poll(void);

#endif
