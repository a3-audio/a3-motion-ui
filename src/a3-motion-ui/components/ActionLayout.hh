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
 *  Two halves. On the right, a card of nine knobs -- three envelopes by
 *  attack, decay and ceiling -- laid out and positioned to read straight
 *  across into the global strip's channel grid beside it. On the left, which
 *  action this slot fires and the script that action carries.
 */
struct ActionLayout
{
  /** Which action clip the ACT key fires on this slot, named. Chosen from the
   *  list it opens, so what stands here is a reading as much as a control. */
  juce::Rectangle<int> actionField;

  /** The mode, beside the name rather than at the end of a row.
   *
   *  It says what a press does to all three envelopes, so it belongs to none
   *  of them -- and it is a state you want to find without reading, which the
   *  tail of a row is not. */
  juce::Rectangle<int> actModeField;

  /** The script the action carries, under its name and taking whatever the
   *  card leaves. */
  juce::Rectangle<int> scriptField;

  /** The text of the script, which is the editor less the two keys at its
   *  foot. Kept apart so a line is never drawn under a key. */
  juce::Rectangle<int> scriptTextField;
  /** Keep what was typed, or throw it away. Typing used to write on every
   *  keystroke -- fine for a file, wrong for a person, who needs to be able
   *  to try a line and take it back. */
  juce::Rectangle<int> saveButton;
  juce::Rectangle<int> cancelButton;

  /** Where the action field's list opens: over the script, which is what it
   *  replaces for a moment. It cannot open outside the bar -- the sphere's GL
   *  context composites above anything drawn over it -- and it must not open
   *  over the knobs, which are what you are about to set. */
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

  /** Fires the action on the shown slot, under the knobs it sets.
   *
   *  Fat on purpose: it is the one thing on this page that happens *now*,
   *  and it is pressed mid-set with one hand while the other is on the
   *  crossfader. Everything else here is preparation. */
  juce::Rectangle<int> fireButton;

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
