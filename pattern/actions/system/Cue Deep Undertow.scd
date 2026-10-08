// Cue Deep Undertow -- puts Deep Undertow on the channel, from the next downbeat.
// Mood: a change of clip -- the next phase of the night.
//
// The clip goes onto the channel and starts on the downbeat (Shift:
// at once), and stays.

// ---- Cue -------------------------------------------------------------
~clip = "Deep Undertow"; // "name"  puts that clip on the channel, from the next downbeat

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
//~tilt = 0;             // -2..2   the figure's plane leant forward (+) or back
//~roll = 0;             // -2..2   the figure's plane leant to the left (+) or right
//~tswp = 0;             // -8..8   turns ~tilt round, like spin
//~rswp = 0;             // -8..8   turns ~roll round, like spin
//~spin = 0;             // -8..8   bars per revolution, sign = direction
//~swell = 0;            // -8..8   sweeps ~reach out of where it sits and back
//~sway = 0;             // -8..8   sweeps ~base towards the pole the sign says
//~fade = 0;             // 0..1    how much of the take the joins take over
//~bias = 0;             // -4..4   where a drawn-through gap leads
//~dir = \forward;       //         \forward \reverse
//~end = \loop;          //         \loop \stop \pause \bounce \random

// ---- Accent ----------------------------------------------------------
//~attack = 2;           // 0..6    rise while ACT is held, in bar fractions
//~decay = 3;            // 0..6    fall once ACT is let go
~envelopeMax = 0;        // 0..1    the 3d it rises to
//~freqAttack = 2;       // 0..6    the same for the filter's cutoff
//~freqDecay = 3;        // 0..6
~freqMax = 0;            // 0..1    0 is off
//~qAttack = 2;          // 0..6    and for its resonance
//~qDecay = 3;           // 0..6
~qMax = 0;               // 0..1    0 is off
//~act = \oneshot;       //         \oneshot \hold
//~then = 1;             // 1..6    fires that button when the accent is over

// ---- Pilot -----------------------------------------------------------
//~game = \none;         //         \none \fakeout \formation \hideseek \callresponse
//~target = \nearest;    //         \nearest \group1..\group8 \crowd \hotspot
//~with = \self;         //         \self \nearest \all
