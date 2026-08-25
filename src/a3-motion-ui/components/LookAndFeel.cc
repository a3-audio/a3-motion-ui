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

#include "LookAndFeel.hh"

namespace a3
{

LookAndFeel_A3::LookAndFeel_A3 ()
{
  applyTheme (theme ());
}

void
LookAndFeel_A3::applyTheme (Theme const &theme)
{
  auto const surface = toColour (theme.surface);
  auto const surfaceRaised = toColour (theme.surfaceRaised);
  auto const background = toColour (theme.background);
  auto const textPrimary = toColour (theme.textPrimary);
  auto const textMuted = toColour (theme.textMuted);
  auto const textOnAccent = toColour (theme.textOnAccent);
  auto const accent = toColour (theme.accent);

  setColour (juce::ResizableWindow::backgroundColourId, background);
  setColour (juce::DocumentWindow::textColourId, textPrimary);

  setColour (juce::Label::textColourId, textPrimary);
  setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);

  // The knob keeps its own greys rather than taking the accent: it is a value
  // readout, and an accent-coloured one would compete with the state colours
  // sharing the screen with it.
  setColour (juce::Slider::thumbColourId, textMuted);
  setColour (juce::Slider::rotarySliderFillColourId, textMuted.darker ());
  setColour (juce::Slider::rotarySliderOutlineColourId, surfaceRaised);
  setColour (juce::Slider::backgroundColourId, surface);
  setColour (juce::Slider::trackColourId, accent);

  setColour (juce::TextButton::buttonColourId, surfaceRaised);
  setColour (juce::TextButton::buttonOnColourId, accent);
  setColour (juce::TextButton::textColourOffId, textPrimary);
  setColour (juce::TextButton::textColourOnId, textOnAccent);

  setColour (juce::ComboBox::backgroundColourId, surfaceRaised);
  setColour (juce::ComboBox::textColourId, textPrimary);
  setColour (juce::ComboBox::outlineColourId, surface);

  setColour (juce::PopupMenu::backgroundColourId, surfaceRaised);
  setColour (juce::PopupMenu::textColourId, textPrimary);
  setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
  setColour (juce::PopupMenu::highlightedTextColourId, textOnAccent);

  setColour (juce::ScrollBar::thumbColourId, textMuted);
}

namespace
{
juce::Font
scaled (juce::Font font)
{
  return font.withHeight (font.getHeight () * theme ().fontScale);
}
}

juce::Font
LookAndFeel_A3::getLabelFont (juce::Label &label)
{
  return scaled (juce::LookAndFeel_V4::getLabelFont (label));
}

juce::Font
LookAndFeel_A3::getComboBoxFont (juce::ComboBox &box)
{
  return scaled (juce::LookAndFeel_V4::getComboBoxFont (box));
}

juce::Font
LookAndFeel_A3::getPopupMenuFont ()
{
  return scaled (juce::LookAndFeel_V4::getPopupMenuFont ());
}

juce::Font
LookAndFeel_A3::getTextButtonFont (juce::TextButton &button, int buttonHeight)
{
  return scaled (
      juce::LookAndFeel_V4::getTextButtonFont (button, buttonHeight));
}

}
