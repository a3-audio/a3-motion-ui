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
                  float bodySize, float potSizeScale)
{
  ActionLayout out;

  auto const padding = juce::jmax (4, bounds.getHeight () / 40);
  auto content = bounds.reduced (padding);

  // The controls first, from the bottom, so that what is left over goes to the
  // picture. A picture is worth having and worth shrinking; a control that has
  // shrunk below a fingertip is worth nothing at all.
  auto const gap = juce::jmax (4, content.getWidth () / 60);
  auto const columns = static_cast<int> (out.controls.size ());
  auto const cellW = (content.getWidth () - (columns - 1) * gap) / columns;

  // Tall enough to carry a knob and its caption, and never taller than half
  // the page -- past that the picture stops being one.
  auto const wantedH = static_cast<int> (headerSize * 5.f);
  auto const rowH = juce::jlimit (fingertipSize, juce::jmax (fingertipSize,
                                                             content.getHeight () / 2),
                                  wantedH);

  out.controlRow = content.removeFromBottom (juce::jmin (rowH, content.getHeight ()));

  auto row = out.controlRow;
  for (int i = 0; i < columns; ++i)
    {
      out.controls[static_cast<size_t> (i)] = row.removeFromLeft (cellW);
      if (i + 1 < columns)
        row.removeFromLeft (gap);
    }

  // Clear of the row, not touching it.
  content.removeFromBottom (gap);
  out.actionField = content;

  // Generous, because the page is: the knob takes the room a whole page can
  // give it rather than the sliver a third of a bar can.
  auto const knobDiam = juce::jmin (
      knobDiameterForFont (bodySize, potSizeScale) * 2,
      juce::jmax (1, juce::jmin (cellW, out.controlRow.getHeight ()) * 2 / 3));

  auto const columnGap = juce::jmax (2, cellW / 20);
  out.metrics = ControlMetrics{
    knobDiam,
    sharedCaptionSize (bodySize, cellW, columnGap,
                       out.controlRow.getHeight ()),
    bodySize,
  };

  return out;
}

}
