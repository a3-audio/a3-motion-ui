#!/usr/bin/env python3
"""Library v2 clips: five for each phase of a night, and Default.

Regenerate with:  python3 tools/library-v2/clips.py

The catalogue is the spec's (.claude/notes/a3-motion-library-v2.md in the
workspace). Values:
  speed          speedLog2 -- 0 as drawn, -1 twice as fast, +1 half as fast
  spin/sway/swell/strX/strY  the bars table: |1| = 32 bars ... |8| = 1/4 bar,
                 bigger is faster, 0 is still, the sign is the direction
  elevationBase  0 overhead, 0.5 ear height, 1 the floor
  reach          spread; negative spreads upward
The envelope fields are inert since the feel moved to the button: they are
Default.json's.
"""
import glob
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
DIR = os.path.join(HERE, '..', '..', 'pattern', 'clips', 'system')
BASE = json.load(open(os.path.join(DIR, 'Default.json')))

# name: (shape, values that differ from Default)
CLIPS = {
    # Warmup -- slow, high, open
    'Warmup Halo':       ('Orbit Circle',      dict(spin=1, elevationBase=0.1, reach=0.35, swell=2)),
    'Warmup Breath':     ('Spiral Breath',     dict(speed=1, elevationBase=0.05, reach=0.5)),
    'Warmup Sunrise':    ('Flower Rose 7',     dict(spin=2, elevationBase=0.15, reach=0.55, sway=-2)),
    'Warmup Horizon':    ('Loop Infinity',     dict(spin=1, elevationBase=0.5, reach=0.3, clipTop=0.3, clipBottom=0.3)),
    'Warmup Drift':      ('Wander Drift',      dict(spin=1, elevationBase=0.25, reach=0.5)),
    # Groove -- on the grid, ear height
    'Groove Four Floor': ('Rhythm Four Floor', dict(elevationBase=0.5, reach=0.6, rotate=0.125)),
    'Groove Tresillo':   ('Rhythm Tresillo',   dict(elevationBase=0.5, reach=0.7, spin=2)),
    'Groove Clave':      ('Rhythm Clave 3-2',  dict(elevationBase=0.45, reach=0.7)),
    'Groove Offbeat':    ('Rhythm Offbeat',    dict(elevationBase=0.5, reach=0.8)),
    'Groove Shuffle':    ('Rhythm Shuffle',    dict(elevationBase=0.5, reach=0.6, spin=3)),
    # Build -- opening, speeding up, rising
    'Build Riser':       ('Spiral Riser',      dict(spin=3, elevationBase=0.3, reach=0.9, sway=-3)),
    'Build Pulse':       ('Orbit Pulse',       dict(spin=4, elevationBase=0.35, reach=0.7, swell=4)),
    'Build Loop':        ('Loop 3-4',          dict(speed=-1, spin=3, elevationBase=0.4, reach=0.75)),
    'Build Gallop':      ('Rhythm Gallop',     dict(spin=4, elevationBase=0.4, reach=0.7)),
    'Build Vortex':      ('Spiral Vortex',     dict(spin=5, elevationBase=0.35, reach=1.0)),
    # Peak -- wide, fast, lifted
    'Peak Anthem':       ('Flower Rose 5',     dict(spin=5, elevationBase=0.1, reach=0.85, swell=3)),
    'Peak Festival':     ('Loop 2-3',          dict(spin=6, elevationBase=0.2, reach=0.85, sway=4)),
    'Peak Carousel':     ('Cycle Epi 7-3',     dict(spin=5, elevationBase=0.2, reach=0.6)),
    'Peak Star':         ('Edge Star',         dict(spin=4, elevationBase=0.15, reach=0.85, swell=3)),
    'Peak Euphoria':     ('Flower Trefoil',    dict(spin=4, elevationBase=0.15, reach=0.8, swell=4)),
    # Drop -- hard, angular, close
    'Drop Impact':       ('Edge Astroid',      dict(spin=-5, elevationBase=0.5, reach=1.0)),
    'Drop Warehouse':    ('Edge Square',       dict(speed=-1, spin=-4, endAction='bounce', elevationBase=0.55, reach=0.9)),
    'Drop Strobe':       ('Edge Corner',       dict(speed=-3, elevationBase=0.5, reach=0.6)),
    'Drop Whirlwind':    ('Cycle Epi 3-1',     dict(spin=8, elevationBase=0.3, reach=0.85)),
    'Drop Ping Pong':    ('Rhythm Ping Pong',  dict(elevationBase=0.5, reach=0.9)),
    # Break -- still, suspended, drawn in
    'Break Standstill':  ('Orbit Arc',         dict(elevationBase=0.5, reach=0.4)),
    'Break Collapse':    ('Spiral Collapse',   dict(spin=1, elevationBase=0.4, reach=0.6)),
    'Break Monolith':    ('Edge Diamond',      dict(speed=1, elevationBase=0.6, reach=0.2)),
    'Break Suspend':     ('Loop Figure 8',     dict(speed=1, elevationBase=0.1, reach=0.45)),
    'Break Heartbeat':   ('Flower Heart',      dict(speed=1, elevationBase=0.3, reach=0.4, swell=-2)),
    # Dub -- throws, swings, near and far
    'Dub Echo':          ('Rhythm Echo',       dict(spin=1, elevationBase=0.6, reach=0.9)),
    'Dub Pendulum':      ('Orbit Pendulum',    dict(sway=3, elevationBase=0.55, reach=0.9)),
    'Dub Tunnel':        ('Spiral Helix',      dict(spin=2, elevationBase=0.75, reach=0.35)),
    'Dub Kepler':        ('Orbit Kepler',      dict(spin=2, elevationBase=0.5, reach=0.8)),
    'Dub Skank':         ('Rhythm Offbeat',    dict(elevationBase=0.6, reach=0.5, fadeReach=0.4)),
    # Deep -- low, slow, dark
    'Deep Undertow':     ('Spiral Collapse',   dict(spin=-1, elevationBase=0.85, reach=0.4)),
    'Deep Sub':          ('Orbit Circle',      dict(spin=-2, elevationBase=1.0, reach=0.25, swell=-2)),
    'Deep Fog':          ('Wander Drift',      dict(spin=1, elevationBase=0.75, reach=0.5, fadeReach=0.6)),
    'Deep Lurk':         ('Wander Wave',       dict(speed=2, elevationBase=0.8, reach=0.3, sway=-2)),
    'Deep Cellar':       ('Cycle Hypo 5-3',    dict(spin=-2, elevationBase=0.85, reach=0.25, strX=2, strY=-2)),
    # Float -- overhead, slow, smooth
    'Float Aurora':      ('Loop 1-2',          dict(spin=1, elevationBase=0.05, reach=0.5, swell=2)),
    'Float Canopy':      ('Cycle Hypo 7-2',    dict(spin=1, elevationBase=0.0, reach=0.7)),
    'Float Blossom':     ('Flower Petal',      dict(spin=2, elevationBase=0.1, reach=0.7, swell=3)),
    'Float Lullaby':     ('Loop Figure 8',     dict(speed=2, elevationBase=0.2, reach=0.4)),
    'Float Cloud':       ('Flower Clover',     dict(speed=1, spin=1, elevationBase=0.1, reach=0.55)),
    # Closing -- descending, slowing, settling
    'Closing Sunset':    ('Flower Rose 4',     dict(spin=1, elevationBase=0.45, reach=0.5, sway=2)),
    'Closing Farewell':  ('Spiral Collapse',   dict(speed=1, elevationBase=0.5, reach=0.5)),
    'Closing Tide':      ('Wander Wave',       dict(spin=1, elevationBase=0.6, reach=0.35, swell=4)),
    'Closing Ember':     ('Orbit Ellipse',     dict(speed=1, spin=1, elevationBase=0.7, reach=0.3)),
    'Closing Still':     ('Orbit Circle',      dict(elevationBase=0.5, reach=0.15)),
}
assert len(CLIPS) == 50


def main():
    for path in glob.glob(os.path.join(DIR, '*.json')):
        if os.path.basename(path) != 'Default.json':
            os.remove(path)
    for name, (shape, values) in CLIPS.items():
        clip = dict(BASE)
        clip['name'] = name
        clip['svg'] = shape
        for key, value in values.items():
            assert key in BASE, key
            clip[key] = value
        with open(os.path.join(DIR, name + '.json'), 'w') as f:
            f.write(json.dumps(clip, indent=2, sort_keys=True) + '\n')
    print('50 clips written')


if __name__ == '__main__':
    main()
