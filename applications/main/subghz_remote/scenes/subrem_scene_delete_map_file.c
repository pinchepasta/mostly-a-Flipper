#include "../subghz_remote_app_i.h"

void subrem_scene_delete_map_file_on_enter(void* context) {
    furi_assert(context);
    SubGhzRemoteApp* app = context;

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(
        &browser_options, SUBREM_APP_EXTENSION, &I_subrem_10px);
    browser_options.base_path = SUBREM_APP_FOLDER;

    // Show file browser to select map file to delete
    if(!dialog_file_browser_show(app->dialogs, app->file_path, app->file_path, &browser_options)) {
        // canceled
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    // Attempt to delete selected file
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool removed = storage_simply_remove(storage, furi_string_get_cstr(app->file_path));
    furi_record_close(RECORD_STORAGE);

    Popup* popup = app->popup;
    popup_set_timeout(popup, 1500);
    popup_set_context(popup, app);

    if(removed) {
        popup_set_header(popup, "DELETED", 63, 16, AlignCenter, AlignBottom);
        popup_set_text(popup, "Map file removed", 63, 30, AlignCenter, AlignBottom);

        // If this was the default path, clear it
        if(subrem_has_default_path()) {
            // load default path and compare
            FuriString* default_path = furi_string_alloc();
            Storage* st = furi_record_open(RECORD_STORAGE);
            FlipperFormat* ff = flipper_format_file_alloc(st);
            if(flipper_format_file_open_existing(ff, SUBREM_APP_CONFIG)) {
                if(flipper_format_read_string(ff, "Default", default_path)) {
                    if(!furi_string_empty(default_path) &&
                       strcmp(furi_string_get_cstr(default_path), furi_string_get_cstr(app->file_path)) == 0) {
                        subrem_clear_default_path();
                    }
                }
            }
            flipper_format_free(ff);
            furi_record_close(RECORD_STORAGE);
            furi_string_free(default_path);
        }
    } else {
        popup_set_header(popup, "ERROR", 63, 16, AlignCenter, AlignBottom);
        popup_set_text(popup, "Could not remove file", 63, 30, AlignCenter, AlignBottom);
    }

    popup_enable_timeout(popup);
    popup_set_callback(popup, NULL);
    view_dispatcher_switch_to_view(app->view_dispatcher, SubRemViewIDPopup);

    // After popup, return back to start scene
    scene_manager_previous_scene(app->scene_manager);
}

bool subrem_scene_delete_map_file_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void subrem_scene_delete_map_file_on_exit(void* context) {
    SubGhzRemoteApp* app = context;
    popup_reset(app->popup);
}
