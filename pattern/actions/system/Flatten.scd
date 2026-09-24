// Flatten -- held, the whole take is pinned to one height.
//
// Flat mode ignores the reach and puts every point of the figure at one
// elevation, so the movement is left with its plan and nothing of its height:
// a take that climbed and dived becomes a take that only goes round. Letting
// go gives the room back.
//
// Ear height, because that is the one elevation a listener does not have to
// look for.

// ---- Shape -----------------------------------------------------------
//~speedLog2 = 0;        // -7..4   how fast, as a power of two; -3 is the 1/8

// ---- Elevation -------------------------------------------------------
//~base = 0;             // 0..1    where the middle sits; 0 north, 1 south
//~reach = 0.5;          // -1..1   how far it spreads, sign says down or up
//~clipTop = 0;          // 0..1    cut this much off the north side
//~clipBottom = 0;       // 0..1    the same from the south side
~flat = true;            // bool    hold one elevation, ignore the reach
~flatElevation = 0.5;    // 0..1    the elevation it is held at

// ---- Motion ----------------------------------------------------------
//~rotate = 0;           // 0..1    standing angle in revolutions; it wraps
//~sqzX = 0;             // -1..1   squeeze front-back; 0 as recorded
//~sqzY = 0;             // -1..1   the same left-right
//~strX = 0;             // -8..8   ~sqzX's own sweep, out and back
//~strY = 0;             // -8..8   the same for ~sqzY
~spin = 4;               // -8..8   bars per revolution, sign = direction
//~swell = 0;            // -8..8   sweeps ~reach out of where it sits and back
//~sway = 0;             // -8..8   sweeps ~base towards the pole the sign says
//~fade = 0;             // 0..1    how much of the take the joins take over
//~bias = 0;             // -4..4   where a drawn-through gap leads
//~dir = \forward;       //         \forward \reverse
//~end = \loop;          //         \loop \stop \pause \bounce \random

// ---- Accent ----------------------------------------------------------
~attack = 1;             // 0..6    rise while ACT is held, in bar fractions
~decay = 3;              // 0..6    fall once ACT is let go
~envelopeMax = 0.85;     // 0..1    the 3d it rises to
~freqAttack = 1;         // 0..6    the same for the filter's cutoff
~freqDecay = 3;          // 0..6
~freqMax = 0.7;          // 0..1    0 is off
//~qAttack = 2;          // 0..6    and for its resonance
//~qDecay = 3;           // 0..6
//~qMax = 0;             // 0..1    0 is off
~act = \hold;            //         \oneshot \hold
