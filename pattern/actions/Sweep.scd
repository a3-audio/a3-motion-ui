// Sweep -- the filter alone, held. Nothing moves.
//
// No movement value is named, so the trajectory is untouched and the cutoff
// does all the work: a long open on the way in, a longer close on the way
// out, and the resonance riding just behind it.

~attack = 4;
~decay = 4;
~envelopeMax = 0.3;

~freqAttack = 5;
~freqDecay = 6;
~freqMax = 1.0;
~qAttack = 4;
~qDecay = 5;
~qMax = 0.5;

~act = \hold;
