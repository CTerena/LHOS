#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include "device_utils.h"
#include "../types.h"

#define KEYBOARD_IRQ 1
#define KEYBOARD_PORT 0x60

typedef struct {
    int shift;
    int caps;
    int ctrl;
    int alt;
} keyboard_state_t;

/* Keycodes */
typedef uint8_t keycode_t;

enum keycodes_ {
    KEY_RESERVED = 0,

    /* Printable area begins */
    KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,
    KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T,
    KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
    KEY_GRAVE, KEY_MINUS, KEY_EQUAL, KEY_LEFTBRACE, KEY_RIGHTBRACE, KEY_BACKSLASH, KEY_SEMICOLON, KEY_APOSTROPHE, KEY_COMMA, KEY_DOT, KEY_SLASH, KEY_SPACE,
    /* Printable area ends */

    KEY_ESC, KEY_BACKSPACE, KEY_TAB, KEY_ENTER, KEY_LEFTCTRL, KEY_LEFTSHIFT,
    KEY_RIGHTSHIFT, KEY_KPASTERISK, KEY_LEFTALT, KEY_CAPSLOCK,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10,
    KEY_NUMLOCK, KEY_SCROLLLOCK,
    KEY_KP7, KEY_KP8, KEY_KP9, KEY_KPMINUS, KEY_KP4, KEY_KP5, KEY_KP6, 
    KEY_KPPLUS, KEY_KP1, KEY_KP2, KEY_KP3, KEY_KP0, KEY_KPDOT,
};

DECLARE_DEVICE_HANDLER(keyboard);
DECLARE_DEVICE_INIT(keyboard);

#endif /* _KEYBOARD_H */
