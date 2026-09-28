#!/usr/bin/env python3
"""Library v2 actions: fifty, named by what they do.

Regenerate with:  python3 tools/library-v2/actions.py

  Move Lift Width Speed Dub  movement only -- all three ceilings written as 0,
                             so they never touch 3d, freq or Q
  FX                         the only ones that change the sound
  Cue                        ~clip = "Name"; puts that clip on the channel

Every script names every parameter (a test holds them to the annotation
table): the ones an action sets are written, the rest stand commented out.
The line layout comes from template.scd beside this file, taken once from a
shipped script before the old ones went.
"""
import glob
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
DIR = os.path.join(HERE, '..', '..', 'pattern', 'actions', 'system')
TEMPLATE = os.path.join(HERE, 'template.scd')
# The defaults a commented-out line shows, where the template's source script
# had set a value of its own.
DEFAULTS = {'attack': '2', 'decay': '3', 'envelopeMax': '1', 'act': '\\oneshot'}

if not os.path.exists(TEMPLATE):
    src = open(os.path.join(DIR, 'Punch.scd')).read().split('\n')
    start = next(i for i, l in enumerate(src) if l.startswith('// ---- Cue'))
    open(TEMPLATE, 'w').write('\n'.join(src[start:]).rstrip('\n') + '\n')
BODY = open(TEMPLATE).read().split('\n')
LINE = re.compile(r'^(//)?~(\w+) = ([^;]*);\s*(//.*)$')
SILENT = dict(envelopeMax=0, freqMax=0, qMax=0)
H, O = '\\hold', '\\oneshot'


def write(name, title, mood, why, params):
    out = [f'// {name} -- {title}', f'// Mood: {mood}', '//']
    out += ['// ' + l for l in why.split('\n')] + ['']
    seen = set()
    for l in BODY:
        m = LINE.match(l)
        if not m:
            out.append(l)
            continue
        _, key, default, comment = m.groups()
        if key in params:
            code = f'~{key} = {params[key]};'
            seen.add(key)
        else:
            code = f'//~{key} = {DEFAULTS.get(key, default)};'
        out.append(f'{code:<25}{comment}' if len(code) < 25 else code + comment)
    assert seen == set(params), (name, set(params) - seen)
    with open(os.path.join(DIR, name + '.scd'), 'w') as f:
        f.write('\n'.join(out).rstrip('\n') + '\n')


def move(name, title, mood, why, params):
    """Every action but FX and Cue: the three ceilings at 0."""
    write(name, title, mood, why, {**params, **SILENT})


A = []
# -- Move (9) ------------------------------------------------------------------
A += [
    (move, 'Move Spin', 'held, the figure turns once every two bars.', 'more -- a steady turn; groove (Q1).', 'The simplest movement there is, locked to the bar.', dict(spin=5, attack=2, decay=3, act=H)),
    (move, 'Move Spin Fast', 'held, a turn every half bar.', 'more -- rush; peak energy (Q1).', 'Speed carries energy: the fastest turn that still reads as a turn.', dict(spin=7, attack=1, decay=3, act=H)),
    (move, 'Move Unwind', 'held, the turn runs the other way, slowly.', 'less -- release; the turn let go (Q4).', 'The same motion backwards and slower: tension let out.', dict(spin=-3, attack=3, decay=5, act=H)),
    (move, 'Move Reverse', 'held, the figure runs backwards.', 'less -- the same idea from behind.', 'Direction only; nothing faster, nothing wider.', dict(dir='\\reverse', attack=1, decay=3, act=H)),
    (move, 'Move Bounce', 'the figure bounces at its ends from here on.', 'more -- back and forth; playful (Q1).', 'The end action becomes bounce for this pass.', dict(end='\\bounce', attack=0, decay=4, act=O)),
    (move, 'Move Freeze', 'held, everything holds still at one height.', 'less -- time stops; suspended (Q3/Q4).', 'No spin, no swell, no sway, the height pinned: only the recorded\nfigure itself still moves.', dict(spin=0, swell=0, sway=0, flat='true', flatElevation=0.5, attack=2, decay=3, act=H)),
    (move, 'Move Tilt', 'held, the figure leans forward and rocks.', 'more -- the room tips towards you (Q1/Q2).', 'Tilt leans the plane; its sweep moves it on the bar.', dict(tilt=0.6, tswp=4, attack=3, decay=3, act=H)),
    (move, 'Move Rock', 'held, the figure swings up and down the room.', 'more -- a swing; groove (Q1).', 'The base sweeps up and down on the bar.', dict(base=0.5, reach=0.35, sway=3, attack=3, decay=4, act=H)),
    (move, 'Move Mirror', 'held, the figure turned half round and run backwards.', 'less -- a reflective turn.', 'Half a turn and the direction reversed: the same idea from the\nother side of the room.', dict(rotate='~rotate + 0.5', dir='\\reverse', attack=2, decay=3, act=H)),
]
# -- Lift (6) ------------------------------------------------------------------
A += [
    (move, 'Lift Up', 'held, the figure rises a third of the way to the ceiling.', 'more -- lifts gently; bright (Q4 -> Q1).', 'Height reads as lift.', dict(base='~base * 0.66', attack=3, decay=3, act=H)),
    (move, 'Lift Overhead', 'the figure snaps to the cap above the listener.', 'more -- up and bright (Q1).', 'The base at the north pole, a small reach.', dict(base=0, reach=0.3, attack=1, decay=3, act=O)),
    (move, 'Lift Ear Level', 'held, a band at ear height.', 'less -- grounded, steady (Q4).', 'Base at the equator, top and bottom cut: any clip becomes a ring.', dict(base=0.5, reach=0.55, clipTop=0.35, clipBottom=0.35, attack=2, decay=3, act=H)),
    (move, 'Lift Down', 'held, the figure sinks below ear height.', 'less -- weight; darker (Q3).', 'Low reads as heavy.', dict(base=0.75, attack=3, decay=4, act=H)),
    (move, 'Lift Floor', 'held, the sound goes under the floor.', 'less -- heavy, dark (Q3).', 'The base at the far pole.', dict(base=1, reach=0.45, attack=4, decay=4, act=H)),
    (move, 'Lift Sway', 'held, the height sways on the bar.', 'more -- a slow swell of height (Q4 -> Q1).', 'The base sweeps between its place and the ceiling.', dict(sway=-4, attack=3, decay=4, act=H)),
]
# -- Width (6) -----------------------------------------------------------------
A += [
    (move, 'Width Open', 'held, the figure spreads half again as wide.', 'more -- opens the room (Q1).', 'Width is the first thing a build-up automates.', dict(reach='~reach * 1.5', attack=3, decay=3, act=H)),
    (move, 'Width Full', 'the figure thrown to the whole sphere.', 'more -- everything, everywhere (Q1/Q2).', 'Reach at one: the whole room.', dict(reach=1, attack=0, decay=3, act=O)),
    (move, 'Width Close', 'held, the figure halves its spread.', 'less -- focused, intimate (Q3/Q4).', 'Open read the other way.', dict(reach='~reach * 0.5', attack=3, decay=3, act=H)),
    (move, 'Width Point', 'held, everything pulls in to one point.', 'less -- the sound comes close; tension (Q2).', 'A sound pulled in to a point reads as near.', dict(reach=0.08, spin=0, attack=3, decay=2, act=H)),
    (move, 'Width Breathe', 'held, the spread opens and closes slowly.', 'less -- a calm breath (Q4).', 'An eight-bar swell from a middle reach.', dict(reach=0.6, swell=3, attack=4, decay=5, act=H)),
    (move, 'Width Squash', 'held, pressed flat, springs back when you let go.', 'less -- pressure (Q2/Q3).', 'Squeezed front to back, stretched left to right.', dict(sqzX=-0.7, sqzY=0.5, attack=2, decay=3, act=H)),
]
# -- Speed (5) -----------------------------------------------------------------
A += [
    (move, 'Speed Double', 'held, the figure plays twice as fast.', 'more -- energy up, same figure (Q1/Q2).', 'One step on the power-of-two table.', dict(speedLog2='~speedLog2 - 1', attack=2, decay=2, act=H)),
    (move, 'Speed Half', 'held, the figure plays half as fast.', 'less -- energy down, same figure (Q4/Q3).', 'One step slower.', dict(speedLog2='~speedLog2 + 1', attack=3, decay=3, act=H)),
    (move, 'Speed Stutter', 'held, the figure chatters at a sixteenth.', 'more -- nervous; a stutter edit in space (Q2).', 'Past a certain speed movement becomes texture.', dict(speedLog2=-3, spin=8, attack=0, decay=1, act=H)),
    (move, 'Speed Tape Stop', 'the figure winds down and stands still.', 'less -- the motor stops; the end of a phrase (Q3).', 'The slowest speed, no spin, a pause at the end of the pass.', dict(speedLog2=4, spin=0, end='\\pause', attack=0, decay=4, act=O)),
    (move, 'Speed Halt', 'everything that moves on its own stops, and the pass ends.', 'less -- the reset.', 'Spin and swell off, a pause at the end.', dict(spin=0, swell=0, end='\\pause', attack=0, decay=1, act=O)),
]
# -- Dub (6) -------------------------------------------------------------------
A += [
    (move, 'Dub Echo Throw', 'the dub throw: reversed, bouncing, gliding out.', 'less -- a repeat that trails away (Q3).', 'A sound thrown into the delay, the repeats falling away -- in\nspace, not in sound.', dict(dir='\\reverse', end='\\bounce', fade=0.8, attack=0, decay=6, act=O)),
    (move, 'Dub Bounce Back', 'a hard swing out and back.', 'more -- a throw that returns (Q2).', 'Full reach, a fast reverse spin, bounce at the ends.', dict(reach=1, spin=-6, end='\\bounce', attack=0, decay=4, act=O)),
    (move, 'Dub Reverse Tape', 'held, backwards and slower, like a tape turned over.', 'less -- the reverse tape trick (Q3).', 'Direction reversed and one step slower.', dict(dir='\\reverse', speedLog2='~speedLog2 + 1', attack=1, decay=3, act=H)),
    (move, 'Dub Spring', 'a spring-reverb shake: a fast swell, short.', 'more -- a splash (Q2).', 'The reach sweeps at its fastest for a moment.', dict(swell=7, attack=0, decay=2, act=O)),
    (move, 'Dub Scatter', 'somewhere else every time it is put on a button.', 'more -- a surprise; playful.', 'The dice are thrown when it is assigned; every press lands in the\nsame place.', dict(reach='rrand(0.3, 1.0)', rotate='rrand(0.0, 1.0)', spin='rrand(-8, 8)', end='\\random', attack=1, decay=4, act=O)),
    (move, 'Dub Stitch', 'held, the gaps in a take glide shut.', 'less -- smooth, calm (Q4).', 'The joins over the gaps take over, and lead elsewhere.', dict(fade=0.75, bias=-3, attack=3, decay=4, act=H)),
]
# -- FX (6): the only actions that change the sound ------------------------------
A += [
    (write, 'FX Punch', 'depth hits, nothing moves.', 'more -- weight on the one; driving (Q2).', 'The 3d swells under the trajectory. Changes the sound.', dict(attack=1, decay=3, envelopeMax=1, freqMax=0, qMax=0, act=O)),
    (write, 'FX Sweep', 'held, the filter opens, nothing moves.', 'more -- the dub woosh; tension that lifts.', 'The cutoff rises over two bars. Changes the sound.', dict(attack=4, decay=4, envelopeMax=0.3, freqAttack=5, freqDecay=6, freqMax=1, qAttack=4, qDecay=5, qMax=0.5, act=H)),
    (write, 'FX Resonate', 'held, the resonance creeps up.', 'more -- slow tension (Q3 -> Q2).', 'Resonance towards the edge, slowly. Changes the sound.', dict(attack=6, decay=6, envelopeMax=0.4, freqAttack=3, freqDecay=4, freqMax=0.6, qAttack=4, qDecay=1, qMax=0.95, act=H)),
    (write, 'FX Riser', 'held, four bars of build: spread, spin, filter.', 'more -- the build before the drop (Q2 -> Q1).', 'The room opens and the filter with it. Changes the sound.', dict(reach=1, swell=4, spin='~spin + 2', attack=6, decay=2, envelopeMax=1, freqAttack=6, freqDecay=2, freqMax=1, qAttack=6, qDecay=1, qMax=0.6, act=H)),
    (write, 'FX Impact', 'the drop: everything at once, then a long fall.', 'more -- the release; impact (Q2 -> Q1).', 'No attack and the longest decay. Changes the sound.', dict(base=0.5, reach=1, attack=0, decay=5, envelopeMax=1, freqAttack=0, freqDecay=4, freqMax=1, qMax=0, act=O)),
    (write, 'FX Swell', 'held, depth and filter rise together, gently.', 'more -- a warm lift (Q4 -> Q1).', 'A slow rise of 3d with the cutoff. Changes the sound.', dict(attack=5, decay=5, envelopeMax=0.7, freqAttack=5, freqDecay=5, freqMax=0.6, qMax=0, act=H)),
]
# -- Cue (12) --------------------------------------------------------------------
CUES = ['Groove Four Floor', 'Build Riser', 'Peak Anthem', 'Drop Impact',
        'Break Standstill', 'Build Pulse', 'Deep Undertow', 'Float Aurora',
        'Closing Sunset', 'Warmup Halo', 'Dub Echo', 'Closing Still']
for clip in CUES:
    # Silent as well: a Cue fires no accent, and if its clip is ever gone the
    # button must not turn into one that changes the sound.
    A.append((move, 'Cue ' + clip,
              f'puts {clip} on the channel, from the next downbeat.',
              'a change of clip -- the next phase of the night.',
              'The clip goes onto the channel and starts on the downbeat (Shift:\nat once), and stays.',
              {'clip': f'"{clip}"'}))
assert len(A) == 50, len(A)


def main():
    for path in glob.glob(os.path.join(DIR, '*.scd')):
        if os.path.basename(path) != 'README.scd':
            os.remove(path)
    for fn, name, title, mood, why, params in A:
        fn(name, title, mood, why, params)
    print('50 actions written')


if __name__ == '__main__':
    main()
