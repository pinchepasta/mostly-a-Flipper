#include "subghz_waterfall.h"

#include <furi.h>
#include <input/input.h>
#include <gui/elements.h>
#include <gui/canvas.h>

#include "../subghz_i.h"
#include "../helpers/subghz_frequency_analyzer_worker.h"
#include "../helpers/subghz_txrx.h"

#define WATERFALL_WIDTH 128
#define WATERFALL_HEIGHT 64
#define WATERFALL_HISTORY 64
#define WATERFALL_TOP 12
#define WATERFALL_PANEL_HEIGHT (WATERFALL_HEIGHT - WATERFALL_TOP)
#define WATERFALL_UPDATE_MS 100
#define RSSI_MIN (-97.0f)
#define RSSI_MAX (-60.0f)

typedef struct {
    float values[WATERFALL_HISTORY];
    uint8_t ind_write;
    bool history_full;
    float trigger;
    float current_rssi;
    float current_frequency;
    uint32_t tuned_frequency;
    bool signal;
    uint8_t signal_hold;
} SubGhzWaterfallModel;

typedef struct SubGhzWaterfall {
    View* view;
    SubGhzFrequencyAnalyzerWorker* worker;
    void* context;
    SubGhzTxRx* txrx;
    uint8_t cursor;
    float trigger_level;
    float last_rssi;
    uint32_t tuned_frequency;
    uint32_t last_update_time;
} SubGhzWaterfall;

static uint8_t subghz_waterfall_rssi_to_y(float rssi) {
    if(rssi <= RSSI_MIN) return WATERFALL_HEIGHT - 1;
    if(rssi >= RSSI_MAX) return 0;

    float normalized = (rssi - RSSI_MIN) / (RSSI_MAX - RSSI_MIN);
    return (uint8_t)((1.0f - normalized) * (WATERFALL_PANEL_HEIGHT - 1));
}

static uint8_t subghz_waterfall_rssi_to_height(float rssi) {
    if(rssi <= RSSI_MIN) return 2;
    if(rssi >= RSSI_MAX) return WATERFALL_PANEL_HEIGHT;

    float normalized = (rssi - RSSI_MIN) / (RSSI_MAX - RSSI_MIN);
    return (uint8_t)(2 + normalized * (WATERFALL_PANEL_HEIGHT - 2));
}

static bool subghz_waterfall_change_frequency(SubGhzWaterfall* instance, int32_t step_hz) {
    SubGhz* subghz = instance->context;
    if(subghz == NULL || subghz->txrx == NULL || instance->worker == NULL) return false;

    int32_t next_frequency = (int32_t)instance->tuned_frequency + step_hz;
    if(next_frequency < 300000000) next_frequency = 300000000;
    if(next_frequency > 960000000) next_frequency = 960000000;

    if(!subghz_txrx_radio_device_is_frequency_valid(subghz->txrx, (uint32_t)next_frequency)) {
        return false;
    }

    SubGhzRadioPreset preset = subghz_txrx_get_preset(subghz->txrx);
    subghz_txrx_set_preset(
        subghz->txrx,
        furi_string_get_cstr(preset.name),
        (uint32_t)next_frequency,
        preset.data,
        preset.data_size);

    if(subghz->last_settings != NULL) {
        subghz->last_settings->frequency = (uint32_t)next_frequency;
    }

    instance->tuned_frequency = (uint32_t)next_frequency;
    subghz_frequency_analyzer_worker_set_frequency(instance->worker, instance->tuned_frequency);
    return true;
}

static void subghz_waterfall_draw(Canvas* canvas, void* model) {
    SubGhzWaterfallModel* waterfall_model = model;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    canvas_draw_frame(canvas, 0, WATERFALL_TOP, WATERFALL_WIDTH, WATERFALL_PANEL_HEIGHT);

    uint8_t count = waterfall_model->history_full ? WATERFALL_HISTORY : waterfall_model->ind_write;
    uint8_t start_index = waterfall_model->history_full ? waterfall_model->ind_write : 0;
    for(uint8_t i = 0; i < count; i++) {
        uint8_t index = (start_index + i) % WATERFALL_HISTORY;
        uint8_t x = i * 2;
        uint8_t height = subghz_waterfall_rssi_to_height(waterfall_model->values[index]);
        uint8_t y = WATERFALL_TOP + WATERFALL_PANEL_HEIGHT - height;

        if(height > 2) {
            canvas_draw_box(canvas, x, y, 2, height);
        } else {
            canvas_draw_dot(canvas, x, WATERFALL_TOP + WATERFALL_PANEL_HEIGHT - 1);
            canvas_draw_dot(canvas, x + 1, WATERFALL_TOP + WATERFALL_PANEL_HEIGHT - 1);
        }
    }

    if(waterfall_model->signal_hold > 0) {
        waterfall_model->signal_hold--;
        waterfall_model->signal = true;
    } else {
        waterfall_model->signal = false;
    }

    elements_button_center(canvas, "");
}

static bool subghz_waterfall_input(InputEvent* event, void* context) {
    SubGhzWaterfall* instance = context;
    if(event->key == InputKeyBack) {
        return false;
    }

    if((event->type == InputTypePress || event->type == InputTypeRepeat) && event->key == InputKeyUp) {
        if(subghz_waterfall_change_frequency(instance, 1000000)) {
            return true;
        }
    }

    if((event->type == InputTypePress || event->type == InputTypeRepeat) && event->key == InputKeyDown) {
        if(subghz_waterfall_change_frequency(instance, -1000000)) {
            return true;
        }
    }

    if(event->type == InputTypePress && event->key == InputKeyLeft) {
        instance->trigger_level -= 1.0f;
        if(instance->trigger_level < RSSI_MIN) instance->trigger_level = RSSI_MIN;
        subghz_frequency_analyzer_worker_set_trigger_level(instance->worker, instance->trigger_level);
        return true;
    }

    if(event->type == InputTypePress && event->key == InputKeyRight) {
        instance->trigger_level += 1.0f;
        if(instance->trigger_level > RSSI_MAX) instance->trigger_level = RSSI_MAX;
        subghz_frequency_analyzer_worker_set_trigger_level(instance->worker, instance->trigger_level);
        return true;
    }

    if(event->type == InputTypePress && event->key == InputKeyOk) {
        SubGhz* subghz = instance->context;
        if(subghz && subghz->scene_manager) {
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneReceiverConfig);
            return true;
        }
    }

    return false;
}

static void subghz_waterfall_pair_callback(void* context, uint32_t frequency, float rssi, bool signal) {
    SubGhzWaterfall* instance = context;
    if(instance == NULL) return;

    uint32_t now = furi_get_tick();
    if(!signal && (now - instance->last_update_time) < WATERFALL_UPDATE_MS) {
        return;
    }
    instance->last_update_time = now;

    with_view_model(
        instance->view,
        SubGhzWaterfallModel * model,
        {
            float waterfall_rssi = rssi;
            if(waterfall_rssi < RSSI_MIN) {
                waterfall_rssi = RSSI_MIN;
            }
            if(waterfall_rssi > RSSI_MAX) {
                waterfall_rssi = RSSI_MAX;
            }
            model->values[model->ind_write++] = waterfall_rssi;
            if(model->ind_write >= WATERFALL_HISTORY) {
                model->ind_write = 0;
                model->history_full = true;
            }
            model->current_rssi = waterfall_rssi;
            model->current_frequency = frequency;
            model->tuned_frequency = instance->tuned_frequency;
            if(signal) {
                model->signal_hold = 5;
            }
            model->signal = signal || (model->signal_hold > 0);
            model->trigger = instance->trigger_level;
        },
        true);
}

static void subghz_waterfall_enter(void* context) {
    SubGhzWaterfall* instance = context;
    SubGhz* subghz = instance->context;
    instance->worker = subghz_frequency_analyzer_worker_alloc(instance->context);
    subghz_frequency_analyzer_worker_set_pair_callback(
        instance->worker,
        subghz_waterfall_pair_callback,
        instance);
    subghz_frequency_analyzer_worker_start(instance->worker);
    instance->trigger_level = -90.0f;
    instance->last_rssi = 0.0f;
    instance->tuned_frequency = subghz->last_settings->frequency;
    subghz_frequency_analyzer_worker_set_frequency(instance->worker, instance->tuned_frequency);

    with_view_model(
        instance->view,
        SubGhzWaterfallModel * model,
        {
            memset(model, 0, sizeof(SubGhzWaterfallModel));
            for(uint8_t i = 0; i < WATERFALL_HISTORY; i++) {
                model->values[i] = RSSI_MIN;
            }
            model->ind_write = 0;
            model->history_full = false;
            model->current_rssi = RSSI_MIN;
            model->trigger = instance->trigger_level;
            model->tuned_frequency = instance->tuned_frequency;
            model->signal_hold = 0;
        },
        true);
}

static void subghz_waterfall_exit(void* context) {
    SubGhzWaterfall* instance = context;
    if(instance->worker != NULL) {
        if(subghz_frequency_analyzer_worker_is_running(instance->worker)) {
            subghz_frequency_analyzer_worker_stop(instance->worker);
        }
        subghz_frequency_analyzer_worker_free(instance->worker);
        instance->worker = NULL;
    }
}

SubGhzWaterfall* subghz_waterfall_alloc(void* context) {
    SubGhzWaterfall* instance = malloc(sizeof(SubGhzWaterfall));
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(SubGhzWaterfallModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, subghz_waterfall_draw);
    view_set_input_callback(instance->view, subghz_waterfall_input);
    view_set_enter_callback(instance->view, subghz_waterfall_enter);
    view_set_exit_callback(instance->view, subghz_waterfall_exit);
    view_set_input_mode(instance->view, ViewInputModeUpDown);
    instance->context = context;
    instance->txrx = context ? ((SubGhz*)context)->txrx : NULL;
    instance->worker = NULL;
    instance->cursor = 0;
    instance->trigger_level = RSSI_MIN;
    instance->last_rssi = 0.0f;
    instance->last_update_time = 0;
    return instance;
}

void subghz_waterfall_free(SubGhzWaterfall* instance) {
    furi_assert(instance);
    if(instance->worker != NULL) {
        if(subghz_frequency_analyzer_worker_is_running(instance->worker)) {
            subghz_frequency_analyzer_worker_stop(instance->worker);
        }
        subghz_frequency_analyzer_worker_free(instance->worker);
        instance->worker = NULL;
    }
    view_free(instance->view);
    free(instance);
}

View* subghz_waterfall_get_view(SubGhzWaterfall* instance) {
    furi_assert(instance);
    return instance->view;
}
