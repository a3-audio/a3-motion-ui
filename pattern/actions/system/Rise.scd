// Rise -- held, the sound climbs overhead and stays while the finger does.
//
// The base is the trajectory's midpoint, 0 at the north pole: setting it to
// zero lifts the whole clip over the listener rather than moving it through
// them. Slow attack, because a rise you can hear arriving is the point.

~base = 0.0;
~reach = 0.45;
~spin = 2;

~attack = 5;
~decay = 4;
~envelopeMax = 0.85;

~freqAttack = 5;
~freqDecay = 4;
~freqMax = 0.8;

~act = \hold;
