/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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

#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <a3-motion-ui/components/StatusBar.hh>

using namespace a3;

// The level key on the bar: it takes a tap in FPV only, and the bar paints
// with it in every level and both views.

namespace
{
struct Bar
{
  juce::Value bpm{ 120.0 };
  StatusBar bar{ bpm };
  int taps = 0;

  explicit Bar (AppView view)
  {
    bar.setBounds (0, 0, 768, bar.preferredHeight ());
    bar.setView (view);
    bar.onPilotKeyTapped = [this] { ++taps; };
  }

  void
  tapThePilotKey ()
  {
    auto const at = bar.pilotKeyArea ().getCentre ();
    auto const source = juce::Desktop::getInstance ().getMainMouseSource ();
    juce::MouseEvent const event (
        source, at.toFloat (), {}, juce::MouseInputSource::defaultPressure, 0.f, 0.f, 0.f,
        0.f, &bar, &bar, juce::Time::getCurrentTime (), at.toFloat (),
        juce::Time::getCurrentTime (), 1, false);
    bar.mouseUp (event);
  }
};
}

TEST (StatusBarPilotKey, ATapInFpvStepsTheLevel)
{
  Bar b (AppView::Fpv);
  ASSERT_FALSE (b.bar.pilotKeyArea ().isEmpty ());
  b.tapThePilotKey ();
  EXPECT_EQ (b.taps, 1);
}

TEST (StatusBarPilotKey, FullTakesNoTap)
{
  Bar b (AppView::Full);
  b.tapThePilotKey ();
  EXPECT_EQ (b.taps, 0);
}

TEST (StatusBarPilotKey, TheBarPaintsEveryLevelInBothViews)
{
  for (auto const view : { AppView::Fpv, AppView::Full })
    for (auto const level : { PilotLevel::Off, PilotLevel::Hint, PilotLevel::Fly })
      {
        Bar b (view);
        b.bar.setPilotLevel (level);
        auto const image = b.bar.createComponentSnapshot (b.bar.getLocalBounds ());
        EXPECT_EQ (image.getWidth (), 768) << pilotLevelWord (level);
      }
}
