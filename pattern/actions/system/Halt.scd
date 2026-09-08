// Halt -- everything that moves on its own stops, and the pass ends.
//
// Pause leaves the playhead where it is, so releasing and starting again
// carries on rather than restarting. The one action that takes things away
// instead of adding them.

~spin = 0;
~swell = 0;
~end = \pause;

~attack = 0;
~decay = 1;
~envelopeMax = 0.5;

~act = \oneshot;
