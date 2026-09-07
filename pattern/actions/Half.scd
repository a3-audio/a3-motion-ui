// Half -- whatever the clip is doing, less of it.
//
// Every line here reads the clip's own value and works from it, so the same
// action means something different on every clip it is fired at.

~reach = ~reach * 0.8;
~spin = ~spin / 2;
~fade = ~fade * 0.5;

~attack = 2;
~decay = 2;
~envelopeMax = 0.4;
~act = \hold;
