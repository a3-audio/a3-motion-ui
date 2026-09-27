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
 *  step six rows down a grid made for seven and leave the seventh empty.
 *
 *  Nine since 2026-09-26: the channel's 3D, FREQ and Q stand under SEND,
 *  a row each, above the keys. They came out of the bar's 4x3 grid, and the
 *  filter row across the foot gave up its height for them. */
constexpr int numMixerRows = numMixerFaceControls - 1 + numChannelPots;

/** How many fields stand across the row the two keys share. */
constexpr int fieldsInTheKeyRow = 2;

/** Where a channel control stands: which row, and which field across it.
 *
 *  One answer rather than three functions, and written the way
 *  rowForMasterPot below is -- the table's order shifted by the rows
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
  static_assert (faceSlot (MixerControl::Fx) == numMixerFaceControls - 1,
                 "FX joins PFL's row by being the control after it, so it has "
                 "to be the last one in the table");

  // The field below is counted from PFL and is bounded by nothing else: a
  // table that separated the two keys while leaving FX last would hand back a
  // field index past the end of the row it names. MixerControls'
  // TheTogglesAreTheLastTwoAndNothingBefore states the same rule from the
  // table's own side; this is the half of it this arithmetic depends on, said
  // where the arithmetic is.
  static_assert (faceSlot (MixerControl::Pfl)
                     == numMixerFaceControls - fieldsInTheKeyRow,
                 "the two keys share a row, so they have to be the last two "
                 "in the table with nothing standing between them");

  auto const shared = faceSlot (MixerControl::Pfl);
  auto const slot = faceSlot (control);

  // Everything above the keys keeps a row to itself, and is the only field
  // in it. The keys are the last row, under the channel pots.
  if (slot < shared)
    return { slot, 0, 1 };

  return { numMixerRows - 1, slot - shared, fieldsInTheKeyRow };
}

/** The row a channel pot stands in: straight after the turned controls, in
 *  channelPotOrder -- 3D under SEND, then FREQ, then Q. */
constexpr int
rowForChannelPot (int index)
{
  return faceSlot (MixerControl::Pfl) + index;
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

/** How many of the filter's controls are turned and stand under RET in the
 *  OUT column: FX FREQ and FX RES. FX MODE is pressed and stands on the keys'
 *  line, like PFL and FX. */
constexpr int filterPotsInOut = numFilterControls - 1;

/** Which of those rows a master pot stands in: the bottom ones, in order,
 *  above FX FREQ and FX RES and the keys' line.
 *
 *  The master is laid out like a channel -- its meters in the column on the
 *  left, its pots beside them -- and the pots stand at the foot, so the last
 *  of them lands on the row the channels' two keys share and the whole block
 *  reads along the same lines as the four strips. The rows above are left to
 *  the meter column's height. */
constexpr int
rowForMasterPot (int faceSlot)
{
  static_assert (numMasterFaceControls + filterPotsInOut < numMixerRows,
                 "the master's pots and the filter no longer fit into a "
                 "strip's rows");
  return numMixerRows - 1 - filterPotsInOut - numMasterFaceControls + faceSlot;
}

/** How many pots the bar's tab lays across its first row: everything that is
 *  turned rather than pressed. */
constexpr int potsAcrossTheBarsStrip = numMixerFaceControls - fieldsInTheKeyRow;

/** How many rows the tab has: the pots across the top, the channel's 3D,
 *  FREQ and Q under them (since 2026-09-26), the two keys at the foot.
 *
 *  The maintainer asked for it after using the tab on the device. Seven cells
 *  across one band leaves the pots a width nobody wants to aim a knob at,
 *  where two rows leave the pots the width they need and the keys the width a
 *  key needs anyway — the same trade the overlay's strip makes down its
 *  column, read the other way round. */
constexpr int rowsDownTheBarsStrip = 3;

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
  auto const columnArea = area;
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
                             minimumMixerStripWidth,
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

      for (int i = 0; i < numMixerFaceControls; ++i)
        {
          auto const index = static_cast<std::size_t> (i);
          auto const cell = cellForMixerControl (mixerFaceOrder[index]);
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
      for (int i = 0; i < numChannelPots; ++i)
        {
          auto const pot = rows.rows[static_cast<std::size_t> (
              rowForChannelPot (i))];
          out.channelPots[static_cast<std::size_t> (channel)]
                         [static_cast<std::size_t> (i)]
              = pot;
          if (pot.getWidth () < floor_)
            rowsFit = false;
        }

      if (split.meter.isEmpty () || split.controls.getWidth () < floor_)
        rowsFit = false;
    }

  // The master is a strip like the four: its meter column on the left,
  // running the height a channel's does, and its pots beside it on the same
  // row grid. While the four are side by side that is exactly a channel's
  // height, so the pots land on their lines; broken two by two they cannot
  // both be lined up with, and the master keeps the height rather than half.
  auto const masterSplit = splitStripForMeter (masterColumn.reduced (gap));
  auto const masterRows = rowsDownStrip (masterSplit.controls, metrics);
  rowsFit = rowsFit && masterRows.fits && !masterSplit.meter.isEmpty ();

  // The output meters fill the column: the room's sub and speakers, and there
  // will be more of them, which is why they get a column rather than a row.
  // Not part of `fits`: a block of meters too small to read is still a page
  // that can be operated -- refusing to draw it would take the pots too.
  auto const meters = outputMeterBlock (masterSplit.meter, metrics);
  out.outputMeters = meters.bars;
  out.outputMeterCaption = meters.caption;

  // The whole column, not just the bars: it is the master's fader, and its
  // handle travels the length of it with the meters standing in its foot.
  out.masterMeter = masterSplit.meter;

  for (int i = 0; i < numMasterFaceControls; ++i)
    out.master[static_cast<std::size_t> (i)]
        = masterRows.rows[static_cast<std::size_t> (rowForMasterPot (i))];

  // The filter under RET: FX FREQ and FX RES on the next two rows, FX MODE on
  // the keys' line.
  for (int i = 0; i < numFilterControls; ++i)
    {
      auto const control = filterControlOrder[static_cast<std::size_t> (i)];
      auto const row
          = control == FilterControl::Mode ? numMixerRows - 1
          : control == FilterControl::Frequency
              ? rowForMasterPot (numMasterFaceControls)
              : rowForMasterPot (numMasterFaceControls + 1);
      out.filter[static_cast<std::size_t> (i)]
          = masterRows.rows[static_cast<std::size_t> (row)];
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
  // Half that column, at its right-hand edge: a whole one came out twice the
  // width of the overlay's meter, and the fader handle on it read as
  // squashed. The other half is air between the last pot and the meter.
  auto const meterColumn = cellAcross (area, columnsAcrossTheBarsStrip,
                                       potsAcrossTheBarsStrip);
  out.channelMeter[0]
      = meterColumn.withTrimmedLeft (meterColumn.getWidth () / 2);
  if (out.channelMeter[0].isEmpty ())
    cellsFit = false;

  // What is left of the width is stepped from the left with the same integer
  // cell width the meter's column was taken with, so the pots stand on the
  // grid the meter is on rather than on one of their own.
  auto const cellW = area.getWidth () / columnsAcrossTheBarsStrip;
  auto const controlArea = area.withWidth (cellW * potsAcrossTheBarsStrip);

  // Three rows of the same height: the pots across the top, the channel's 3D,
  // FREQ and Q under the first three of them, the keys at the foot. Stepped
  // from the top with an integer row height, so the remainder lands against
  // the bottom edge rather than between the rows.
  auto const rowH = controlArea.getHeight () / rowsDownTheBarsStrip;
  auto const potRow = controlArea.withHeight (rowH);
  auto const channelPotRow = potRow.withY (potRow.getY () + rowH);
  auto const keyRow = channelPotRow.withY (channelPotRow.getY () + rowH);

  // 3D, FREQ and Q one cell in from the left: SEND stands before them in
  // that row since 2026-09-27, the channel's sends side by side.
  for (int i = 0; i < numChannelPots; ++i)
    {
      auto const cell
          = cellAcross (channelPotRow, potsAcrossTheBarsStrip, i + 1);
      out.channelPots[0][static_cast<std::size_t> (i)] = cell;
      if (cell.getWidth () < floor_ || cell.getHeight () < floor_)
        cellsFit = false;
    }

  // Across in the table's order, the way the overlay goes down it -- the same
  // list read the other way rather than a second list that agrees with it, and
  // the same split of it into what is turned and what is pressed.
  for (int i = 0; i < numMixerFaceControls; ++i)
    {
      auto const index = static_cast<std::size_t> (i);
      auto const control = mixerFaceOrder[index];
      auto const where = cellForMixerControl (control);
      auto const cell
          = mixerControlIsAToggle (control)
                ? cellAcross (keyRow, where.fields, where.field)
            : control == MixerControl::FxSend
                ? cellAcross (channelPotRow, potsAcrossTheBarsStrip, 0)
                : cellAcross (potRow, potsAcrossTheBarsStrip, i);

      out.controls[0][index] = cell;

      if (cell.getWidth () < floor_ || cell.getHeight () < floor_)
        cellsFit = false;
    }

  out.fits = cellsFit;
  return out;
}

}
