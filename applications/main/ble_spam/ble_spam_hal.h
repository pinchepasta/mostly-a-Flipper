#pragma once

#include <stdbool.h>
#include <stdint.h>

/** True if there is enough free internal RAM to bring up the BLE controller +
 *  Bluedroid for spam/walk. On a no-PSRAM board the app's own UI + UUID DB can
 *  leave less than this, in which case ble_spam_hal_start() would fail; callers
 *  should check this at launch and refuse gracefully instead of retry-looping. */
bool ble_spam_hal_have_ram(void);

/** Stop btshim, init BLE controller + Bluedroid for raw advertising.
 *  @return true on success */
bool ble_spam_hal_start(void);

/** Stop BLE spam advertising, deinit stack, restart btshim. */
void ble_spam_hal_stop(void);

/** Set raw advertising data and start advertising.
 *  @param data   complete raw AD structure (max 31 bytes)
 *  @param len    data length
 *  @return true if data was set successfully */
bool ble_spam_hal_set_adv_data(const uint8_t* data, uint8_t len);

/** Stop current advertising (call between payload switches). */
void ble_spam_hal_stop_adv(void);

/** Set a new random static BLE address. Call while advertising is stopped. */
void ble_spam_hal_set_random_addr(void);

/** Set a specific BLE address for cloning. Call while advertising is stopped. */
void ble_spam_hal_set_addr(const uint8_t addr[6]);
