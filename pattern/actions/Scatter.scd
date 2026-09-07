// Scatter -- somewhere else every time it is chosen.
//
// rrand and .choose throw their dice when the script is picked, so choosing
// it again is how you ask for another one. The same seed gives the same
// throw, which is what makes a random action something you can rehearse.

~spin = rrand(-8, 8);
~swell = [-4, 0, 4].choose;
~reach = rrand(0.3, 1.0);
~rotate = rrand(0.0, 1.0);
~dir = [\forward, \reverse].choose;
~end = \random;

// The holes go somewhere else as well: fade opens them and bias sends each
// one wherever it likes, which is the same idea one layer down.
~fade = rrand(0.0, 1.0);
~bias = [-4, 0, 4].choose;

~attack = [0, 1, 2].choose;
~decay = rrand(2, 5);
~envelopeMax = rrand(0.5, 1.0);

~act = \oneshot;
