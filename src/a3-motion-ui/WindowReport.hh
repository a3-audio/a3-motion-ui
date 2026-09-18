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

namespace a3
{

/** One line saying what the window and the screen were, for the log.
 *
 *  A cold start comes back with the whole interface at twice its size about
 *  once a week, and it cannot be reproduced from a running system: whatever
 *  decides it happens in the seconds while i3 turns the panel off and on and
 *  rotates it. Measured on 2026-09-18 from the recorder's frames -- "BPM
 *  126.2" 121 px wide against 61 -- so the window is laid out at half its
 *  size and magnified. That is a display scale of 2, which JUCE takes from
 *  the screen's DPI (round(dpi/96) in juce_XWindowSystem_linux.cpp), and
 *  nothing wrote down what the DPI was at that moment.
 *
 *  Written on every resize, which is where the startup sequence shows. */
juce::String windowReportLine (juce::Rectangle<int> window, int screenWidth,
                               int screenHeight, double dpi, double scale);

/** The same line for the window as it stands, read off the desktop. */
void logWindowReport (juce::Rectangle<int> window);

}
