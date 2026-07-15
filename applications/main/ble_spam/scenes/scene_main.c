#include "../ble_spam_app.h"
#include "../ble_spam_hal.h"

enum MainMenuIndex {
    MainMenuIndexBleSpam,
    MainMenuIndexBleWalk,
    MainMenuIndexBleAutoWalk,
    MainMenuIndexBleTracker,
    MainMenuIndexBleRaceDetector,
};

static void main_menu_callback(void* context, uint32_t index) {
    BleSpamApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void ble_spam_scene_main_on_enter(void* context) {
    BleSpamApp* app = context;

    // Returning to the main menu means the user left the spam section: fully
    // tear the controller down here (idempotent — no-op on first entry or if it
    // was never started). Keeping it up only while navigating spam<->running is
    // what avoids the per-attack init/deinit leak.
    ble_spam_hal_stop();

    submenu_add_item(app->submenu, "Spam", MainMenuIndexBleSpam, main_menu_callback, app);
    submenu_add_item(app->submenu, "Walk", MainMenuIndexBleWalk, main_menu_callback, app);
    submenu_add_item(
        app->submenu, "Auto Walk", MainMenuIndexBleAutoWalk, main_menu_callback, app);
    submenu_add_item(
        app->submenu, "Tracker", MainMenuIndexBleTracker, main_menu_callback, app);
    submenu_add_item(
        app->submenu,
        "Airoha RACE",
        MainMenuIndexBleRaceDetector,
        main_menu_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, BleSpamViewSubmenu);
}

bool ble_spam_scene_main_on_event(void* context, SceneManagerEvent event) {
    BleSpamApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case MainMenuIndexBleSpam:
            scene_manager_next_scene(app->scene_manager, BleSpamSceneSpamMenu);
            consumed = true;
            break;
        case MainMenuIndexBleWalk:
            scene_manager_next_scene(app->scene_manager, BleSpamSceneWalkScan);
            consumed = true;
            break;
        case MainMenuIndexBleAutoWalk:
            scene_manager_next_scene(app->scene_manager, BleSpamSceneAutoWalk);
            consumed = true;
            break;
        case MainMenuIndexBleTracker:
            scene_manager_next_scene(app->scene_manager, BleSpamSceneTrackerScan);
            consumed = true;
            break;
        case MainMenuIndexBleRaceDetector:
            scene_manager_next_scene(app->scene_manager, BleSpamSceneRaceDetector);
            consumed = true;
            break;
        }
    }

    return consumed;
}

void ble_spam_scene_main_on_exit(void* context) {
    BleSpamApp* app = context;
    submenu_reset(app->submenu);
}
