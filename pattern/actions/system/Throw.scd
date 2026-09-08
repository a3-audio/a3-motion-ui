// Throw -- a hard swing out and a filter that opens behind it.
//
// A script is read once, top to bottom, when it is chosen. ~name on the
// right reads what the clip has now, so a line can move a value rather than
// only set it.

~spin = -6;
~reach = 1.0;
~end = \bounce;

// The accent: fast up, slow back down.
~attack = 0;
~decay = 4;
~envelopeMax = 1.0;

// The cutoff opens with it; the resonance comes in later and leaves sooner.
~freqAttack = 1;
~freqDecay = 4;
~freqMax = 0.9;
~qAttack = 3;
~qDecay = 2;
~qMax = 0.6;

~act = \oneshot;
