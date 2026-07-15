#include "remote.h"
#include "../subghz_remote_app_i.h"

#include <input/input.h>
#include <gui/elements.h>

#include <lib/toolbox/path.h>

#define SUBREM_VIEW_REMOTE_MAX_LABEL_LENGTH 30
#define SUBREM_VIEW_REMOTE_LEFT_OFFSET      10
#define SUBREM_VIEW_REMOTE_RIGHT_OFFSET     0

struct SubRemViewRemote {
    View* view;
    SubRemViewRemoteCallback callback;
    void* context;
};

typedef struct {
    char* labels[SubRemSubKeyNameMaxCount];

    SubRemViewRemoteState state;

    uint8_t pressed_btn;
    bool is_external;
} SubRemViewRemoteModel;

void subrem_view_remote_set_callback(
    SubRemViewRemote* subrem_view_remote,
    SubRemViewRemoteCallback callback,
    void* context) {
    furi_assert(subrem_view_remote);

    subrem_view_remote->callback = callback;
    subrem_view_remote->context = context;
}

void subrem_view_remote_update_data_labels(
    SubRemViewRemote* subrem_view_remote,
    SubRemSubFilePreset** subs_presets) {
    furi_assert(subrem_view_remote);
    furi_assert(subs_presets);

    FuriString* labels[SubRemSubKeyNameMaxCount];
    SubRemSubFilePreset* sub_preset;

    for(uint8_t i = 0; i < SubRemSubKeyNameMaxCount; i++) {
        sub_preset = subs_presets[i];
        switch(sub_preset->load_state) {
        case SubRemLoadSubStateOK:
            if(!furi_string_empty(sub_preset->label)) {
                labels[i] = furi_string_alloc_set(sub_preset->label);
            } else if(!furi_string_empty(sub_preset->file_path)) {
                labels[i] = furi_string_alloc();
                path_extract_filename(sub_preset->file_path, labels[i], true);
            } else {
                labels[i] = furi_string_alloc_set("Empty Label");
            }
            break;

        case SubRemLoadSubStateErrorNoFile:
            labels[i] = furi_string_alloc_set("[X] Can't open file");
            break;

        case SubRemLoadSubStateErrorFreq:
        case SubRemLoadSubStateErrorMod:
        case SubRemLoadSubStateErrorProtocol:
            labels[i] = furi_string_alloc_set("[X] Error in .sub file");
            break;

        default:
            labels[i] = furi_string_alloc_set("");
            break;
        }
    }

    with_view_model(
        subrem_view_remote->view,
        SubRemViewRemoteModel * model,
        {
            for(uint8_t i = 0; i < SubRemSubKeyNameMaxCount; i++) {
                strncpy(
                    model->labels[i],
                    furi_string_get_cstr(labels[i]),
                    SUBREM_VIEW_REMOTE_MAX_LABEL_LENGTH);
            }
        },
        true);

    for(uint8_t i = 0; i < SubRemSubKeyNameMaxCount; i++) {
        furi_string_free(labels[i]);
    }
}

void subrem_view_remote_set_state(
    SubRemViewRemote* subrem_view_remote,
    SubRemViewRemoteState state,
    uint8_t presed_btn) {
    furi_assert(subrem_view_remote);
    with_view_model(
        subrem_view_remote->view,
        SubRemViewRemoteModel * model,
        {
            model->state = state;
            model->pressed_btn = presed_btn;
        },
        true);
}

void subrem_view_remote_set_radio(SubRemViewRemote* subrem_view_remote, bool external) {
    furi_assert(subrem_view_remote);
    with_view_model(
        subrem_view_remote->view,
        SubRemViewRemoteModel * model,
        { model->is_external = external; },
        true);
}

static void subrem_view_remote_draw_button(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    uint8_t width,
    uint8_t height,
    uint8_t index,
    const char* label,
    bool is_pressed) {
    static const char* const button_text[SubRemSubKeyNameMaxCount] = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9"};

    if(is_pressed) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, x, y, width, height);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, x, y, width, height);
        canvas_set_color(canvas, ColorBlack);
    }

    canvas_draw_rframe(canvas, x, y, width, height, 2);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, x + 2, y + 2, AlignLeft, AlignTop, button_text[index]);

    canvas_set_font(canvas, FontSecondary);
    elements_text_box(
        canvas,
        x + 1,
        y + 4,
        width - 2,
        height - 5,
        AlignCenter,
        AlignCenter,
        label,
        false);

    canvas_set_color(canvas, ColorBlack);
}

void subrem_view_remote_draw(Canvas* canvas, SubRemViewRemoteModel* model) {
    const uint8_t canvas_w = canvas_width(canvas);
    const uint8_t canvas_h = canvas_height(canvas);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Statusbar
    canvas_draw_icon(canvas, 0, 0, &I_status_bar);
    if(model->state == SubRemViewRemoteStateOFF) {
        canvas_invert_color(canvas);
        canvas_draw_rbox(canvas, 12, 0, 52 - 12, 13, 2);
        canvas_invert_color(canvas);
        canvas_draw_rframe(canvas, 12, 0, 52 - 12, 13, 2);
        canvas_draw_str_aligned(canvas, 32, 3, AlignCenter, AlignTop, "Preview");
    } else {
        canvas_draw_icon(
            canvas,
            0,
            2,
            (model->is_external) ? &I_External_antenna_20x12 : &I_Internal_antenna_20x12);
        canvas_draw_icon(canvas, 50, 0, &I_Status_cube_14x14);
        if(model->state == SubRemViewRemoteStateSending) {
            canvas_draw_icon_ex(canvas, 52, 3, &I_Pin_arrow_up_7x9, IconRotation90);
        }
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, canvas_w / 2, 15, AlignCenter, AlignTop, "RF Shortcut");

    if(model->state != SubRemViewRemoteStateOFF) {
        const uint8_t button_gap = 4;
        uint8_t button_w = (canvas_w - 8 - 2 * button_gap) / 3;
        if(button_w > 24) button_w = 24;
        if(button_w < 16) button_w = 16;

        uint8_t button_h = (canvas_h - 34 - 2 * button_gap) / 3;
        if(button_h > 14) button_h = 14;
        if(button_h < 10) button_h = 10;

        const uint8_t panel_w = 3 * button_w + 2 * button_gap;
        const uint8_t panel_x = (canvas_w - panel_w) / 2;
        const uint8_t panel_y = 22;

        for(uint8_t i = 0; i < SubRemSubKeyNameMaxCount; i++) {
            const bool is_pressed =
                (model->state == SubRemViewRemoteStateSending) && (model->pressed_btn == i);
            const uint8_t col = i % 3;
            const uint8_t row = i / 3;
            const uint8_t x = panel_x + col * (button_w + button_gap);
            const uint8_t y = panel_y + row * (button_h + button_gap);

            subrem_view_remote_draw_button(
                canvas,
                x,
                y,
                button_w,
                button_h,
                i,
                model->labels[i],
                is_pressed);
        }
    } else {
        const uint8_t footer_y = canvas_h - 11;
        canvas_draw_icon(canvas, 2, footer_y, &I_ButtonLeft_4x7);
        canvas_draw_str_aligned(canvas, 8, footer_y + 7, AlignLeft, AlignBottom, "Back");

        canvas_draw_icon(canvas, 58, footer_y, &I_ButtonRight_4x7);
        canvas_draw_str_aligned(canvas, 56, footer_y + 7, AlignRight, AlignBottom, "Save");
    }
}

bool subrem_view_remote_input(InputEvent* event, void* context) {
    furi_assert(context);
    SubRemViewRemote* subrem_view_remote = context;

    if(event->key == InputKeyBack && event->type == InputTypePress) {
        bool is_stopping = false;
        with_view_model(
            subrem_view_remote->view,
            SubRemViewRemoteModel * model,
            {
                if(model->state == SubRemViewRemoteStateSending) {
                    is_stopping = true;
                    model->pressed_btn = 0;
                }
            },
            true);

        //Cant send exit the app inside that with_model,locks the model and the app will hang and not unload!
        if(is_stopping)
            subrem_view_remote->callback(
                SubRemCustomEventViewRemoteForcedStop, subrem_view_remote->context);
        else
            subrem_view_remote->callback(
                SubRemCustomEventViewRemoteBack, subrem_view_remote->context);

        return true;
    }
    // BACK button processing end

    if(event->key == InputKeyUp && event->type == InputTypePress) {
        subrem_view_remote->callback(
            SubRemCustomEventViewRemoteStartUP, subrem_view_remote->context);
        return true;
    } else if(event->key == InputKeyDown && event->type == InputTypePress) {
        subrem_view_remote->callback(
            SubRemCustomEventViewRemoteStartDOWN, subrem_view_remote->context);
        return true;
    } else if(event->key == InputKeyLeft && event->type == InputTypePress) {
        subrem_view_remote->callback(
            SubRemCustomEventViewRemoteStartLEFT, subrem_view_remote->context);
        return true;
    } else if(event->key == InputKeyRight && event->type == InputTypePress) {
        subrem_view_remote->callback(
            SubRemCustomEventViewRemoteStartRIGHT, subrem_view_remote->context);
        return true;
    } else if(event->key == InputKeyOk && event->type == InputTypeLong) {
        subrem_view_remote->callback(
            SubRemCustomEventViewRemoteStartEXTRA, subrem_view_remote->context);
        return true;
    } else if(event->key == InputKeyOk && event->type == InputTypePress) {
        subrem_view_remote->callback(
            SubRemCustomEventViewRemoteStartOK, subrem_view_remote->context);
        return true;
    } else if(event->type == InputTypeText) {
        char key_char = (char)event->key;
        uint8_t start_index = SubRemSubKeyNameMaxCount;
        if(key_char >= '1' && key_char <= '9') {
            start_index = (uint8_t)(key_char - '1');
        } else if(key_char >= 'a' && key_char <= 'i') {
            start_index = (uint8_t)(key_char - 'a');
        } else if(key_char >= 'A' && key_char <= 'I') {
            start_index = (uint8_t)(key_char - 'A');
        }

        if(start_index < SubRemSubKeyNameMaxCount) {
            SubRemCustomEvent event_id =
                (SubRemCustomEvent)(SubRemCustomEventViewRemoteStartUP + start_index);
            if(event_id <= SubRemCustomEventViewRemoteStart9) {
                subrem_view_remote->callback(event_id, subrem_view_remote->context);
                subrem_view_remote->callback(
                    SubRemCustomEventViewRemoteStop, subrem_view_remote->context);
            }
        }
        return true;
    } else if(event->type == InputTypeRelease) {
        subrem_view_remote->callback(SubRemCustomEventViewRemoteStop, subrem_view_remote->context);
        return true;
    }

    return true;
}

void subrem_view_remote_enter(void* context) {
    furi_assert(context);
}

void subrem_view_remote_exit(void* context) {
    furi_assert(context);
}

SubRemViewRemote* subrem_view_remote_alloc(void) {
    SubRemViewRemote* subrem_view_remote = malloc(sizeof(SubRemViewRemote));

    // View allocation and configuration
    subrem_view_remote->view = view_alloc();
    view_allocate_model(
        subrem_view_remote->view, ViewModelTypeLocking, sizeof(SubRemViewRemoteModel));
    view_set_context(subrem_view_remote->view, subrem_view_remote);
    view_set_orientation(subrem_view_remote->view, ViewOrientationHorizontal);
    view_set_draw_callback(subrem_view_remote->view, (ViewDrawCallback)subrem_view_remote_draw);
    view_set_input_callback(subrem_view_remote->view, subrem_view_remote_input);
    view_set_enter_callback(subrem_view_remote->view, subrem_view_remote_enter);
    view_set_exit_callback(subrem_view_remote->view, subrem_view_remote_exit);

    with_view_model(
        subrem_view_remote->view,
        SubRemViewRemoteModel * model,
        {
            model->state = SubRemViewRemoteStateIdle;

            for(uint8_t i = 0; i < SubRemSubKeyNameMaxCount; i++) {
                model->labels[i] = malloc(sizeof(char) * SUBREM_VIEW_REMOTE_MAX_LABEL_LENGTH + 1);
                strcpy(model->labels[i], "");
            }

            model->pressed_btn = 0;
            model->is_external = false;
        },
        true);
    return subrem_view_remote;
}

void subrem_view_remote_free(SubRemViewRemote* subghz_remote) {
    furi_assert(subghz_remote);

    with_view_model(
        subghz_remote->view,
        SubRemViewRemoteModel * model,
        {
            for(uint8_t i = 0; i < SubRemSubKeyNameMaxCount; i++) {
                free(model->labels[i]);
            }
        },
        true);
    view_free(subghz_remote->view);
    free(subghz_remote);
}

View* subrem_view_remote_get_view(SubRemViewRemote* subrem_view_remote) {
    furi_assert(subrem_view_remote);
    return subrem_view_remote->view;
}
