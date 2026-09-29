// A3 Motion -- action scripts
//
// An action is a script. Each channel has six action buttons, A1 to A6 --
// six pads on the panel, six fields on the ACTION page -- and each carries
// one script. Pressing the button runs it over the channel's clip for as long
// as the accent lasts, then the clip comes back to itself. This file is a
// script too: every line of it is a comment, so firing it changes nothing.
// It is here to be read in the editor in FILES > ACTIONS.
//
// The shipped actions are named by what they do:
//   Move  Lift  Width  Speed  Dub  -- movement only; all three ceilings at 0,
//                                      so they never touch 3d, freq or Q
//   FX                             -- the only ones that change the sound
//   Cue                            -- ~clip = "Name"; puts that clip on the
//                                      channel and starts it on the downbeat
// Every script carries a "Mood:" line: "more" moves the room towards energy
// and openness, "less" towards calm and weight. The shipped sets put a gentle
// more/less pair on A1/A2, a strong one on A3/A4, FX on A5 and the Cue into
// the next phase of the night on A6.
//
//
// ---- The shape of it ------------------------------------------------------
//
// One assignment per line, SuperCollider's environment variables:
//
//     ~spin = 4;
//     ~reach = 0.8;
//
// Anybody who has typed ~freq = 400; into a SuperCollider window already knows
// how to write one of these. Blank lines and // comments are fine anywhere.
//
// A name you do not write is a name you do not change. The script starts from
// whatever the clip is set to now, so an action that says only
//
//     ~spin = -6;
//
// spins the clip and leaves everything else exactly as the hand left it. That
// is the difference between an action and a preset.
//
// A commented-out line is a name you do not write, which is why every script
// here carries every parameter with its range beside it and comments out the
// ones it does not touch. The script is then its own reference: what a name
// may be is on the line, not in this file. Uncomment a line to take it over,
// comment it again to hand it back.
//
//
// ---- What you can write on the right --------------------------------------
//
//     ~reach = 0.8;                  a number
//     ~spin = -6;                    negative is a direction, not an error
//     ~reach = 0.4 + 0.1;            + - * / , and * binds tighter than +
//     ~reach = (0.4 + 0.2) * 0.5;    brackets say otherwise
//     ~end = \bounce;                a symbol, out of that name's own list
//     ~flat = true;                  true or false
//     ~reach = rrand(0.3, 0.9);      a number between the two
//     ~spin = rrand(2, 6);           whole ends give a whole number
//     ~end = choose([\loop, \bounce, \random]);
//     ~reach = reach + 0.1;          a name on the right reads what it is now
//     ~clipTop = reach * 0.5;        ... including one an earlier line set
//
// The dice are thrown when a script is put on a button, and kept: every press
// of that button lands in the same place, worked out against the clip as it
// is at the press. That is what makes a random action something you can
// rehearse rather than something that happens to you. Put the script on the
// button again to throw again.
//
// A line the reader cannot make sense of is reported under the editor and the
// rest of the script still runs. A value outside its range is brought back to
// the end of it rather than refused.
//
//
// ---- CLIP: the shape ------------------------------------------------------
//
//   ~speedLog2      -7..4    how fast the clip plays, as a power of two.
//                            0 is as recorded, negative is faster: -3 is the
//                            bar's 1/8. The bar offers 0, -3, -4 and -6; the
//                            rest of the range is reachable only from here
//                            and from a clip file.
//
//
// ---- CLIP: the elevation --------------------------------------------------
//
//   ~base            0..1    where the middle of the trajectory sits.
//                            0 is the north pole, 1 the south, 0.5 the
//                            equator. This is the line the graphic draws.
//   ~reach          -1..1    how far the trajectory spreads from that line,
//                            and which way: positive spreads it down towards
//                            the floor, negative up towards the ceiling.
//                            What runs past a pole comes back on the far
//                            side. The swell moves the size, never the sign.
//   ~clipTop         0..1    a hard bound, cutting this much off the north
//                            pole side whatever the reach says.
//   ~clipBottom      0..1    the same from the south pole side.
//   ~flat        true/false  hold one elevation and ignore the reach.
//   ~flatElevation   0..1    the elevation it is held at.
//   ~mirrorSouth true/false  dead. Kept so clips written before ~base still
//                            load; what it meant is ~base = 1.
//
//
// ---- CLIP: the motion -----------------------------------------------------
//
//   ~rotate          0..1    the standing angle, in revolutions. It wraps
//                            rather than stopping: 1.2 is a fifth of a turn
//                            past the top, not "as far as it goes".
//   ~sqzX           -1..1    squeeze or stretch the figure along the room's
//                            front-back axis. 0 is as recorded, -1 half,
//                            +1 double. The screen mirrors the coordinates,
//                            so this is what you see as the vertical.
//   ~sqzY           -1..1    the same along left-right -- the horizontal.
//   ~strX           -8..8    ~sqzX's own sweep: the axis travels out of where
//                            it was set and back, on the same bars-per-cycle
//                            table as spin. Positive pulls it out, negative
//                            presses it flat. What swell is to reach.
//   ~strY           -8..8    the same for ~sqzY.
//   ~tilt           -2..2    the figure's plane leant forward (+) or back,
//                            in quarter turns: 1 stands it on its front
//                            edge, 2 turns it upside down. An angle on a
//                            ring, so it wraps like ~rotate: 3 is -1.
//   ~roll           -2..2    the figure's plane leant to the left (+) or
//                            right, the same way.
//   ~tswp           -8..8    turns ~tilt round, like spin turns ~rotate: the
//                            plane keeps turning over, forward for positive,
//                            back for negative, one whole turn in the
//                            bars-per-revolution of ~spin's table. 0 stands
//                            still at ~tilt.
//   ~rswp           -8..8    the same for ~roll: the plane tumbles sideways.
//   ~spin           -8..8    how fast the whole figure turns under the blob,
//                            as bars per revolution: |1| is 32 bars, then
//                            16, 8, 4, 2, 1, 1/2, and |8| is a quarter bar.
//                            0 stands still. The sign is which way round.
//   ~swell          -8..8    the same clock, sweeping ~reach out of where it
//                            was set and back. Positive opens the coverage
//                            towards the far pole, negative closes it.
//   ~sway           -8..8    the same, sweeping ~base towards the pole the
//                            sign points at. What swell is to reach.
//   ~fade            0..1    how much of the take the joins over its gaps
//                            take over. 0 plays every jump as a jump; 1
//                            gives half of each end to the join beside it,
//                            so the movement glides and only pauses at each
//                            hole. The crossing is walked at one speed --
//                            the wider the gap, the more of the ends it
//                            spends getting across.
//   ~bias           -4..4    where a drawn-through gap leads. 0 is the next
//                            point in time, negative the nearest other run,
//                            positive a random one; the magnitude mixes.
//   ~dir                     \forward  \reverse
//   ~end                     \loop  \stop  \pause  \bounce  \random
//                            what happens when the pass runs out. \stop goes
//                            back to the beginning, \pause stands still where
//                            it is.
//
//
// ---- ACTION: the accent ---------------------------------------------------
//
// These nine and ~act are how the button plays the action -- its feel. The
// ACTION page writes them here (2026-09-29): a knob or the mode key changes
// this file's line, in place, for every button that holds the script. What
// the page shows is what the file says. A line left commented out takes the
// clip's value at the moment the script is assigned.
//
//   ~attack          0..6    how long the accent takes to rise while ACT is
//                            held: 1/16 of a bar, 1/8, 1/4, 1/2, 1, 2, 4.
//   ~decay           0..6    how long it falls once ACT is let go.
//   ~envelopeMax     0..1    the 3d it rises to. The channel's own 3d is the
//                            floor and this is the ceiling, so an accent
//                            never pushes a value down.
//   ~freqAttack      0..6    the same for the filter's cutoff,
//   ~freqDecay       0..6
//   ~freqMax         0..1    0 being off.
//   ~qAttack         0..6    and for its resonance.
//   ~qDecay          0..6
//   ~qMax            0..1
//   ~act                     \oneshot  \hold
//                            whether ACT fires and lets go, or holds the clip
//                            for as long as the finger is down.
//   ~then            1..6    the button of the same channel that fires when
//                            this action's accent is over; commented out,
//                            nothing follows. ACTION's "then" key writes it.
//
//
// ---- Writing one ----------------------------------------------------------
//
// Dial a clip until it sounds the way the button should make it sound, then
// FROM CLIP in FILES > ACTIONS. That writes every name above with the clip's value, which
// is a good place to start reading from -- and a bad action to keep, because
// an action that names everything overwrites everything. Comment out the
// lines you did not mean and keep the two or three you did; the commented
// ones stay as the list of what else you could reach for.
