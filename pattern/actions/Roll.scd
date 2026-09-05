// Roll -- new dice on the turn only.
//
// Scatter throws everything; this throws the spin and the direction and
// leaves the shape alone, so the clip stays recognisable and still surprises.
// Picking the script again is how you ask for another throw.

~spin = rrand(-8, 8);
~dir = [\forward, \reverse].choose;

~attack = 1;
~decay = 3;
~envelopeMax = rrand(0.6, 1.0);

~act = \oneshot;
