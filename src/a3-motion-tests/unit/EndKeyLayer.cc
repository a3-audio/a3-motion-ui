/*

  A3 Motion UI
  Copyright (C) 2023 Patric Schmitz

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/


#include <gtest/gtest.h>

#include <a3-motion-ui/io/EndKeyLayer.hh>

using namespace a3;

namespace
{
EndKeyEvent
function (FunctionKey key, bool down)
{
  return { EndKeyMeaning::plain (key), down };
}

EndKeyEvent
scenePad (index_t pad, bool down)
{
  return { EndKeyMeaning::scenePad (pad), down };
}

using Events = std::vector<EndKeyEvent>;
}

// Plain, the end keys are what they say: TAP and SHIFT their functions, PLAY
// all and the actions the scene pads -- the same pad on every channel.
TEST (EndKeyLayer, PlainKeysAreTheirOwnFunctionOrTheirScenePad)
{
  EndKeyLayer layer;

  EXPECT_EQ (layer.set (EndKey::Tap, true),
             (Events{ function (FunctionKey::Tap, true) }));
  EXPECT_EQ (layer.set (EndKey::Tap, false),
             (Events{ function (FunctionKey::Tap, false) }));

  EXPECT_EQ (layer.set (EndKey::PlayAll, true),
             (Events{ scenePad (padIndexFor (PadFunction::PlayPause), true) }));
  EXPECT_EQ (layer.set (EndKey::PlayAll, false),
             (Events{ scenePad (padIndexFor (PadFunction::PlayPause), false) }));

  EXPECT_EQ (layer.set (EndKey::Action2, true),
             (Events{ scenePad (padIndexForAction (1), true) }));
  EXPECT_EQ (layer.set (EndKey::Action6, true),
             (Events{ scenePad (padIndexForAction (5), true) }));
}

// With SHIFT held a key is only its shifted function: SHIFT+TAP is the clock
// and does not also tap.
TEST (EndKeyLayer, WithShiftAKeyIsOnlyItsShiftedFunction)
{
  EndKeyLayer layer;
  ASSERT_EQ (layer.set (EndKey::Shift, true),
             (Events{ function (FunctionKey::Shift, true) }));

  EXPECT_EQ (layer.set (EndKey::Tap, true),
             (Events{ function (FunctionKey::ClockMode, true) }));
  EXPECT_EQ (layer.set (EndKey::Action1, true),
             (Events{ function (FunctionKey::RecMode, true) }));
  EXPECT_EQ (layer.set (EndKey::Action6, true),
             (Events{ function (FunctionKey::Menu, true) }));
  EXPECT_FALSE (layer.isDown (FunctionKey::Tap));
}

// SHIFT on an action key is never "the shifted action on every channel" (a
// preview, as Shift+ACT is on a pad): row 4 has no shifted function and does
// nothing at all.
TEST (EndKeyLayer, ShiftOnRowFourDoesNothing)
{
  EndKeyLayer layer;
  layer.set (EndKey::Shift, true);

  EXPECT_EQ (layer.set (EndKey::Action3, true), Events{});
  EXPECT_EQ (layer.set (EndKey::Action4, true), Events{});
  EXPECT_EQ (layer.set (EndKey::Action3, false), Events{});
}

// REC is held while SHIFT and PLAY all both are: that is what makes
// REC + a channel's Play pad record onto it, as the REC key did.
TEST (EndKeyLayer, ShiftThenPlayAllHoldsRec)
{
  EndKeyLayer layer;
  layer.set (EndKey::Shift, true);

  EXPECT_EQ (layer.set (EndKey::PlayAll, true),
             (Events{ function (FunctionKey::Record, true) }));
  EXPECT_TRUE (layer.isDown (FunctionKey::Record));
  EXPECT_TRUE (layer.isDown (FunctionKey::Shift));

  EXPECT_EQ (layer.set (EndKey::PlayAll, false),
             (Events{ function (FunctionKey::Record, false) }));
  EXPECT_FALSE (layer.isDown (FunctionKey::Record));
  EXPECT_TRUE (layer.isDown (FunctionKey::Shift));
}

// Letting go of SHIFT first ends REC as well, and the later release of PLAY
// all is no scene release: its press was never a scene press.
TEST (EndKeyLayer, ReleasingShiftEndsRec)
{
  EndKeyLayer layer;
  layer.set (EndKey::Shift, true);
  layer.set (EndKey::PlayAll, true);

  EXPECT_EQ (layer.set (EndKey::Shift, false),
             (Events{ function (FunctionKey::Record, false),
                      function (FunctionKey::Shift, false) }));
  EXPECT_FALSE (layer.isDown (FunctionKey::Record));

  EXPECT_EQ (layer.set (EndKey::PlayAll, false), Events{});
}

// PLAY all first and SHIFT after is not REC: PLAY all has already toggled the
// room by then, and a chord that took back what one of its keys had done
// would be no chord a hand can trust.
TEST (EndKeyLayer, PlayAllThenShiftIsNotRec)
{
  EndKeyLayer layer;
  layer.set (EndKey::PlayAll, true);

  EXPECT_EQ (layer.set (EndKey::Shift, true),
             (Events{ function (FunctionKey::Shift, true) }));
  EXPECT_FALSE (layer.isDown (FunctionKey::Record));

  EXPECT_EQ (layer.set (EndKey::PlayAll, false),
             (Events{ scenePad (padIndexFor (PadFunction::PlayPause), false) }));
}

// A key keeps the meaning it was pressed with: an action held from before
// SHIFT goes on being the action, and its release reaches the action.
TEST (EndKeyLayer, AKeyKeepsTheMeaningItWasPressedWith)
{
  EndKeyLayer layer;
  layer.set (EndKey::Action5, true);
  layer.set (EndKey::Shift, true);

  EXPECT_EQ (layer.set (EndKey::Action5, false),
             (Events{ scenePad (padIndexForAction (4), false) }));
}
