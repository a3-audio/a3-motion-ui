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

#include <a3-motion-ui/WindowReport.hh>

#include <gtest/gtest.h>

using namespace a3;

// A cold start comes back wrong roughly once a week and cannot be reproduced
// from a running system. Measured on 2026-09-18 from the recorder's frames:
// everything was drawn at exactly twice the size -- "BPM 126.2" 121 px wide
// against 61 px -- so the window was laid out at half the size it has and
// magnified. What decides that is the display scale JUCE reads at the moment
// the window is made, and nothing wrote it down.
TEST (WindowReport, ItSaysWhatTheWindowAndTheScreenWere)
{
  auto const line = windowReportLine ({ 0, 0, 768, 1024 }, 768, 1024, 121.0,
                                      2.0);

  EXPECT_TRUE (line.contains ("window 768x1024+0+0")) << line;
  EXPECT_TRUE (line.contains ("screen 768x1024")) << line;
  EXPECT_TRUE (line.contains ("dpi 121")) << line;
  EXPECT_TRUE (line.contains ("scale 2")) << line;
}

// The one that matters is read at a glance: a scale that is not one is the
// fault, so it says so in words.
TEST (WindowReport, AScaleThatIsNotOneIsCalledOut)
{
  EXPECT_TRUE (windowReportLine ({ 0, 0, 768, 1024 }, 768, 1024, 184.0, 2.0)
                   .contains ("SCALED"));
  EXPECT_FALSE (windowReportLine ({ 0, 0, 768, 1024 }, 768, 1024, 121.0, 1.0)
                    .contains ("SCALED"));
}

// And the window the layout was built for, which is what a wrong scale
// actually costs: half the panel.
TEST (WindowReport, ItSaysTheSizeTheLayoutIsBuiltFor)
{
  auto const line = windowReportLine ({ 0, 0, 768, 1024 }, 768, 1024, 184.0,
                                      2.0);
  EXPECT_TRUE (line.contains ("logical 384x512")) << line;
}
