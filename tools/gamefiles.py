#!/usr/bin/env python3
r"""Put your copy of Monster Truck Madness 2's files in game/, from the disc.

    py -3 tools/gamefiles.py "Monster Truck Madness 2.iso"   # a 2048-byte ISO
    py -3 tools/gamefiles.py E:\                             # the CD in a drive, or a copy of it

The 1998 demo goes in game-demo/ instead (tools/variant.py):

    py -3 tools/gamefiles.py --demo "C:\Program Files\Microsoft Games\Monster Truck Madness 2 Demo"
    py -3 tools/gamefiles.py --demo --exe MTM2Demo.exe <folder>   # if its exe has another name

The demo's installer (MtM2trial.EXE, 18 MB) can be given as it is: it is a
cabinet behind a small setup program, and the files come out of the
cabinet with cabextract or 7-Zip if either is installed, else Windows' own
expand.exe; the installer is never run. They are laid out as its setup
did: Monsterx.exe as Monster.exe, Monsterx.ini as system\monsterx.ini.

    py -3 tools/gamefiles.py --demo MtM2trial.EXE

A folder the demo was installed to works too (its exe copied as
Monster.exe; --exe names it if it is not Monster.exe or Monsterx.exe), and
a demo disc laid out like the full game's (ENGLISH\, SHARED\) is read like
the full game.

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


DEMO_SKIP = ('SETUP.EXE', 'SETUPENU.DLL', 'DSETUP.DLL', 'DSETUP16.DLL', 'DSETUP32.DLL')


def demo_installer(src, out):
    """The demo's self-extracting installer: its cabinet, unpacked, laid out."""
    import subprocess
    import tempfile
    data = open(src, 'rb').read()
    at = data.find(b'MSCF\0\0\0\0')
    if at < 0:
        sys.exit('%s has no cabinet in it: is this MtM2trial.EXE?' % src)
    size = struct.unpack_from('<I', data, at + 8)[0]           # cbCabinet
    with tempfile.TemporaryDirectory() as tmp:
        cab = os.path.join(tmp, 'demo.cab')
        with open(cab, 'wb') as f:
            f.write(data[at:at + size])
        files = os.path.join(tmp, 'files')
        os.makedirs(files)
        tries = [['cabextract', '-q', '-d', files, cab], ['7z', 'x', '-y', '-o' + files, cab],
                 ['7za', 'x', '-y', '-o' + files, cab], ['expand', '-F:*', cab, files]]
        for cmd in tries:
            if shutil.which(cmd[0]) and subprocess.run(cmd, stdout=subprocess.DEVNULL).returncode == 0 \
                    and os.listdir(files):
                break
        else:
            sys.exit('cannot unpack the cabinet: install cabextract or 7-Zip')
        n = 0
        os.makedirs(os.path.join(out, 'system'), exist_ok=True)
        for f in os.listdir(files):
            up = f.upper()
            if up in DEMO_SKIP:
                continue
            dst = {'MONSTERX.EXE': 'Monster.exe', 'MONSTERX.INI': os.path.join('system', 'monsterx.ini')}.get(up, f)
            shutil.copyfile(os.path.join(files, f), os.path.join(out, dst))
            n += 1
    if not os.path.exists(os.path.join(out, 'Monster.exe')):
        sys.exit('no Monsterx.exe in the cabinet: is this the Monster Truck Madness 2 trial?')
    return n


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
    if not exe and want not in names and 'MONSTERX.EXE' in names:
        want = 'MONSTERX.EXE'                                   # the trial's name for it
    if want not in names:
        exes = sorted(f for f in os.listdir(out) if f.upper().endswith('.EXE') and is_pe(os.path.join(out, f)))
        sys.exit('no %s in %s. Its programs: %s\nName the game with --exe NAME.'
                 % (exe or 'Monster.exe', src, ', '.join(exes) or 'none'))
    if names[want] != 'Monster.exe':
        os.replace(os.path.join(out, names[want]), os.path.join(out, 'Monster.exe'))
    return n


def main():
    ap = argparse.ArgumentParser(description='Copy your Monster Truck Madness 2 files into game/ (or game-demo/).')
    ap.add_argument('source', help='the ISO, the disc, or (--demo) MtM2trial.EXE or the folder the demo installed to')
    ap.add_argument('--demo', action='store_true', help='the 1998 demo, into game-demo/')
    ap.add_argument('--exe', help="the game's exe in an installed folder, if not Monster.exe")
    a = ap.parse_args()
    src, out = a.source, variant.paths(a.demo)['game']
    if a.demo and os.path.isfile(src) and is_pe(src):
        print('%d files in %s' % (demo_installer(src, out), out))
        return
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
