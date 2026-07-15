/*!
 *  @file flipper-xremote/views/xremote_learn_view.c
    @license This project is released under the GNU GPLv3 License
 *  @copyright (c) 2023 Sandro Kalatozishvili (s.kalatoz@gmail.com)
 *
 * @brief Learn new remote page view components and functionality.
 */

#include "xremote_learn_view.h"
#include "../xremote_learn.h"
#include "../xremote_app.h"

static void xremote_learn_view_draw_callback(Canvas* canvas, void* context) {
    furi_assert(context);
    XRemoteViewModel* model = context;
    XRemoteLearnContext* learn_ctx = model->context;

    XRemoteAppContext* app_ctx = xremote_learn_get_app_context(learn_ctx);
    const char* button_name = xremote_learn_get_curr_button_name(learn_ctx);

    ViewOrientation orientation = app_ctx->app_settings->orientation;
    xremote_canvas_draw_header(canvas, orientation, "Learn");

    char info_text[128];
    snprintf(
        info_text,
        sizeof(info_text),
        "Press\n\"%s\"\nbutton on\nthe remote.",
        button_name != NULL ? button_name : "");

    if(orientation == ViewOrientationHorizontal) {
        elements_multiline_text_aligned(canvas, 0, 12, AlignLeft, AlignTop, info_text);
        xremote_canvas_draw_button_wide(
            canvas, model->selected == 0, 68, 22, "Finish", XRemoteIconEnter);
        xremote_canvas_draw_button_wide(
            canvas, model->selected == 1, 68, 40, "Skip", XRemoteIconArrowRight);
    } else {
        elements_multiline_text_aligned(canvas, 0, 30, AlignLeft, AlignTop, info_text);
        xremote_canvas_draw_button_wide(
            canvas, model->selected == 0, 0, 82, "Finish", XRemoteIconEnter);
        xremote_canvas_draw_button_wide(
            canvas, model->selected == 1, 0, 100, "Skip", XRemoteIconArrowRight);
    }

    const char* exit_str = xremote_app_context_get_exit_str(app_ctx);
    xremote_canvas_draw_exit_footer(canvas, orientation, exit_str);
}

static void xremote_learn_success_view_draw_callback(Canvas* canvas, void* context) {
    furi_assert(context);
    XRemoteViewModel* model = context;
    XRemoteLearnContext* learn_ctx = model->context;

    XRemoteAppContext* app_ctx = xremote_learn_get_app_context(learn_ctx);
    InfraredSignal* ir_signal = xremote_learn_get_ir_signal(learn_ctx);

    xremote_canvas_draw_header(canvas, app_ctx->app_settings->orientation, NULL);
    const char* button_name = xremote_learn_get_curr_button_name(learn_ctx);
    char signal_info[128];

    if(infrared_signal_is_raw(ir_signal)) {
        InfraredRawSignal* raw = infrared_signal_get_raw_signal(ir_signal);

        snprintf(
            signal_info,
            sizeof(signal_info),
            "Name: %s\n"
            "Type: RAW\n"
            "T-Size: %u\n"
            "D-Cycle: %.2f\n",
            button_name,
            raw->timings_size,
            (double)raw->duty_cycle);
    } else {
        InfraredMessage* message = infrared_signal_get_message(ir_signal);
        const char* infrared_protocol = infrared_get_protocol_name(message->protocol);

        snprintf(
            signal_info,
            sizeof(signal_info),
            "Name: %s\n"
            "Proto: %s\n"
            "Addr: 0x%lX\n"
            "Cmd: 0x%lX\n",
            button_name,
            infrared_protocol,
            message->address,
            message->command);
    }

    if(app_ctx->app_settings->orientation == ViewOrientationHorizontal) {
        canvas_draw_str_aligned(canvas, 0, 0, AlignLeft, AlignTop, "Received signal");
        elements_multiline_text_aligned(canvas, 0, 16, AlignLeft, AlignTop, signal_info);
        xremote_canvas_draw_button_wide(
            canvas, model->selected == 0, 68, 12, "Finish", XRemoteIconEnter);
        xremote_canvas_draw_button_wide(
            canvas, model->selected == 1, 68, 30, "Next", XRemoteIconArrowRight);
        xremote_canvas_draw_button_wide(
            canvas, model->selected == 2, 68, 48, "Retry", XRemoteIconBack);
    } else {
        canvas_draw_str_aligned(canvas, 0, 12, AlignLeft, AlignTop, "Received signal");
        elements_multiline_text_aligned(canvas, 0, 27, AlignLeft, AlignTop, signal_info);
        xremote_canvas_draw_button_wide(
            canvas, model->selected == 0, 0, 76, "Finish", XRemoteIconEnter);
        xremote_canvas_draw_button_wide(
            canvas, model->selected == 1, 0, 94, "Next", XRemoteIconArrowRight);
        xremote_canvas_draw_button_wide(
            canvas, model->selected == 2, 0, 112, "Retry", XRemoteIconBack);
    }
}

static void xremote_learn_success_view_process(XRemoteView* view, InputEvent* event) {
    with_view_model(
        xremote_view_get_view(view),
        XRemoteViewModel * model,
        {
            XRemoteLearnContext* learn_ctx = xremote_view_get_context(view);
            model->context = learn_ctx;

            if(event->type == InputTypeShort && event->key == InputKeyUp) {
                model->selected = (model->selected + 2) % 3;
            } else if(event->type == InputTypeShort && event->key == InputKeyDown) {
                model->selected = (model->selected + 1) % 3;
            } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
                if(model->selected == 0) {
                    xremote_learn_send_event(learn_ctx, XRemoteEventSignalFinish);
                } else if(model->selected == 1) {
                    xremote_learn_send_event(learn_ctx, XRemoteEventSignalSave);
                } else {
                    xremote_learn_send_event(learn_ctx, XRemoteEventSignalRetry);
                }
            } else if(
                (event->type == InputTypeShort || event->type == InputTypeLong) &&
                event->key == InputKeyBack) {
                xremote_learn_send_event(learn_ctx, XRemoteEventSignalAskExit);
            }
        },
        true);
}

static void xremote_learn_view_process(XRemoteView* view, InputEvent* event) {
    with_view_model(
        xremote_view_get_view(view),
        XRemoteViewModel * model,
        {
            XRemoteLearnContext* learn_ctx = xremote_view_get_context(view);
            XRemoteAppContext* app_ctx = xremote_view_get_app_context(view);
            XRemoteAppExit exit = app_ctx->app_settings->exit_behavior;
            model->context = learn_ctx;

            if(event->type == InputTypeShort && event->key == InputKeyUp) {
                model->selected = (model->selected + 1) % 2;
            } else if(event->type == InputTypeShort && event->key == InputKeyDown) {
                model->selected = (model->selected + 1) % 2;
            } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
                if(model->selected == 0 && xremote_learn_has_buttons(learn_ctx)) {
                    xremote_learn_send_event(learn_ctx, XRemoteEventSignalFinish);
                } else if(model->selected == 1) {
                    xremote_learn_send_event(learn_ctx, XRemoteEventSignalSkip);
                }
            } else if(
                (event->type == InputTypeShort || event->type == InputTypeLong) &&
                event->key == InputKeyBack) {
                if((event->type == InputTypeShort && exit == XRemoteAppExitPress) ||
                   (event->type == InputTypeLong && exit == XRemoteAppExitHold)) {
                    xremote_learn_send_event(learn_ctx, XRemoteEventSignalAskExit);
                }
            }
        },
        true);
}

static bool xremote_learn_success_view_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    XRemoteView* view = (XRemoteView*)context;
    xremote_learn_success_view_process(view, event);
    return true;
}

static bool xremote_learn_view_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    XRemoteView* view = (XRemoteView*)context;
    xremote_learn_view_process(view, event);
    return true;
}

XRemoteView* xremote_learn_success_view_alloc(void* app_ctx, void* learn_ctx) {
    XRemoteView* view = xremote_view_alloc(
        app_ctx,
        xremote_learn_success_view_input_callback,
        xremote_learn_success_view_draw_callback);
    xremote_view_set_context(view, learn_ctx, NULL);

    with_view_model(
        xremote_view_get_view(view),
        XRemoteViewModel * model,
        {
            model->context = learn_ctx;
            model->right_pressed = false;
            model->back_pressed = false;
            model->ok_pressed = false;
            model->selected = 0;
        },
        true);

    return view;
}

XRemoteView* xremote_learn_view_alloc(void* app_ctx, void* learn_ctx) {
    XRemoteView* view = xremote_view_alloc(
        app_ctx, xremote_learn_view_input_callback, xremote_learn_view_draw_callback);
    xremote_view_set_context(view, learn_ctx, NULL);

    with_view_model(
        xremote_view_get_view(view),
        XRemoteViewModel * model,
        {
            model->context = learn_ctx;
            model->right_pressed = false;
            model->back_pressed = false;
            model->ok_pressed = false;
            model->selected = 0;
        },
        true);

    return view;
}
