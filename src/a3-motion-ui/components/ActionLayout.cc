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
#include <a3-motion-ui/theme/Theme.hh>

namespace a3
{

namespace
{
constexpr int motionColumns = ActionLayout::motionColumns;
constexpr int motionRows = ActionLayout::motionRows;

/** The MOTION tile in what the card has under its name: four across in
 *  motionParamOrder, rows as tall as the AUDIO tile's knobs want (`rowCap`)
 *  or as the card allows, whichever is less -- smaller than a fingertip
 *  rather than off the page. */
void
layOutMotionTile (ActionLayout &out, juce::Rectangle<int> grid, int rowCap)
{
  auto const rowH = juce::jmin (rowCap, grid.getHeight () / motionRows);
  auto const colW = grid.getWidth () / motionColumns;

  for (int at = 0; at < numMotionParams; ++at)
    {
      auto const column = at % motionColumns;
      auto const row = at / motionColumns;
      auto const cell = juce::Rectangle<int>{ grid.getX () + column * colW,
                                              grid.getY () + row * rowH, colW,
                                              rowH };
      out.motionControls[static_cast<size_t> (
          motionParamOrder[static_cast<size_t> (at)])]
          = cell.reduced (juce::roundToInt (theme ().paddingHair));
    }
}
}

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
  // Wide enough for either tile: AUDIO's gutter and three columns, or
  // MOTION's four -- one card for both, so switching tiles moves nothing
  // left of it.
  auto const gridW = juce::jmax (labelW + 3 * colW, motionColumns * colW);
  auto const cardW = juce::jmin (content.getWidth () * 2 / 3,
                                 gridW + 2 * juce::jmax (2, gridW / 40) + 6);

  out.card = content.removeFromRight (cardW);
  content.removeFromRight (gap);

  auto grid = sectionContentBounds (out.card);

  // The card's two tiles are tabs along its top (maintainer, 2026-09-28):
  // the name of what the card shows is the key that shows it. Taller than a
  // caption was, because it is touched: a fingertip, like every key.
  out.cardCaption = grid.removeFromTop (juce::jmax (
      fingertipSize, static_cast<int> (headerSize * 1.4f)));
  {
    auto tabs = out.cardCaption;
    auto const hair = juce::roundToInt (theme ().paddingHair);
    out.audioKey = tabs.removeFromLeft (tabs.getWidth () / 2).reduced (hair, 0);
    out.motionKey = tabs.reduced (hair, 0);
  }
  grid.removeFromTop (gap / 2);

  // Where the rows begin: at the top of what the card has left, so the knobs
  // are the first thing under the name and the room below them is free for
  // the key that fires the thing. They used to be centred, which put them
  // halfway down a card with nothing under them.
  //
  // The global strip's rows still win when it offers a set that fits, so the
  // two blocks read across at one height -- but never lower than the top,
  // which is what "centred" was really costing.
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
            = band.removeFromLeft (colW).reduced (juce::roundToInt (theme ().paddingHair));
    }

  layOutMotionTile (out, grid.withX (grid.getX () + indent).withWidth (gridW),
                    juce::jmax (cellFloor, static_cast<int> (gridKnob * 1.35f)));

  // Left of the card, left to right (2026-09-28): the six buttons, the list
  // the chosen one is assigned from, and a column of keys -- EDIT, then the
  // chosen button's mode. No ACT key: the six fields fire their own actions.
  auto const keyH = juce::jmax (fingertipSize,
                                static_cast<int> (headerSize * 2.f));
  auto keys = content.removeFromRight (
      juce::jmax (fingertipSize * 3 / 2, content.getWidth () / 7));
  content.removeFromRight (gap);
  out.editButton = keys.removeFromTop (juce::jmin (keyH, keys.getHeight ()));
  keys.removeFromTop (gap);
  out.actModeField = keys.removeFromTop (juce::jmin (keyH, keys.getHeight ()));
  keys.removeFromTop (gap);
  // What comes after the accent, under the mode: both say what a press of
  // this button does over time.
  out.afterKey = keys.removeFromTop (juce::jmin (keyH, keys.getHeight ()));

  // Three rows of two, as the pads stand on the panel: A1 A2 / A3 A4 / A5 A6.
  // A quarter each, so an action's name fits under its number; the list
  // keeps what is left, which is still more than a row of names needs.
  auto const fieldW = juce::jmax (fingertipSize, content.getWidth () / 4);
  auto fields = content.removeFromLeft (2 * fieldW + gap);
  content.removeFromLeft (gap);
  auto const fieldH = (fields.getHeight () - 2 * gap) / 3;
  for (int row = 0; row < 3; ++row)
    {
      auto band = fields.withTrimmedTop (row * (fieldH + gap))
                      .withHeight (fieldH);
      out.actionFields[static_cast<size_t> (row * 2)]
          = band.removeFromLeft (fieldW);
      band.removeFromLeft (gap);
      out.actionFields[static_cast<size_t> (row * 2 + 1)] = band;
    }

  out.actionListRowHeight
      = juce::jmax (fingertipSize, content.getHeight () / 7);
  out.actionListArea = content;

  auto const columnGap = juce::jmax (2, colW / 20);
  out.metrics = ControlMetrics{
    knobDiam,
    sharedCaptionSize (bodySize, colW, columnGap, rowH),
    bodySize,
  };

  return out;
}

ActionFieldParts
actionFieldParts (juce::Rectangle<int> field)
{
  ActionFieldParts out;
  auto inner = field.reduced (field.getHeight () / 8);
  auto numberRow = inner.removeFromTop (inner.getHeight () / 2);

  // A square as tall as the number's row, in its right end: a letter read at
  // a glance, and never over the number or the name.
  out.modeBadge = numberRow.removeFromRight (numberRow.getHeight ());
  numberRow.removeFromRight (numberRow.getHeight () / 4);
  out.number = numberRow;
  out.name = inner;
  return out;
}

std::vector<juce::Rectangle<int> >
actionListRows (ActionLayout const &layout)
{
  auto const area = layout.actionListArea;
  auto const rowH = juce::jmax (1, layout.actionListRowHeight);
  std::vector<juce::Rectangle<int> > rows;
  for (int row = 0; row < actionListVisibleRows (layout); ++row)
    rows.push_back (area.withY (area.getY () + row * rowH).withHeight (rowH));
  return rows;
}

int
actionListVisibleRows (ActionLayout const &layout)
{
  auto const rowH = juce::jmax (1, layout.actionListRowHeight);
  return juce::jmax (1, layout.actionListArea.getHeight () / rowH);
}

}
