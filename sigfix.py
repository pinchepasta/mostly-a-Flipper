#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# MFP subghz scene compile fixer  v3.1.0
#
# v3.1.0
#   * extract the nested counter-mult callback to file scope (a nested
#     function is a local symbol; nothing outside its parent can link to it)
#
# Usage:
#   python3 sigfix.py --probe [repo_root]
#   python3 sigfix.py [repo_root]

import os
import re
import sys
import shutil

VERSION = "3.1.0"
SCENES = "applications/main/subghz/scenes"
START = os.path.join(SCENES, "subghz_scene_start.c")
SIG = os.path.join(SCENES, "subghz_scene_signal_settings.c")
CALLBACK = "subghz_scene_signal_settings_counter_mult_changed"
TABLES = ("mfp_mult_value", "mfp_mult_text")
MARK = "MFP_SCENE_FIX"
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


def brace_depth(t, idx):
    d, i = 0, 0
    while i < idx:
        c = t[i]
        if c == '/' and t[i + 1:i + 2] == '/':
            j = t.find('\n', i)
            i = j if j >= 0 else idx
            continue
        if c == '/' and t[i + 1:i + 2] == '*':
            j = t.find('*/', i + 2)
            i = (j + 2) if j >= 0 else idx
            continue
        if c == '"':
            i += 1
            while i < idx and t[i] != '"':
                if t[i] == '\\':
                    i += 1
                i += 1
        elif c == '{':
            d += 1
        elif c == '}':
            d -= 1
        i += 1
    return d


def dedent_block(s):
    lines = s.splitlines()
    inds = [len(l) - len(l.lstrip()) for l in lines if l.strip()]
    k = min(inds) if inds else 0
    return "\n".join((l[k:] if len(l) >= k else l) if l.strip() else ""
                     for l in lines)


def first_function_pos(t):
    for m in re.finditer(r'(?m)^[A-Za-z_][\w\s\*]*\b\w+\s*\([^;{)]*\)\s*\{', t):
        line = t[t.rfind("\n", 0, m.start()) + 1:m.start()]
        if line.lstrip().startswith("#"):
            continue
        return m.start()
    return None


# --------------------------------------------------------------------------- #
def fix_menu_token():
    rp = find_file(os.path.basename(START), START)
    if not rp:
        note("FAIL", START, "not found")
        return
    t = read(rp)
    toks = []
    for m in re.finditer(r'submenu_add_item\s*\(', t):
        close = match_paren(t, m.end() - 1)
        if close < 0:
            continue
        args = [a.strip() for a in t[m.end():close].split(",")]
        if len(args) >= 3 and args[1].startswith('"'):
            toks.append((args[1].strip('"'), args[2]))
    ours = [tk for lab, tk in toks if lab == "Rolljam"]
    if not ours:
        note("ALREADY", rp, 'no "Rolljam" menu item')
        return
    token = ours[0]
    siblings = [tk for lab, tk in toks if lab != "Rolljam"]
    if any(re.search(r'\b' + re.escape(token) + r'\b', body)
           for _, body, _, _ in find_enums(t)):
        note("ALREADY", rp, token + " declared")
        return
    best = None
    for p in walk():
        if not p.endswith((".c", ".h")):
            continue
        try:
            ft = read(rel(p))
        except Exception:
            continue
        for name, body, bspan, m in find_enums(ft):
            hits = sum(1 for s in siblings
                       if re.search(r'\b' + re.escape(s) + r'\b', body))
            if hits and (best is None or hits > best[0]):
                best = (hits, rel(p), name, bspan, ft)
    if best is None:
        t = insert_after_includes(
            t, "/* " + MARK + ": submenu index */\nenum { %s = 64 };\n" % token)
        write(rp, t)
        note("PATCHED", rp, "local enum emitted")
        return
    hits, host, ename, bspan, ht = best
    if not re.search(r'\b' + re.escape(token) + r'\b', ht):
        body = ht[bspan[0]:bspan[1]]
        ind = re.findall(r'(?m)^([ \t]*)\w', body)
        ht = ht[:bspan[1]] + (ind[-1] if ind else "    ") + token + ",\n" + ht[bspan[1]:]
        write(host, ht)
        note("PATCHED", host, token + " added to enum " + ename)


def match_paren(t, i):
    d = 0
    while i < len(t):
        c = t[i]
        if c == '(':
            d += 1
        elif c == ')':
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


def insert_after_includes(t, block):
    ms = list(re.finditer(r'^#include .*$', t, re.M))
    if not ms:
        return block + "\n" + t
    m = ms[-1]
    return t[:m.end()] + "\n" + block + t[m.end():]


def find_enums(t):
    return [(m.group(3) or m.group(1) or "<anon>", m.group(2), m.span(2), m)
            for m in re.finditer(
                r'(?:typedef\s+)?enum\s*(\w*)\s*\{([^}]*)\}\s*(\w*)\s*;', t, re.S)]


def extract_nested_callback():
    rp = find_file(os.path.basename(SIG), SIG)
    if not rp:
        note("FAIL", SIG, "not found")
        return
    t = read(rp)
    m = re.search(r'(?m)^([ \t]*)((?:static\s+)?void\s+' + re.escape(CALLBACK) +
                  r'\s*\([^;{]*\)\s*\{)', t)
    if not m:
        note("WARN", rp, CALLBACK + " definition not found")
        return
    depth = brace_depth(t, m.start())
    if depth == 0:
        note("INFO", rp, "callback already at file scope")
        return

    b = t.index("{", m.start() + len(m.group(1)) + len(m.group(2)) - 1)
    e = match_brace(t, b)
    if e < 0:
        note("WARN", rp, "unbalanced callback body")
        return
    start = t.rfind("\n", 0, m.start()) + 1
    nl = t.find("\n", e)
    end = (nl + 1) if nl >= 0 else len(t)
    fn = dedent_block(t[start:end])

    rest = t[:start] + t[end:]
    at = first_function_pos(rest)
    if at is None:
        at = len(rest)
    block = "/* " + MARK + ": extracted from enclosing scope (was nested) */\n" + fn
    out = rest[:at] + block + ("\n" if not block.endswith("\n") else "") + rest[at:]
    write(rp, out)
    note("PATCHED", rp, "callback extracted to file scope (was nested, depth %d)" % depth)
    note("INFO", rp, fn.splitlines()[0][:80])


def probe():
    for path, lo, hi in ((SIG, 1, 26), (SIG, 100, 145)):
        p = find_file(os.path.basename(path), path)
        if not p:
            continue
        lines = read(p).splitlines()
        print("\n--- %s  [%d-%d]" % (p, lo, hi))
        for i in range(max(0, lo - 1), min(len(lines), hi)):
            print("%5d| %s" % (i + 1, lines[i][:110]))


def main():
    print("MFP subghz scene fixer v" + VERSION + (" [probe]" if PROBE else ""))
    print("root: " + ROOT)
    if not os.path.isdir(ROOT):
        print("!! not a directory")
        return 1
    if PROBE:
        probe()
        return 0

    for fn in (fix_menu_token, extract_nested_callback):
        try:
            fn()
        except Exception as e:
            note("FAIL", fn.__name__, "%s: %s" % (type(e).__name__, e))

    print("\n== actions")
    for st, rp, msg in LOG:
        print("  %-8s %-50s %s" % (st, rp[:50], msg))

    print("\n== next")
    print('  . "$HOME/esp/esp-idf/export.sh"')
    print('  idf.py -B build_cardputer_adv build')
    return 0


if __name__ == "__main__":
    sys.exit(main())