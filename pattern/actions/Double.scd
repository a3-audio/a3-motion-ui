// Double -- twice the tempo and twice the turn, whatever the clip had.
//
// Both lines read the clip's own value first, so this means something
// different on every clip it is fired at -- which is what makes it worth
// keeping on a pad rather than dialling in.

~speedLog2 = ~speedLog2 + 1;
~spin = ~spin * 2;

~attack = 0;
~decay = 2;
~envelopeMax = 0.85;

~act = \oneshot;
