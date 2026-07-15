#include "nrf24_channel_source.h"
#include "nrf24_jam_config.h"
#include "nrf24_jam_presets.h"
#include "../nrf24_app.h"

#include <stdio.h>
#include <string.h>

void nrf24_jam_state_init(Nrf24JamState* st) {
    memset(st, 0, sizeof(*st));
    st->source = Nrf24SourceProtocol;
    /* Default to BLE advertising (ch 37/38/39): only 3 channels but the ones
     * every BLE device uses for discovery/connection setup — concentrating all
     * energy there is what makes BLE jamming actually bite. */
    st->protocol = Nrf24JamPresetBleAdv;
    st->manual_channel = 76;
    /* wifi/activity start unscanned; everything else zeroed. */
}

const char* nrf24_source_type_label(uint8_t source) {
    switch(source) {
    case Nrf24SourceProtocol:
        return "Protocol";
    case Nrf24SourceManual:
        return "Manual";
    case Nrf24SourceWifi:
        return "WiFi";
    case Nrf24SourceActivity:
        return "Activity";
    default:
        return "?";
    }
}

/* WiFi channel center frequency in MHz (2.4 GHz band). */
static uint16_t wifi_channel_center_mhz(uint8_t wifi_ch) {
    if(wifi_ch == 14) return 2484;
    if(wifi_ch < 1) wifi_ch = 1;
    if(wifi_ch > 13) wifi_ch = 13;
    return (uint16_t)(2412 + (wifi_ch - 1) * 5);
}

static uint8_t wifi_selected_channel(Nrf24App* app) {
    if(app->wifi_ap_count == 0 || app->wifi_aps == NULL) return 0;
    uint8_t idx = app->jam.wifi_index;
    if(idx >= app->wifi_ap_count) idx = 0;
    return app->wifi_aps[idx].primary;
}

static size_t nrf24_expand_channels(
    const uint8_t* in,
    size_t count,
    uint8_t* out,
    size_t cap,
    uint8_t radius) {
    uint8_t seen[125] = {0};
    size_t n = 0;
    for(size_t i = 0; i < count && n < cap; i++) {
        int base = in[i];
        for(int delta = -radius; delta <= radius; delta++) {
            int ch = base + delta;
            if(ch < 0) ch = 0;
            else if(ch > 124) ch = 124;
            if(seen[ch]) continue;
            seen[ch] = 1;
            if(n < cap) out[n++] = (uint8_t)ch;
        }
    }
    return n;
}

void nrf24_source_selection_label(Nrf24App* app, char* buf, size_t cap) {
    Nrf24JamState* st = &app->jam;
    switch(st->source) {
    case Nrf24SourceProtocol:
        snprintf(buf, cap, "%s", nrf24_jam_preset_short((Nrf24JamPreset)st->protocol));
        break;
    case Nrf24SourceManual:
        snprintf(buf, cap, "CH %u", (unsigned)st->manual_channel);
        break;
    case Nrf24SourceWifi:
        if(app->wifi_ap_count == 0) {
            if(cap) buf[0] = '\0';
        } else {
            uint8_t idx = st->wifi_index < app->wifi_ap_count ? st->wifi_index : 0;
            const char* ssid = (const char*)app->wifi_aps[idx].ssid;
            if(ssid[0] == '\0') ssid = "<hidden>";
            /* Cap at the width that fits next to the strategy badge. */
            snprintf(buf, cap, "%.14s", ssid);
        }
        break;
    case Nrf24SourceActivity:
        if(!st->activity_scanned) {
            if(cap) buf[0] = '\0';
        } else if(st->activity_wide || st->activity_count == 0) {
            snprintf(buf, cap, "Wide");
        } else {
            snprintf(buf, cap, "Smart %uch", st->activity_count);
        }
        break;
    default:
        snprintf(buf, cap, "?");
        break;
    }
}

size_t nrf24_source_fill_channels(Nrf24App* app, uint8_t* out, size_t cap) {
    if(out == NULL || cap == 0) return 0;
    Nrf24JamState* st = &app->jam;
    Nrf24JamConfig* cfg = nrf24_jam_config_get(app->jam.source, app->jam.protocol);
    bool broad = (cfg != NULL) && (cfg->wide_spectrum != 0);

    switch(st->source) {
    case Nrf24SourceProtocol: {
        size_t n = nrf24_jam_preset_fill_channels((Nrf24JamPreset)st->protocol, out, cap);
        if(!broad) return n;
        uint8_t expanded[125];
        size_t ex = nrf24_expand_channels(out, n, expanded, cap, 4);
        memcpy(out, expanded, ex);
        return ex;
    }

    case Nrf24SourceManual: {
        uint8_t ch = st->manual_channel > 124 ? 124 : st->manual_channel;
        if(!broad) {
            out[0] = ch;
            return 1;
        }
        uint8_t expanded[125];
        size_t n = nrf24_expand_channels(&ch, 1, expanded, cap, 4);
        memcpy(out, expanded, n);
        return n;
    }

    case Nrf24SourceWifi: {
        if(app->wifi_ap_count == 0) return 0;
        uint16_t center = wifi_channel_center_mhz(wifi_selected_channel(app));
        int half_width_mhz = broad ? 18 : 10;
        int min_mhz = (int)center - half_width_mhz;
        int max_mhz = (int)center + half_width_mhz;
        if(min_mhz < 2400) min_mhz = 2400;
        if(max_mhz > 2524) max_mhz = 2524;
        uint8_t nrf_min = (uint8_t)(min_mhz - 2400);
        uint8_t nrf_max = (uint8_t)(max_mhz - 2400);
        size_t n = 0;
        for(uint8_t ch = nrf_min; ch <= nrf_max && n < cap; ch++) {
            out[n++] = ch;
        }
        return n;
    }

    case Nrf24SourceActivity: {
        if(st->activity_wide || st->activity_count == 0) {
            /* Wide sweep over the scanned band. */
            size_t n = 0;
            for(uint8_t ch = NRF24_ACTIVITY_CH_MIN; ch <= (broad ? 124 : NRF24_ACTIVITY_CH_MAX) && n < cap; ch++) {
                out[n++] = ch;
            }
            return n;
        } else {
            size_t n = st->activity_count;
            if(n > cap) n = cap;
            memcpy(out, st->activity_ch, n);
            if(broad) {
                uint8_t expanded[125];
                size_t ex = nrf24_expand_channels(out, n, expanded, cap, 4);
                memcpy(out, expanded, ex);
                return ex;
            }
            return n;
        }
    }

    default:
        return 0;
    }
}

void nrf24_source_select(Nrf24App* app, int dir) {
    Nrf24JamState* st = &app->jam;
    switch(st->source) {
    case Nrf24SourceProtocol:
        st->protocol = (uint8_t)(
            (st->protocol + Nrf24JamPresetCount + (dir > 0 ? 1 : -1)) % Nrf24JamPresetCount);
        break;
    case Nrf24SourceManual: {
        int32_t ch = (int32_t)st->manual_channel + dir;
        if(ch < 0) ch = 125;
        else if(ch > 125) ch = 0;
        st->manual_channel = (uint8_t)ch;
        break;
    }
    case Nrf24SourceWifi:
        if(app->wifi_ap_count > 0) {
            st->wifi_index = (uint8_t)(
                (st->wifi_index + app->wifi_ap_count + (dir > 0 ? 1 : -1)) % app->wifi_ap_count);
        }
        break;
    case Nrf24SourceActivity:
        /* Left/Right toggles Smart ⇄ Wide (only if we have Top-N targets). */
        if(st->activity_count > 0) st->activity_wide = !st->activity_wide;
        break;
    default:
        break;
    }
}

void nrf24_source_cycle_type(Nrf24App* app) {
    app->jam.source = (uint8_t)((app->jam.source + 1) % Nrf24SourceCount);
}

bool nrf24_source_needs_scan(Nrf24App* app) {
    Nrf24JamState* st = &app->jam;
    if(st->source == Nrf24SourceWifi) return !st->wifi_scanned;
    if(st->source == Nrf24SourceActivity) return !st->activity_scanned;
    return false;
}
