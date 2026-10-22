#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# MFP rolling-counter restore call fixer  v2.6.0
#
# nupatch.sh v2.4.0 fix_startup_restore() captured the whole argument list of
# subghz_last_settings_load() and appended ->rolling_counter_mult to it, which
# only worked when that call had a single argument.  It does not.  Produced:
#
#   furi_hal_subghz_set_rolling_counter_mult(subghz->last_settings, 0->rolling_counter_mult);
#
# Correct:
#
#   furi_hal_subghz_set_rolling_counter_mult(subghz->last_settings->rolling_counter_mult);
#
# Usage:
#   python3 rcfix.py --probe [repo_root]
#   python3 rcfix.py [repo_root]

import os
import re
import sys
import shutil

VERSION = "2.6.0"
FUNC = "furi_hal_subghz_set_rolling_counter_mult"
MEMBER = "rolling_counter_mult"
HDR = "applications/main/subghz/helpers/subghz_last_settings.h"
SRC = "applications/main/subghz/helpers/subghz_last_settings.c"
MARK = "MFP_RC_FIX2"
BACKUP_NAME = ".mfp-rolljam-backup"
LOG = []


def parse_args(argv):
    root, probe = os.getcwd(), False
    for a in argv:
        if a in ("-V", "--version"):
            print(VERSION)
            sys.exit(0)
        if a in ("-h", "--help"):
            print(__doc__)
            sys.exit(0)
        if a == "--probe":
            probe = True
        elif not a.startswith("-"):
            root = os.path.abspath(a)
    return root, probe


ROOT, PROBE = parse_args(sys.argv[1:])
BACKUP = os.path.join(ROOT, BACKUP_NAME)


def note(st, where, msg):
    LOG.append((st, where, msg))


def rel(p):
    return os.path.relpath(p, ROOT)


def read(rp):
    with open(os.path.join(ROOT, rp), "r", encoding="utf-8",
              errors="surrogateescape") as f:
        return f.read()


def write(rp, txt):
    p = os.path.join(ROOT, rp)
    d = os.path.join(BACKUP, rp)
    if not os.path.exists(d):
        os.makedirs(os.path.dirname(d), exist_ok=True)
        shutil.copy2(p, d)
    with open(p, "w", encoding="utf-8", errors="surrogateescape") as f:
        f.write(txt)


def walk():
    for dp, dns, fns in os.walk(ROOT):
        dns[:] = sorted(d for d in dns
                        if not d.startswith(".") and not d.startswith("build"))
        for fn in sorted(fns):
            yield os.path.join(dp, fn)


def find_file(name, prefer=None):
    if prefer and os.path.exists(os.path.join(ROOT, prefer)):
        return prefer
    hits = [rel(p) for p in walk() if os.path.basename(p) == name]
    hits.sort(key=lambda s: (s.count("/"), len(s)))
    return hits[0] if hits else None


def match_brace(t, i):
    d = 0
    while i < len(t):
        c = t[i]
        if c == '{':
            d += 1
        elif c == '}':
            d -= 1
            if d == 0:
                return i
        elif c == '"':
            i += 1
            while i < len(t) and t[i] != '"':
                if t[i] == '\\':
                    i += 1
                i += 1
        i += 1
    return -1


CALL_RX = re.compile(r'\b' + re.escape(FUNC) + r'\s*\(([^;()]*)\)')


def sub_call(m):
    parts = [p.strip() for p in m.group(1).split(",")]
    if len(parts) < 2:
        return m.group(0)
    if not re.search(r'(?:->|\.)' + MEMBER + r'\s*$', parts[-1]):
        return m.group(0)
    a = parts[0]
    if a.startswith("&"):
        a = a[1:].strip()
        sep = "->" if "->" in a else "."
    else:
        sep = "->"
    return FUNC + "(" + a + sep + MEMBER + ")"


def fix_calls():
    hits = 0
    for p in walk():
        if not p.endswith((".c", ".cpp")):
            continue
        rp = rel(p)
        try:
            t = read(rp)
        except Exception:
            continue
        if FUNC not in t:
            continue
        new = CALL_RX.sub(sub_call, t)
        if new == t:
            continue
        write(rp, new)
        hits += 1
        for m in re.finditer(re.escape(FUNC) + r'\s*\([^;]*\);', new):
            note("PATCHED", rp, m.group(0).strip()[:86])
    if not hits:
        note("ALREADY", "tree", "no mangled " + FUNC + "() calls")


def fn_span(t, name):
    m = re.search(r'\b' + re.escape(name) + r'\s*\(', t)
    if not m:
        return None
    b = t.find('{', m.end())
    if b < 0:
        return None
    e = match_brace(t, b)
    return (b, e) if e > b else None


def ff_arg(body, fn):
    m = re.search(re.escape(fn) + r'\s*\(\s*([A-Za-z_]\w*)', body)
    return m.group(1) if m else None


def ensure_member():
    hp = find_file(os.path.basename(HDR), HDR)
    cp = find_file(os.path.basename(SRC), SRC)
    if not hp or not cp:
        note("FAIL", HDR, "subghz_last_settings.{h,c} not found")
        return

    ht = read(hp)
    m = re.search(r'typedef\s+struct\s*\{', ht)
    if not m:
        note("FAIL", hp, "no typedef struct in settings header")
        return
    b = ht.find('{', m.end() - 1)
    e = match_brace(ht, b)
    body = ht[b + 1:e]
    if re.search(r'\b' + MEMBER + r'\b', body):
        note("OK", hp, "SubGhzLastSettings already has " + MEMBER)
        return
    if PROBE:
        note("INFO", hp, "would add int32_t " + MEMBER + " to SubGhzLastSettings")
        return

    ht = ht[:e] + "    int32_t " + MEMBER + "; /* " + MARK + " */\n" + ht[e:]
    write(hp, ht)
    note("PATCHED", hp, "int32_t " + MEMBER + " added to SubGhzLastSettings")

    st = read(cp)

    sp = fn_span(st, "subghz_last_settings_load")
    if sp:
        b2, e2 = sp
        bd = st[b2 + 1:e2]
        ff = ff_arg(bd, "flipper_format_read_uint32")
        inst = re.search(r'\(\s*([A-Za-z_]\w*\s*\*?\s*)?([A-Za-z_]\w*)\s*[,)]',
                         st[b2:e2])
        var = inst.group(2) if inst else "instance"
        if ff:
            blk = ("    /* %s */\n"
                   "    uint32_t mfp_rc_mult = 1;\n"
                   "    if(!flipper_format_read_uint32(\n"
                   "           %s, \"RollingCounterMult\", &mfp_rc_mult, 1)) {\n"
                   "        mfp_rc_mult = 1;\n"
                   "    }\n"
                   "    %s->%s = (int32_t)mfp_rc_mult;\n" % (MARK, ff, var, MEMBER))
            at = b2 + 1 + bd.rfind("return true;")
            st = st[:at] + blk + st[at:]
            note("PATCHED", cp, "load: RollingCounterMult read")
        else:
            note("WARN", cp, "no flipper_format_read_uint32 in load()")
    else:
        note("WARN", cp, "subghz_last_settings_load() not found")

    sp = fn_span(st, "subghz_last_settings_save")
    if sp:
        b2, e2 = sp
        bd = st[b2 + 1:e2]
        ff = ff_arg(bd, "flipper_format_write_uint32")
        inst = re.search(r'\(\s*([A-Za-z_]\w*\s*\*?\s*)?([A-Za-z_]\w*)\s*[,)]',
                         st[b2:e2])
        var = inst.group(2) if inst else "instance"
        if ff:
            blk = ("    /* %s */\n"
                   "    uint32_t mfp_rc_mult = (uint32_t)%s->%s;\n"
                   "    flipper_format_write_uint32(\n"
                   "        %s, \"RollingCounterMult\", &mfp_rc_mult, 1);\n"
                   % (MARK, var, MEMBER, ff))
            at = b2 + 1 + bd.rfind("return true;")
            st = st[:at] + blk + st[at:]
            note("PATCHED", cp, "save: RollingCounterMult written")
        else:
            note("WARN", cp, "no flipper_format_write_uint32 in save()")
    else:
        note("WARN", cp, "subghz_last_settings_save() not found")

    write(cp, st)


def verify():
    out = []
    bad = []
    for p in walk():
        if not p.endswith((".c", ".cpp")):
            continue
        rp = rel(p)
        try:
            t = read(rp)
        except Exception:
            continue
        for m in re.finditer(re.escape(FUNC) + r'\s*\([^;]*\);', t):
            if "," in m.group(0):
                bad.append(rp + ": " + m.group(0).strip()[:70])
    out.append(("OK" if not bad else "FAIL", "calls",
                "all single-argument" if not bad else bad[0]))
    for extra in bad[1:]:
        out.append(("FAIL", "calls", extra))

    hp = find_file(os.path.basename(HDR), HDR)
    if hp:
        ht = read(hp)
        m = re.search(r'typedef\s+struct\s*\{', ht)
        b = ht.find('{', m.end() - 1) if m else -1
        e = match_brace(ht, b) if b > 0 else -1
        ok = e > b and re.search(r'\b' + MEMBER + r'\b', ht[b + 1:e])
        out.append(("OK" if ok else "FAIL", hp, MEMBER + " present in struct"))
    return out


def main():
    print("MFP rolling-counter call fixer v" + VERSION + (" [probe]" if PROBE else ""))
    print("root: " + ROOT)
    if not os.path.isdir(ROOT):
        print("!! not a directory")
        return 1
    if not os.environ.get("IDF_PATH"):
        print("!! IDF_PATH not set (idf.py needs it)")

    try:
        fix_calls()
        ensure_member()
    except Exception as e:
        note("FAIL", "fix", "%s: %s" % (type(e).__name__, e))

    print("\n== actions")
    for st, rp, msg in LOG:
        print("  %-8s %-50s %s" % (st, rp[:50], msg))

    if PROBE:
        return 0

    print("\n== verify")
    for st, rp, msg in verify():
        print("  %-8s %-50s %s" % (st, rp[:50], msg))

    print("\n== next")
    print('  . "$HOME/esp/esp-idf/export.sh"')
    print('  idf.py -B build_cardputer_adv build')
    return 0


if __name__ == "__main__":
    sys.exit(main())