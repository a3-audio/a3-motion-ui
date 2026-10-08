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
#include <a3-motion-ui/components/DeskMeter.hh>
#include <a3-motion-ui/components/VuMeterView.hh>
#include <a3-motion-ui/theme/Theme.hh>

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

// -- FULL's channel meters on the desk's scale (2026-10-08) ------------------
//
// The maintainer saw Motion red where the desk and StemDeck were yellow. The
// channel meters of FULL -- the MIXER overlay, the bar's MIX tab and the
// channel faces -- now show a channel as the desk's displays do, as FPV's
// strips do (DeskMeter).

namespace
{
juce::Image
paintedChannelMeter (VuReading reading)
{
  VuMeterView view;
  view.setBounds (0, 0, 24, 240);
  view.level = [reading] { return reading; };
  return view.createComponentSnapshot (view.getLocalBounds ());
}

float
amplitudeAtDb (float db)
{
  return std::pow (10.f, db / 20.f);
}

int
pixelsOf (juce::Image const &image, juce::Colour colour)
{
  auto count = 0;
  for (int y = 0; y < image.getHeight (); ++y)
    for (int x = 0; x < image.getWidth (); ++x)
      count += image.getPixelAt (x, y) == colour ? 1 : 0;
  return count;
}

void
writeMeterSnapshot (juce::Image const &image, char const *name)
{
  auto const dir
      = juce::SystemStats::getEnvironmentVariable ("A3_SNAPSHOT_DIR", {});
  if (dir.isEmpty ())
    return;
  auto file = juce::File (dir).getChildFile (name);
  file.deleteFile ();
  juce::FileOutputStream out (file);
  juce::PNGImageFormat ().writeImageToStream (image, out);
}
}

// The reported fault: a -5 dBFS peak is the desk's sixth LED, yellow. The
// old meter put red from -6 dBFS and painted the head of this bar red.
TEST (VuMeterView, AChannelMeterAtMinusFiveShowsYellowNotRed)
{
  auto const image = paintedChannelMeter ({ amplitudeAtDb (-5.f), 0.f });
  writeMeterSnapshot (image, "full-meter-minus5.png");
  EXPECT_EQ (pixelsOf (image, vuBandColour (theme (), vuRedBand)), 0);
  EXPECT_GT (pixelsOf (image, vuBandColour (theme (), vuYellowBand)), 0);
}

TEST (VuMeterView, AChannelMeterIsTheDesksMeter)
{
  // -5 dBFS with -2 held: six eighths of the bar, the held line at seven.
  VuReading const reading{ amplitudeAtDb (-5.f), amplitudeAtDb (-2.f) };
  auto const image = paintedChannelMeter (reading);
  writeMeterSnapshot (image, "full-meter-minus5-held.png");

  juce::Image expected (juce::Image::ARGB, 24, 240, true);
  {
    juce::Graphics g (expected);
    paintDeskMeter (g, { 0, 0, 24, 240 }, reading, VuDirection::Up);
  }
  auto const m = deskMeterGeometry ({ 0, 0, 24, 240 }, reading, VuDirection::Up);
  ASSERT_FALSE (m.hold.isEmpty ());
  for (auto const at : { m.bar.getCentre (), m.hold.getCentre () })
    EXPECT_EQ (image.getPixelAt (at.x, at.y), expected.getPixelAt (at.x, at.y));
}

TEST (VuMeterView, AChannelMeterAtRestShowsNoLevel)
{
  auto const image = paintedChannelMeter ({});
  writeMeterSnapshot (image, "full-meter-rest.png");
  for (auto band : { vuGreenBand, vuYellowBand, vuRedBand })
    EXPECT_EQ (pixelsOf (image, vuBandColour (theme (), band)), 0) << band;
}

// An over (the held peak above full scale): the full bar, hatched.
TEST (VuMeterView, AClippingChannelIsAHatchedFullBar)
{
  auto const image = paintedChannelMeter ({ 1.f, 1.2f });
  writeMeterSnapshot (image, "full-meter-clip.png");
  auto const red = vuBandColour (theme (), vuRedBand);
  auto const clean = pixelsOf (paintedChannelMeter ({ 1.f, 1.f }), red);
  EXPECT_GT (pixelsOf (image, red), 0);
  EXPECT_LT (pixelsOf (image, red), clean);
}

// The master column shows the room's outputs, not a channel: the desk draws
// those on its own rows, linear in dB from -60, with no colours or marks --
// so they keep the -60..0 bar they had.
TEST (VuMeterView, TheOutputMetersKeepTheContinuousBar)
{
  MixerState state;
  VuLevels levels;
  MixerComponent mixer (state, levels);
  mixer.setBounds (0, 0, roomy, roomy);
  auto continuous = 0;
  auto desk = 0;
  for (auto *child : mixer.getChildren ())
    if (auto *view = dynamic_cast<VuMeterView *> (child))
      (view->scale () == VuMeterScale::Continuous ? continuous : desk)++;
  EXPECT_EQ (continuous, numOutputMeters);
  EXPECT_EQ (desk, numChannelsInitial);
}
