#include "../wlan_app.h"
#include "../wlan_hal.h"

#include <dialogs/dialogs.h>
#include <storage/storage.h>

enum {
    SsidSpamModeFunny = 0,
    SsidSpamModeRickroll = 1,
    SsidSpamModeRandom = 2,
    SsidSpamModeCustom = 3,
    SsidSpamModeFile = 4,
};

static void ssid_spam_submenu_cb(void* context, uint32_t index) {
    WlanApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static bool ssid_spam_select_file(WlanApp* app) {
    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
    furi_record_open(RECORD_STORAGE);
    FuriString* file_path = furi_string_alloc_set_str(STORAGE_EXT_PATH_PREFIX);
    DialogsFileBrowserOptions browser;
    dialog_file_browser_set_basic_options(&browser, "txt", NULL);
    browser.base_path = STORAGE_EXT_PATH_PREFIX;

    bool ok = dialog_file_browser_show(dialogs, file_path, file_path, &browser);
    if(ok) {
        strncpy(app->beacon_file_path, furi_string_get_cstr(file_path), sizeof(app->beacon_file_path) - 1);
        app->beacon_file_path[sizeof(app->beacon_file_path) - 1] = '\0';
    } else {
        app->beacon_file_path[0] = '\0';
    }

    furi_string_free(file_path);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_DIALOGS);
    return ok;
}

void wlan_app_scene_ssid_spam_on_enter(void* context) {
    WlanApp* app = context;
    submenu_reset(app->submenu);
    submenu_set_header_centered(app->submenu, "SSID Spam");
    submenu_add_item(app->submenu, "Funny SSIDs", SsidSpamModeFunny,
        ssid_spam_submenu_cb, app);
    submenu_add_item(app->submenu, "Rick Roll", SsidSpamModeRickroll,
        ssid_spam_submenu_cb, app);
    submenu_add_item(app->submenu, "Random SSIDs", SsidSpamModeRandom,
        ssid_spam_submenu_cb, app);
    submenu_add_item(app->submenu, "Custom SSIDs", SsidSpamModeCustom,
        ssid_spam_submenu_cb, app);
    submenu_add_item(app->submenu, "From .txt File", SsidSpamModeFile,
        ssid_spam_submenu_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, WlanAppViewSubmenu);
}

bool wlan_app_scene_ssid_spam_on_event(void* context, SceneManagerEvent event) {
    WlanApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case SsidSpamModeFunny:
            app->beacon_mode = WlanHalBeaconModeFunny;
            scene_manager_next_scene(app->scene_manager, WlanAppSceneSsidSpamRun);
            consumed = true;
            break;
        case SsidSpamModeRickroll:
            app->beacon_mode = WlanHalBeaconModeRickroll;
            scene_manager_next_scene(app->scene_manager, WlanAppSceneSsidSpamRun);
            consumed = true;
            break;
        case SsidSpamModeRandom:
            app->beacon_mode = WlanHalBeaconModeRandom;
            scene_manager_next_scene(app->scene_manager, WlanAppSceneSsidSpamRun);
            consumed = true;
            break;
        case SsidSpamModeCustom:
            scene_manager_next_scene(app->scene_manager, WlanAppSceneSsidSpamCustom);
            consumed = true;
            break;
        case SsidSpamModeFile:
            app->beacon_mode = WlanHalBeaconModeFileList;
            if(ssid_spam_select_file(app)) {
                scene_manager_next_scene(app->scene_manager, WlanAppSceneSsidSpamRun);
                consumed = true;
            }
            break;
        }
    }
    return consumed;
}

void wlan_app_scene_ssid_spam_on_exit(void* context) {
    WlanApp* app = context;
    submenu_reset(app->submenu);
}
