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

using namespace a3;

namespace
{
constexpr auto roomy = 700;

// The hit area over a meter, found where the page lays the meter out -- by
// the same layout function, so the test asks for the target a finger lands
// on rather than for a position in the list of children.
TouchControl *
touchOver (juce::Component &page, juce::Rectangle<int> meter)
{
  for (auto *child : page.getChildren ())
    if (auto *touch = dynamic_cast<TouchControl *> (child))
      if (touch->isVisible () && touch->getBounds () == meter)
        return touch;
  return nullptr;
}
}

// Two taps on a meter put its channel at full volume -- "double tap soll auf
// volle lautstärke springen". The bar's tab first: its one meter is whichever
// channel the strip is showing.
TEST (MixerMeterTouch, TwoTapsOnTheStripsMeterAskForFullVolume)
{
  MixerState state;
  VuLevels levels;
  MixerStripComponent strip (state, levels);
  strip.setBounds (0, 0, roomy, roomy / 4);

  auto asked = -1;
  strip.onMeterDoubleTapped = [&asked] (int channel) { asked = channel; };

  auto const layout = layOutMixerStrip (strip.getLocalBounds (),
                                        mixerControlMetrics ());
  auto *touch = touchOver (strip, layout.channelMeter[0]);
  ASSERT_NE (touch, nullptr);
  ASSERT_TRUE (touch->onDoubleTap);

  touch->onDoubleTap (0, 0);

  EXPECT_EQ (asked, 0);
}

TEST (MixerMeterTouch, TwoTapsOnAnOverlayMeterAskForThatChannel)
{
  MixerState state;
  VuLevels levels;
  MixerComponent mixer (state, levels);
  mixer.setBounds (0, 0, roomy, roomy);

  auto asked = -1;
  mixer.onMeterDoubleTapped = [&asked] (int channel) { asked = channel; };

  auto area = mixer.getLocalBounds ();
  area.removeFromTop (OverlayButtons::preferredHeight ()
                      + 2 * OverlayButtons::preferredMargin ());
  auto const layout = layOutMixerOverlay (area, mixerControlMetrics ());

  for (int channel = 0; channel < numChannelsInitial; ++channel)
    {
      auto *touch = touchOver (
          mixer, layout.channelMeter[static_cast<std::size_t> (channel)]);
      ASSERT_NE (touch, nullptr) << channel;
      ASSERT_TRUE (touch->onDoubleTap) << channel;

      touch->onDoubleTap (0, 0);
      EXPECT_EQ (asked, channel);
    }
}

// The knob stays as it was: two taps there are still nothing, because VOL
// has no rest position (mixerControlRestPosition). Only the meter jumps.
TEST (MixerMeterTouch, TwoTapsOnTheVolumeKnobStillDoNothing)
{
  EXPECT_FALSE (mixerControlRestPosition (MixerControl::Volume).has_value ());
}
