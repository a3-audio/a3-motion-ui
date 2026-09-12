// Whirl -- the trajectory turns as fast as it can, once.
//
// Spin is a signed power of two in bars per cycle and eight is the fastest
// step there is, so the whole shape goes round underneath the blob. Bounce
// at the end, because a whirl that stops dead reads as a fault.

~spin = 8;
~reach = 0.8;
~end = \bounce;

~attack = 1;
~decay = 4;
~envelopeMax = 1.0;

~freqAttack = 2;
~freqDecay = 4;
~freqMax = 0.6;

~act = \oneshot;
