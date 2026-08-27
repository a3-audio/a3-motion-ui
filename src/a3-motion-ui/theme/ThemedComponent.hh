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

namespace a3
{

/** A component that keeps a copy of something from the theme.
 *
 *  Most components read the theme while painting, so a skin change needs
 *  nothing but a repaint. Some cannot: the status bar draws with
 *  juce::Labels, and a Label takes its colour and its font once and keeps
 *  them. Under a new skin those labels went on showing the old skin's
 *  accent — the status bar was the one part of the screen a skin could not
 *  reach.
 *
 *  Implement this to be told, and set everything you cache in applyTheme().
 *  Prefer reading the theme in paint() where that is possible; this is for
 *  what genuinely cannot. */
struct ThemedComponent
{
  virtual ~ThemedComponent () = default;

  /** Re-read everything this component caches from the theme. Called on the
   *  message thread, after the new theme is in force and before the repaint. */
  virtual void applyTheme () = 0;
};

/** Tell every ThemedComponent in `root`'s tree, `root` included, that the
 *  theme changed.
 *
 *  Hidden components are told as well: they can be shown again later, and
 *  nothing would tell them then. */
void applyThemeToTree (juce::Component &root);

}
