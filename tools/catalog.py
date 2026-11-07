#!/usr/bin/env python3
"""Function catalog for Monster.exe (MTM2): pcrecomp disasm32.

    py -3 tools/catalog.py            # -> work/functions_monster.json
    py -3 tools/catalog.py --demo     # the demo: game-demo/ -> work/demo/ (tools/variant.py)

The entry point is the seed; the rest is recursive descent, prologues and a
data scan, plus work/seeds.json from earlier runs:

    py -3 ../pcrecomp/tools/disasm/seed_from_log.py scratch/run.log game/Monster.exe --seeds work/seeds.json

The catalog is derived from the game binary, so it lives in work/
and is never committed.
"""
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import variant  # noqa: E402

_HERE = variant.ROOT
TOOLS = os.path.join(os.environ.get('PCRECOMP', os.path.join(_HERE, '..', 'pcrecomp')), 'tools')
DEMO = variant.take_demo_flag(sys.argv)
V = variant.paths(DEMO)
if not os.path.exists(os.path.join(V['game'], 'Monster.exe')):
    sys.exit('no Monster.exe in %s -- run tools/gamefiles.py%s first' % (V['game'], ' --demo' if DEMO else ''))

os.makedirs(V['work'], exist_ok=True)     # gitignored, so absent in a fresh copy
# Addresses earlier runs found (indirect calls into code the scans missed),
# kept by pcrecomp seed_from_log.py; also derived from the binary, so in work/.
SEEDS = V['seeds']
subprocess.run([sys.executable, os.path.join(TOOLS, 'disasm', 'disasm32.py'),
                os.path.join(V['game'], 'Monster.exe'), '-o', V['catalog']]
               + (['--seed-functions', SEEDS] if os.path.exists(SEEDS) else []), check=True)
