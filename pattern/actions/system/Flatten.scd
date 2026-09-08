// Flatten -- held, the whole take is pinned to one height.
//
// Flat mode ignores the reach and puts every point of the figure at one
// elevation, so the movement is left with its plan and nothing of its height:
// a take that climbed and dived becomes a take that only goes round. Letting
// go gives the room back.
//
// Ear height, because that is the one elevation a listener does not have to
// look for.

~flat = true;
~flatElevation = 0.5;
~spin = 4;

~attack = 1;
~decay = 3;
~envelopeMax = 0.85;

~freqAttack = 1;
~freqDecay = 3;
~freqMax = 0.7;

~act = \hold;
