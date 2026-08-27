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

#include <JuceHeader.h>

#include <a3-motion-ui/components/LookAndFeel.hh>

using namespace a3;

namespace
{

// Every ColourId is filled from a role rather than from a literal. Without
// this the widgets keep the greys LookAndFeel_V4 ships with, and a skin change
// moves the sphere while leaving the menu around it untouched.

Theme
distinctTheme ()
{
  Theme skin;
  skin.background = { 1, 2, 3 };
  skin.surface = { 4, 5, 6 };
  skin.surfaceRaised = { 7, 8, 9 };
  skin.textPrimary = { 10, 11, 12 };
  skin.textMuted = { 13, 14, 15 };
  skin.textOnAccent = { 16, 17, 18 };
  skin.accent = { 19, 20, 21 };
  return skin;
}

TEST (LookAndFeelTheme, TheWindowAndTextComeFromTheirRoles)
{
  LookAndFeel_A3 lookAndFeel;
  lookAndFeel.applyTheme (distinctTheme ());

  EXPECT_EQ (lookAndFeel.findColour (juce::ResizableWindow::backgroundColourId),
             juce::Colour (1, 2, 3));
  EXPECT_EQ (lookAndFeel.findColour (juce::Label::textColourId),
             juce::Colour (10, 11, 12));
}

TEST (LookAndFeelTheme, ButtonsCarryTheAccentAndTheTextThatSitsOnIt)
{
  LookAndFeel_A3 lookAndFeel;
  lookAndFeel.applyTheme (distinctTheme ());

  EXPECT_EQ (lookAndFeel.findColour (juce::TextButton::buttonOnColourId),
             juce::Colour (19, 20, 21));
  EXPECT_EQ (lookAndFeel.findColour (juce::TextButton::textColourOnId),
             juce::Colour (16, 17, 18))
      << "text on the accent needs its own role, or it can end up unreadable";
}

// Guards against a ColourId being set from a literal instead of a role: no id
// the app draws with may still hold what LookAndFeel_V4 shipped.
TEST (LookAndFeelTheme, NoColourIdIsLeftAtItsJuceDefault)
{
  juce::LookAndFeel_V4 stock;
  LookAndFeel_A3 lookAndFeel;
  lookAndFeel.applyTheme (distinctTheme ());

  int const ids[] = {
    juce::ResizableWindow::backgroundColourId,
    juce::DocumentWindow::textColourId,
    juce::Label::textColourId,
    juce::Slider::thumbColourId,
    juce::Slider::rotarySliderFillColourId,
    juce::Slider::rotarySliderOutlineColourId,
    juce::Slider::backgroundColourId,
    juce::Slider::trackColourId,
    juce::TextButton::buttonColourId,
    juce::TextButton::buttonOnColourId,
    juce::TextButton::textColourOffId,
    juce::TextButton::textColourOnId,
    juce::ComboBox::backgroundColourId,
    juce::ComboBox::textColourId,
    juce::ComboBox::outlineColourId,
    juce::PopupMenu::backgroundColourId,
    juce::PopupMenu::textColourId,
    juce::PopupMenu::highlightedBackgroundColourId,
    juce::PopupMenu::highlightedTextColourId,
    juce::ScrollBar::thumbColourId,
  };

  for (auto const id : ids)
    EXPECT_NE (lookAndFeel.findColour (id), stock.findColour (id))
        << "colour id 0x" << std::hex << id << " still holds the JUCE default";
}

// A LookAndFeel outlives the theme it was built from, so a skin change has to
// reach it a second time.
TEST (LookAndFeelTheme, ASecondThemeReplacesTheFirst)
{
  LookAndFeel_A3 lookAndFeel;
  lookAndFeel.applyTheme (distinctTheme ());

  Theme other;
  other.background = { 200, 100, 50 };
  lookAndFeel.applyTheme (other);

  EXPECT_EQ (lookAndFeel.findColour (juce::ResizableWindow::backgroundColourId),
             juce::Colour (200, 100, 50));
}

}
