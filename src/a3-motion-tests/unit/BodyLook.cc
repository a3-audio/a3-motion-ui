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

#include <cmath>

#include <JuceHeader.h>

#include <a3-motion-engine/flight/FlightTuning.hh>
#include <a3-motion-ui/components/LookAndFeel.hh>
#include <a3-motion-ui/components/fpv/BodyLook.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

using namespace a3;

TEST (BodyLook, HeavierIsBiggerButNotLinear)
{
  FlightTuning const t;
  auto const blob = 40.f;
  EXPECT_GT (bodyRadius (t.hotspotMass, blob, t), bodyRadius (t.crowdMass, blob, t));
  EXPECT_GT (bodyRadius (t.crowdMass, blob, t), bodyRadius (t.groupMass, blob, t));
  EXPECT_LT (bodyRadius (t.hotspotMass, blob, t), 3.f * bodyRadius (t.groupMass, blob, t));
  EXPECT_FLOAT_EQ (bodyRadius (t.groupMass, blob, t), bodyRadiusOfBlob * blob);
}

// The planets got lighter (2026-10-08), the drawing did not shrink: a group
// is drawn at a blob's size, a crowd sqrt(2) and a hotspot sqrt(3) of it.
TEST (BodyLook, LighterPlanetsAreDrawnAsBefore)
{
  FlightTuning const t;
  auto const blob = 40.f;
  auto const group = bodyRadius (t.groupMass, blob, t);
  EXPECT_FLOAT_EQ (bodyRadius (t.crowdMass, blob, t), group * std::sqrt (2.f));
  EXPECT_FLOAT_EQ (bodyRadius (t.hotspotMass, blob, t), group * std::sqrt (3.f));
}

TEST (BodyLook, ADeadZoneIsTheSameSizeAsACrowd)
{
  FlightTuning const t;
  EXPECT_FLOAT_EQ (bodyRadius (t.deadZoneMass, 40.f, t), bodyRadius (t.crowdMass, 40.f, t));
}

TEST (BodyLook, ThePulseGrowsTheDiscLessThanThePull)
{
  for (auto pulse : { 1.3f, 1.6f })
    {
      auto const scale = bodyPulseScale (pulse);
      EXPECT_GT (scale, 1.f);
      EXPECT_LT (scale, pulse);
    }
  EXPECT_FLOAT_EQ (bodyPulseScale (1.f), 1.f) << "no pulse, no breath";
}

TEST (BodyLook, LabelsCountFromOne)
{
  EXPECT_EQ (bodyLabel (0), "G1");
  EXPECT_EQ (bodyLabel (7), "G8");
}

TEST (BodyLook, NegativeMassRepels)
{
  EXPECT_EQ (bodyRole (-2.f), BodyRole::Repel);
  EXPECT_EQ (bodyRole (1.f), BodyRole::Attract);
  EXPECT_EQ (bodyRole (3.f), BodyRole::Attract);
}

// Hue on this screen already means a channel, and a guest group is nobody's
// channel; red is kept for "not here".
TEST (BodyLook, AGroupIsNeutralAndOnlyADeadZoneIsRed)
{
  auto same = [] (ThemeColour a, ThemeColour b) {
    return toColour (a) == toColour (b);
  };
  EXPECT_TRUE (same (bodyColour (BodyRole::Attract), theme ().textPrimary));
  EXPECT_TRUE (same (bodyColour (BodyRole::Repel), theme ().danger));
}

namespace
{
constexpr int side = 200;
constexpr float radius = 50.f;
juce::Point<float> const centre{ 100.f, 100.f };

struct Painter
{
  Painter () { juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel); }
  ~Painter () { juce::LookAndFeel::setDefaultLookAndFeel (nullptr); }

  juce::Image
  paint (bool hidden = false, float pulse = 1.f) const
  {
    juce::Image image (juce::Image::ARGB, side, side, true);
    juce::Graphics g (image);
    g.fillAll (toColour (theme ().background));
    BodyPaint body;
    body.centre = centre;
    body.radius = radius;
    body.label = "G1";
    body.pulse = pulse;
    body.fontHeight = radius * 0.5f;
    body.hidden = hidden;
    paintBody (g, body);
    return image;
  }

  LookAndFeel_A3 lookAndFeel;
};

void
writeSnapshot (juce::Image const &image, juce::String const &name)
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

int
pixelsAwayFromBackground (juce::Image const &image)
{
  auto const bg = toColour (theme ().background);
  auto count = 0;
  for (int y = 0; y < image.getHeight (); ++y)
    for (int x = 0; x < image.getWidth (); ++x)
      count += image.getPixelAt (x, y) != bg ? 1 : 0;
  return count;
}

}

TEST (BodyLookPaint, TheLabelPaints)
{
  Painter p;
  auto const image = p.paint ();
  EXPECT_GT (pixelsAwayFromBackground (image), 0);
  writeSnapshot (image, "fpv-body-label.png");
}

// The shader paints the mark on the dance floor, where it lies with the
// floor; painted on the glass as well it would sit over the very mark.
TEST (BodyLookPaint, TheMarkIsTheShaders)
{
  Painter p;
  auto const bg = toColour (theme ().background);
  auto const image = p.paint ();
  for (auto const dy : { -0.6f, 0.6f })
    EXPECT_EQ (image.getPixelAt (juce::roundToInt (centre.x),
                                 juce::roundToInt (centre.y + radius * dy)),
               bg)
        << "nothing painted inside at " << dy;
}

TEST (BodyLookPaint, AHiddenMarksLabelDims)
{
  Painter p;
  auto const label = juce::Rectangle<float> (radius, radius * 0.6f)
                         .withCentre (centre)
                         .toNearestInt ();
  auto const brightest = [&] (juce::Image const &image) {
    auto most = 0.f;
    for (int y = label.getY (); y < label.getBottom (); ++y)
      for (int x = label.getX (); x < label.getRight (); ++x)
        most = std::max (most, image.getPixelAt (x, y).getBrightness ());
    return most;
  };
  EXPECT_LT (brightest (p.paint (true)), brightest (p.paint ()));
}

// The rings lie on the dance floor with the mark, so the shader draws them
// too; the glass keeps only the word.
TEST (BodyLookPaint, TheRingsAreTheShaders)
{
  Painter p;
  auto const bg = toColour (theme ().background);
  for (auto const dx : { -1.4f, -1.2f, 1.2f, 1.4f })
    EXPECT_EQ (p.paint ().getPixelAt (juce::roundToInt (centre.x + radius * dx),
                                         juce::roundToInt (centre.y)),
               bg)
        << "nothing at " << dx << " radii";
}

