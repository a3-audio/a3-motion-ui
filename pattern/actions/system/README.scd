// A3 Motion -- action scripts
//
// An action is a script. ACT on a pad runs it, and what it leaves behind is
// what the clip in that slot is set to. This file is a script too: every line
// of it is a comment, so firing it changes nothing. It is here to be read in
// the editor on the ACTION page, where the scripts are.
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
// The randomness is seeded per firing, so the same clip fired again with the
// same seed throws the same numbers. That is what makes a random action
// something you can rehearse rather than something that happens to you.
//
// A line the reader cannot make sense of is reported under the editor and the
// rest of the script still runs. A value outside its range is brought back to
// the end of it rather than refused.
//
//
// ---- CLIP: the shape ------------------------------------------------------
//
//   ~speedLog2      -8..8    how fast the clip plays, as a power of two.
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
// These nine and ~act belong to the slot, not to the action. A fired action
// leaves them alone whatever it says, because the ACTION page is showing them
// and a page that showed an attack the action had quietly replaced would be a
// page that lies. Write them in a clip file or dial them on the page; writing
// them here is harmless and does nothing.
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
//
//
// ---- Writing one ----------------------------------------------------------
//
// Dial a clip until it sounds the way ACT should make it sound, then Save
// Action in FILES. That writes every name above with the clip's value, which
// is a good place to start reading from -- and a bad action to keep, because
// an action that names everything overwrites everything. Delete the lines you
// did not mean and keep the two or three you did.
