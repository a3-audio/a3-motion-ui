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

#include "MixerLayout.hh"

#include <a3-motion-ui/components/ControllerLayout.hh>

namespace a3
{

namespace
{
/** How much of the overlay's height the filter's own row takes.
 *
 *  A sixth, which is what the row across the foot has always had — the master
 *  has left it for a column of its own and the three that remain simply have
 *  the width to themselves. The filter is neither channel nor master, and it
 *  is the one global control a hand reaches for constantly mid-set: three
 *  controls across the full width are the biggest targets on the page. Never
 *  less than a row's floor, so on a short screen the row keeps a target
 *  rather than a share. */
constexpr float filterRowOfHeight = 1.f / 6.f;

/** How much of that row's width the filter's three fields take.
 *
 *  Half, which is what "half the width each" comes to: every field gets half
 *  of the third of the row it used to have. The filter is three controls, not
 *  a band across the foot of the page, and at half a cell each the row reads
 *  like a strip's key row -- which is what it is, one deck-wide control
 *  standing for all four. The block is centred in what it no longer fills:
 *  hung on one edge under five columns it would read as a row that ran out
 *  rather than as one that was placed. */
constexpr float filterFieldsOfRowWidth = 1.f / 2.f;

/** The master's share of the width, taken before the channels break.
 *
 *  A fifth, because the page is five strips and they should read as five of
 *  one kind. Never less than a row's floor, so on a short screen the column
 *  keeps a target rather than a share.
 *
 *  **Taken first, rather than by breaking the width into five.**
 *  breakColumns halves its count, so five would come out as two columns —
 *  and two columns of five is three rows, six cells for five strips, with
 *  cellIn admitting an index that stands for nothing. The master is not a
 *  channel anyway. What taking it first buys on a narrow screen is that the
 *  four fall into a 2x2 block with the master standing beside them, rather
 *  than a five-way split coming apart. */
constexpr float masterColumnOfWidth = 1.f / 5.f;

/** The air a control leaves inside its cell.
 *
 *  A fraction of the cell rather than a number of pixels, so it keeps its
 *  proportion as the overlay grows. What it buys is that a strip reads as a
 *  block of controls with its neighbours beside it rather than as one
 *  continuous field — the alternative is a drawn line, and a line here would
 *  be one more mark over a sphere that is already showing through. */
constexpr float controlGapOfCell = 1.f / 24.f;

/** How tall a row has to be to be worth drawing: a fingertip, and the pot the
 *  skin asks for if that is larger. The fingertip is the floor for anything
 *  hit in a hurry; the pot is the performer's own setting, and a row that
 *  clipped it would answer a skin change by drawing half a knob. */
int
rowFloor (ControlMetrics metrics)
{
  return juce::jmax (fingertipSize, metrics.knobDiam);
}

int
gapIn (juce::Rectangle<int> cell)
{
  return juce::jmax (1, juce::roundToInt (
                            static_cast<float> (juce::jmin (
                                cell.getWidth (), cell.getHeight ()))
                            * controlGapOfCell));
}

/** `count` cells across `row`, each with its own air around it.
 *
 *  Measured from the left with an integer cell width, the way cellIn() and
 *  ClipSettingsLayout's colW are: taken from the right the remainder lands
 *  between the cells instead of against the edge, and a row that nearly lines
 *  up reads as a mistake where one that lines up exactly reads as structure. */
juce::Rectangle<int>
cellAcross (juce::Rectangle<int> row, int count, int index)
{
  if (row.isEmpty () || count <= 0)
    return {};

  auto const cellW = row.getWidth () / count;
  auto const cell = juce::Rectangle<int> (row.getX () + cellW * index,
                                          row.getY (), cellW,
                                          row.getHeight ());

  return cell.reduced (gapIn (cell));
}

/** How many rows a strip has, against the seven controls standing in them.
 *
 *  Six. PFL and FX are the only two of the seven that are pressed rather than
 *  turned, and a key wants a fingertip rather than a whole row of a column --
 *  so they share the last one at half its width each, and the height that
 *  frees goes to the six rows above rather than to air at the foot.
 *
 *  **The row count is therefore no longer the control count**, which is why
 *  the two are separate names. Dividing the strip by the control count would
 *  step six rows down a grid made for seven and leave the seventh empty. */
constexpr int numMixerRows = numMixerControls - 1;

/** How many fields stand across the row the two keys share. */
constexpr int fieldsInTheKeyRow = 2;

/** Where a channel control stands: which row, and which field across it.
 *
 *  One answer rather than three functions, and written the way
 *  rowForMasterControl below is -- the table's order shifted by the rows
 *  collapsed above it, rather than a second table saying where each control
 *  goes. Two such tables is how a control ends up in one place on the overlay
 *  and another on the tab. */
struct ControlCell
{
  int row;
  int field;
  int fields;
};

constexpr ControlCell
cellForMixerControl (MixerControl control)
{
  static_assert (controlSlot (MixerControl::Fx) == numMixerControls - 1,
                 "FX joins PFL's row by being the control after it, so it has "
                 "to be the last one in the table");

  // The field below is counted from PFL and is bounded by nothing else: a
  // table that separated the two keys while leaving FX last would hand back a
  // field index past the end of the row it names. MixerControls'
  // TheTogglesAreTheLastTwoAndNothingBefore states the same rule from the
  // table's own side; this is the half of it this arithmetic depends on, said
  // where the arithmetic is.
  static_assert (controlSlot (MixerControl::Pfl)
                     == numMixerControls - fieldsInTheKeyRow,
                 "the two keys share a row, so they have to be the last two "
                 "in the table with nothing standing between them");

  auto const shared = controlSlot (MixerControl::Pfl);
  auto const slot = controlSlot (control);

  // Everything above the keys keeps a row to itself, and is the only field
  // in it.
  if (slot < shared)
    return { slot, 0, 1 };

  return { shared, slot - shared, fieldsInTheKeyRow };
}

/** One of the fields a row is divided into, side by side across it.
 *
 *  No air of its own, exactly as the rows below have none between them: the
 *  gap this page uses is taken once around a whole strip, and a field given
 *  air would be shorter than the rows above it -- which is the equality the
 *  master's column is lined up on. Two keys drawn edge to edge still read as
 *  two, because each carries its own rounded outline.
 *
 *  Measured from the left with an integer field width, for the reason
 *  cellAcross is. */
juce::Rectangle<int>
fieldAcrossRow (juce::Rectangle<int> row, int fields, int index)
{
  if (row.isEmpty () || fields <= 1)
    return row;

  auto const fieldW = row.getWidth () / fields;
  return juce::Rectangle<int> (row.getX () + fieldW * index, row.getY (),
                               fieldW, row.getHeight ());
}

/** One column's rows, top to bottom, and whether they are worth drawing. */
struct StripRows
{
  std::array<juce::Rectangle<int>, numMixerRows> rows;
  bool fits;
};

/** The rows of one strip, top to bottom, every one the same height.
 *
 *  The controls are all of one kind — turned or pressed — so none of them has
 *  a claim on more of the column than its neighbours. A row that was taller
 *  than the ones around it used to be the volume, back when it was thrown
 *  rather than turned and needed the length to travel in.
 *
 *  Because it is a function of the strip's *height* alone, two columns of the
 *  same height come out with the same rows — which is what puts the master's
 *  volume exactly on the channels' line rather than near it, and the reason
 *  this is one function instead of two arrangements that agree today. It
 *  counts rows, never controls, so the two keys sharing one changes the
 *  height for the channels and the master alike. */
StripRows
rowsDownStrip (juce::Rectangle<int> strip, ControlMetrics metrics)
{
  StripRows out{};
  out.fits = !strip.isEmpty ();
  if (strip.isEmpty ())
    return out;

  auto const floor_ = rowFloor (metrics);
  auto const rowH = strip.getHeight () / numMixerRows;

  if (rowH < floor_ || strip.getWidth () < floor_)
    out.fits = false;

  auto y = strip.getY ();
  for (int i = 0; i < numMixerRows; ++i)
    {
      out.rows[static_cast<std::size_t> (i)] = juce::Rectangle<int> (
          strip.getX (), y, strip.getWidth (), rowH);
      y += rowH;
    }

  return out;
}

/** Which of those rows a master control stands in.
 *
 *  The master's five sit on the channels' own row grid, because five levels on
 *  one line is the whole point of standing it beside them: its volume takes
 *  the volume row and the other four fill the rows above it in the table's
 *  order. Which row that is is asked of cellForMixerControl rather than of the
 *  table, so the two columns move together when the channels' rows change.
 *
 *  **The row that leaves is empty on purpose.** It is where a channel's two
 *  keys stand, and the master's output level meters go there — a layout that
 *  filled it with anything else now would only have to be undone. */
constexpr int
rowForMasterControl (MasterControl control)
{
  static_assert (numMasterControls - 1
                     <= cellForMixerControl (MixerControl::Volume).row,
                 "the master's other controls no longer fit above the volume");

  if (control == MasterControl::Volume)
    return cellForMixerControl (MixerControl::Volume).row;

  auto const slot = controlSlot (control);
  return slot < controlSlot (MasterControl::Volume) ? slot : slot - 1;
}

/** The rows of the master's column that no control of its own stands in.
 *
 *  Read off rowForMasterControl rather than written down as "the last one":
 *  which rows are free follows from where the master's five sit, and a number
 *  here would be a second answer to a question that already has one -- exactly
 *  the drift the static_assert above guards against from the other side. It is
 *  what let the block follow the keys into a single row without being told.
 *
 *  Returned as one rectangle because that is what the meters want: five thin
 *  bars in a single block, the way a multi-channel meter is drawn, rather
 *  than five widgets sharing out the rows. */
juce::Rectangle<int>
rowsNoMasterControlStandsIn (StripRows const &rows)
{
  std::array<bool, numMixerRows> claimed{};
  for (auto const control : masterControlOrder)
    claimed[static_cast<std::size_t> (rowForMasterControl (control))] = true;

  juce::Rectangle<int> block;
  for (int i = 0; i < numMixerRows; ++i)
    if (!claimed[static_cast<std::size_t> (i)])
      block = block.getUnion (rows.rows[static_cast<std::size_t> (i)]);

  return block;
}

/** How many pots the bar's tab lays across its first row: everything that is
 *  turned rather than pressed. */
constexpr int potsAcrossTheBarsStrip = numMixerControls - fieldsInTheKeyRow;

/** How many rows the tab has: the pots across the top, the two keys under
 *  them.
 *
 *  The maintainer asked for it after using the tab on the device. Seven cells
 *  across one band leaves the pots a width nobody wants to aim a knob at,
 *  where two rows leave the pots the width they need and the keys the width a
 *  key needs anyway — the same trade the overlay's strip makes down its
 *  column, read the other way round. */
constexpr int rowsDownTheBarsStrip = 2;

/** How many columns the tab lays across: the five pots, and the meter
 *  standing after them.
 *
 *  The meter takes a column the width of a control rather than the near-third
 *  of the width REAPER's measurement gives it. That measurement is of a
 *  *portrait* strip, where a third of 92 px is a bar far taller than it is
 *  wide; a third of a landscape band would be a block wider than it is tall,
 *  which is not a meter. What carries across from REAPER is the arrangement —
 *  a full-height column beside the controls — and here that is one column of
 *  six, at the far right rather than the left because the band is read left to
 *  right and the level is read at the end of it. */
constexpr int columnsAcrossTheBarsStrip = potsAcrossTheBarsStrip + 1;
}

MixerLayout
layOutMixerOverlay (juce::Rectangle<int> area, ControlMetrics metrics)
{
  MixerLayout out{};

  if (area.isEmpty ())
    return out;

  auto const floor_ = rowFloor (metrics);

  // The filter's row comes off the bottom first: it belongs to neither the
  // channels nor the master, its height does not depend on how the strips
  // break, and taking it first is what lets the columns be laid out in what
  // is left rather than in a guess at it.
  auto columnArea = area;
  auto const filterRow = columnArea.removeFromBottom (juce::jmax (
      floor_, juce::roundToInt (static_cast<float> (area.getHeight ())
                                * filterRowOfHeight)));

  // Half-width fields side by side rather than the full width in thirds, and
  // centred in the row they no longer fill. See filterFieldsOfRowWidth.
  auto const filterFields = filterRow.withSizeKeepingCentre (
      juce::roundToInt (static_cast<float> (filterRow.getWidth ())
                        * filterFieldsOfRowWidth),
      filterRow.getHeight ());

  for (int i = 0; i < numFilterControls; ++i)
    out.filter[static_cast<std::size_t> (i)]
        = cellAcross (filterFields, numFilterControls, i);

  // Then the master's column off the right, before the four break in what is
  // left of the width. The count below stays numChannelsInitial for that
  // reason: the master is a strip, not a channel, and it has already taken
  // its share.
  auto stripArea = columnArea;
  auto const masterColumn = stripArea.removeFromRight (juce::jmax (
      floor_, juce::roundToInt (static_cast<float> (columnArea.getWidth ())
                                * masterColumnOfWidth)));

  // Twice minimumMotionHeight as the height threshold, not once: a strip
  // carries seven controls down its length, and one only minimumMotionHeight
  // tall cannot hold them at fingertip size. That is the coarse rule the
  // break is made on; whether the rows *actually* clear their floor is asked
  // below, where the row height is known.
  out.strips = breakColumns (stripArea, numChannelsInitial,
                             static_cast<int> (minimumChannelWidth),
                             static_cast<int> (minimumMotionHeight * 2.f));

  // The arrangement holds exactly the four channels, in every break of them:
  // four columns, two by two, or one by four. Six cells would mean the master
  // had been counted in, and cellIn would then admit an index for a strip
  // that is not there.
  jassert (out.strips.columns * out.strips.rows == numChannelsInitial);

  // One gap for all five columns, taken from a channel's cell rather than
  // from each column's own size. The gap is what sets a column's top edge,
  // and five columns whose gaps differed by a pixel would put the five
  // levels on five lines.
  auto const firstCell = cellIn (stripArea, out.strips, 0);
  auto const gap = gapIn (firstCell.isEmpty () ? masterColumn : firstCell);

  auto rowsFit = true;

  for (int channel = 0; channel < numChannelsInitial; ++channel)
    {
      auto const cell = cellIn (stripArea, out.strips, channel);
      if (cell.isEmpty ())
        {
          rowsFit = false;
          continue;
        }

      // The meter takes its column off the left of the whole strip, and the
      // rows are then stepped down what is left -- so the seven controls all
      // narrow by the same amount and none of them is drawn across a meter
      // laid under it. Split before the rows rather than after, because the
      // meter's job is to be the strip's full height and a cut taken out of
      // one row could never give it that.
      //
      // The master's column is deliberately not split: it has no input meter,
      // its output block stands in the rows its own controls leave, and its
      // five have to keep the channels' row *heights* for the five levels to
      // stand on one line. Splitting the width does not touch those heights,
      // which is what lets the two arrangements stay one function.
      auto const split = splitStripForMeter (cell.reduced (gap));

      auto const rows = rowsDownStrip (split.controls, metrics);
      rowsFit = rowsFit && rows.fits;
      out.channelMeter[static_cast<std::size_t> (channel)] = split.meter;

      for (int i = 0; i < numMixerControls; ++i)
        {
          auto const index = static_cast<std::size_t> (i);
          auto const cell = cellForMixerControl (mixerControlOrder[index]);
          auto const control = fieldAcrossRow (
              rows.rows[static_cast<std::size_t> (cell.row)], cell.fields,
              cell.field);

          out.controls[static_cast<std::size_t> (channel)][index] = control;

          // The trap the shared row brings: a column wide enough to carry a
          // row can still be too narrow to carry two fields across it, and a
          // key under a fingertip has to make the page say so rather than
          // shrink quietly. rowsDownStrip cannot ask this -- it measures the
          // row, and the master's column has no field to split.
          if (control.getWidth () < floor_)
            rowsFit = false;
        }

      // A strip so narrow that the meter leaves the controls nothing is a
      // strip that cannot be operated, and the page says so rather than
      // drawing controls nobody can land on.
      if (split.meter.isEmpty () || split.controls.getWidth () < floor_)
        rowsFit = false;
    }

  // The master stands on the same grid, its column running the full height
  // the strips have: while the four are side by side that is exactly a
  // channel's height, so the two arrangements are one and its volume lands on
  // their line. Broken two by two they cannot both be lined up with, and the
  // master keeps the height rather than half of it.
  auto const masterRows = rowsDownStrip (masterColumn.reduced (gap), metrics);
  rowsFit = rowsFit && masterRows.fits;

  // The output meters take the rows the master's own controls leave, which is
  // what Task 10 kept them for. Not part of `fits`: a block of meters is read
  // rather than touched, so a page whose meters came out too small to be
  // useful is still a page that can be operated -- and refusing to draw the
  // mixer over it would take away the controls as well.
  auto const meters
      = outputMeterBlock (rowsNoMasterControlStandsIn (masterRows), metrics);
  out.outputMeters = meters.bars;
  out.outputMeterCaption = meters.caption;

  for (int i = 0; i < numMasterControls; ++i)
    {
      auto const index = static_cast<std::size_t> (i);
      out.master[index] = masterRows.rows[static_cast<std::size_t> (
          rowForMasterControl (masterControlOrder[index]))];
    }

  out.fits = out.strips.fits && rowsFit;
  return out;
}

MixerLayout
layOutMixerStrip (juce::Rectangle<int> area, ControlMetrics metrics)
{
  MixerLayout out{};

  if (area.isEmpty ())
    return out;

  // Nothing to break: the tab carries one strip by definition, so the single
  // column is a statement rather than a result. Written into the same field
  // the overlay fills so a component can read either arrangement the same
  // way.
  out.strips = { 1, 1, true };

  auto const floor_ = rowFloor (metrics);
  auto cellsFit = true;

  // The tab's strip carries the channel's meter the way the overlay's does --
  // its own full-length column beside the controls, spanning every row of them
  // -- but at the far right of the band rather than before it. A band is read
  // left to right and the level is read at the end of it; the overlay's strips
  // are columns and the level is read before them.
  out.channelMeter[0] = cellAcross (area, columnsAcrossTheBarsStrip,
                                    potsAcrossTheBarsStrip);
  if (out.channelMeter[0].isEmpty ())
    cellsFit = false;

  // What is left of the width is stepped from the left with the same integer
  // cell width the meter's column was taken with, so the pots stand on the
  // grid the meter is on rather than on one of their own.
  auto const cellW = area.getWidth () / columnsAcrossTheBarsStrip;
  auto const controlArea = area.withWidth (cellW * potsAcrossTheBarsStrip);

  // Two rows of the same height: the pots across the top, the keys under them.
  // Stepped from the top with an integer row height, so the remainder lands
  // against the bottom edge rather than between the two rows.
  auto const rowH = controlArea.getHeight () / rowsDownTheBarsStrip;
  auto const potRow = controlArea.withHeight (rowH);
  auto const keyRow = potRow.withY (potRow.getY () + rowH);

  // Across in the table's order, the way the overlay goes down it -- the same
  // list read the other way rather than a second list that agrees with it, and
  // the same split of it into what is turned and what is pressed.
  for (int i = 0; i < numMixerControls; ++i)
    {
      auto const index = static_cast<std::size_t> (i);
      auto const control = mixerControlOrder[index];
      auto const where = cellForMixerControl (control);
      auto const cell
          = mixerControlIsAToggle (control)
                ? cellAcross (keyRow, where.fields, where.field)
                : cellAcross (potRow, potsAcrossTheBarsStrip, i);

      out.controls[0][index] = cell;

      if (cell.getWidth () < floor_ || cell.getHeight () < floor_)
        cellsFit = false;
    }

  out.fits = cellsFit;
  return out;
}

}
