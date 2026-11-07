"""Where each version of the game lives: the full game and the 1998 demo.

Every step takes --demo to work on the demo instead of the full game, and
keeps it apart, so both can be lifted and built side by side:

             full game                  demo
  files      game/                      game-demo/
  catalog    work/functions_monster.json  work/demo/functions_monster.json
  lift       src/recomp/gen/            src/recomp/gen-demo/

All of these are derived from your own copy and gitignored.
"""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def paths(demo):
    work = os.path.join(ROOT, 'work', 'demo') if demo else os.path.join(ROOT, 'work')
    return {
        'name': 'demo' if demo else 'full game',
        'game': os.path.join(ROOT, 'game-demo' if demo else 'game'),
        'work': work,
        'catalog': os.path.join(work, 'functions_monster.json'),
        'seeds': os.path.join(work, 'seeds.json'),
        'stats': os.path.join(work, 'lift_stats.json'),
        'gen': os.path.join(ROOT, 'src', 'recomp', 'gen-demo' if demo else 'gen'),
    }


def take_demo_flag(argv):
    """Remove --demo from argv; True if it was there."""
    if '--demo' in argv:
        argv.remove('--demo')
        return True
    return False
