#pragma once
#include "stdint.h"
#include "stdbool.h"
#include <furi_hal_resources.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RECORD_NOTIFICATION "notification"

typedef struct NotificationApp NotificationApp;
typedef struct {
    float frequency;
    float volume;
} NotificationMessageDataSound;

typedef struct {
    uint8_t value;
} NotificationMessageDataLed;

typedef struct {
    bool on;
} NotificationMessageDataVibro;

typedef struct {
    uint32_t length;
} NotificationMessageDataDelay;

typedef struct {
    float speaker_volume;
    bool vibro;
    float display_brightness;
} NotificationMessageDataForcedSettings;

typedef struct {
    uint16_t on_time;
    uint16_t period;
    Light color;
} NotificationMessageDataLedBlink;

typedef union {
    NotificationMessageDataSound sound;
    NotificationMessageDataLed led;
    NotificationMessageDataLedBlink led_blink;
    NotificationMessageDataVibro vibro;
    NotificationMessageDataDelay delay;
    NotificationMessageDataForcedSettings forced_settings;
} NotificationMessageData;

typedef enum {
    NotificationMessageTypeVibro,

    NotificationMessageTypeSoundOn,
    NotificationMessageTypeSoundOff,

    NotificationMessageTypeLedRed,
    NotificationMessageTypeLedGreen,
    NotificationMessageTypeLedBlue,

    NotificationMessageTypeLedBlinkStart,
    NotificationMessageTypeLedBlinkStop,
    NotificationMessageTypeLedBlinkColor,

    NotificationMessageTypeDelay,

    NotificationMessageTypeLedDisplayBacklight,
    NotificationMessageTypeLedDisplayBacklightForceOn,
    NotificationMessageTypeLedDisplayBacklightEnforceOn,
    NotificationMessageTypeLedDisplayBacklightEnforceAuto,

    NotificationMessageTypeDoNotReset,

    NotificationMessageTypeForceSpeakerVolumeSetting,
    NotificationMessageTypeForceVibroSetting,
    NotificationMessageTypeForceDisplayBrightnessSetting,

    NotificationMessageTypeLedBrightnessSettingApply,

    NotificationMessageTypeLcdContrastUpdate,
} NotificationMessageType;

typedef struct {
    NotificationMessageType type;
    NotificationMessageData data;
} NotificationMessage;

typedef const NotificationMessage* NotificationSequence[];

void notification_message(NotificationApp* app, const NotificationSequence* sequence);
void notification_message_block(NotificationApp* app, const NotificationSequence* sequence);

/**
 * @brief Send internal (apply to permanent layer) notification message. Think twice before use.
 *
 * @param app notification record content
 * @param sequence notification sequence
 */
void notification_internal_message(NotificationApp* app, const NotificationSequence* sequence);

/**
 * @brief Send internal (apply to permanent layer) notification message and wait for notification end. Think twice before use.
 *
 * @param app notification record content
 * @param sequence notification sequence
 */
void notification_internal_message_block(
    NotificationApp* app,
    const NotificationSequence* sequence);

/**
 * @brief Flush any pending settings save (LCD backlight, UI colors, LED
 * config, night shift, volume) to flash and block until it's durably
 * written. The regular save path is a fire-and-forget queue message, which
 * is fine during normal use but must not be raced against a reboot or
 * power-off — call this first in that case, or a save queued moments
 * earlier can be lost, which is what makes display settings appear to
 * "reset" after a restart.
 *
 * @param app notification record content
 */
void notification_message_save_settings_blocking(NotificationApp* app);

#ifdef __cplusplus
}
#endif
