#include <furi.h>
#include <furi_hal_version.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/view.h>
#include <gui/modules/text_input.h>
#include <dolphin/dolphin.h>
#include <dolphin/helpers/dolphin_state.h>
#include <string.h>

#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include "../../services/namechanger/namechanger.h"

#include <assets_icons.h>

#define MOODS_TOTAL  3
#define BUTTHURT_MAX 3

typedef struct {
    ViewDispatcher* view_dispatcher;
    TextInput* text_input;
    char name_buf[FURI_HAL_VERSION_ARRAY_NAME_LENGTH];
    DolphinStats stats;
} PassportCtx;

static const Icon* const portrait_happy[BUTTHURT_MAX] = {
    &I_passport_happy1_46x49,
    &I_passport_happy2_46x49,
    &I_passport_happy3_46x49};
static const Icon* const portrait_ok[BUTTHURT_MAX] = {
    &I_passport_okay1_46x49,
    &I_passport_okay2_46x49,
    &I_passport_okay3_46x49};
static const Icon* const portrait_bad[BUTTHURT_MAX] = {
    &I_passport_bad1_46x49,
    &I_passport_bad2_46x49,
    &I_passport_bad3_46x49};

static const Icon* const* portraits[MOODS_TOTAL] = {portrait_happy, portrait_ok, portrait_bad};

static void passport_save_name(const char* name) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    do {
        if(!flipper_format_file_open_always(file, NAMECHANGER_PATH)) break;
        if(!flipper_format_write_header_cstr(file, NAMECHANGER_HEADER, NAMECHANGER_VERSION)) break;
        if(!flipper_format_write_string_cstr(file, "Name", name)) break;
    } while(false);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);

    version_set_custom_name(NULL, strdup(name));
    furi_hal_version_set_name(version_get_custom_name(NULL));
}

static bool passport_name_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);
    size_t len = strlen(text);
    if(len < 2 || len > 8) {
        furi_string_printf(error, "Name must be 2-8 chars");
        return false;
    }
    for(; *text; ++text) {
        const char c = *text;
        if((c < '0' || c > '9') && (c < 'A' || c > 'Z') && (c < 'a' || c > 'z')) {
            furi_string_printf(error, "Only letters and numbers allowed");
            return false;
        }
    }
    return true;
}

static void passport_text_input_done(void* context) {
    PassportCtx* ctx = context;
    passport_save_name(ctx->name_buf);
    text_input_reset(ctx->text_input);
    view_dispatcher_switch_to_view(ctx->view_dispatcher, 0);
}

static bool passport_view_input(InputEvent* input, void* context) {
    PassportCtx* ctx = context;
    if(input->type == InputTypeShort) {
        if(input->key == InputKeyBack) {
            view_dispatcher_stop(ctx->view_dispatcher);
            return true;
        } else if(input->key == InputKeyLeft) {
            const char* cur = furi_hal_version_get_name_ptr();
            strncpy(ctx->name_buf, cur ? cur : "", sizeof(ctx->name_buf));
            ctx->name_buf[sizeof(ctx->name_buf) - 1] = '\0';

            text_input_set_header_text(ctx->text_input, "Change name (2-8 chars)");
            text_input_set_validator(ctx->text_input, passport_name_validator, NULL);
            text_input_set_minimum_length(ctx->text_input, 2);
            text_input_set_result_callback(
                ctx->text_input,
                passport_text_input_done,
                ctx,
                ctx->name_buf,
                sizeof(ctx->name_buf),
                true);

            view_dispatcher_switch_to_view(ctx->view_dispatcher, 1);
            return true;
        }
    }
    return false;
}

static void passport_view_draw(Canvas* canvas, void* model) {
    DolphinStats* stats = model;

    char level_str[20];
    char mood_str[32];
    uint8_t mood = 0;

    if(stats->butthurt <= 4) {
        mood = 0;
        snprintf(mood_str, 20, "Mood: Happy");
    } else if(stats->butthurt <= 9) {
        mood = 1;
        snprintf(mood_str, 20, "Mood: Ok");
    } else {
        mood = 2;
        snprintf(mood_str, 20, "Mood: Angry");
    }

    uint32_t xp_progress = 0;
    uint32_t xp_to_levelup = dolphin_state_xp_to_levelup(stats->icounter);
    uint32_t xp_for_current_level =
        xp_to_levelup + dolphin_state_xp_above_last_levelup(stats->icounter);
    if(stats->level == 3) {
        xp_progress = 0;
    } else {
        xp_progress = xp_to_levelup * 64 / xp_for_current_level;
    }

    // multipass
    canvas_draw_icon(canvas, 0, 0, &I_passport_left_6x46);
    canvas_draw_icon(canvas, 0, 46, &I_passport_bottom_128x18);
    canvas_draw_line(canvas, 6, 0, 125, 0);
    canvas_draw_line(canvas, 127, 2, 127, 47);
    canvas_draw_dot(canvas, 126, 1);

    // portrait
    furi_assert((stats->level > 0) && (stats->level <= 3));
    canvas_draw_icon(canvas, 9, 5, portraits[mood][stats->level - 1]);
    canvas_draw_line(canvas, 58, 16, 123, 16);
    canvas_draw_line(canvas, 58, 30, 123, 30);
    canvas_draw_line(canvas, 58, 44, 123, 44);

    const char* my_name = furi_hal_version_get_name_ptr();
    snprintf(level_str, 20, "Level: %hu", stats->level);
    canvas_draw_str(canvas, 58, 12, my_name ? my_name : "Unknown");
    canvas_draw_str(canvas, 58, 26, mood_str);
    canvas_draw_str(canvas, 58, 40, level_str);

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 123 - xp_progress, 47, xp_progress + 1, 6);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, 123, 47, 123, 52);
}

int32_t passport_app(void* p) {
    UNUSED(p);

    PassportCtx* ctx = malloc(sizeof(PassportCtx));
    memset(ctx, 0, sizeof(PassportCtx));

    Dolphin* dolphin = furi_record_open(RECORD_DOLPHIN);
    ctx->stats = dolphin_stats(dolphin);
    furi_record_close(RECORD_DOLPHIN);

    ctx->view_dispatcher = view_dispatcher_alloc();

    View* passport_view = view_alloc();
    view_set_draw_callback(passport_view, passport_view_draw);
    view_set_context(passport_view, ctx);
    // allocate view model and copy initial stats so draw callback receives model pointer
    view_allocate_model(passport_view, ViewModelTypeLockFree, sizeof(DolphinStats));
    void* model_ptr = view_get_model(passport_view);
    memcpy(model_ptr, &ctx->stats, sizeof(DolphinStats));
    view_commit_model(passport_view, true);
    view_set_input_callback(passport_view, passport_view_input);

    ctx->text_input = text_input_alloc();

    view_dispatcher_add_view(ctx->view_dispatcher, 0, passport_view);
    view_dispatcher_add_view(ctx->view_dispatcher, 1, text_input_get_view(ctx->text_input));

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(ctx->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(ctx->view_dispatcher, 0);
    view_dispatcher_run(ctx->view_dispatcher);

    // cleanup
    text_input_free(ctx->text_input);
    view_dispatcher_remove_view(ctx->view_dispatcher, 0);
    view_dispatcher_remove_view(ctx->view_dispatcher, 1);
    view_dispatcher_free(ctx->view_dispatcher);
    view_free(passport_view);
    furi_record_close(RECORD_GUI);
    free(ctx);

    return 0;
}
