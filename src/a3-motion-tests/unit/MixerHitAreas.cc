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

#include <gtest/gtest.h>

#include <a3-motion-ui/components/MixerComponent.hh>
#include <a3-motion-ui/components/TouchControl.hh>
#include <a3-motion-ui/components/MixerStripComponent.hh>
#include <a3-motion-ui/theme/Theme.hh>

using namespace a3;

namespace
{
// What is left with a hit area of its own: the keys. Everything that is
// turned is a slider now (PotKnob, VuFader), and a slider answers for itself.
int
keysPerChannel ()
{
  auto keys = 0;
  for (auto const control : mixerFaceOrder)
    keys += mixerControlIsAToggle (control) ? 1 : 0;
  return keys;
}

// The pages' children are their TouchControls and their faders; the faders
// are JUCE sliders and answer for themselves, so what is counted here is the
// hit areas the page puts on its own controls.
int
liveHitAreas (juce::Component const &component)
{
  auto live = 0;
  for (auto *child : component.getChildren ())
    live += (child->isVisible ()
             && dynamic_cast<TouchControl const *> (child) != nullptr)
                ? 1
                : 0;
  return live;
}

// A window nothing can be laid out in. Both pages answer `fits = false` well
// before this -- the point is only that they do, and that they say so the
// same way at any size below their floor.
constexpr auto tooSmall = 60;
constexpr auto roomy = 700;
}

// The rule ClipSettingsComponent::setPage already states: a hit area with
// nothing under it is how a finger changes a value it cannot see. Both pages
// draw one line of text instead of a mixer when the layout does not fit, and
// used to leave all fifteen -- respectively seven -- targets exactly where
// they would have been. Pot Size is a skin value the performer dials on the
// device, and it is what the row floor is made of, so this is reachable
// without resizing anything: raise it far enough and the page is a sentence
// with a live gain drag across it.
TEST (MixerHitAreas, TheOverlayTakesNoTouchesWhileItDrawsTheSentence)
{
  MixerState state;
  VuLevels levels;
  MixerComponent mixer (state, levels);

  mixer.setBounds (0, 0, tooSmall, tooSmall);

  EXPECT_EQ (liveHitAreas (mixer), 0);
}

TEST (MixerHitAreas, TheOverlayTakesThemAgainOnceItFits)
{
  MixerState state;
  VuLevels levels;
  MixerComponent mixer (state, levels);

  mixer.setBounds (0, 0, roomy, roomy);

  // The channels' keys and the filter's mode key; the master is all knobs.
  EXPECT_EQ (liveHitAreas (mixer),
             numChannelsInitial * keysPerChannel () + 1);
}

// The tab in the settings bar is the same page in a different shape, and it
// went wrong the same way. It matters more here, not less: the bar stays on
// screen under the mixer overlay, so a strip that has given up on drawing
// itself is still the thing a hand lands on.
TEST (MixerHitAreas, TheStripTakesNoTouchesWhileItDrawsTheSentence)
{
  MixerState state;
  VuLevels levels;
  MixerStripComponent strip (state, levels);

  strip.setBounds (0, 0, tooSmall, tooSmall);

  EXPECT_EQ (liveHitAreas (strip), 0);
}

TEST (MixerHitAreas, TheStripTakesThemAgainOnceItFits)
{
  MixerState state;
  VuLevels levels;
  MixerStripComponent strip (state, levels);

  strip.setBounds (0, 0, roomy, roomy / 4);

  EXPECT_EQ (liveHitAreas (strip), keysPerChannel ());
}
