#!/usr/bin/env bash
# one-file-patcher2.sh — implements the four gaps left by patch 1:
#   1. un-hide  Saved -> Signal Settings  (it is gated behind FuriHalRtcFlagDebug,
#      so on a stock device the whole rolling-code bypass is unreachable)
#   2. expose the global rolling counter multiplier in the UI, persisted
#   3. keep CounterMode when a .sub is re-saved (Emulate rewrites the file, so
#      without this the mode is silently lost after the first transmission)
#   4. ship documentation/SubGHzCounterMode.md
#
# Idempotent. Independent of one-file-patcher.sh (run in any order).
# Usage: ./one-file-patcher2.sh [--dry-run] [--check] [--no-backup] [--root=DIR]
set -euo pipefail

VERSION="1.0.0"
ROOT="$PWD"; DRY=0; CHECK=0; NOBACKUP=0

usage() {
  cat <<EOF
one-file-patcher2.sh v$VERSION — rolling-code bypass: missing pieces

  --check        report only, change nothing
  --dry-run      show what would change, write nothing
  --no-backup    skip .mfp-rolling-bypass2-backup/
  --root=DIR     patch the tree at DIR instead of \$PWD
  -h, --help     this text
EOF
}

for a in "$@"; do
  case "$a" in
    --check)     CHECK=1 ;;
    --dry-run)   DRY=1 ;;
    --no-backup) NOBACKUP=1 ;;
    --root=*)    ROOT="${a#*=}" ;;
    -h|--help)   usage; exit 0 ;;
    *) echo "unknown arg: $a" >&2; usage; exit 2 ;;
  esac
done

command -v python3 >/dev/null 2>&1 || { echo "error: python3 is required" >&2; exit 1; }
export MFP_ROOT="$ROOT" MFP_DRY="$DRY" MFP_CHECK="$CHECK" MFP_NOBACKUP="$NOBACKUP" MFP_VERSION="$VERSION"
exec python3 - <<'PYEOF'
import os, re, sys, shutil

ROOT    = os.environ["MFP_ROOT"].rstrip("/")
DRY     = os.environ["MFP_DRY"] == "1"
CHECK   = os.environ["MFP_CHECK"] == "1"
NOBACK  = os.environ["MFP_NOBACKUP"] == "1"
VERSION = os.environ["MFP_VERSION"]
MARK    = "MFP_RC_BYPASS2"
BAKDIR  = os.path.join(ROOT, ".mfp-rolling-bypass2-backup")

C = {"ok": "\033[32m", "warn": "\033[33m", "err": "\033[31m", "dim": "\033[2m", "off": "\033[0m"}
if not sys.stdout.isatty(): C = {k: "" for k in C}
stats = {"patched": 0, "already": 0, "skipped": 0, "failed": 0}

def say(status, rel, note=""):
    print("  %s %s%s" % (
        {"PATCHED": C["ok"]+"PATCHED"+C["off"], "ALREADY": C["dim"]+"ALREADY"+C["off"],
         "SKIP": C["warn"]+"SKIP   "+C["off"], "FAIL": C["err"]+"FAIL   "+C["off"]}[status],
        rel, ("  "+C["dim"]+note+C["off"]) if note else ""))
    stats[{"PATCHED":"patched","ALREADY":"already","SKIP":"skipped","FAIL":"failed"}[status]] += 1

def p(rel): return os.path.join(ROOT, rel)
def read(rel):
    with open(p(rel), "r", encoding="utf-8", errors="surrogateescape") as f: return f.read()
def backup(rel):
    if NOBACK or DRY or CHECK: return
    dst = os.path.join(BAKDIR, rel)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    if not os.path.exists(dst): shutil.copy2(p(rel), dst)
def write(rel, data, orig):
    if data == orig: return False
    if not (DRY or CHECK):
        backup(rel)
        tmp = p(rel) + ".mfp2.tmp"
        with open(tmp, "w", encoding="utf-8", errors="surrogateescape") as f: f.write(data)
        os.replace(tmp, p(rel))
    return True
def after(text, anchor, payload, last=False):
    i = text.rfind(anchor) if last else text.find(anchor)
    if i < 0: return None
    j = i + len(anchor); return text[:j] + payload + text[j:]
def before(text, anchor, payload, last=False):
    i = text.rfind(anchor) if last else text.find(anchor)
    if i < 0: return None
    return text[:i] + payload + text[i:]

print("\n%sone-file-patcher2.sh v%s%s — rolling-code bypass: missing pieces" % (C["ok"], VERSION, C["off"]))
print("root: %s" % ROOT)
if CHECK: print("mode: check only")
elif DRY: print("mode: dry run")
print("")

REQUIRED = [
    "applications/main/subghz/scenes/subghz_scene_saved_menu.c",
    "applications/main/subghz/scenes/subghz_scene_signal_settings.c",
    "applications/main/subghz/subghz.c",
    "applications/main/subghz/subghz_last_settings.h",
    "applications/main/subghz/subghz_last_settings.c",
    "components/furi_hal/furi_hal_subghz.h",
]
missing = [r for r in REQUIRED if not os.path.exists(p(r))]
if missing:
    print("%serror:%s unexpected tree, missing:" % (C["err"], C["off"]))
    for m in missing: print("   " + m)
    sys.exit(1)

# ==========================================================================
# 1. un-hide "Signal Settings"
# ==========================================================================
rel = "applications/main/subghz/scenes/subghz_scene_saved_menu.c"
src = read(rel)
if MARK in src:
    say("ALREADY", rel, "Signal Settings already unconditional")
else:
    m = re.search(r"if\(furi_hal_rtc_is_flag_set\(FuriHalRtcFlagDebug\)\)\s*\{(?P<body>.*?)\};", src, re.S)
    if not m:
        say("FAIL", rel, "debug gate not found")
    else:
        repl = ("/* %s: the rolling-code counter mode lives here; keep it reachable\n"
                "     * without turning on the debug flag */\n%s" % (MARK, m.group("body").strip("\n")))
        if write(rel, src[:m.start()] + repl + src[m.end():], src):
            say("PATCHED", rel, "Signal Settings always visible")

# ==========================================================================
# 3a. persist the multiplier (must come before 2 so 2 can rely on the field)
# ==========================================================================
MULT_FIELD = "    int32_t rolling_counter_mult; /* %s: rolling-code step on TX */\n" % MARK
FIELD_OK = False

rel = "applications/main/subghz/subghz_last_settings.h"
src = read(rel)
if "rolling_counter_mult" in src:
    FIELD_OK = True
    say("ALREADY", rel, "rolling_counter_mult field")
else:
    m = re.search(r"typedef\s+struct\s*\{(?P<body>.*?)\}\s*SubGhzLastSettings\s*;", src, re.S)
    if not m:
        say("FAIL", rel, "SubGhzLastSettings struct not found")
    else:
        out = src[:m.end("body")] + MULT_FIELD + src[m.end("body"):]
        if write(rel, out, src):
            FIELD_OK = True
            say("PATCHED", rel, "rolling_counter_mult field")

rel = "applications/main/subghz/subghz_last_settings.c"
src = read(rel)
if MARK in src:
    FIELD_OK = FIELD_OK and True
    say("ALREADY", rel, "multiplier load/save")
else:
    lo = src.find("void subghz_last_settings_load")
    hi = src.find("bool subghz_last_settings_save")
    if lo < 0 or hi < 0 or hi <= lo:
        say("SKIP", rel, "load/save not found")
    else:
        head, load, save, tail = src[:lo], src[lo:hi], src[hi:], src[hi:]
        tail = ""
        ok = True

        # default value
        d = after(load, "furi_assert(instance);",
                  "\n    instance->rolling_counter_mult = 1; /* %s: default step */" % MARK)
        if d is None: ok = False; say("SKIP", rel, "default anchor")
        else: load = d

        # read (last thing in the do-while, so a missing field just rewinds)
        r = before(load, "} while(0);",
                   '        if(!flipper_format_read_uint32(\n'
                   '               fff_data_file, "RollingCounterMult", (uint32_t*)&instance->rolling_counter_mult, 1)) {\n'
                   '            flipper_format_rewind(fff_data_file);\n'
                   '        }\n', last=True)
        if r is None: ok = False; say("SKIP", rel, "read anchor")
        else: load = r

        # write
        w = before(save, "saved = true;",
                   '        uint32_t mfp_mult = (uint32_t)instance->rolling_counter_mult;\n'
                   '        if(!flipper_format_write_uint32(file, "RollingCounterMult", &mfp_mult, 1)) {\n'
                   '            break;\n'
                   '        }\n')
        if w is None: ok = False; say("SKIP", rel, "write anchor")
        else: save = w

        if ok and write(rel, head + load + save + tail, src):
            say("PATCHED", rel, "multiplier persisted")
        elif not ok:
            FIELD_OK = False

# push the saved value into the HAL at startup
rel = "applications/main/subghz/subghz.c"
src = read(rel)
if MARK in src:
    say("ALREADY", rel, "multiplier applied at startup")
elif not FIELD_OK:
    say("SKIP", rel, "no field to read")
else:
    out = after(src, "subghz_last_settings_load(subghz->last_settings);",
                "\n    /* %s: restore the user's rolling-code step */\n"
                "    furi_hal_subghz_set_rolling_counter_mult(\n"
                "        subghz->last_settings->rolling_counter_mult);" % MARK)
    if out is None:
        say("FAIL", rel, "last_settings_load call not found")
    elif write(rel, out, src):
        say("PATCHED", rel, "multiplier restored on startup")

# ==========================================================================
# 2. counter multiplier in the UI
# ==========================================================================
rel = "applications/main/subghz/scenes/subghz_scene_signal_settings.c"
src = read(rel)
if "subghz_scene_signal_settings_counter_mult_changed" in src:
    say("ALREADY", rel, "counter multiplier item")
else:
    if FIELD_OK:
        body = """/* ---- %(M)s: global rolling-code counter step ---- */
#define MFP_MULT_COUNT 5
static const char* const mfp_mult_text[MFP_MULT_COUNT] = {
    "+1 (default)", "0 (freeze)", "+2", "+4", "-1"};
static const int32_t mfp_mult_value[MFP_MULT_COUNT] = {1, 0, 2, 4, -1};

void subghz_scene_signal_settings_counter_mult_changed(VariableItem* item) {
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, mfp_mult_text[idx]);

    SubGhz* subghz = variable_item_get_context(item);
    subghz->last_settings->rolling_counter_mult = mfp_mult_value[idx];
    furi_hal_subghz_set_rolling_counter_mult(mfp_mult_value[idx]);
    subghz_last_settings_save(subghz->last_settings);
}
/* ---- end %(M)s ---- */
""" % {"M": MARK}

        on_enter = """
    /* %(M)s: global counter step, appended so existing item indices do not shift */
    {
        int32_t mfp_cur = subghz->last_settings->rolling_counter_mult;
        int32_t mfp_idx = 0;
        for(uint8_t mfp_i = 0; mfp_i < MFP_MULT_COUNT; mfp_i++) {
            if(mfp_mult_value[mfp_i] == mfp_cur) mfp_idx = mfp_i;
        }
        furi_hal_subghz_set_rolling_counter_mult(mfp_mult_value[mfp_idx]);

        item = variable_item_list_add(
            variable_item_list,
            "Counter Mult",
            MFP_MULT_COUNT,
            subghz_scene_signal_settings_counter_mult_changed,
            subghz);
        variable_item_set_current_value_index(item, mfp_idx);
        variable_item_set_current_value_text(item, mfp_mult_text[mfp_idx]);
    }
""" % {"M": MARK}
    else:
        body = """/* ---- %(M)s: global rolling-code counter step (session only) ---- */
#define MFP_MULT_COUNT 5
static const char* const mfp_mult_text[MFP_MULT_COUNT] = {
    "+1 (default)", "0 (freeze)", "+2", "+4", "-1"};
static const int32_t mfp_mult_value[MFP_MULT_COUNT] = {1, 0, 2, 4, -1};

void subghz_scene_signal_settings_counter_mult_changed(VariableItem* item) {
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, mfp_mult_text[idx]);
    furi_hal_subghz_set_rolling_counter_mult(mfp_mult_value[idx]);
}
/* ---- end %(M)s ---- */
""" % {"M": MARK}

        on_enter = """
    /* %(M)s: global counter step */
    {
        int32_t mfp_cur = furi_hal_subghz_get_rolling_counter_mult();
        int32_t mfp_idx = 0;
        for(uint8_t mfp_i = 0; mfp_i < MFP_MULT_COUNT; mfp_i++) {
            if(mfp_mult_value[mfp_i] == mfp_cur) mfp_idx = mfp_i;
        }
        item = variable_item_list_add(
            variable_item_list,
            "Counter Mult",
            MFP_MULT_COUNT,
            subghz_scene_signal_settings_counter_mult_changed,
            subghz);
        variable_item_set_current_value_index(item, mfp_idx);
        variable_item_set_current_value_text(item, mfp_mult_text[mfp_idx]);
    }
""" % {"M": MARK}

    out = after(src, "void subghz_scene_signal_settings_byte_input_callback(void* context) {",
                "\n" + body)
    if out is None:
        say("FAIL", rel, "helper anchor not found")
    else:
        anchor = 'variable_item_set_locked(item, (button_not_available), "Not available\\nfor this\\nprotocol !");'
        out2 = after(out, anchor, "\n" + on_enter)
        if out2 is None:
            say("FAIL", rel, "on_enter anchor not found")
        elif write(rel, out2, src):
            say("PATCHED", rel, "Counter Mult item" + (" (persisted)" if FIELD_OK else ""))

# ==========================================================================
# 3b. keep CounterMode when the file is re-saved
#     Emulate re-saves the .sub, so without this the mode dies on first TX.
# ==========================================================================
EXCLUDE = {"build", "managed_components", "node_modules", ".git",
           ".mfp-rolling-bypass-backup", ".mfp-rolling-bypass2-backup"}
SIG = r"bool\s+subghz_save_protocol_to_file\s*\((?P<params>[^)]*)\)\s*\{"
found = False
for dirpath, dirnames, files in os.walk(ROOT):
    dirnames[:] = [d for d in dirnames if d not in EXCLUDE and not d.startswith(".")]
    for fn in sorted(files):
        if not fn.endswith(".c"): continue
        rel = os.path.relpath(os.path.join(dirpath, fn), ROOT)
        try: src = read(rel)
        except OSError: continue
        m = re.search(SIG, src, re.S)
        if not m: continue
        found = True
        if MARK + ": keep CounterMode" in src:
            say("ALREADY", rel, "CounterMode preserved on save")
            break
        names = []
        for prm in m.group("params").split(","):
            nm = re.search(r"(\w+)\s*$", prm.strip())
            names.append(nm.group(1) if nm else None)
        if len(names) != 3 or not all(names):
            say("SKIP", rel, "unexpected signature: %s" % m.group("params").strip())
            break
        sub, ff, dst = names
        payload = """
    /* %(M)s: keep CounterMode across saves — Emulate rewrites this file, and the
     * protocol serializer does not emit CounterMode itself. */
    {
        const char* mfp_src = (%(D)s && %(D)s[0]) ?
                                  %(D)s :
                                  ((%(S)s && %(S)s->file_path) ?
                                       furi_string_get_cstr(%(S)s->file_path) :
                                       NULL);
        if(mfp_src && %(F)s) {
            Storage* mfp_st = furi_record_open(RECORD_STORAGE);
            FlipperFormat* mfp_rd = flipper_format_file_alloc(mfp_st);
            uint32_t mfp_cm = 0;
            if(flipper_format_file_open_existing(mfp_rd, mfp_src) &&
               flipper_format_read_uint32(mfp_rd, "CounterMode", &mfp_cm, 1)) {
                flipper_format_rewind(%(F)s);
                flipper_format_insert_or_update_uint32(%(F)s, "CounterMode", &mfp_cm, 1);
            }
            flipper_format_file_close(mfp_rd);
            flipper_format_free(mfp_rd);
            furi_record_close(RECORD_STORAGE);
        }
    }
""" % {"M": MARK, "S": sub, "F": ff, "D": dst}
        if write(rel, src[:m.end()] + payload + src[m.end():], src):
            say("PATCHED", rel, "CounterMode preserved on save")
        break
if not found:
    say("SKIP", "subghz app", "subghz_save_protocol_to_file not located")

# ==========================================================================
# 4. documentation
# ==========================================================================
DOC = """# Sub-GHz Rolling-Code Counter Mode (desync bypass)

Lets a clone live next to the original remote. The counter step is changed so the
receiver keeps accepting both, instead of the clone and the real remote
knocking each other out of sync.

Reach it on the device: **Saved -> pick a signal -> Signal Settings**.
`Counter Mode` is per signal (written into the `.sub` as `CounterMode: X`),
`Counter Mult` is global and persists across reboots.

With no `CounterMode` line, mode `0` applies: behave exactly like a normal
remote, counter += `Counter Mult` (default `+1`).

## Modes

| Protocol | Modes |
|---|---|
| Nice FloR-S | 0 default, 1 `0001/FFFE`, 2 `0000/0001` |
| CAME Atomo | 0 default, 1 `0000/0001/FFFE/FFFF`, 2 `807B/807C/007B/007C`, 3 freeze |
| Alutech AT-4N | 0 default, 1 `0000/0001/FFFE/FFFF`, 2 `0000..0005` |
| KeeLoq | 0 default, 1 `0000/0001/FFFE/FFFF`, 2 `+0x3333`, 3 `8006/8007/0006/0007`, 4 `807B/807C/007B/007C`, 5 `0000/FFFF`, 6 freeze, 7 `+0x3333` burst |
| V2 Phoenix | 0 default, 1 `0000/0001/FFFE/FFFF`, 2 `0000..0004` |

An out-of-range mode falls back to that protocol's last mode. Setting
`Counter Mult` to `0` freezes the counter for every dynamic protocol.

## Manual editing