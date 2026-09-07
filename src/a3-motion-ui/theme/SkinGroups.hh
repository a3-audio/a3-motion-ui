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

/** How the skin editor's list is ordered and where its headings fall.
 *
 *  The list used to be alphabetical, grouped by the file's own nesting — which
 *  put `background` and `surface` forty rows apart with the speaker light's
 *  thirty-four between them, and left the twenty-one values that actually
 *  design a skin scattered around the blocks that tune a shader. A skin has
 *  eighty-five keys; sorted by their spelling, finding two that belong
 *  together is a search.
 *
 *  Grouped by what a value *is* instead. The order below is the order of the
 *  list: what a skin is made of first — surfaces, text, states, channels, the
 *  sphere, type — then what the effects are made of.
 *
 *  A path that matches nothing lands in the last group rather than vanishing.
 *  That matters more than it looks: the parameter list is derived from the
 *  file precisely so a new key needs no registering, and a grouping that
 *  silently dropped what it did not recognise would take that back.
 */

/** The group a skin path belongs to, by its heading. Never empty. */
juce::String skinGroupFor (juce::String const &path);

/** Where that group sits in the list. Lower comes first. */
int skinGroupOrder (juce::String const &group);

/** The heading given to anything not otherwise placed. */
juce::String skinUngroupedHeading ();

}
