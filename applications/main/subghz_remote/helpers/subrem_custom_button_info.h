#pragma once

#include <stdint.h>

enum CustomButton {
    ButtonOK,
    Button1,
    Button2,
    Button3,
    Button4,
    Button5,
    Button6,
    Button7,
    Button8,
    Button9,
    NumButtons
};

extern const char* const custom_button_text[NumButtons];

const char* subrem_custom_button_display_text(uint8_t button);
