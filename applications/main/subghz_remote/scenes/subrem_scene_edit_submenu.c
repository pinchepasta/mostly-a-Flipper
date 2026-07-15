#include "../subghz_remote_app_i.h"
#include "../helpers/subrem_custom_event.h"
#include "../helpers/subrem_custom_button_info.h"
#include <stdio.h>

const char* const custom_button_text[NumButtons] = {
    "Default",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
};

const char* subrem_custom_button_display_text(uint8_t button) {
    static char text[6];
    if(button < NumButtons) {
        return custom_button_text[button];
    }
    snprintf(text, sizeof(text), "0x%02X", button);
    return text;
}

typedef enum {
    SubRemSceneEditSubMenuStateList,
    SubRemSceneEditSubMenuStateTextInput,
} SubRemSceneEditSubMenuState;

void subrem_scene_edit_submenu_text_input_callback(void* context) {
    furi_assert(context);
    SubGhzRemoteApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, SubRemCustomEventSceneEditSubmenu);
}

void subrem_scene_edit_submenu_button_input_callback(void* context) {
    furi_assert(context);
    SubGhzRemoteApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, SubRemCustomEventSceneEditButtonInputDone);
}

void subrem_scene_edit_submenu_callback(void* context, uint32_t index) {
    furi_assert(context);
    SubGhzRemoteApp* app = context;

    if(index == EditSubmenuIndexEditButton) {
        SubRemSubFilePreset* sub_preset = app->map_preset->subs_preset[app->chosen_sub];
        if(sub_preset->button == ButtonOK) {
            app->button_tmp[0] = '\0';
        } else if(sub_preset->button < NumButtons) {
            snprintf(app->button_tmp, sizeof(app->button_tmp), "%s", custom_button_text[sub_preset->button]);
        } else {
            app->button_tmp[0] = '\0';
        }
        text_input_set_header_text(app->text_input, "Button (1-9/a-i)");
        text_input_set_result_callback(
            app->text_input,
            subrem_scene_edit_submenu_button_input_callback,
            app,
            app->button_tmp,
            sizeof(app->button_tmp),
            false);
#ifndef FW_ORIGIN_Official
        text_input_set_minimum_length(app->text_input, 0);
#endif
        scene_manager_set_scene_state(
            app->scene_manager, SubRemSceneEditSubMenu, SubRemSceneEditSubMenuStateTextInput);
        view_dispatcher_switch_to_view(app->view_dispatcher, SubRemViewIDTextInput);
        return;
    }

    view_dispatcher_send_custom_event(app->view_dispatcher, index);
    if(index == EditSubmenuIndexEditLabel) {
        view_dispatcher_send_custom_event(app->view_dispatcher, SubRemCustomEventEnterEditLabel);
    } else if(index == EditSubmenuIndexEditFile) {
        view_dispatcher_send_custom_event(app->view_dispatcher, SubRemCustomEventEnterEditFile);
    }
}

void subrem_scene_edit_submenu_var_list_change_callback(VariableItem* item) {
    furi_assert(item);
    SubGhzRemoteApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, subrem_custom_button_display_text(index));
    SubRemSubFilePreset* sub_preset = app->map_preset->subs_preset[app->chosen_sub];
    sub_preset->button = index;
}

void subrem_scene_edit_submenu_on_enter(void* context) {
    furi_assert(context);

    SubGhzRemoteApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    VariableItem* item;

    SubRemSubFilePreset* sub_preset = app->map_preset->subs_preset[app->chosen_sub];
    variable_item_list_set_enter_callback(var_item_list, subrem_scene_edit_submenu_callback, app);

    variable_item_list_add(var_item_list, "Edit Label", 0, NULL, NULL);
    variable_item_list_add(var_item_list, "Edit File", 0, NULL, NULL);
    item = variable_item_list_add(
        var_item_list,
        "Button",
        NumButtons,
        subrem_scene_edit_submenu_var_list_change_callback,
        app);

    variable_item_set_current_value_index(item, sub_preset->button);
    variable_item_set_current_value_text(
        item, subrem_custom_button_display_text(sub_preset->button));

    variable_item_list_add(var_item_list, "Clear Slot", 0, NULL, NULL);

    variable_item_list_set_selected_item(
        var_item_list, scene_manager_get_scene_state(app->scene_manager, SubRemSceneEditSubMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, SubRemViewIDVariableItemList);
}

bool subrem_scene_edit_submenu_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);

    SubGhzRemoteApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        if(scene_manager_get_scene_state(app->scene_manager, SubRemSceneEditSubMenu) ==
           SubRemSceneEditSubMenuStateTextInput) {
            scene_manager_set_scene_state(
                app->scene_manager, SubRemSceneEditSubMenu, SubRemSceneEditSubMenuStateList);
            view_dispatcher_switch_to_view(app->view_dispatcher, SubRemViewIDVariableItemList);
            return true;
        }
    }

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubRemCustomEventEnterEditLabel) {
            scene_manager_next_scene(app->scene_manager, SubRemSceneEditLabel);
            consumed = true;
        } else if(event.event == SubRemCustomEventEnterEditFile) {
            scene_manager_next_scene(app->scene_manager, SubRemSceneOpenSubFile);
            consumed = true;
        } else if(event.event == SubRemCustomEventSceneEditButtonInputDone) {
            SubRemSubFilePreset* sub_preset = app->map_preset->subs_preset[app->chosen_sub];
            uint8_t button = sub_preset->button;
            char key = app->button_tmp[0];
            if(key == '\0') {
                button = ButtonOK;
            } else if(key >= '1' && key <= '9') {
                button = (uint8_t)(key - '1' + 1);
            } else if(key >= 'a' && key <= 'i') {
                button = (uint8_t)(key - 'a' + 1);
            } else if(key >= 'A' && key <= 'I') {
                button = (uint8_t)(key - 'A' + 1);
            }
            if(button < NumButtons) {
                sub_preset->button = button;
            }
            VariableItem* item = variable_item_list_get(app->var_item_list, EditSubmenuIndexEditButton);
            if(item) {
                variable_item_set_current_value_index(item, sub_preset->button);
                variable_item_set_current_value_text(
                    item, subrem_custom_button_display_text(sub_preset->button));
            }
            app->map_not_saved = true;
            scene_manager_set_scene_state(
                app->scene_manager, SubRemSceneEditSubMenu, SubRemSceneEditSubMenuStateList);
            view_dispatcher_switch_to_view(app->view_dispatcher, SubRemViewIDVariableItemList);
            consumed = true;
        } else if(event.event == EditSubmenuIndexClearSlot) {
            subrem_sub_file_preset_reset(app->map_preset->subs_preset[app->chosen_sub]);
            app->map_not_saved = true;
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    }

    return consumed;
}

void subrem_scene_edit_submenu_on_exit(void* context) {
    furi_assert(context);

    SubGhzRemoteApp* app = context;
    variable_item_list_reset(app->var_item_list);
    text_input_reset(app->text_input);
}
