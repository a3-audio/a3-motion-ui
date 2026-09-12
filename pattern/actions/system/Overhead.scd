// Overhead -- the clip snaps to the cap above the listener.
//
// Base at the north pole with a short reach keeps the whole trajectory in
// the top of the sphere, and the turn keeps it from sitting still up there.

~base = 0.0;
~reach = 0.3;
~spin = 4;
~end = \loop;

~attack = 1;
~decay = 3;
~envelopeMax = 0.9;

~freqAttack = 2;
~freqDecay = 3;
~freqMax = 0.7;

~act = \oneshot;
