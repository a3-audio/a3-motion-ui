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

#include <a3-motion-ui/theme/FunctionKeyColours.hh>
#include <a3-motion-ui/theme/TransportLook.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

using namespace a3;

// Whose tempo is running has one answer and one colour, wherever it is shown
// — the status bar reads it by the same rule.
TEST (FunctionKeyColours, TheClockKeyCarriesItsModesColour)
{
  for (int mode : { 0, 1, 2 })
    {
      FunctionKeyLook look;
      look.clockMode = mode;

      EXPECT_EQ (functionKeyColour (FunctionKey::ClockMode, look),
                 Colours::clockMode (mode));
    }
}

// The one key coloured while nothing is happening, and red while it is:
// recording writes over what you cannot get back, so the key that does it
// says so before it is pressed as well as after. Running is the same red,
// brighter — a difference in loudness rather than a second thing to learn.
TEST (FunctionKeyColours, RecordIsRedAndBrighterWhileRunning)
{
  FunctionKeyLook look;
  auto const resting = functionKeyColour (FunctionKey::Record, look);
  EXPECT_EQ (resting, toColour (theme ().danger));

  look.recording = true;
  auto const running = functionKeyColour (FunctionKey::Record, look);

  EXPECT_NE (running, resting);

  // Perceived brightness, not getBrightness(): danger is a fully saturated
  // red, whose brightness is already 1, so brighter() can only take it
  // towards white. What the eye reads is the perceived value, and that is
  // what has to go up.
  EXPECT_GT (running.getPerceivedBrightness (),
             resting.getPerceivedBrightness ());

  // Still red: the hue is what the key means, and it does not change.
  EXPECT_NEAR (running.getHue (), resting.getHue (), 0.02f);
}

// A modifier you cannot see at a glance is one you will get wrong — but only
// while it is down; held is the whole of what it has to say.
TEST (FunctionKeyColours, ShiftIsColouredOnlyWhileHeld)
{
  FunctionKeyLook look;
  EXPECT_TRUE (functionKeyColour (FunctionKey::Shift, look).isTransparent ());

  look.shiftHeld = true;
  EXPECT_EQ (functionKeyColour (FunctionKey::Shift, look),
             toColour (theme ().accent));
}

TEST (FunctionKeyColours, TapIsColouredOnABeatAndUnderAFinger)
{
  FunctionKeyLook look;
  EXPECT_TRUE (functionKeyColour (FunctionKey::Tap, look).isTransparent ());

  look.tapBeat = true;
  EXPECT_EQ (functionKeyColour (FunctionKey::Tap, look),
             toColour (theme ().accent));

  look = {};
  look.tapPressed = true;
  EXPECT_EQ (functionKeyColour (FunctionKey::Tap, look),
             toColour (theme ().accent));
}

// The two that say what they do with a word. A colour on them would be a
// colour that means nothing, and every colour that means nothing makes the
// ones that mean something harder to read.
TEST (FunctionKeyColours, MenuAnswersToNothingButItself)
{
  // It used to carry no colour at all, on the argument that a colour meaning
  // nothing makes the ones that mean something harder to read. It is blue now
  // -- a key you want to find rather than read. What still holds is the part
  // that mattered: nothing happening elsewhere on the device changes it.
  FunctionKeyLook quiet;
  FunctionKeyLook busy;
  busy.recording = true;
  busy.shiftHeld = true;
  busy.tapBeat = true;
  busy.clockMode = 2;
  busy.recMode = 2;

  EXPECT_EQ (functionKeyColour (FunctionKey::Menu, quiet),
             functionKeyColour (FunctionKey::Menu, busy));
}


// The rec mode says how much of an old take a pass will destroy, so it says it
// on the scale the rest of the device uses: the accent for the one that mends,
// the warning for the one that holds on, danger for the one that clears.
TEST (FunctionKeyColours, TheRecModeIsColouredByHowMuchItDestroys)
{
  EXPECT_EQ (recModeColour (0), toColour (theme ().accent));
  EXPECT_EQ (recModeColour (1), toColour (theme ().warning));
  EXPECT_EQ (recModeColour (2), toColour (theme ().danger));

  // Out of range is the safe one, not an absent colour: this arrives from a
  // settings file that can be older than the list.
  EXPECT_EQ (recModeColour (7), toColour (theme ().accent));

  FunctionKeyLook look;
  look.recMode = 2;
  EXPECT_EQ (functionKeyColour (FunctionKey::RecMode, look),
             recModeColour (2));
}

// ── One colour per clip action, wherever it is drawn ─────────────────────

TEST (TransportLook, TheFourActionsEachKeepOneColour)
{
  // The point of the rule: REC in the header, REC on the panel and REC on a
  // pad are the same function, and a function that changes colour between
  // corners of the screen is one you have to read rather than recognise.
  EXPECT_EQ (transportColour (TransportKey::Record),
             toColour (theme ().danger));
  EXPECT_EQ (transportColour (TransportKey::Stop), toColour (theme ().danger));
  EXPECT_EQ (transportColour (TransportKey::Action),
             toColour (theme ().highlight));
}

TEST (TransportLook, PlayIsGreenWhetherOrNotItIsRunning)
{
  // The colour says which key this is, not what it is doing. A key that
  // changes colour with its state has to be looked at twice -- once to find it
  // and once to read it -- and finding it is the job that matters mid-set.
  // Running is said by the shape and by the key's own ground instead.
  EXPECT_EQ (transportColour (TransportKey::PlayPause),
             toColour (theme ().accent));
}

TEST (TransportLook, APadReachesTheSameRuleAsTheHeaderKey)
{
  for (auto const function : { PadFunction::PlayPause, PadFunction::Stop,
                               PadFunction::Action })
    EXPECT_EQ (padFunctionColour (function),
               transportColour (transportKeyForPad (function)))
        << "pad function " << static_cast<int> (function);
}

TEST (TransportLook, EveryPadFunctionButSettingsHasAShape)
{
  for (auto const function : { PadFunction::PlayPause, PadFunction::Stop,
                               PadFunction::Action })
    EXPECT_TRUE (hasTransportGlyph (function));

  // It opens a menu. There is no fifty-year-old shape for that, and one
  // invented here would have to be learned -- which is what a word is.
  EXPECT_FALSE (hasTransportGlyph (PadFunction::Settings));
}

TEST (TransportLook, SettingsHasNoColourOfItsOwn)
{
  // It opens a menu. A colour that means nothing makes the ones that mean
  // something harder to read.
  EXPECT_TRUE (padFunctionColour (PadFunction::Settings).isTransparent ());
}

TEST (TransportLook, TheRecordFunctionKeyIsTheSameRedAsTheTransportKey)
{
  FunctionKeyLook look{};
  EXPECT_EQ (functionKeyColour (FunctionKey::Record, look),
             transportColour (TransportKey::Record));
}

TEST (TransportLook, EveryMarkReadsOnTheBarsOwnGround)
{
  // The header keys sit on the bar, whose ground the skin does choose -- so
  // unlike a pad's channel colour it can be held to a floor. WCAG's 3:1 for
  // large text; these are marks a few millimetres across in a dark room.
  auto const ground = toColour (theme ().surface);

  for (auto const key : { TransportKey::Record, TransportKey::Stop,
                          TransportKey::PlayPause, TransportKey::Action })
    EXPECT_GE (contrastRatio (transportColour (key), ground), 3.f)
        << transportColour (key).toString ();
}

TEST (TransportLook, ContrastRatioIsSymmetricAndBounded)
{
  EXPECT_NEAR (contrastRatio (juce::Colours::white, juce::Colours::black),
               21.f, 0.1f);
  EXPECT_NEAR (contrastRatio (juce::Colours::black, juce::Colours::white),
               21.f, 0.1f);
  EXPECT_NEAR (contrastRatio (juce::Colours::red, juce::Colours::red), 1.f,
               0.001f);
}

TEST (FunctionKeyColours, MenuIsBlueWhetherOrNotItIsOpen)
{
  // The colour says which key this is; being inside the menu is said by the
  // key lighting up, the same way Shift says it is held. A key that only had
  // a colour while you were in it would be a key you could not find your way
  // to.
  FunctionKeyLook closed;
  FunctionKeyLook open;
  open.menuOpen = true;

  EXPECT_EQ (functionKeyColour (FunctionKey::Menu, closed),
             toColour (theme ().notice));
  EXPECT_EQ (functionKeyColour (FunctionKey::Menu, open),
             toColour (theme ().notice));
}

TEST (FunctionKeyColours, MenuIsNotTheSameColourAsAnyStateKey)
{
  // Blue has to stay distinguishable from the three that mean something is
  // happening: recording, running, and the accent.
  FunctionKeyLook look;
  auto const menu = functionKeyColour (FunctionKey::Menu, look);

  EXPECT_NE (menu, transportColour (TransportKey::Record));
  EXPECT_NE (menu, transportColour (TransportKey::PlayPause));
  EXPECT_NE (menu, transportColour (TransportKey::Action));
}
