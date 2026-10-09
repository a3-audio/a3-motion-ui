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

#include "StatusBarLayout.hh"

#include <a3-motion-ui/io/Workspaces.hh>

#include <cmath>

namespace a3
{

int
statusLabelWidth (juce::Font const &font, juce::String const &text, int air)
{
  constexpr float strongestSquash = 0.7f;
  auto const whole = juce::GlyphArrangement::getStringWidth (font, text);
  return static_cast<int> (std::ceil (whole * strongestSquash)) + air;
}

StatusBarLayout
statusBarLayout (juce::Rectangle<int> row, int barWidth, int padding,
                 int bpmWidth, int readoutWidth)
{
  StatusBarLayout out{};

  if (row.isEmpty () || barWidth <= 0)
    return out;

  auto const rowHeight = row.getHeight ();

  // What the beat display takes. Both bounds are the ones resized() has
  // always applied.
  auto const tickWidth = juce::jmin (
      juce::roundToInt (static_cast<float> (row.getWidth ())
                        * statusTickWidthOfRow),
      juce::roundToInt (static_cast<float> (barWidth)
                        * statusTickWidthOfBar));

  // Centred on the whole bar rather than on what the labels leave over: it is
  // the one thing here that is looked at rather than read.
  out.tick
      = juce::Rectangle<int> (tickWidth,
                              juce::roundToInt (
                                  static_cast<float> (rowHeight)
                                  * statusTickHeightOfRow))
            .withCentre ({ barWidth / 2, row.getCentreY () });

  // The workspace switch closes the row, where StemDeck has it -- the same
  // numbers in both apps, so the key under the finger stays put when the
  // workspace changes.
  auto const switcher = workspaces::switcherGeometry (barWidth);
  out.workspacesKey = row.withX (barWidth - switcher.margin - switcher.arrowWidth)
                          .withWidth (switcher.arrowWidth);
  out.deckKey = row.withX (out.workspacesKey.getX () - switcher.gap
                           - switcher.appKeyWidth)
                    .withWidth (switcher.appKeyWidth);

  // The bar's own keys, one size: wide enough for a word, as tall as the
  // row. CLOCK leads it, left of the tempo it decides; CLEAN, the on-screen
  // keyboard and MENU stand before the switch.
  auto rest = row.withRight (out.deckKey.getX () - switcher.margin);
  auto const keyW = juce::jmin (rowHeight * statusKeyWidthOfHeight,
                                row.getWidth () / 8);
  out.menuKey = rest.removeFromRight (keyW);
  out.keyboardKey = rest.removeFromRight (keyW);
  out.cleanKey = rest.removeFromRight (keyW);
  // The pilots' level beside CLEAN, only where the beat display keeps its
  // floor between it and the bar's middle.
  auto const floorHalf = statusTickMinWidthOfHeight * rowHeight / 2;
  if (out.cleanKey.getX () - keyW >= barWidth / 2 + floorHalf)
    out.pilotKey = rest.removeFromRight (keyW);
  out.clockKey = rest.removeFromLeft (keyW);
  out.viewKey = rest.removeFromLeft (keyW);
  out.breathKey = rest.removeFromLeft (keyW);

  // Still centred, but never under a key: on a narrow bar the display gives
  // way rather than the keys.
  auto const innermostKey
      = out.pilotKey.isEmpty () ? out.cleanKey.getX () : out.pilotKey.getX ();
  auto const tickRoom = 2 * (innermostKey - barWidth / 2);
  if (out.tick.getWidth () > tickRoom)
    out.tick = out.tick.withSizeKeepingCentre (juce::jmax (0, tickRoom),
                                               out.tick.getHeight ());

  // The readings say what they need, and the display gives way to them,
  // still centred, down to its floor: the tempo is read mid-set, the beat
  // display is only looked at and still reads narrower.
  auto const readingsNeed = bpmWidth + readoutWidth + 2 * padding;
  auto const readingsHave = out.tick.getX () - rest.getX ();
  if (readingsNeed > readingsHave)
    {
      auto const floor = statusTickMinWidthOfHeight * rowHeight;
      auto const wanted
          = 2 * (barWidth / 2 - (rest.getX () + readingsNeed));
      auto const width = juce::jmin (out.tick.getWidth (),
                                     juce::jmax (floor, wanted));
      out.tick = out.tick.withSizeKeepingCentre (width, out.tick.getHeight ());
    }

  // What is left of the display's left edge carries the two readings: the
  // tempo, then what was last done, standing against the display so the
  // right end is keys only. The readout gets the larger share, being the
  // longer text.
  auto left = rest.withRight (juce::jmin (rest.getRight (), out.tick.getX ()))
                  .withTrimmedLeft (padding)
                  .withTrimmedRight (padding);
  out.bpm = left.removeFromLeft (juce::jmin (
      left.getWidth (), juce::jmax (left.getWidth () * 2 / 5, bpmWidth)));
  out.readout = left;

  return out;
}

}
