#include "../js_modules.h"

void* js_badusb_create(struct mjs* mjs, mjs_val_t* object, JsModules* modules);
void js_badusb_destroy(void* inst);

static const JsModuleDescriptor js_badkeyboard_desc = {
    "badkeyboard",
    js_badusb_create,
    js_badusb_destroy,
    NULL,
};

static const FlipperAppPluginDescriptor plugin_descriptor = {
    .appid = PLUGIN_APP_ID,
    .ep_api_version = PLUGIN_API_VERSION,
    .entry_point = &js_badkeyboard_desc,
};

const FlipperAppPluginDescriptor* js_badkeyboard_ep(void) {
    return &plugin_descriptor;
}
