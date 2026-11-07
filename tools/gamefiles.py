#!/usr/bin/env python3
r"""Put your copy of Monster Truck Madness 2's files in game/, from the disc.

    py -3 tools/gamefiles.py "Monster Truck Madness 2.iso"   # a 2048-byte ISO
    py -3 tools/gamefiles.py E:\                             # the CD in a drive, or a copy of it

The 1998 demo goes in game-demo/ instead (tools/variant.py):

    py -3 tools/gamefiles.py --demo "C:\Program Files\Microsoft Games\Monster Truck Madness 2 Demo"
    py -3 tools/gamefiles.py --demo --exe MTM2Demo.exe <folder>   # if its exe has another name

The demo comes as an installer: run it (on any Windows PC) and give this the
folder it installed to. That folder is copied as it is, its exe as
Monster.exe. A demo disc laid out like the full game's (ENGLISH\, SHARED\)
is read like the full game.

The disc keeps the game in two folders: ENGLISH\ (Monster.ex_, which is a
plain PE despite the name, the renderer DLLs, UI/SOUND/TRUCK2.POD, videos)
and SHARED\ (the track PODs, music, the other renderer DLLs). The ACME
installer merged them into one folder; so does this, except SHARED\VIDEOS,
which the game opens as Shared\Videos\ under its folder. Setup, DirectX 5 and
Indeo are not needed and not copied.

Disc images are read with pcrecomp's ISO9660 walker
(tools/assets/iso_peek.py), so nothing is mounted. It supplies your own copy
to the local build; nothing is downloaded, and none of it is ever committed.
"""
import argparse
import os
import shutil
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import variant  # noqa: E402

ROOT = variant.ROOT
LANG = 'ENGLISH/'
SKIP = ('SETUP', 'DSETUP', 'MSVCRT.DLL', 'EULA.TXT')     # installer and redistributables
RENAME = {'MONSTER.EX_': 'Monster.exe'}


def target(p):
    """Where disc path p goes under game/, or None."""
    up = p.upper()
    for pre in (LANG, 'SHARED/'):
        if up.startswith(pre):
            rel = p[len(pre):]
            if rel.upper().startswith(SKIP):
                return None
            if pre == 'SHARED/' and rel.upper().startswith('VIDEOS/'):
                return 'Shared/' + rel          # opened as Shared\Videos\MSLogo.smk
            return RENAME.get(rel.upper(), rel)
    return None


def is_pe(path):
    try:
        with open(path, 'rb') as f:
            head = f.read(64)
            if head[:2] != b'MZ':
                return False
            f.seek(struct.unpack_from('<I', head, 0x3C)[0])
            return f.read(4) == b'PE\0\0'
    except (OSError, struct.error):
        return False


def installed_folder(src, out, exe):
    """An installed copy (the demo): copied as it is, its exe as Monster.exe."""
    skip = ('UNINST', 'ST5UNST', 'ISUNINST', 'DEISL1')    # uninstallers and their logs
    n = 0
    for d, _, fs in os.walk(src):
        for f in fs:
            if f.upper().startswith(skip):
                continue
            dst = os.path.join(out, os.path.relpath(os.path.join(d, f), src))
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copyfile(os.path.join(d, f), dst)
            n += 1
    names = {f.upper(): f for f in os.listdir(out)}
    want = (exe or 'Monster.exe').upper()
    if want not in names:
        exes = sorted(f for f in os.listdir(out) if f.upper().endswith('.EXE') and is_pe(os.path.join(out, f)))
        sys.exit('no %s in %s. Its programs: %s\nName the game with --exe NAME.'
                 % (exe or 'Monster.exe', src, ', '.join(exes) or 'none'))
    if names[want] != 'Monster.exe':
        os.replace(os.path.join(out, names[want]), os.path.join(out, 'Monster.exe'))
    return n


def main():
    ap = argparse.ArgumentParser(description='Copy your Monster Truck Madness 2 files into game/ (or game-demo/).')
    ap.add_argument('source', help='the ISO, the disc, or (--demo) the folder the demo installed to')
    ap.add_argument('--demo', action='store_true', help='the 1998 demo, into game-demo/')
    ap.add_argument('--exe', help="the game's exe in an installed folder, if not Monster.exe")
    a = ap.parse_args()
    src, out = a.source, variant.paths(a.demo)['game']
    if os.path.isdir(src):
        files = [(os.path.relpath(os.path.join(d, f), src).replace('\\', '/'), os.path.join(d, f))
                 for d, _, fs in os.walk(src) for f in fs]
        reader = None
        if not any(p.upper().startswith(LANG) for p, _ in files):
            if not a.demo:
                sys.exit('%s has no %s folder: is this the Monster Truck Madness 2 disc?' % (src, LANG))
            print('%d files in %s' % (installed_folder(src, out, a.exe), out))
            return
    else:
        sys.path.insert(0, os.path.join(os.environ.get('PCRECOMP', os.path.join(ROOT, '..', 'pcrecomp')),
                                        'tools', 'assets'))
        import iso_peek
        reader = iso_peek.Reader(src)
        files = [(p, (off, n)) for p, off, n in iso_peek.walk(reader)]
    n = 0
    for p, where in files:
        rel = target(p)
        if rel is None:
            continue
        dst = os.path.join(out, *rel.split('/'))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        if reader is None:
            shutil.copyfile(where, dst)
        else:
            off, size = where
            with open(dst, 'wb') as f:
                f.write(reader.read(off, size))
        n += 1
    if not os.path.exists(os.path.join(out, 'Monster.exe')):
        sys.exit('no %sMONSTER.EX_ in %s: is this the Monster Truck Madness 2 disc?' % (LANG, src))
    print('%d files in %s' % (n, out))


if __name__ == '__main__':
    main()
