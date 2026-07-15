/** Compatibility shim for upstream Flipper external apps.
 *
 * Upstream firmware exposes FuriString via <furi/core/string.h>. This ESP32
 * port renamed the header to furi_string.h. Apps written against the official
 * API include the original path, so forward to it here.
 *
 * This lives under components/compat/ (not components/furi/core/) on purpose:
 * components/furi/core/ is a direct -I include root, so a string.h placed
 * there would shadow libc's <string.h>. The compat root has no top-level
 * string.h, so only the fully-qualified <furi/core/string.h> resolves here.
 *
 * @file string.h
 */

#pragma once

#include "furi_string.h"
