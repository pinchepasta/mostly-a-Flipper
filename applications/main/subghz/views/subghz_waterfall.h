#pragma once

#include <gui/view.h>

typedef struct SubGhzWaterfall SubGhzWaterfall;

SubGhzWaterfall* subghz_waterfall_alloc(void* context);

void subghz_waterfall_free(SubGhzWaterfall* instance);

View* subghz_waterfall_get_view(SubGhzWaterfall* instance);
