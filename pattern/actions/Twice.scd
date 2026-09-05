// Twice -- Half read the other way: more of whatever the clip is doing.
//
// Reach, spin, swell and fade all doubled from where they stand. Held rather
// than one-shot, because "more" is something you want to be able to back out
// of by letting go.

~reach = ~reach * 2;
~spin = ~spin * 2;
~swell = ~swell * 2;
~fade = ~fade * 2;

~attack = 2;
~decay = 2;
~envelopeMax = 0.8;

~act = \hold;
