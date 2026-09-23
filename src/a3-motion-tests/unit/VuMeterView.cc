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
#include <a3-motion-ui/components/MixerStripComponent.hh>
#include <a3-motion-ui/components/OverlayButtons.hh>
#include <a3-motion-ui/components/VuMeterView.hh>

using namespace a3;

namespace
{
constexpr auto roomy = 700;

VuMeterView *
viewOver (juce::Component &page, juce::Rectangle<int> where)
{
  for (auto *child : page.getChildren ())
    if (auto *view = dynamic_cast<VuMeterView *> (child))
      if (view->isVisible () && view->getBounds () == where)
        return view;
  return nullptr;
}
}

// A meter is its own component, and an opaque one: the level changes twenty
// five times a second, and a meter that asked its page to redraw for it made
// the page redraw whole -- 533 full pages in two and a half seconds of
// dragging, measured on the rig, which is what a fader felt as catching.
TEST (VuMeterView, AMeterPaintsItselfAndAsksNobodyElseTo)
{
  VuMeterView view;

  EXPECT_TRUE (view.isOpaque ());
}

TEST (VuMeterView, TheOverlayGivesEveryMeterOne)
{
  MixerState state;
  VuLevels levels;
  MixerComponent mixer (state, levels);
  mixer.setBounds (0, 0, roomy, roomy);

  auto area = mixer.getLocalBounds ();
  area.removeFromTop (OverlayButtons::preferredHeight ()
                      + 2 * OverlayButtons::preferredMargin ());
  auto const layout = layOutMixerOverlay (area, mixerControlMetrics ());

  for (int channel = 0; channel < numChannelsInitial; ++channel)
    EXPECT_NE (viewOver (mixer,
                         layout.channelMeter[static_cast<std::size_t> (channel)]),
               nullptr)
        << channel;

  for (std::size_t i = 0; i < static_cast<std::size_t> (numOutputMeters); ++i)
    EXPECT_NE (viewOver (mixer, layout.outputMeters[i]), nullptr) << i;
}

TEST (VuMeterView, TheBarsTabGivesItsMeterOne)
{
  MixerState state;
  VuLevels levels;
  MixerStripComponent strip (state, levels);
  strip.setBounds (0, 0, roomy, roomy / 4);

  auto const layout = layOutMixerStrip (strip.getLocalBounds (),
                                        mixerControlMetrics ());

  EXPECT_NE (viewOver (strip, layout.channelMeter[0]), nullptr);
}

// The fader stands over the meter, not beside it: the handle is the layer
// above -- "faderknob liegt in der ebene drüber".
TEST (VuMeterView, TheFaderStandsOverTheMeter)
{
  MixerState state;
  VuLevels levels;
  MixerStripComponent strip (state, levels);
  strip.setBounds (0, 0, roomy, roomy / 4);

  auto const layout = layOutMixerStrip (strip.getLocalBounds (),
                                        mixerControlMetrics ());
  auto *view = viewOver (strip, layout.channelMeter[0]);
  ASSERT_NE (view, nullptr);

  auto const children = strip.getChildren ();
  auto meterAt = -1;
  auto faderAt = -1;
  for (int i = 0; i < children.size (); ++i)
    {
      if (children[i] == view)
        meterAt = i;
      if (dynamic_cast<VuFader *> (children[i]) != nullptr)
        faderAt = i;
    }

  ASSERT_GE (meterAt, 0);
  ASSERT_GE (faderAt, 0);
  EXPECT_GT (faderAt, meterAt) << "the fader is behind its meter";
}
