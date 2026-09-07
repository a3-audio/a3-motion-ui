// Quarter -- the figure is turned a quarter and plays at a quarter the pace.
//
// A standing turn, not a spin: rotate moves where the shape faces and leaves
// it there, so this is the one that re-aims a take rather than setting it
// going. With the speed down two powers of two, a bar's figure takes four,
// which is long enough to hear the turn as a new position rather than as a
// move.
//
// One-shot: it is a decision about where the clip points, and a decision you
// have to keep holding is not one.

~rotate = ~rotate + 0.25;
~speedLog2 = ~speedLog2 + 2;
~clipTop = 0.15;

~attack = 5;
~decay = 5;
~envelopeMax = 0.7;

~act = \oneshot;
