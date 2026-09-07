// Rock -- held, the whole figure swings up and down the room.
//
// Sway moves the elevation base, so this is the one that moves the figure
// bodily rather than reshaping it: the take keeps its plan and the room tips
// under it. Started from the middle, because a rock wants somewhere to go in
// both directions.
//
// Two bars a swing, which is slow enough to feel like weight rather than a
// wobble.

~base = 0.5;
~sway = 3;
~reach = 0.35;

~attack = 4;
~decay = 4;
~envelopeMax = 0.7;

~qAttack = 3;
~qDecay = 4;
~qMax = 0.35;

~act = \hold;
