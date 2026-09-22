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
#include <a3-motion-ui/components/LookAndFeel.hh>
#include <a3-motion-ui/components/VuFader.hh>
#include <a3-motion-ui/components/VuMeter.hh>

using namespace a3;

namespace
{
constexpr auto roomy = 700;

VuFader *
faderOver (juce::Component &page, juce::Rectangle<int> meter)
{
  for (auto *child : page.getChildren ())
    if (auto *fader = dynamic_cast<VuFader *> (child))
      if (fader->isVisible () && fader->getBounds () == meter)
        return fader;
  return nullptr;
}
}

// The fader is JUCE's, so the drag is JUCE's: relative (it must not jump to
// where the finger landed), stepless over 0..1, and with no text box of its
// own to steal room from the meter behind it.
TEST (VuFader, ItIsARelativeVerticalSliderOverZeroToOne)
{
  VuFader fader;

  EXPECT_EQ (fader.getSliderStyle (), juce::Slider::LinearVertical);
  EXPECT_EQ (fader.getMinimum (), 0.0);
  EXPECT_EQ (fader.getMaximum (), 1.0);
  EXPECT_EQ (fader.getInterval (), 0.0);
  EXPECT_EQ (fader.getTextBoxPosition (), juce::Slider::NoTextBox);
}

TEST (MixerFaders, EachChannelMeterCarriesOne)
{
  MixerState state;
  VuLevels levels;
  MixerComponent mixer (state, levels);
  mixer.setBounds (0, 0, roomy, roomy);

  auto area = mixer.getLocalBounds ();
  area.removeFromTop (OverlayButtons::preferredHeight ()
                      + 2 * OverlayButtons::preferredMargin ());
  auto const layout = layOutMixerOverlay (area, mixerControlMetrics ());

  auto moved = -1.f;
  auto movedChannel = -1;
  mixer.onMeterDraggedTo = [&] (int channel, float value) {
    movedChannel = channel;
    moved = value;
  };

  for (int channel = 0; channel < numChannelsInitial; ++channel)
    {
      auto *fader = faderOver (
          mixer, layout.channelMeter[static_cast<std::size_t> (channel)]);
      ASSERT_NE (fader, nullptr) << channel;

      fader->setValue (0.42, juce::sendNotificationSync);
      EXPECT_EQ (movedChannel, channel);
      EXPECT_NEAR (moved, 0.42f, 0.001f);
    }
}

// Two taps put the channel at full volume, as they did on the hand-drawn
// handle -- now a double click, which is what a slider hears.
TEST (MixerFaders, TwoTapsOnAChannelFaderAskForFullVolume)
{
  MixerState state;
  VuLevels levels;
  MixerComponent mixer (state, levels);
  mixer.setBounds (0, 0, roomy, roomy);

  auto area = mixer.getLocalBounds ();
  area.removeFromTop (OverlayButtons::preferredHeight ()
                      + 2 * OverlayButtons::preferredMargin ());
  auto const layout = layOutMixerOverlay (area, mixerControlMetrics ());

  auto asked = -1;
  mixer.onMeterDoubleTapped = [&asked] (int channel) { asked = channel; };

  auto *fader = faderOver (mixer, layout.channelMeter[1]);
  ASSERT_NE (fader, nullptr);
  ASSERT_TRUE (fader->onDoubleTapped);

  fader->onDoubleTapped ();

  EXPECT_EQ (asked, 1);
}

// The master's has none: full volume there is the whole room at once.
TEST (MixerFaders, TheMastersFaderHasNoDoubleTap)
{
  MixerState state;
  VuLevels levels;
  MixerComponent mixer (state, levels);
  mixer.setBounds (0, 0, roomy, roomy);

  auto area = mixer.getLocalBounds ();
  area.removeFromTop (OverlayButtons::preferredHeight ()
                      + 2 * OverlayButtons::preferredMargin ());
  auto const layout = layOutMixerOverlay (area, mixerControlMetrics ());

  auto *fader = faderOver (mixer, layout.masterMeter);
  ASSERT_NE (fader, nullptr);
  EXPECT_FALSE (fader->onDoubleTapped);

  auto reached = -1.f;
  mixer.onMasterMeterDraggedTo = [&reached] (float value) { reached = value; };
  fader->setValue (0.6, juce::sendNotificationSync);

  EXPECT_NEAR (reached, 0.6f, 0.001f);
}

TEST (MixerFaders, TheBarsTabCarriesOneToo)
{
  MixerState state;
  VuLevels levels;
  MixerStripComponent strip (state, levels);
  strip.setBounds (0, 0, roomy, roomy / 4);

  auto const layout = layOutMixerStrip (strip.getLocalBounds (),
                                        mixerControlMetrics ());

  auto moved = -1.f;
  strip.onMeterDraggedTo = [&moved] (int, float value) { moved = value; };

  auto *fader = faderOver (strip, layout.channelMeter[0]);
  ASSERT_NE (fader, nullptr);

  fader->setValue (0.3, juce::sendNotificationSync);

  EXPECT_NEAR (moved, 0.3f, 0.001f);
}

// Coloured the way a JUCE slider is coloured: per component, through the
// colour ids the LookAndFeel draws from. Ours draws the cap, but nothing
// about *which* colour it is belongs to us.
TEST (VuFader, ItsHandleColourIsTheSlidersThumbColour)
{
  VuFader fader;

  fader.setHandleColour (juce::Colours::hotpink);

  EXPECT_EQ (fader.findColour (juce::Slider::thumbColourId),
             juce::Colours::hotpink);
}

// JUCE lays a slider's travel over the track less its thumb, so it has to be
// told how thick ours is: with the stock radius the cap walked out from under
// the finger on the overlay's long faders, a little more with every pixel.
TEST (VuFader, JuceIsToldHowThickTheCapIs)
{
  LookAndFeel_A3 lookAndFeel;
  VuFader fader;
  fader.setLookAndFeel (&lookAndFeel);
  fader.setBounds (0, 0, 46, 400);

  EXPECT_EQ (lookAndFeel.getSliderThumbRadius (fader),
             vuFaderHandle (fader.getLocalBounds (), 0.5f).getHeight () / 2);

  fader.setLookAndFeel (nullptr);
}

// JUCE's relative drag is calibrated in "pixels for the full range", and it
// defaults to 250 -- on a 490 px track that ran the handle 1.8 times as fast
// as the finger, measured on the rig. The travel the handle actually has is
// what the finger has to spend.
TEST (VuFader, AFullDragIsExactlyTheHandlesTravel)
{
  VuFader fader;
  fader.setBounds (0, 0, 46, 490);

  auto const bounds = fader.getLocalBounds ();
  auto const travel
      = bounds.getHeight () - vuFaderHandle (bounds, 0.f).getHeight ();

  EXPECT_EQ (fader.getMouseDragSensitivity (), travel);

  // And it follows the bounds: the bar's tab has a much shorter meter.
  fader.setBounds (0, 0, 46, 260);
  auto const shorter = fader.getLocalBounds ();
  EXPECT_EQ (fader.getMouseDragSensitivity (),
             shorter.getHeight () - vuFaderHandle (shorter, 0.f).getHeight ());
}

// Laying the slider out is the base class's job, and an override that forgets
// to call it leaves the track one pixel wide -- the handle then vanishes.
TEST (VuFader, ItStillLaysOutItsOwnTrack)
{
  VuFader fader;
  fader.setBounds (0, 0, 46, 490);
  fader.setValue (0.5);

  auto const positionAtHalf = fader.getPositionOfValue (0.5);

  EXPECT_GT (positionAtHalf, fader.getHeight () / 4);
  EXPECT_LT (positionAtHalf, fader.getHeight () * 3 / 4);
}
