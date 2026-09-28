// Echo Throw -- the dub throw: reversed, bouncing, trailing off.
// Mood: less -- a repeat that trails away; dub (Q3).
//
// A dub engineer throws a sound into the delay and lets the repeats fall
// away. Here the figure turns round, bounces at its ends, glides through its
// gaps, and the filter decays with it over four bars.

// ---- Shape -----------------------------------------------------------
//~speedLog2 = 0;        // -7..4   how fast, as a power of two; -3 is the 1/8

// ---- Elevation -------------------------------------------------------
//~base = 0;             // 0..1    where the middle sits; 0 north, 1 south
//~reach = 0.5;          // -1..1   how far it spreads, sign says down or up
//~clipTop = 0;          // 0..1    cut this much off the north side
//~clipBottom = 0;       // 0..1    the same from the south side
//~flat = false;         // bool    hold one elevation, ignore the reach
//~flatElevation = 0.5;  // 0..1    the elevation it is held at

// ---- Motion ----------------------------------------------------------
//~rotate = 0;           // 0..1    standing angle in revolutions; it wraps
//~sqzX = 0;             // -1..1   squeeze front-back; 0 as recorded
//~sqzY = 0;             // -1..1   the same left-right
//~strX = 0;             // -8..8   ~sqzX's own sweep, out and back
//~strY = 0;             // -8..8   the same for ~sqzY
//~tilt = 0;             // -1..1   the figure's plane leant forward (+) or back
//~roll = 0;             // -1..1   the figure's plane leant to the left (+) or right
//~tswp = 0;             // -8..8   ~tilt's own sweep, out and back
//~rswp = 0;             // -8..8   ~roll's own sweep, out and back
//~spin = 0;             // -8..8   bars per revolution, sign = direction
//~swell = 0;            // -8..8   sweeps ~reach out of where it sits and back
//~sway = 0;             // -8..8   sweeps ~base towards the pole the sign says
~fade = 0.8;             // 0..1    how much of the take the joins take over
//~bias = 0;             // -4..4   where a drawn-through gap leads
~dir = \reverse;         //         \forward \reverse
~end = \bounce;          //         \loop \stop \pause \bounce \random

// ---- Accent ----------------------------------------------------------
~attack = 0;             // 0..6    rise while ACT is held, in bar fractions
~decay = 6;              // 0..6    fall once ACT is let go
~envelopeMax = 0.8;      // 0..1    the 3d it rises to
~freqAttack = 0;         // 0..6    the same for the filter's cutoff
~freqDecay = 6;          // 0..6
~freqMax = 0.5;          // 0..1    0 is off
~qAttack = 0;            // 0..6    and for its resonance
~qDecay = 5;             // 0..6
~qMax = 0.5;             // 0..1    0 is off
~act = \oneshot;         //         \oneshot \hold
