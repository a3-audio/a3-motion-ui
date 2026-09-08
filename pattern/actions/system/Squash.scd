// Squash -- held, the figure is pressed flat and springs back when you let go.
//
// The two squeezes are the only thing on the clip page that changes a figure's
// shape rather than where it sits, and nothing was reaching for them. Pressed
// on one axis and stretched on the other, so the figure keeps its area and
// changes its character: a circle becomes an ellipse lying across the room.
//
// The sweeps go with them. A squeeze that only stood still would be a
// different figure; one that breathes is the same figure being leant on.

~sqzX = -0.7;
~sqzY = 0.5;
~strX = 5;
~strY = -5;

~attack = 2;
~decay = 3;
~envelopeMax = 0.8;

~freqAttack = 2;
~freqDecay = 3;
~freqMax = 0.55;

~act = \hold;
