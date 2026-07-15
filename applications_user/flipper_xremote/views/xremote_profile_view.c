#include "xremote_profile_view.h"
#include "../xremote_app.h"
#include "../xremote_profiles.h"
#include <string.h>

/*
 * Profile remotes use the original developer button renderer:
 * I_Button_18x18 + a dedicated 11x11 bitmap icon. There is no runtime
 * text drawn over the button shell. The source labels are bitmap assets too.
 */

typedef struct {
    const char* const* commands;
    uint8_t count;
} XRemoteProfileMap;

static const char* const tv_commands[] = {
    "Power", "Input", "Home",
    "Settings", "Back", "Mute",
    "Vol_dn", "Up", "Vol_up",
    "Left", "Ok", "Right",
    "Play_pa", "Down",
};

static const char* const sound_commands[] = {
    "Power", "Mute", "Play_pa",
    "AUX", "HDMI_ARC", "HDMI_IN",
    "Optical", "Bluetooth", "EQ",
    "Prev", "Vol_up", "Next",
    "Bass_dn", "Vol_dn", "Bass_up",
    "Treb_dn", "Reset", "Treb_up",
};

static const char* const fan_commands[] = {
    "Power", "Oscillate",
    "Timer", "Speed",
    "Night_Mode",
};

static const char* const heater_commands[] = {
    "Power", "Heat_High_Low",
    "Oscillate", "Timer",
    "Temp_dn", "Temp_up",
};

static bool xremote_profile_eq(const char* a, const char* b) {
    return a && b && strcmp(a, b) == 0;
}

static XRemoteProfileMap xremote_profile_get_map(XRemoteDeviceProfile profile) {
    switch(profile) {
    case XRemoteDeviceProfileTV:
        return (XRemoteProfileMap){tv_commands, COUNT_OF(tv_commands)};
    case XRemoteDeviceProfileSoundSystem:
        return (XRemoteProfileMap){sound_commands, COUNT_OF(sound_commands)};
    case XRemoteDeviceProfileFan:
        return (XRemoteProfileMap){fan_commands, COUNT_OF(fan_commands)};
    case XRemoteDeviceProfileHeater:
        return (XRemoteProfileMap){heater_commands, COUNT_OF(heater_commands)};
    default:
        return (XRemoteProfileMap){NULL, 0};
    }
}

static const Icon* xremote_profile_icon(const char* command) {
    if(xremote_profile_eq(command, "Power")) return &I_Power_Icon_11x11;
    if(xremote_profile_eq(command, "Input")) return &I_Input_Icon_11x11;
    if(xremote_profile_eq(command, "Home")) return &I_Home_Icon_11x11;
    if(xremote_profile_eq(command, "Settings")) return &I_Settings_Icon_11x11;
    if(xremote_profile_eq(command, "Back")) return &I_Back_Icon_11x11;
    if(xremote_profile_eq(command, "Up")) return &I_Up_Icon_11x11;
    if(xremote_profile_eq(command, "Down")) return &I_Down_Icon_11x11;
    if(xremote_profile_eq(command, "Left")) return &I_Left_Icon_11x11;
    if(xremote_profile_eq(command, "Right")) return &I_Right_Icon_11x11;
    if(xremote_profile_eq(command, "Ok")) return &I_Ok_Icon_11x11;
    if(xremote_profile_eq(command, "Vol_up")) return &I_Volup_Icon_11x11;
    if(xremote_profile_eq(command, "Vol_dn")) return &I_Voldown_Icon_11x11;
    if(xremote_profile_eq(command, "Mute")) return &I_Mute_Icon_11x11;
    if(xremote_profile_eq(command, "Play_pa")) return &I_PlayPause_Icon_11x11;
    if(xremote_profile_eq(command, "Prev")) return &I_Prev_Icon_11x11;
    if(xremote_profile_eq(command, "Next")) return &I_Next_Icon_11x11;
    if(xremote_profile_eq(command, "Oscillate")) return &I_Oscillate_Icon_11x11;
    if(xremote_profile_eq(command, "Timer")) return &I_Timer_Icon_11x11;
    if(xremote_profile_eq(command, "Speed")) return &I_Fan_Icon_11x11;
    if(xremote_profile_eq(command, "Night_Mode")) return &I_Night_Icon_11x11;
    if(xremote_profile_eq(command, "Heat_High_Low")) return &I_HeatHL_Icon_11x11;
    if(xremote_profile_eq(command, "Temp_up")) return &I_TempUp_Icon_11x11;
    if(xremote_profile_eq(command, "Temp_dn")) return &I_TempDown_Icon_11x11;
    if(xremote_profile_eq(command, "Bass_up")) return &I_BassUp_Icon_11x11;
    if(xremote_profile_eq(command, "Bass_dn")) return &I_BassDown_Icon_11x11;
    if(xremote_profile_eq(command, "Treb_up")) return &I_TrebUp_Icon_11x11;
    if(xremote_profile_eq(command, "Treb_dn")) return &I_TrebDown_Icon_11x11;
    if(xremote_profile_eq(command, "AUX")) return &I_Aux_Icon_11x11;
    if(xremote_profile_eq(command, "HDMI_ARC")) return &I_Arc_Icon_11x11;
    if(xremote_profile_eq(command, "HDMI_IN")) return &I_Hdmi_Icon_11x11;
    if(xremote_profile_eq(command, "Optical")) return &I_Opt_Icon_11x11;
    if(xremote_profile_eq(command, "Bluetooth")) return &I_Bt_Icon_11x11;
    if(xremote_profile_eq(command, "EQ")) return &I_Eq_Icon_11x11;
    if(xremote_profile_eq(command, "Reset")) return &I_Reset_Icon_11x11;
    return &I_Ok_Icon_11x11;
}

static void xremote_profile_draw_button(
    Canvas* canvas,
    XRemoteViewModel* model,
    const XRemoteProfileMap* map,
    uint8_t index,
    uint8_t x,
    uint8_t y) {
    if(index >= map->count) return;
    xremote_canvas_draw_button_png(
        canvas,
        model->selected == index,
        x,
        y,
        xremote_profile_icon(map->commands[index]));
}

static void xremote_profile_draw_vertical(
    Canvas* canvas,
    XRemoteViewModel* model,
    XRemoteDeviceProfile profile,
    const XRemoteProfileMap* map) {
    if(profile == XRemoteDeviceProfileSoundSystem) {
        /* 18 original-size bitmap buttons: the whole portrait canvas is the remote. */
        for(uint8_t i = 0; i < map->count; i++) {
            xremote_profile_draw_button(canvas, model, map, i, 2 + (i % 3) * 21, 8 + (i / 3) * 19);
        }
    } else if(profile == XRemoteDeviceProfileTV) {
        for(uint8_t i = 0; i < map->count; i++) {
            xremote_profile_draw_button(canvas, model, map, i, 2 + (i % 3) * 21, 20 + (i / 3) * 20);
        }
    } else {
        /* Fan and heater use a centered two-column physical-remote layout. */
        for(uint8_t i = 0; i < map->count; i++) {
            xremote_profile_draw_button(canvas, model, map, i, 12 + (i % 2) * 22, 32 + (i / 2) * 24);
        }
    }
}

static void xremote_profile_draw_horizontal(
    Canvas* canvas,
    XRemoteViewModel* model,
    XRemoteDeviceProfile profile,
    const XRemoteProfileMap* map) {
    if(profile == XRemoteDeviceProfileSoundSystem) {
        for(uint8_t i = 0; i < map->count; i++) {
            xremote_profile_draw_button(canvas, model, map, i, 4 + (i % 6) * 20, 2 + (i / 6) * 21);
        }
    } else if(profile == XRemoteDeviceProfileTV) {
        for(uint8_t i = 0; i < map->count; i++) {
            xremote_profile_draw_button(canvas, model, map, i, 4 + (i % 6) * 20, 2 + (i / 6) * 21);
        }
    } else {
        for(uint8_t i = 0; i < map->count; i++) {
            xremote_profile_draw_button(canvas, model, map, i, 34 + (i % 3) * 22, 12 + (i / 3) * 22);
        }
    }
}

static void xremote_profile_draw_callback(Canvas* canvas, void* context) {
    XRemoteViewModel* model = context;
    XRemoteView* view = (XRemoteView*)model->context;
    XRemoteAppContext* app_ctx = xremote_view_get_app_context(view);
    XRemoteAppButtons* buttons = xremote_view_get_context(view);
    if(!app_ctx || !buttons) return;

    XRemoteProfileMap map = xremote_profile_get_map(buttons->profile);
    if(!map.count) return;

    ViewOrientation orient = app_ctx->app_settings->orientation;
    if(orient == ViewOrientationVertical || orient == ViewOrientationVerticalFlip) {
        xremote_profile_draw_vertical(canvas, model, buttons->profile, &map);
    } else {
        xremote_profile_draw_horizontal(canvas, model, buttons->profile, &map);
    }
}

static void xremote_profile_process(XRemoteView* view, InputEvent* event) {
    XRemoteAppButtons* buttons = xremote_view_get_context(view);
    if(!buttons) return;

    XRemoteProfileMap map = xremote_profile_get_map(buttons->profile);
    if(!map.count) return;

    with_view_model(
        xremote_view_get_view(view),
        XRemoteViewModel * model,
        {
            if(event->type == InputTypePress &&
               (event->key == InputKeyUp || event->key == InputKeyLeft)) {
                xremote_view_model_move_selection(model, -1, map.count);
            } else if(event->type == InputTypePress &&
                      (event->key == InputKeyDown || event->key == InputKeyRight)) {
                xremote_view_model_move_selection(model, 1, map.count);
            } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
                xremote_view_send_ir_msg_by_name(view, map.commands[model->selected]);
            }
        },
        true);
}

static bool xremote_profile_input_callback(InputEvent* event, void* context) {
    XRemoteView* view = context;
    XRemoteAppContext* app_ctx = xremote_view_get_app_context(view);

    if(event->key == InputKeyBack && event->type == InputTypeShort &&
       app_ctx->app_settings->exit_behavior == XRemoteAppExitPress) {
        return false;
    }
    if(event->key == InputKeyBack && event->type == InputTypeLong &&
       app_ctx->app_settings->exit_behavior == XRemoteAppExitHold) {
        return false;
    }

    xremote_profile_process(view, event);
    return true;
}

XRemoteView* xremote_profile_view_alloc(void* app_ctx, void* model_ctx) {
    XRemoteView* view = xremote_view_alloc(
        app_ctx, xremote_profile_input_callback, xremote_profile_draw_callback);
    xremote_view_set_context(view, model_ctx, NULL);

    with_view_model(
        xremote_view_get_view(view),
        XRemoteViewModel * model,
        {
            model->context = view;
            xremote_view_model_select(model, 0);
        },
        true);

    return view;
}
