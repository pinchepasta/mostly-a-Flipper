#include "../subghz_i.h"
#include "subghz_scene_waterfall.h"

void subghz_scene_waterfall_on_enter(void* context) {
    SubGhz* subghz = context;
    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdWaterfall);
}

bool subghz_scene_waterfall_on_event(void* context, SceneManagerEvent event) {
    UNUSED(event);
    SubGhz* subghz = context;
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(subghz->scene_manager);
        return true;
    }
    return false;
}

void subghz_scene_waterfall_on_exit(void* context) {
    UNUSED(context);
}
