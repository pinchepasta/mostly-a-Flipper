#include "esp_now_app.h"

#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <stdlib.h>

static const char* TAG = "EspNowApp";

static void esp_now_send_cb(const uint8_t* mac_addr, esp_now_send_status_t status) {
    if(mac_addr == NULL) return;
    ESP_LOGD(TAG, "esp_now_send_cb: %02X:%02X:%02X:%02X:%02X:%02X status=%d",
             mac_addr[0],
             mac_addr[1],
             mac_addr[2],
             mac_addr[3],
             mac_addr[4],
             mac_addr[5],
             status);
}

static bool esp_now_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    EspNowApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool esp_now_app_back_event_callback(void* context) {
    furi_assert(context);
    EspNowApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void esp_now_app_tick_event_callback(void* context) {
    furi_assert(context);
    EspNowApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

static EspNowApp* esp_now_app_alloc(void) {
    EspNowApp* app = malloc(sizeof(EspNowApp));

    app->gui = furi_record_open(RECORD_GUI);

    app->scene_manager = scene_manager_alloc(&esp_now_app_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, esp_now_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, esp_now_app_back_event_callback);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, esp_now_app_tick_event_callback, 250);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, EspNowViewSubmenu, submenu_get_view(app->submenu));

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, EspNowViewWidget, widget_get_view(app->widget));

    // Packet storage
    app->packet_capacity = ESP_NOW_PACKETS_MAX;
    app->packets = malloc(sizeof(EspNowPacket) * app->packet_capacity);
    app->packet_count = 0;
    app->last_displayed_count = 0;
    app->selected_index = 0;
    app->sniffing = false;
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    app->text_buf = furi_string_alloc();

    return app;
}

static void esp_now_app_free(EspNowApp* app) {
    esp_now_app_stop(app);

    view_dispatcher_remove_view(app->view_dispatcher, EspNowViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, EspNowViewWidget);

    free(app->submenu);
    free(app->widget);

    free(app->scene_manager);
    free(app->view_dispatcher);

    furi_mutex_free(app->mutex);
    free(app->packets);
    furi_string_free(app->text_buf);

    furi_record_close(RECORD_GUI);
    app->gui = NULL;

    free(app);
}

static bool esp_now_own_event_loop = false;

static bool esp_now_init_once(void) {
    esp_err_t err = esp_netif_init();
    if(err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif_init: %s", esp_err_to_name(err));
        return false;
    }

    err = esp_event_loop_create_default();
    if(err == ESP_OK) {
        esp_now_own_event_loop = true;
    } else if(err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_event_loop_create_default: %s", esp_err_to_name(err));
        return false;
    }

    if(!esp_netif_get_handle_from_ifkey("WIFI_STA_DEF")) {
        esp_netif_create_default_wifi_sta();
    }

    return true;
}

struct EspNowInitArgs {
    EspNowApp* app;
    SemaphoreHandle_t done;
    bool success;
};

static void esp_now_wifi_task(void* arg) {
    struct EspNowInitArgs* init_args = arg;
    EspNowApp* app = init_args->app;

    vTaskSetThreadLocalStoragePointer(NULL, 0, NULL);

    ESP_LOGI(TAG, "Starting WiFi + ESP-NOW in task");

    init_args->success = false;
    if(!esp_now_init_once()) goto done;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.static_rx_buf_num = 8;
    cfg.dynamic_rx_buf_num = 24;
    cfg.dynamic_tx_buf_num = 32;

    esp_err_t err = esp_wifi_init(&cfg);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init: %s", esp_err_to_name(err));
        goto done;
    }

    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_storage: %s", esp_err_to_name(err));
        esp_wifi_deinit();
        goto done;
    }

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_mode: %s", esp_err_to_name(err));
        esp_wifi_deinit();
        goto done;
    }

    err = esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    if(err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_set_protocol: %s", esp_err_to_name(err));
    }

    err = esp_wifi_set_ps(WIFI_PS_NONE);
    if(err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_set_ps: %s", esp_err_to_name(err));
    }

    err = esp_wifi_start();
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start: %s", esp_err_to_name(err));
        esp_wifi_deinit();
        goto done;
    }

    err = esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    if(err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_set_channel: %s", esp_err_to_name(err));
    }

    err = esp_now_init();
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "esp_now_init: %s", esp_err_to_name(err));
        esp_wifi_stop();
        esp_wifi_deinit();
        goto done;
    }

    esp_now_register_send_cb(esp_now_send_cb);

    app->sniffing = true;
    init_args->success = true;

done:
    xSemaphoreGive(init_args->done);
    vTaskDelete(NULL);
}

bool esp_now_app_start(EspNowApp* app) {
    if(app->sniffing) return true;

    app->sniffing = false;

    struct EspNowInitArgs* init_args = malloc(sizeof(*init_args));
    if(!init_args) return false;

    init_args->app = app;
    init_args->done = xSemaphoreCreateBinary();
    init_args->success = false;
    if(!init_args->done) {
        free(init_args);
        return false;
    }

    if(xTaskCreate(esp_now_wifi_task, "esp_now_wifi", 32 * 1024, init_args, 5, NULL) != pdPASS) {
        vSemaphoreDelete(init_args->done);
        free(init_args);
        return false;
    }

    xSemaphoreTake(init_args->done, portMAX_DELAY);
    bool success = init_args->success;
    vSemaphoreDelete(init_args->done);
    free(init_args);
    return success;
}

void esp_now_app_stop(EspNowApp* app) {
    if(!app->sniffing) return;

    ESP_LOGI(TAG, "Stopping ESP-NOW + WiFi");
    esp_now_deinit();
    esp_wifi_stop();
    esp_wifi_deinit();
    if(esp_now_own_event_loop) {
        esp_event_loop_delete_default();
        esp_now_own_event_loop = false;
    }

    app->sniffing = false;
}

int32_t esp_now_app(void* args) {
    UNUSED(args);

    EspNowApp* app = esp_now_app_alloc();

    scene_manager_next_scene(app->scene_manager, EspNowAppSceneMenu);
    view_dispatcher_run(app->view_dispatcher);

    esp_now_app_free(app);
    return 0;
}
