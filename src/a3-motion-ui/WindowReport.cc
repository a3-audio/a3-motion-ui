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

#include "WindowReport.hh"

namespace a3
{

juce::String
windowReportLine (juce::Rectangle<int> window, int screenWidth,
                  int screenHeight, double dpi, double scale)
{
  auto const logicalWidth = scale > 0.0
                                ? juce::roundToInt (window.getWidth () / scale)
                                : window.getWidth ();
  auto const logicalHeight
      = scale > 0.0 ? juce::roundToInt (window.getHeight () / scale)
                    : window.getHeight ();

  juce::String line;
  line << "window " << window.getWidth () << "x" << window.getHeight () << "+"
       << window.getX () << "+" << window.getY () << "  screen " << screenWidth
       << "x" << screenHeight << "  dpi " << juce::String (dpi, 1)
       << "  scale " << juce::String (scale, 2) << "  logical " << logicalWidth
       << "x" << logicalHeight;

  // Said in a word, because this is the line somebody reads after a bad start
  // and the number is easy to skim past.
  if (std::abs (scale - 1.0) > 0.01)
    line << "  SCALED";

  return line;
}

void
logWindowReport (juce::Rectangle<int> window)
{
  auto const &displays = juce::Desktop::getInstance ().getDisplays ();
  auto const *display = displays.getPrimaryDisplay ();

  auto const area = display != nullptr ? display->totalArea
                                       : juce::Rectangle<int>{};

  juce::Logger::writeToLog (
      "layout: "
      + windowReportLine (window, area.getWidth (), area.getHeight (),
                          display != nullptr ? display->dpi : 0.0,
                          display != nullptr ? display->scale : 0.0));
}

}
