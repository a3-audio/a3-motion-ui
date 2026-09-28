#!/usr/bin/env python3
"""Library v2 sets: one for each phase of a night.

Regenerate with:  python3 tools/library-v2/sets.py   (after clips.py and actions.py)

The six buttons are laid out the same way in every set, so the hands learn one
panel: A1/A2 a gentle more/less, A3/A4 a strong more/less, A5 the FX (the only
button that changes the sound), A6 the Cue into the next phase of the night.
3d rises with the phase's energy, Q with its tension, freq stays at 0.45.
"""
import glob
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..', '..', 'pattern')
DIR = os.path.join(ROOT, 'sessions', 'system')

SETS = [  # name, its four clips (without the prefix), six actions, 3d, q
    ('Warmup',  ['Halo', 'Breath', 'Sunrise', 'Horizon'],
     ['Lift Up', 'Width Close', 'Move Spin', 'Move Freeze', 'FX Swell', 'Cue Groove Four Floor'], 0.30, 0.10),
    ('Groove',  ['Four Floor', 'Tresillo', 'Clave', 'Offbeat'],
     ['Move Spin', 'Speed Half', 'Speed Double', 'Move Freeze', 'FX Punch', 'Cue Build Riser'], 0.40, 0.10),
    ('Build',   ['Riser', 'Pulse', 'Loop', 'Gallop'],
     ['Width Open', 'Width Close', 'Speed Double', 'Speed Half', 'FX Riser', 'Cue Peak Anthem'], 0.45, 0.30),
    ('Peak',    ['Anthem', 'Festival', 'Carousel', 'Star'],
     ['Width Full', 'Lift Ear Level', 'Move Spin Fast', 'Move Unwind', 'FX Impact', 'Cue Drop Impact'], 0.55, 0.10),
    ('Drop',    ['Impact', 'Warehouse', 'Strobe', 'Whirlwind'],
     ['Speed Stutter', 'Speed Halt', 'Width Full', 'Width Point', 'FX Punch', 'Cue Break Standstill'], 0.55, 0.30),
    ('Break',   ['Standstill', 'Collapse', 'Monolith', 'Suspend'],
     ['Lift Overhead', 'Move Freeze', 'Width Breathe', 'Width Point', 'FX Sweep', 'Cue Build Pulse'], 0.30, 0.10),
    ('Dub',     ['Echo', 'Pendulum', 'Tunnel', 'Kepler'],
     ['Dub Spring', 'Dub Stitch', 'Dub Bounce Back', 'Dub Echo Throw', 'FX Sweep', 'Cue Deep Undertow'], 0.40, 0.20),
    ('Deep',    ['Undertow', 'Sub', 'Fog', 'Lurk'],
     ['Lift Sway', 'Lift Floor', 'Move Rock', 'Lift Down', 'FX Resonate', 'Cue Float Aurora'], 0.35, 0.25),
    ('Float',   ['Aurora', 'Canopy', 'Blossom', 'Lullaby'],
     ['Lift Overhead', 'Width Close', 'Width Breathe', 'Move Unwind', 'FX Swell', 'Cue Closing Sunset'], 0.30, 0.10),
    ('Closing', ['Sunset', 'Farewell', 'Tide', 'Ember'],
     ['Lift Up', 'Lift Down', 'Width Open', 'Speed Tape Stop', 'FX Swell', 'Cue Warmup Halo'], 0.25, 0.10),
]
SPREAD = [-0.04, 0.0, 0.02, 0.04]


def main():
    clips = {os.path.basename(p)[:-5]: json.load(open(p)).get('svg')
             for p in glob.glob(os.path.join(ROOT, 'clips', 'system', '*.json'))}
    actions = {os.path.basename(p)[:-4]
               for p in glob.glob(os.path.join(ROOT, 'actions', 'system', '*.scd'))}
    for path in glob.glob(os.path.join(DIR, '*.json')):
        os.remove(path)
    for name, names, acts, threeD, q in SETS:
        for a in acts:
            assert a in actions, a
        channels = []
        for i, c in enumerate(names):
            clip = f'{name} {c}'
            assert clip in clips, clip
            channels.append({'threeD': round(threeD + SPREAD[i], 2), 'freq': 0.45, 'q': q,
                             'slots': [{'pattern': clips[clip], 'clip': clip,
                                        'recordLengthLog2': 3}],
                             'actions': [{'script': a} for a in acts]})
        with open(os.path.join(DIR, name + '.json'), 'w') as f:
            json.dump({'name': name, 'channels': channels}, f, indent=2)
            f.write('\n')
    print('10 sets written')


if __name__ == '__main__':
    main()
