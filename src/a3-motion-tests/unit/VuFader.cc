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
TEST (VuFader, ItIsAVerticalSliderOverZeroToOneThatSnapsToTheFinger)
{
  VuFader fader;

  // JUCE's own mapping, which needs no pixel constant of ours: the handle
  // lands under the finger whatever the track's length. What keeps it from
  // jumping is that only the handle can be grabbed -- see vuFaderGrabs.
  EXPECT_TRUE (fader.getSliderSnapsToMousePosition ());
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

// Only the cap can be grabbed, the way a fader on a desk can: a thumb landing
// anywhere else would otherwise throw the channel to wherever it touched.
TEST (VuFader, OnlyTheHandleIsGrabbed)
{
  auto const track = juce::Rectangle<int> (0, 0, 46, 490);
  auto const handle = vuFaderHandle (track, 0.5f);

  EXPECT_TRUE (vuFaderGrabs (handle, handle.getCentre ()));
  EXPECT_TRUE (vuFaderGrabs (handle, { handle.getCentreX (), handle.getY () }));
  EXPECT_FALSE (vuFaderGrabs (handle, { handle.getCentreX (), track.getY () }));
  EXPECT_FALSE (vuFaderGrabs (handle, { handle.getCentreX (), track.getBottom () }));
}

// With a little to spare above and below it: a fingertip is wider than the
// cap is tall, and a grab that missed by two pixels would read as the fader
// being dead.
TEST (VuFader, TheGrabHasACatchZoneAroundTheHandle)
{
  auto const track = juce::Rectangle<int> (0, 0, 46, 490);
  auto const handle = vuFaderHandle (track, 0.5f);

  EXPECT_TRUE (vuFaderGrabs (handle, { handle.getCentreX (),
                                       handle.getY () - fingertipSize / 4 }));
  EXPECT_TRUE (vuFaderGrabs (handle, { handle.getCentreX (),
                                       handle.getBottom () + fingertipSize / 4 }));
  EXPECT_FALSE (vuFaderGrabs (handle, { handle.getCentreX (),
                                        handle.getY () - fingertipSize * 2 }));
}

// The cap has to reach both ends of the grey track: JUCE lays a slider out
// through getSliderLayout, and the stock one keeps a margin the drawing knows
// nothing about -- measured on the rig, the cap stopped 29 px short at each
// end of a 491 px track.
TEST (VuFader, TheCapReachesBothEndsOfItsTrack)
{
  LookAndFeel_A3 lookAndFeel;
  VuFader fader;
  fader.setLookAndFeel (&lookAndFeel);
  fader.setBounds (0, 0, 46, 592);

  auto const track = fader.getLocalBounds ();

  // Where the slider puts the handle at either end, drawn: flush with the
  // track, since a cap cannot travel past the end of its own fader.
  auto const atFull = vuFaderHandleAt (
      track, juce::roundToInt (fader.getPositionOfValue (1.0)));
  auto const atNothing = vuFaderHandleAt (
      track, juce::roundToInt (fader.getPositionOfValue (0.0)));

  EXPECT_EQ (atFull.getY (), track.getY ());
  EXPECT_EQ (atNothing.getBottom (), track.getBottom ());

  fader.setLookAndFeel (nullptr);
}

// The whole component is the track. JUCE's stock layout keeps a margin for a
// thumb of its own size, and a slider that was sized before our LookAndFeel
// reached it kept that margin -- on the rig the cap stopped 29 px short of
// each end of the grey meter.
TEST (VuFader, TheTrackIsTheWholeComponent)
{
  LookAndFeel_A3 lookAndFeel;
  VuFader fader;
  fader.setBounds (0, 0, 46, 592);

  auto const layout = lookAndFeel.getSliderLayout (fader);

  EXPECT_EQ (layout.sliderBounds, fader.getLocalBounds ());
  EXPECT_TRUE (layout.textBoxBounds.isEmpty ());
}
