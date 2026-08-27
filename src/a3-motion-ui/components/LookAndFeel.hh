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

#pragma once

#include <JuceHeader.h>

#include <a3-motion-ui/theme/ThemeColours.hh>

namespace a3
{

/*
 * Our custom A3 LookAndFeel class.
 *
 * A note on the enum values for our custom ColourIds. JUCE uses
 * per-Component globally unique ColourIds, so we use the same
 * mechanism for our own Components. JUCE IDs start at 0x1000000, A3
 * IDs start at 0x03000000.
 *
 * Formatting convention of IDs is as follows: 0x03000204 identifies
 * color 4 of the A3 widget with running Component number 2. Every
 * newly introduced Component class increases the component number by
 * one. The per-Component custom colors are then numbered 00-ff in the
 * last 2 hex digits, making for 255 possible custom colors per custom
 * Component type.
 */
class LookAndFeel_A3 : public juce::LookAndFeel_V4
{
public:
  LookAndFeel_A3 ();

  /** Point every ColourId this app draws with at the role it belongs to.
   *
   *  Called again after a skin change, which is why it is not just the
   *  constructor: a LookAndFeel outlives the theme it was built from. */
  void applyTheme (Theme const &theme);

  /** Every widget that does not set its own font asks here for one, so this is
   *  where the Font Size setting can reach a component that never mentions it
   *  — including one added later. The factor multiplies whatever base the
   *  widget already had, so a deliberate size keeps its proportion. */
  juce::Font getLabelFont (juce::Label &label) override;
  juce::Font getComboBoxFont (juce::ComboBox &box) override;
  juce::Font getPopupMenuFont () override;
  juce::Font getTextButtonFont (juce::TextButton &button,
                                int buttonHeight) override;
};


/** Put a freshly loaded theme in force everywhere on the message thread:
 *  the LookAndFeel's cached ColourIds, every component that caches something
 *  of its own, and a repaint.
 *
 *  `inTree` is any component in the window — its LookAndFeel is resolved
 *  through the parent chain, the same chain its children resolve colours
 *  through, and its top-level component is the tree that gets walked. */
void applyThemeEverywhere (Theme loaded, juce::Component &inTree);

}
