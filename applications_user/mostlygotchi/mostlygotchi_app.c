#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <furi_hal_random.h>

#define BRUCEGOTCHI_WIDTH 128
#define BRUCEGOTCHI_HEIGHT 64
#define BRUCEGOTCHI_TICK_MS 250

typedef enum {
    BruceGotchiEventTypeInput,
    BruceGotchiEventTypeTick,
} BruceGotchiEventType;

typedef struct {
    BruceGotchiEventType type;
    InputEvent input;
} BruceGotchiEvent;

typedef struct {
    FuriMutex* mutex;
    FuriMessageQueue* event_queue;
    FuriTimer* timer;
    ViewPort* view_port;
    bool running;
    uint8_t mood_index;
    uint8_t frame;
} BruceGotchiApp;

static const char* const mood_names[] = {"Hello!", "Chill", "Busy", "Happy"};
static const char* const mood_lines[] = {
    "BruceGotchi is awake",
    "Scanning for friends",
    "Keeping the vibe alive",
    "Ready for mischief",
};

static void brucegotchi_next_mood(BruceGotchiApp* app, bool randomize) {
    if(randomize) {
        uint8_t random_byte = 0;
        furi_hal_random_fill_buf(&random_byte, 1);
        app->mood_index = random_byte % (sizeof(mood_names) / sizeof(mood_names[0]));
    } else {
        app->mood_index = (app->mood_index + 1) % (sizeof(mood_names) / sizeof(mood_names[0]));
    }
    app->frame = 0;
}

static void brucegotchi_draw_callback(Canvas* const canvas, void* ctx) {
    furi_assert(ctx);

    BruceGotchiApp* app = ctx;
    if(furi_mutex_acquire(app->mutex, 25) != FuriStatusOk) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 8, 10, "mostlyGotchi");

    canvas_draw_frame(canvas, 8, 18, 112, 34);

    const uint8_t head_x = 44;
    const uint8_t head_y = 28;
    canvas_draw_circle(canvas, head_x, head_y, 12);
    canvas_draw_circle(canvas, head_x - 4, head_y - 3, 1);
    canvas_draw_circle(canvas, head_x + 4, head_y - 3, 1);

    if(app->frame % 2 == 0) {
        canvas_draw_line(canvas, head_x - 4, head_y + 4, head_x + 4, head_y + 4);
    } else {
        canvas_draw_line(canvas, head_x - 4, head_y + 4, head_x - 1, head_y + 7);
        canvas_draw_line(canvas, head_x + 1, head_y + 7, head_x + 4, head_y + 4);
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 60, 26, mood_names[app->mood_index]);
    canvas_draw_str(canvas, 60, 38, mood_lines[app->mood_index]);

    canvas_draw_str(canvas, 10, 58, "Back: exit  OK: change");

    furi_mutex_release(app->mutex);
}

static void brucegotchi_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);

    BruceGotchiApp* app = ctx;
    BruceGotchiEvent event = {.type = BruceGotchiEventTypeInput, .input = *input_event};
    furi_message_queue_put(app->event_queue, &event, FuriWaitForever);
}

static void brucegotchi_tick_callback(void* ctx) {
    furi_assert(ctx);

    BruceGotchiApp* app = ctx;
    BruceGotchiEvent event = {.type = BruceGotchiEventTypeTick};
    furi_message_queue_put(app->event_queue, &event, 0);
}

int32_t mostlygotchi_app(void* p) {
    UNUSED(p);

    BruceGotchiApp app = {0};
    app.running = true;
    app.mood_index = 0;
    app.frame = 0;

    app.mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(app.mutex == NULL) return 255;

    app.event_queue = furi_message_queue_alloc(8, sizeof(BruceGotchiEvent));
    if(app.event_queue == NULL) {
        furi_mutex_free(app.mutex);
        return 255;
    }

    app.view_port = view_port_alloc();
    if(app.view_port == NULL) {
        furi_message_queue_free(app.event_queue);
        furi_mutex_free(app.mutex);
        return 255;
    }

    view_port_draw_callback_set(app.view_port, brucegotchi_draw_callback, &app);
    view_port_input_callback_set(app.view_port, brucegotchi_input_callback, &app);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, app.view_port, GuiLayerFullscreen);

    app.timer = furi_timer_alloc(brucegotchi_tick_callback, FuriTimerTypePeriodic, &app);
    if(app.timer == NULL) {
        gui_remove_view_port(gui, app.view_port);
        view_port_free(app.view_port);
        furi_message_queue_free(app.event_queue);
        furi_mutex_free(app.mutex);
        furi_record_close(RECORD_GUI);
        return 255;
    }

    furi_timer_start(app.timer, BRUCEGOTCHI_TICK_MS);

    while(app.running) {
        BruceGotchiEvent event;
        FuriStatus status = furi_message_queue_get(app.event_queue, &event, FuriWaitForever);
        if(status != FuriStatusOk) continue;

        if(furi_mutex_acquire(app.mutex, FuriWaitForever) != FuriStatusOk) continue;

        if(event.type == BruceGotchiEventTypeInput) {
            if(event.input.key == InputKeyBack) {
                app.running = false;
            } else if(event.input.key == InputKeyOk) {
                brucegotchi_next_mood(&app, false);
            } else if(event.input.key == InputKeyUp || event.input.key == InputKeyDown) {
                brucegotchi_next_mood(&app, true);
            }
        } else if(event.type == BruceGotchiEventTypeTick) {
            app.frame++;
            if(app.frame % 3 == 0) {
                brucegotchi_next_mood(&app, true);
            }
        }

        furi_mutex_release(app.mutex);
    }

    furi_timer_stop(app.timer);
    furi_timer_free(app.timer);
    gui_remove_view_port(gui, app.view_port);
    view_port_free(app.view_port);
    furi_message_queue_free(app.event_queue);
    furi_mutex_free(app.mutex);
    furi_record_close(RECORD_GUI);

    return 0;
}
