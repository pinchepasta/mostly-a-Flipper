#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef enum {
    XRemoteDeviceProfileGeneral = 0,
    XRemoteDeviceProfileTV,
    XRemoteDeviceProfileSoundSystem,
    XRemoteDeviceProfileFan,
    XRemoteDeviceProfileHeater,
    XRemoteDeviceProfileCustom,
} XRemoteDeviceProfile;
const char* xremote_profile_get_name(XRemoteDeviceProfile profile);
uint8_t xremote_profile_button_count(XRemoteDeviceProfile profile);
const char* xremote_profile_button_name(XRemoteDeviceProfile profile, uint8_t index);
bool xremote_profile_store(const char* remote_path, XRemoteDeviceProfile profile);
bool xremote_profile_load(const char* remote_path, XRemoteDeviceProfile* profile);
