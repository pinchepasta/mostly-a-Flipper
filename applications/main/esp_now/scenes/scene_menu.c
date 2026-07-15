#include "../esp_now_app.h"

#include <dialogs/dialogs.h>
#include <esp_log.h>
#include <esp_now.h>
#include <storage/storage.h>
#include <stdio.h>
#include <string.h>

static const uint8_t kBroadcastMac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

enum SubmenuIndex {
    SubmenuIndexReceive,
    SubmenuIndexSend,
};

static void esp_now_scene_menu_submenu_callback(void* context, uint32_t index) {
    EspNowApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static bool esp_now_app_menu_ensure_peer(const uint8_t mac[6]) {
    if(esp_now_is_peer_exist(mac)) return true;

    esp_now_peer_info_t peer_info = {0};
    memcpy(peer_info.peer_addr, mac, 6);
    peer_info.channel = 1;
    peer_info.ifidx = WIFI_IF_STA;
    peer_info.encrypt = false;

    esp_err_t err = esp_now_add_peer(&peer_info);
    if(err == ESP_OK || err == ESP_ERR_ESPNOW_EXIST) {
        return true;
    }
    ESP_LOGW("EspNowApp", "esp_now_add_peer failed: %s", esp_err_to_name(err));
    return false;
}

static void esp_now_app_menu_show_message(const char* title, const char* body) {
    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
    DialogMessage* message = dialog_message_alloc();
    dialog_message_set_header(message, title, 0, 0, AlignCenter, AlignTop);
    dialog_message_set_text(message, body, 0, 16, AlignCenter, AlignTop);
    dialog_message_set_buttons(message, NULL, "OK", NULL);
    dialog_message_show(dialogs, message);
    dialog_message_free(message);
    furi_record_close(RECORD_DIALOGS);
}

static void esp_now_app_menu_send_file(EspNowApp* app) {
    if(!esp_now_app_start(app)) {
        esp_now_app_menu_show_message("Send Failed", "Unable to start ESP-NOW");
        return;
    }

    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
    Storage* storage = furi_record_open(RECORD_STORAGE);

    FuriString* file_path = furi_string_alloc_set_str(STORAGE_EXT_PATH_PREFIX);
    DialogsFileBrowserOptions browser;
    dialog_file_browser_set_basic_options(&browser, NULL, NULL);
    browser.base_path = STORAGE_EXT_PATH_PREFIX;

    if(!dialog_file_browser_show(dialogs, file_path, file_path, &browser)) {
        furi_string_free(file_path);
        furi_record_close(RECORD_STORAGE);
        furi_record_close(RECORD_DIALOGS);
        return;
    }

    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, furi_string_get_cstr(file_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        esp_now_app_menu_show_message("Send Failed", "Unable to open file");
        storage_file_free(file);
        furi_string_free(file_path);
        furi_record_close(RECORD_STORAGE);
        furi_record_close(RECORD_DIALOGS);
        return;
    }

    if(!esp_now_app_menu_ensure_peer(kBroadcastMac)) {
        esp_now_app_menu_show_message("Send Failed", "Unable to register broadcast peer");
        storage_file_close(file);
        storage_file_free(file);
        furi_string_free(file_path);
        furi_record_close(RECORD_STORAGE);
        furi_record_close(RECORD_DIALOGS);
        return;
    }

    uint8_t buffer[ESP_NOW_PKT_MAX_DATA];
    size_t total_sent = 0;
    size_t packet_count = 0;
    bool send_error = false;

    while(!storage_file_eof(file)) {
        size_t read_len = storage_file_read(file, buffer, sizeof(buffer));
        if(read_len == 0) break;

        esp_err_t err = esp_now_send(kBroadcastMac, buffer, read_len);
        if(err != ESP_OK) {
            send_error = true;
            break;
        }

        total_sent += read_len;
        packet_count++;
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(file_path);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_DIALOGS);

    if(send_error) {
        esp_now_app_menu_show_message("Send Failed", "ESP-NOW send error");
    } else {
        char status[64];
        snprintf(status, sizeof(status), "Sent %zu bytes in %zu packets", total_sent, packet_count);
        esp_now_app_menu_show_message("File Sent", status);
    }

    esp_now_app_stop(app);
}

void esp_now_app_scene_menu_on_enter(void* context) {
    EspNowApp* app = context;

    submenu_add_item(
        app->submenu,
        "Receive a file via esp now",
        SubmenuIndexReceive,
        esp_now_scene_menu_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Choose a file and send it with esp now",
        SubmenuIndexSend,
        esp_now_scene_menu_submenu_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, EspNowViewSubmenu);
}

bool esp_now_app_scene_menu_on_event(void* context, SceneManagerEvent event) {
    EspNowApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexReceive) {
            scene_manager_next_scene(app->scene_manager, EspNowAppSceneSniff);
            consumed = true;
        } else if(event.event == SubmenuIndexSend) {
            esp_now_app_menu_send_file(app);
            consumed = true;
        }
    }

    return consumed;
}

void esp_now_app_scene_menu_on_exit(void* context) {
    EspNowApp* app = context;
    submenu_reset(app->submenu);
}
