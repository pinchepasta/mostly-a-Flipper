/**
 * @file target_input.c
 * Improved Input driver for M5Stack Cardputer — Full 8x7 matrix scan with debounce.
 * Based on the plan provided by the user.
 */

#include "target_input.h"
#include <boards/board.h>
#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <furi.h>

#define TAG "InputCardputer"

/* ---- Timing ---- */
#define DEBOUNCE_MS             10U
#define ROW_SETTLE_US           20U
#define INPUT_LONG_PRESS_MS     500U
#define INPUT_REPEAT_MS         200U

/* ---- Navigation key matrix positions (aligned with physical labels) ---- */
/* Logical mapping:
 * Up:    ; (addr 1, row 5)
 * Down:  . (addr 0, row 5)
 * Left:  , (addr 4, row 5)
 * Right: / (addr 4, row 6)
 * OK:    Enter (addr 1, row 6)
 * Back:  Bksp (addr 3, row 6) or Esc (addr 7, row 0)
 */

/* ---- State ---- */
static uint8_t raw_state[8] = {0};
static uint8_t debounced_state[8] = {0};
static uint32_t last_change_time[8] = {0};

typedef struct {
    bool pressed;
    uint32_t press_started_at;
    bool long_press_sent;
    uint32_t last_repeat_at;
} StandardNavState;

static StandardNavState nav_state[InputKeyMAX] = {0};

/* ---- Helpers ---- */

static void input_publish(FuriPubSub* pubsub, InputKey key, InputType type, uint32_t sequence) {
    if(key == InputKeyMAX) return;
    InputEvent event = {
        .sequence_source  = INPUT_SEQUENCE_SOURCE_HARDWARE,
        .sequence_counter = sequence,
        .key              = key,
        .type             = type,
    };
    furi_pubsub_publish(pubsub, &event);
}

/* 
 * Maps matrix (addr, row_idx) to Furi InputKey.
 * row_idx corresponds to the bit position in the 7-bit column read (index in col_pins).
 */
static InputKey get_key_from_matrix(uint8_t addr, uint8_t row_idx) {
    /* Up: ; */
    if(addr == 1 && row_idx == 5) return InputKeyUp;
    /* Down: . */
    if(addr == 0 && row_idx == 5) return InputKeyDown;
    /* Left: , */
    if(addr == 4 && row_idx == 5) return InputKeyLeft;
    /* Right: / */
    if(addr == 4 && row_idx == 6) return InputKeyRight;
    /* OK: Enter */
    if(addr == 1 && row_idx == 6) return InputKeyOk;
    /* Back: Backspace or Esc (`) */
    if((addr == 3 && row_idx == 6) || (addr == 7 && row_idx == 0)) return InputKeyBack;

    return InputKeyMAX;
}

/* ---- Public interface ---- */

void target_input_init(void) {
    /* 1. Detach all matrix pins from any previous peripheral states (e.g., I2C) */
    const gpio_num_t matrix_pins[] = {
        BOARD_KB_PIN_A0, BOARD_KB_PIN_A1, BOARD_KB_PIN_A2, /* 8, 9, 11 */
        13, 15, 3, 4, 5, 6, 7                              /* Columns */
    };
    
    for(int i = 0; i < 10; i++) {
        gpio_reset_pin(matrix_pins[i]);
    }

    /* 2. Decoder address pins as outputs */
    gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << BOARD_KB_PIN_A0) | 
                        (1ULL << BOARD_KB_PIN_A1) | 
                        (1ULL << BOARD_KB_PIN_A2),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&out_cfg));

    /* Column pins as inputs with pullups (13, 15, 3, 4, 5, 6, 7) */
    uint64_t col_mask = (1ULL << 13) | (1ULL << 15) | (1ULL << 3) | 
                        (1ULL << 4)  | (1ULL << 5)  | (1ULL << 6) | (1ULL << 7);
    
    gpio_config_t in_cfg = {
        .pin_bit_mask = col_mask,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&in_cfg));

    FURI_LOG_I(TAG, "Standard Cardputer 8x7 matrix driver initialized (pins reset)");
}

void target_input_poll(FuriPubSub* pubsub, uint32_t* sequence_counter) {
    uint32_t now = furi_get_tick();
    static const uint8_t col_pins[] = {13, 15, 3, 4, 5, 6, 7};

    for(uint8_t addr = 0; addr < 8; addr++) {
        /* Set decoder address */
        gpio_set_level(BOARD_KB_PIN_A0, addr & 0x01);
        gpio_set_level(BOARD_KB_PIN_A1, (addr >> 1) & 0x01);
        gpio_set_level(BOARD_KB_PIN_A2, (addr >> 2) & 0x01);
        
        /* Wait for decoder and matrix to settle */
        esp_rom_delay_us(ROW_SETTLE_US);

        /* Read all 7 columns for this address */
        uint8_t current_row_raw = 0;
        for(uint8_t c = 0; c < 7; c++) {
            if(gpio_get_level(col_pins[c]) == 0) {
                current_row_raw |= (1u << c);
            }
        }

        /* Debounce logic */
        if(current_row_raw != raw_state[addr]) {
            raw_state[addr] = current_row_raw;
            last_change_time[addr] = now;
        } else if((now - last_change_time[addr]) >= furi_ms_to_ticks(DEBOUNCE_MS)) {
            if(current_row_raw != debounced_state[addr]) {
                uint8_t changed = current_row_raw ^ debounced_state[addr];
                
                for(uint8_t c = 0; c < 7; c++) {
                    if((changed >> c) & 1u) {
                        InputKey key = get_key_from_matrix(addr, c);
                        bool pressed = (current_row_raw >> c) & 1u;
                        
                        if(key != InputKeyMAX) {
                            if (pressed) {
                                nav_state[key].pressed = true;
                                nav_state[key].press_started_at = now;
                                nav_state[key].long_press_sent = false;
                                input_publish(pubsub, key, InputTypePress, ++(*sequence_counter));
                            } else {
                                nav_state[key].pressed = false;
                                if (!nav_state[key].long_press_sent) {
                                    input_publish(pubsub, key, InputTypeShort, ++(*sequence_counter));
                                }
                                input_publish(pubsub, key, InputTypeRelease, ++(*sequence_counter));
                            }
                        }
                    }
                }
                debounced_state[addr] = current_row_raw;
            }
        }
    }

    /* --- Long-press and repeat for held nav keys --- */
    uint32_t long_press_ticks = furi_ms_to_ticks(INPUT_LONG_PRESS_MS);
    uint32_t repeat_ticks     = furi_ms_to_ticks(INPUT_REPEAT_MS);

    for (int k = 0; k < InputKeyMAX; k++) {
        if (!nav_state[k].pressed) continue;

        uint32_t held = now - nav_state[k].press_started_at;
        if (!nav_state[k].long_press_sent && held >= long_press_ticks) {
            nav_state[k].long_press_sent = true;
            nav_state[k].last_repeat_at  = now;
            input_publish(pubsub, k, InputTypeLong, ++(*sequence_counter));
        } else if (nav_state[k].long_press_sent && (now - nav_state[k].last_repeat_at) >= repeat_ticks) {
            nav_state[k].last_repeat_at = now;
            input_publish(pubsub, k, InputTypeRepeat, ++(*sequence_counter));
        }
    }
}
