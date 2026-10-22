#include "rolljam_espnow.h"

#include <string.h>
#include <furi.h>

#include <esp_idf_version.h>
#include <esp_err.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <nvs_flash.h>

#define TAG "RolljamEspNow"

typedef struct {
    RolljamPacket packet;
    uint8_t mac[6];
    int8_t rssi;
} RolljamEvent;

struct RolljamEspNow {
    FuriMessageQueue* queue;
    RolljamPeer peers[ROLLJAM_MAX_PEERS];
    uint8_t peer_count;
    uint32_t seq;
    bool up;
};

/* ESP-NOW callbacks carry no context, so a single live instance is assumed. */
static RolljamEspNow* rolljam_singleton = NULL;

static const uint8_t rolljam_broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

const char* rolljam_role_name(uint8_t role) {
    switch(role) {
    case RolljamRoleSniffer: return "SNIFFER";
    case RolljamRoleJammer:  return "JAMMER";
    default:                 return "UNSET";
    }
}

static int rolljam_peer_find(RolljamEspNow* instance, const uint8_t* mac) {
    for(uint8_t i = 0; i < instance->peer_count; i++) {
        if(!memcmp(instance->peers[i].mac, mac, 6)) return i;
    }
    return -1;
}

static void
    rolljam_peer_touch(RolljamEspNow* instance, const uint8_t* mac, uint8_t role, int8_t rssi) {
    int idx = rolljam_peer_find(instance, mac);
    if(idx < 0) {
        if(instance->peer_count >= ROLLJAM_MAX_PEERS) return;
        idx = instance->peer_count++;
        memset(&instance->peers[idx], 0, sizeof(RolljamPeer));
        memcpy(instance->peers[idx].mac, mac, 6);
        FURI_LOG_I(
            TAG,
            "peer joined %02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    instance->peers[idx].role = role;
    instance->peers[idx].rssi = rssi;
    instance->peers[idx].last_seen = furi_get_tick();
}

static void rolljam_peer_register(const uint8_t* mac) {
    if(esp_now_is_peer_exist(mac)) return;
    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(peer));
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = 0;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    if(esp_now_add_peer(&peer) != ESP_OK) FURI_LOG_W(TAG, "add_peer failed");
}

static void rolljam_send_cb(const uint8_t* mac, esp_now_send_status_t status) {
    UNUSED(mac);
    if(status != ESP_NOW_SEND_SUCCESS) FURI_LOG_D(TAG, "send status %d", (int)status);
}

#if defined(ESP_IDF_VERSION_MAJOR) && (ESP_IDF_VERSION_MAJOR >= 5)
static void rolljam_recv_cb(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    const uint8_t* mac = info ? info->src_addr : NULL;
    int8_t rssi = (info && info->rx_ctrl) ? info->rx_ctrl->rssi : 0;
#else
static void rolljam_recv_cb(const uint8_t* mac, const uint8_t* data, int len) {
    int8_t rssi = 0;
#endif
    RolljamEspNow* instance = rolljam_singleton;
    if(!instance || !mac || !data) return;
    if(len < (int)sizeof(RolljamPacket)) return;

    RolljamPacket packet;
    memcpy(&packet, data, sizeof(packet));
    if(packet.magic != ROLLJAM_ESPNOW_MAGIC) return;

    rolljam_peer_touch(instance, mac, packet.role, rssi);

    RolljamEvent event;
    event.packet = packet;
    memcpy(event.mac, mac, 6);
    event.rssi = rssi;
    furi_message_queue_put(instance->queue, &event, 0);
}

RolljamEspNow* rolljam_espnow_alloc(void) {
    RolljamEspNow* instance = malloc(sizeof(RolljamEspNow));
    memset(instance, 0, sizeof(RolljamEspNow));
    instance->queue = furi_message_queue_alloc(16, sizeof(RolljamEvent));
    return instance;
}

void rolljam_espnow_free(RolljamEspNow* instance) {
    furi_assert(instance);
    rolljam_espnow_stop(instance);
    furi_message_queue_free(instance->queue);
    free(instance);
}

bool rolljam_espnow_start(RolljamEspNow* instance, uint8_t channel) {
    furi_assert(instance);
    if(instance->up) return true;

    rolljam_singleton = instance;

    esp_err_t err = nvs_flash_init();
    if(err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if(err != ESP_OK && err != ESP_ERR_INVALID_STATE) FURI_LOG_W(TAG, "nvs: %d", (int)err);

    /* The rest of the firmware may already own these; tolerate INVALID_STATE. */
    err = esp_netif_init();
    if(err != ESP_OK && err != ESP_ERR_INVALID_STATE) FURI_LOG_W(TAG, "netif: %d", (int)err);

    err = esp_event_loop_create_default();
    if(err != ESP_OK && err != ESP_ERR_INVALID_STATE) FURI_LOG_W(TAG, "event loop: %d", (int)err);

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wifi_cfg);
    if(err != ESP_OK && err != ESP_ERR_INVALID_STATE) FURI_LOG_W(TAG, "wifi init: %d", (int)err);

    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);
    if(esp_wifi_start() != ESP_OK) FURI_LOG_W(TAG, "wifi start failed");
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_ps(WIFI_PS_NONE);

    if(esp_now_init() != ESP_OK) {
        FURI_LOG_E(TAG, "esp_now_init failed");
        rolljam_singleton = NULL;
        return false;
    }

    esp_now_register_recv_cb(rolljam_recv_cb);
    esp_now_register_send_cb(rolljam_send_cb);
    rolljam_peer_register(rolljam_broadcast_mac);

    instance->up = true;
    FURI_LOG_I(TAG, "espnow up on channel %u", channel);
    return true;
}

void rolljam_espnow_stop(RolljamEspNow* instance) {
    furi_assert(instance);
    if(!instance->up) return;
    esp_now_unregister_recv_cb();
    esp_now_unregister_send_cb();
    esp_now_deinit();
    esp_wifi_stop();
    esp_wifi_deinit();
    instance->up = false;
    rolljam_singleton = NULL;
}

bool rolljam_espnow_is_up(RolljamEspNow* instance) {
    furi_assert(instance);
    return instance->up;
}

bool rolljam_espnow_send(RolljamEspNow* instance, const RolljamPacket* packet) {
    furi_assert(instance);
    if(!instance->up) return false;

    RolljamPacket out = *packet;
    out.magic = ROLLJAM_ESPNOW_MAGIC;
    out.version = ROLLJAM_ESPNOW_VERSION;
    out.seq = ++instance->seq;

    rolljam_peer_register(rolljam_broadcast_mac);
    return esp_now_send(rolljam_broadcast_mac, (const uint8_t*)&out, sizeof(out)) == ESP_OK;
}

bool rolljam_espnow_recv(
    RolljamEspNow* instance,
    RolljamPacket* packet,
    uint8_t* src_mac,
    int8_t* rssi,
    uint32_t timeout_ms) {
    furi_assert(instance);
    RolljamEvent event;
    if(furi_message_queue_get(instance->queue, &event, timeout_ms) != FuriStatusOk) return false;
    if(packet) *packet = event.packet;
    if(src_mac) memcpy(src_mac, event.mac, 6);
    if(rssi) *rssi = event.rssi;
    return true;
}

uint8_t rolljam_espnow_peer_count(RolljamEspNow* instance) {
    furi_assert(instance);
    return instance->peer_count;
}

bool rolljam_espnow_peer_get(RolljamEspNow* instance, uint8_t index, RolljamPeer* peer) {
    furi_assert(instance);
    if(index >= instance->peer_count) return false;
    *peer = instance->peers[index];
    return true;
}

void rolljam_espnow_peer_expire(RolljamEspNow* instance, uint32_t timeout_ms) {
    furi_assert(instance);
    uint32_t now = furi_get_tick();
    for(uint8_t i = 0; i < instance->peer_count;) {
        if((now - instance->peers[i].last_seen) > timeout_ms) {
            FURI_LOG_I(TAG, "peer expired");
            for(uint8_t j = i; j + 1 < instance->peer_count; j++) {
                instance->peers[j] = instance->peers[j + 1];
            }
            instance->peer_count--;
        } else {
            i++;
        }
    }
}

void rolljam_espnow_peer_forget(RolljamEspNow* instance) {
    furi_assert(instance);
    instance->peer_count = 0;
}
