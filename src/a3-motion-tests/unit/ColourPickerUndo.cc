/*

  A3 Motion UI
  Copyright (C) 2026 Patric Schmitz

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

#include <JuceHeader.h>

#include <a3-motion-ui/components/ColourPickerComponent.hh>

using namespace a3;

// The picker keeps the edit box's contract (#55): Escape and Back put back
// the colour it opened with, done keeps the new one. Before, every way out
// kept it, and the only way back was to dial the old colour in again.

namespace
{
juce::Colour const opened{ 200, 40, 40 };

/** Arm the first row and turn it: the colour moves off the one it opened
 *  with, the way the encoder moves it. */
void
turnAway (ColourPickerComponent &picker)
{
  picker.toggleEditing ();
  picker.navigate (20);
}
}

TEST (ColourPickerUndo, CancelPutsBackTheColourItOpenedWith)
{
  ColourPickerComponent picker;
  picker.setColour (opened, "accent");
  turnAway (picker);
  ASSERT_NE (picker.getColour (), opened) << "the turn changed nothing";

  picker.cancel ();
  EXPECT_EQ (picker.getColour (), opened);
}

// The sphere shows the colour while it is picked, so putting it back has to
// reach the sphere too -- through the same callback a turn does.
TEST (ColourPickerUndo, CancelTellsTheSkinAndThenLeaves)
{
  ColourPickerComponent picker;
  picker.setColour (opened, "accent");
  turnAway (picker);

  juce::Colour told;
  auto left = false;
  picker.onColourChanged = [&] { told = picker.getColour (); };
  picker.onCancel = [&] {
    EXPECT_EQ (told, opened) << "left before the skin had the old colour back";
    left = true;
  };

  picker.cancel ();
  EXPECT_TRUE (left);
}

TEST (ColourPickerUndo, EscapeCancels)
{
  ColourPickerComponent picker;
  picker.setColour (opened, "accent");
  turnAway (picker);

  auto left = false;
  picker.onCancel = [&] { left = true; };
  EXPECT_TRUE (picker.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
  EXPECT_EQ (picker.getColour (), opened);
  EXPECT_TRUE (left);
}

// Opening it again on another colour is a new starting point, not the first.
TEST (ColourPickerUndo, EachOpeningIsItsOwnWayBack)
{
  ColourPickerComponent picker;
  picker.setColour (opened, "accent");
  turnAway (picker);

  juce::Colour const second{ 10, 120, 220 };
  picker.setColour (second, "warning");
  turnAway (picker);
  picker.cancel ();
  EXPECT_EQ (picker.getColour (), second);
}

// Channel 1 is channels.0 in the file; the picker says the number on the
// panel.
TEST (ColourPickerUndo, ChannelsAreCountedFromOne)
{
  EXPECT_EQ (colourPickerTitle ("channels.0"), "channel 1");
  EXPECT_EQ (colourPickerTitle ("channels.3"), "channel 4");
  EXPECT_EQ (colourPickerTitle ("accent"), "accent");
  EXPECT_EQ (colourPickerTitle ("channelsGlow"), "channelsGlow");
}

// A page test must paint: the card with its title, after a cancel.
TEST (ColourPickerUndo, PaintsAfterACancel)
{
  ColourPickerComponent picker;
  picker.setBounds (0, 0, 768, 329);
  picker.setColour (opened, "channels.0");
  turnAway (picker);
  picker.cancel ();

  juce::Image image (juce::Image::ARGB, 768, 329, true);
  juce::Graphics g (image);
  picker.paintEntireComponent (g, false);
  EXPECT_EQ (picker.title (), "channel 1");
}

// Back and MENU both walk out through toggleGlobalSettings, the overlay's
// close through closeAllOverlays; both leave a mask without keeping it.
// Read from the source, as ChannelPotTurn does: the component that owns
// them cannot be built in the runner.
namespace
{
juce::String
uiBodyOf (juce::String const &signature)
{
  auto const text = juce::File (A3_UI_SOURCE_DIR)
                        .getChildFile ("components/A3MotionUIComponent.cc")
                        .loadFileAsString ();
  return text.fromFirstOccurrenceOf (signature, false, false)
      .upToFirstOccurrenceOf ("\n}\n", false, false);
}
}

TEST (ColourPickerUndo, BackAndCloseDoNotKeepThePickedColour)
{
  for (auto const *signature : { "A3MotionUIComponent::toggleGlobalSettings ()",
                                 "A3MotionUIComponent::closeAllOverlays ()" })
    {
      auto const body = uiBodyOf (signature);
      ASSERT_TRUE (body.isNotEmpty ()) << signature;
      EXPECT_TRUE (body.contains ("_colourPicker->cancel ()")) << signature;
    }
}
