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

#include <a3-motion-ui/theme/Theme.hh>

namespace a3
{

/** A role's colour, ready to draw with. Roles are opaque; transparency is a
 *  state (disabled, inactive) and belongs to the alpha the caller applies. */
juce::Colour toColour (ThemeColour const &colour);
juce::Colour toColour (ThemeColour const &colour, float alpha);

/** Shorthands for the two colours drawn all over the 2D components. Functions,
 *  not constants: they have to follow a skin change, and a `const juce::Colour`
 *  is fixed at static-init time — before any skin has been read. */
namespace Colours
{
juce::Colour background ();

/** What a clock mode looks like: 0 internal, 1 external, 2 Pro DJ Link.
 *
 *  One rule, because the status bar and the clip settings bar both say which
 *  clock is running and two answers to that question is one too many. EXT is
 *  a warning — the tempo is somebody else's. PIO is a notice: also not ours,
 *  but not a state to be careful about. INT is the accent. */
juce::Colour clockMode (int mode);

/** The ground a row of cells sits on. Derived from the background rather than
 *  being its own role: a skin that darkens the background should carry the
 *  status bar with it, and two tokens would have to be kept in step by hand. */
juce::Colour statusBar ();

/** Anything the bar or an overlay *reads*: a control's value, the caption
 *  naming it, the word on a button face.
 *
 *  Grey either way. A value used to be written in the channel's colour once
 *  its section was picked, which put a red or a white word beside a grey one
 *  and made the difference look like it meant something about the setting
 *  rather than about which section a finger last touched. What says whose
 *  section this is, is the ground behind it -- the colour belongs to the
 *  highlight, not to the reading.
 *
 *  `isSelected` decides emphasis only, and it is full opacity rather than an
 *  alpha rung: "selected" has always meant no dimming at all, which the
 *  alpha-less overload already says, and full opacity is the absence of an
 *  emphasis decision rather than one of its rungs. See
 *  issues/a3-motion-ui-metric-role-deviations.md (Task 16).
 *
 *  One function because there were four of it: ClipSettingsComponent's
 *  controlColour and captionColour, BarKnob's captionColour and BarButton's
 *  faceTextColour, all four the same two lines. What kept them apart was a
 *  ruling that knobs keep the channel's colour when selected and buttons do
 *  not -- true of BarKnob::controlColour, which does exactly that and stays
 *  where it is, and untrue of every one of these four. */
juce::Colour barText (bool isSelected);
}

}
