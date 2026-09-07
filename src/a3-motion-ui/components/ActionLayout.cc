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

#include "ActionLayout.hh"

#include <a3-motion-ui/components/ClipSettingsCaptions.hh>
#include <a3-motion-ui/components/ControllerLayout.hh>

namespace a3
{

ActionLayout
layOutActionPage (juce::Rectangle<int> bounds, float headerSize,
                  float bodySize, float potSizeScale,
                  juce::Rectangle<int> gridReference)
{
  ActionLayout out;

  auto const padding = juce::jmax (4, bounds.getHeight () / 40);
  auto content = bounds.reduced (padding);

  auto const gap = juce::jmax (4, content.getWidth () / 60);

  // Generous, because the page is: the knob takes the room a whole page can
  // give it rather than the sliver a third of a bar can. Worked out first --
  // the card is sized to hold the grid, not the other way round.
  auto const knobDiam = knobDiameterForFont (bodySize, potSizeScale);
  auto const gridKnob = static_cast<int> (knobDiam * 1.2f);

  // A cell gives a pixel back on each side so the knobs do not touch, so the
  // floor a cell is measured against is two over the fingertip's.
  auto const cellFloor = fingertipSize + 2;

  // Rows come from the global strip when it offers them, so the two blocks
  // read across at one height. Only when they fit and only when they carry a
  // target: lining up is worth having, and worth losing to a knob a finger
  // can actually land on.
  auto const referenceFits
      = !gridReference.isEmpty ()
        && gridReference.getY () >= content.getY ()
        && gridReference.getBottom () <= content.getBottom ()
        && gridReference.getHeight () / ActionLayout::numRows >= cellFloor;

  auto const rowH
      = referenceFits
            ? gridReference.getHeight () / ActionLayout::numRows
            : juce::jmax (cellFloor,
                          juce::jmin (content.getHeight () / 4,
                                      static_cast<int> (gridKnob * 1.35f)));

  // The card at the right, wide enough for a gutter and three knob columns.
  auto const labelW = juce::jmax (fingertipSize, rowH);
  auto const colW = juce::jmax (cellFloor,
                                juce::jmax (gridKnob + 2,
                                            static_cast<int> (gridKnob * 1.35f)));
  auto const gridW = labelW + 3 * colW;
  auto const cardW = juce::jmin (content.getWidth () * 2 / 3,
                                 gridW + 2 * juce::jmax (2, gridW / 40) + 6);

  out.card = content.removeFromRight (cardW);
  content.removeFromRight (gap);

  auto grid = sectionContentBounds (out.card);

  // The card is named before anything stands on it.
  out.cardCaption = grid.removeFromTop (
      juce::jmin (grid.getHeight () / 5, static_cast<int> (headerSize * 1.4f)));
  grid.removeFromTop (gap / 2);

  // Where the rows begin: at the top of what the card has left, so the knobs
  // are the first thing under the name and the room below them is free for
  // the key that fires the thing. They used to be centred, which put them
  // halfway down a card with nothing under them.
  //
  // The global strip's rows still win when it offers a set that fits, so the
  // two blocks read across at one height -- but never lower than the top,
  // which is what "centred" was really costing.
  auto const blockH = ActionLayout::numRows * rowH;
  auto const top = referenceFits
                       ? juce::jmax (grid.getY (), gridReference.getY ())
                       : grid.getY ();

  auto const indent = juce::jmax (0, (grid.getWidth () - gridW) / 2);

  for (int row = 0; row < ActionLayout::numRows; ++row)
    {
      auto band = juce::Rectangle<int>{ grid.getX () + indent,
                                        top + row * rowH, gridW, rowH };
      out.rows[static_cast<size_t> (row)] = band;

      out.rowLabels[static_cast<size_t> (row)] = band.removeFromLeft (labelW);
      for (int i = 0; i < 3; ++i)
        out.controls[static_cast<size_t> (row * 3 + i)]
            = band.removeFromLeft (colW).reduced (1);
    }

  // And the key that fires it, in what the knobs left. Fat: it is the one
  // thing on this page that happens now.
  {
    auto below = grid.withTop (top + blockH + gap);
    if (below.getHeight () >= fingertipSize)
      out.fireButton
          = below.removeFromTop (
                juce::jmin (below.getHeight (), rowH * 3 / 2))
                .withSizeKeepingCentre (
                    juce::jmin (gridW, below.getWidth ()),
                    juce::jmin (below.getHeight (), rowH * 3 / 2));
  }

  // What is left is the action's: its name and mode on one line, the script
  // it carries under them.
  auto const nameH = juce::jmax (fingertipSize,
                                 static_cast<int> (headerSize * 2.f));
  auto nameRow = content.removeFromTop (juce::jmin (nameH, content.getHeight ()));
  out.actModeField = nameRow.removeFromRight (
      juce::jmin (labelW + colW, nameRow.getWidth () / 3));
  nameRow.removeFromRight (gap);
  out.actionField = nameRow;

  content.removeFromTop (gap);
  out.scriptField = content;

  // Two keys at the foot, taken off the text rather than drawn across it.
  {
    auto keys = out.scriptField;
    auto const keyH = juce::jmax (
        fingertipSize, juce::jmin (keys.getHeight () / 4,
                                   static_cast<int> (headerSize * 2.f)));

    auto row = keys.removeFromBottom (juce::jmin (keyH, keys.getHeight ()));
    out.scriptTextField = keys.withTrimmedBottom (gap / 2);

    auto const keyGap = juce::jmax (2, row.getWidth () / 40);
    auto const keyW = juce::jmax (fingertipSize, (row.getWidth () - keyGap) / 2);

    out.cancelButton = row.removeFromRight (keyW);
    row.removeFromRight (keyGap);
    out.saveButton = row.removeFromRight (
        juce::jmin (keyW, juce::jmax (fingertipSize, row.getWidth ())));
  }

  out.actionListRowHeight
      = juce::jmax (fingertipSize, out.scriptField.getHeight () / 7);
  out.actionListArea = out.scriptField;

  auto const columnGap = juce::jmax (2, colW / 20);
  out.metrics = ControlMetrics{
    knobDiam,
    sharedCaptionSize (bodySize, colW, columnGap, rowH),
    bodySize,
  };

  return out;
}

int
actionListVisibleRows (ActionLayout const &layout)
{
  auto const rowH = juce::jmax (1, layout.actionListRowHeight);
  return juce::jmax (1, layout.actionListArea.getHeight () / rowH);
}

}
