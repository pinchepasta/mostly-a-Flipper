#include "../esp_now_app.h"

#include <esp_now.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <string.h>

#define TAG "EspNowSniff"
#define ESP_NOW_RX_QUEUE_LENGTH 8

typedef struct {
    uint8_t mac[6];
    uint8_t data[ESP_NOW_PKT_MAX_DATA];
    uint8_t len;
} EspNowRxItem;

static EspNowApp* g_app = NULL;
static QueueHandle_t g_esp_now_rx_queue = NULL;
static TaskHandle_t g_esp_now_rx_task = NULL;
static volatile bool g_esp_now_rx_running = false;

static void esp_now_rx_worker_task(void* arg) {
    (void)arg;
    EspNowRxItem item;
    while(g_esp_now_rx_running || xQueueReceive(g_esp_now_rx_queue, &item, pdMS_TO_TICKS(100)) == pdTRUE) {
        if(!g_app) continue;
        if(furi_mutex_acquire(g_app->mutex, FuriWaitForever) != FuriStatusOk) continue;
        if(g_app->packet_count < g_app->packet_capacity) {
            EspNowPacket* pkt = &g_app->packets[g_app->packet_count];
            memcpy(pkt->mac, item.mac, sizeof(pkt->mac));
            memcpy(pkt->data, item.data, item.len);
            pkt->data_len = item.len;
            pkt->timestamp = xTaskGetTickCount();
            g_app->packet_count++;
        }
        furi_mutex_release(g_app->mutex);
    }
}

static void esp_now_recv_cb(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if(!g_esp_now_rx_queue || !info || !data || len <= 0) return;

    EspNowRxItem item;
    memcpy(item.mac, info->src_addr, sizeof(item.mac));
    item.len = (uint8_t)((size_t)len > ESP_NOW_PKT_MAX_DATA ? ESP_NOW_PKT_MAX_DATA : (size_t)len);
    memcpy(item.data, data, item.len);
    xQueueSend(g_esp_now_rx_queue, &item, 0);
}

static void esp_now_sniff_start(EspNowApp* app) {
    if(app->sniffing) return;

    if(!esp_now_app_start(app)) {
        ESP_LOGW(TAG, "Failed to start ESP-NOW");
        return;
    }

    g_esp_now_rx_queue = xQueueCreate(ESP_NOW_RX_QUEUE_LENGTH, sizeof(EspNowRxItem));
    if(!g_esp_now_rx_queue) {
        ESP_LOGE(TAG, "Failed to create ESP-NOW RX queue");
        return;
    }

    if(xTaskCreate(esp_now_rx_worker_task, "esp_now_rx", 4096, NULL, 5, &g_esp_now_rx_task) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create ESP-NOW RX worker");
        vQueueDelete(g_esp_now_rx_queue);
        g_esp_now_rx_queue = NULL;
        return;
    }

    g_esp_now_rx_running = true;
    g_app = app;
    esp_now_register_recv_cb(esp_now_recv_cb);
    ESP_LOGI(TAG, "ESP-NOW sniffing active");
}

static void esp_now_sniff_stop(EspNowApp* app) {
    if(!app->sniffing) return;

    ESP_LOGI(TAG, "Stopping ESP-NOW + WiFi");

    g_esp_now_rx_running = false;
    esp_now_unregister_recv_cb();
    g_app = NULL;
    if(g_esp_now_rx_task) {
        vTaskDelete(g_esp_now_rx_task);
        g_esp_now_rx_task = NULL;
    }
    if(g_esp_now_rx_queue) {
        vQueueDelete(g_esp_now_rx_queue);
        g_esp_now_rx_queue = NULL;
    }
    esp_now_app_stop(app);
}

static void esp_now_scene_sniff_submenu_callback(void* context, uint32_t index) {
    EspNowApp* app = context;
    app->selected_index = index;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void esp_now_app_scene_sniff_on_enter(void* context) {
    EspNowApp* app = context;

    // Reset packet storage
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->packet_count = 0;
    app->last_displayed_count = 0;
    furi_mutex_release(app->mutex);

    submenu_set_header(app->submenu, "esp now Sniffer");
    view_dispatcher_switch_to_view(app->view_dispatcher, EspNowViewSubmenu);

    esp_now_sniff_start(app);
}

bool esp_now_app_scene_sniff_on_event(void* context, SceneManagerEvent event) {
    EspNowApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        // Packet selected
        scene_manager_next_scene(app->scene_manager, EspNowAppScenePacketInfo);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeTick) {
        // Check for new packets and update submenu
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        size_t count = app->packet_count;

        if(count > app->last_displayed_count) {
            for(size_t i = app->last_displayed_count; i < count; i++) {
                EspNowPacket* pkt = &app->packets[i];
                furi_string_printf(
                    app->text_buf,
                    "%02X:%02X:%02X:%02X:%02X:%02X (%d B)",
                    pkt->mac[0],
                    pkt->mac[1],
                    pkt->mac[2],
                    pkt->mac[3],
                    pkt->mac[4],
                    pkt->mac[5],
                    pkt->data_len);
                submenu_add_item(
                    app->submenu,
                    furi_string_get_cstr(app->text_buf),
                    i,
                    esp_now_scene_sniff_submenu_callback,
                    app);
            }
            app->last_displayed_count = count;
        }

        furi_mutex_release(app->mutex);
    }

    return consumed;
}

void esp_now_app_scene_sniff_on_exit(void* context) {
    EspNowApp* app = context;
    esp_now_sniff_stop(app);
    submenu_reset(app->submenu);
}
