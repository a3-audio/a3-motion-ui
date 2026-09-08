// Rewind -- the take runs backwards, one step quicker, and bounces out.
//
// Reversing a movement is the cheapest way to make a familiar clip
// unfamiliar, and at double speed it lands back where it started.

~dir = \reverse;
~speedLog2 = ~speedLog2 + 1;
~end = \bounce;

~attack = 0;
~decay = 3;
~envelopeMax = 0.8;

~freqAttack = 1;
~freqDecay = 3;
~freqMax = 0.55;

~act = \oneshot;
