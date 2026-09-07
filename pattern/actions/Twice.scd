// Twice -- Half read the other way: more of whatever the clip is doing.
//
// Reach, spin, swell and fade all doubled from where they stand. Held rather
// than one-shot, because "more" is something you want to be able to back out
// of by letting go.

// The pace as well as the movement -- it is called Twice. One power of two
// down is half the bars per cycle, which is twice as fast.
~speedLog2 = ~speedLog2 - 1;

~reach = ~reach * 2;
~spin = ~spin * 2;
~swell = ~swell * 2;
~fade = ~fade * 2;

~attack = 2;
~decay = 2;
~envelopeMax = 0.8;

~act = \hold;
