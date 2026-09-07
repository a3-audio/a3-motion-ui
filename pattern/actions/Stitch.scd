// Stitch -- the holes in a take are joined up, and they lead somewhere else.
//
// Fade says how much of the take the joins take over and bias says where each
// one leads: at the nearest other run, the movement closes into one line; sent
// at random, a tapped take becomes a walk through its own taps in an order
// nobody played.
//
// Only a take with holes in it hears this at all -- a figure has none -- which
// is what makes it the one to reach for over a recording.

~fade = 0.75;
~bias = -3;

~attack = 3;
~decay = 4;
~envelopeMax = 0.75;

~qAttack = 2;
~qDecay = 3;
~qMax = 0.3;

~act = \hold;
