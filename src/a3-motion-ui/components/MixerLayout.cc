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

#include <a3-motion-ui/components/BarFader.hh>
#include <a3-motion-ui/components/ControllerLayout.hh>

namespace a3
{

namespace
{
/** How much of the overlay's height the summing row takes.
 *
 *  A sixth, because the eight controls in it are the ones a hand reaches for
 *  once a set is running — the master and the filter — while the four strips
 *  above carry seven controls each and are what the overlay is for. Never
 *  less than a row's floor, so on a short screen the row keeps a target
 *  rather than a share. */
constexpr float summingRowOfHeight = 1.f / 6.f;

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
}

MixerLayout
layOutMixerOverlay (juce::Rectangle<int> area, ControlMetrics metrics)
{
  MixerLayout out{};

  if (area.isEmpty ())
    return out;

  auto const floor_ = rowFloor (metrics);

  // The summing row comes off the bottom first: it is the one part of the
  // overlay whose size does not depend on how the strips break, and taking it
  // first is what lets the strips be laid out in whatever is left rather than
  // in a guess at it.
  auto stripArea = area;
  auto const summing = stripArea.removeFromBottom (juce::jmax (
      floor_, juce::roundToInt (static_cast<float> (area.getHeight ())
                                * summingRowOfHeight)));

  auto const numSumming = numMasterControls + numFilterControls;
  for (int i = 0; i < numMasterControls; ++i)
    out.master[static_cast<std::size_t> (i)]
        = cellAcross (summing, numSumming, i);
  for (int i = 0; i < numFilterControls; ++i)
    out.filter[static_cast<std::size_t> (i)]
        = cellAcross (summing, numSumming, numMasterControls + i);

  // Twice minimumMotionHeight as the height threshold, not once: a strip
  // carries seven controls down its length, and one only minimumMotionHeight
  // tall cannot hold them at fingertip size. That is the coarse rule the
  // break is made on; whether the rows *actually* clear their floor is asked
  // below, where the row height is known.
  out.strips = breakColumns (stripArea, numChannelsInitial,
                             static_cast<int> (minimumChannelWidth),
                             static_cast<int> (minimumMotionHeight * 2.f));

  auto rowsFit = true;

  for (int channel = 0; channel < numChannelsInitial; ++channel)
    {
      auto const cell = cellIn (stripArea, out.strips, channel);
      if (cell.isEmpty ())
        {
          rowsFit = false;
          continue;
        }

      auto const strip = cell.reduced (gapIn (cell));

      // The volume is the one control in the strip a hand throws rather than
      // turns -- mixerControlIsAFader() is where that is decided, once, for
      // this page and the bar's -- so it is given the height a throw needs
      // and the other six share what is left. Equal rows put the fader in a
      // cell wider than it was tall, where its cap fills its own track and
      // the throw comes out a pixel long.
      //
      // The room it may take is what is left once the others have their
      // floor, so a strip can never be so generous to the fader that the
      // controls above it stop being hittable.
      auto const others = numMixerControls - 1;
      auto const volumeRoom = strip.getHeight () - others * floor_;
      auto const throwHeight
          = volumeRoom > 0 ? faderHeightForThrow (strip.getWidth (),
                                                  volumeRoom, metrics)
                           : 0;
      if (throwHeight == 0)
        rowsFit = false;

      // Back to equal rows when the throw cannot be had. `fits` is already
      // false and the overlay draws a sentence instead of a mixer, but the
      // rectangles still have to be sane: a caller that paints anyway must
      // not paint one control over another.
      auto const volumeH = throwHeight > 0
                               ? throwHeight
                               : strip.getHeight () / numMixerControls;
      auto const rowH
          = juce::jmax (0, (strip.getHeight () - volumeH) / others);

      if (rowH < floor_ || strip.getWidth () < floor_)
        rowsFit = false;

      // Down the strip in the table's order, which is what makes
      // MixerControls the authority rather than a list that happens to agree
      // with what is drawn.
      auto y = strip.getY ();
      for (int i = 0; i < numMixerControls; ++i)
        {
          auto const height
              = mixerControlIsAFader (
                    mixerControlOrder[static_cast<std::size_t> (i)])
                    ? volumeH
                    : rowH;

          out.controls[static_cast<std::size_t> (channel)]
                      [static_cast<std::size_t> (i)]
              = juce::Rectangle<int> (strip.getX (), y, strip.getWidth (),
                                      height);
          y += height;
        }
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

  // Across in the table's order, the way the overlay goes down it -- the same
  // list read the other way rather than a second list that agrees with it.
  for (int i = 0; i < numMixerControls; ++i)
    {
      auto const index = static_cast<std::size_t> (i);
      auto const cell = cellAcross (area, numMixerControls, i);
      out.controls[0][index] = cell;

      if (cell.getWidth () < floor_ || cell.getHeight () < floor_)
        cellsFit = false;

      // Which control is a fader is a rule, not something read off the cell
      // -- so the layout has to ask whether the cell it is about to hand the
      // fader can be thrown in at all, and say `fits = false` when it cannot.
      // The alternative is a component quietly drawing a pot there, which is
      // the same control changing shape between the two pages.
      if (mixerControlIsAFader (mixerControlOrder[index])
          && faderHeightForThrow (cell.getWidth (), cell.getHeight (), metrics)
                 == 0)
        cellsFit = false;
    }

  out.fits = cellsFit;
  return out;
}

}
