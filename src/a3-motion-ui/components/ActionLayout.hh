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

#include <juce_gui_basics/juce_gui_basics.h>

#include <a3-motion-ui/components/ClipSettingsLayout.hh>

#include <array>

namespace a3
{

/** Where the ACTION page puts things.
 *
 *  On the right, a card of nine knobs -- three envelopes by attack, decay and
 *  ceiling -- for the chosen button's feel, positioned to read straight
 *  across into the global strip's channel grid beside it. Left of it, left
 *  to right: the six action buttons, the list the chosen one is assigned
 *  from, and a column of keys: EDIT, then the mode (2026-09-28). There is no
 *  ACT key: each of the six fields fires its own action.
 */
struct ActionLayout
{
  /** The six action buttons, three rows of two as on the panel: A1 A2 /
   *  A3 A4 / A5 A6, indexed by button. */
  std::array<juce::Rectangle<int>, 6> actionFields;

  /** The chosen button's mode, under EDIT in the key column. It says what a
   *  press does to all three envelopes, so it belongs to none of the rows. */
  juce::Rectangle<int> actModeField;

  /** Opens the action's script in FILES, beside the list there -- the
   *  editor that stood on this page moved on 2026-09-27. */
  juce::Rectangle<int> editButton;


  /** The list the chosen button is assigned from, open all the time between
   *  the buttons and the keys. Never over the knobs. */
  juce::Rectangle<int> actionListArea;
  /** A row of that list. A fingertip, whatever the page's size: picking a
   *  script mid-set is a tap, and a row you have to aim at is one you miss. */
  int actionListRowHeight = 0;

  /** The knobs stand on a card at the right, the way every other block of
   *  controls in the bar does. */
  juce::Rectangle<int> card;

  /** What the card is called. "Audio", because that is what the nine knobs
   *  do -- the accent and the two filters -- and a card of nine unnamed knobs
   *  beside a script is a card you have to work out. */
  juce::Rectangle<int> cardCaption;

  /** Three envelopes' worth of attack, decay and ceiling, in reading order --
   *  which is also the order the handler expects. The 3d accent first,
   *  because it is what ACT has always done, then the cutoff, then the
   *  resonance. */
  static constexpr int numRows = 3;
  std::array<juce::Rectangle<int>, numRows * 3> controls;

  /** The bands the controls stand on, top to bottom. */
  std::array<juce::Rectangle<int>, numRows> rows;

  /** Which envelope each row is, in a gutter down the left of the grid --
   *  the same arrangement the global strip names its channel rows with. */
  std::array<juce::Rectangle<int>, numRows> rowLabels;

  /** Knob size and text sizes, worked out for these cells -- the same
   *  numbers the clip bar hands its own knobs, so the two pages draw one
   *  knob rather than two similar ones. */
  ControlMetrics metrics{ 0, 0.f, 0.f };
};

/** How many rows of the action list are on screen at once.
 *
 *  A fingertip-high row in a field a few rows tall shows far fewer entries
 *  than there are scripts, so the list has to be scrollable -- and what it
 *  scrolls by is this. Its own function because the component may not
 *  guess it: a list drawn from row zero with a window it has not measured is
 *  a list whose last entries cannot be reached at all. */
int actionListVisibleRows (ActionLayout const &layout);

/** @param headerSize the theme's header size, which the control row is
 *                    measured against.
 *  @param bodySize   the theme's body size, which sets the knob and its
 *                    captions. */
/** @param gridReference where the global strip's three channel rows stand, in
 *                       this page's own coordinates -- the page puts its own
 *                       rows on exactly those, so 3d, freq and q read
 *                       straight across the bar. Empty, or too tall to fit,
 *                       and the page lays itself out instead: lining up is
 *                       worth having and worth losing, and a knob drawn past
 *                       the bottom edge is worth neither. */
ActionLayout layOutActionPage (juce::Rectangle<int> bounds,
                               float headerSize, float bodySize,
                               float potSizeScale,
                               juce::Rectangle<int> gridReference);

}
