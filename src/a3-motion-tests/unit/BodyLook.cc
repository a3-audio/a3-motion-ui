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

#include <a3-motion-ui/components/LookAndFeel.hh>
#include <a3-motion-ui/components/fpv/BodyLook.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

using namespace a3;

TEST (BodyLook, HeavierIsBiggerButNotLinear)
{
  auto const blob = 40.f;
  EXPECT_GT (bodyRadius (3.f, blob), bodyRadius (2.f, blob));
  EXPECT_GT (bodyRadius (2.f, blob), bodyRadius (1.f, blob));
  EXPECT_LT (bodyRadius (3.f, blob), 3.f * bodyRadius (1.f, blob));
  EXPECT_FLOAT_EQ (bodyRadius (1.f, blob), bodyRadiusOfBlob * blob);
}

TEST (BodyLook, ADeadZoneIsTheSameSizeAsACrowd)
{
  EXPECT_FLOAT_EQ (bodyRadius (-2.f, 40.f), bodyRadius (2.f, 40.f));
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

TEST (BodyLook, ASmallBodyIsStillAFingertipToHit)
{
  auto const fingertip = 34.f;
  EXPECT_FLOAT_EQ (bodyHitRadius (1.f, 10.f, fingertip), fingertip / 2.f);
  auto const big = bodyRadius (3.f, 80.f);
  EXPECT_FLOAT_EQ (bodyHitRadius (3.f, 80.f, fingertip), big)
      << "a disc bigger than a fingertip is hit where it is drawn";
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
  paint (float mass, float pulse = 1.f, float hold = 0.f) const
  {
    juce::Image image (juce::Image::ARGB, side, side, true);
    juce::Graphics g (image);
    g.fillAll (toColour (theme ().background));
    BodyPaint body;
    body.centre = centre;
    body.radius = radius;
    body.mass = mass;
    body.label = "G1";
    body.pulse = pulse;
    body.ringRadius = radius * 1.4f;
    body.holdProgress = hold;
    body.stroke = theme ().strokeMedium;
    body.fontHeight = radius * 0.5f;
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

/** Pixels where red clearly leads green and blue: the danger colour. */
int
redPixels (juce::Image const &image, juce::Rectangle<int> area)
{
  auto count = 0;
  for (int y = area.getY (); y < area.getBottom (); ++y)
    for (int x = area.getX (); x < area.getRight (); ++x)
      {
        auto const c = image.getPixelAt (x, y);
        count += c.getRed () > c.getGreen () + 40 && c.getRed () > c.getBlue () + 40
                     ? 1
                     : 0;
      }
  return count;
}
}

TEST (BodyLookPaint, EveryWeightAndTheHoldPaint)
{
  Painter p;
  struct Look
  {
    char const *name;
    float mass;
    float hold;
  };
  for (auto look : { Look{ "group", 1.f, 0.f }, Look{ "crowd", 2.f, 0.f },
                     Look{ "hotspot", 3.f, 0.f }, Look{ "deadzone", -2.f, 0.f },
                     Look{ "held", 2.f, 0.6f } })
    {
      auto const image = p.paint (look.mass, 1.f, look.hold);
      EXPECT_GT (pixelsAwayFromBackground (image), 0) << look.name;
      writeSnapshot (image, juce::String ("fpv-body-") + look.name + ".png");
    }
}

// Below the label, inside the disc: a grey, not a hue.
TEST (BodyLookPaint, AGroupsDiscIsNeutral)
{
  Painter p;
  auto const c = p.paint (2.f).getPixelAt (juce::roundToInt (centre.x),
                                           juce::roundToInt (centre.y + radius * 0.75f));
  auto const bg = toColour (theme ().background);
  EXPECT_NE (c, bg) << "the disc is filled";
  auto const dr = c.getRed () - bg.getRed ();
  auto const dg = c.getGreen () - bg.getGreen ();
  auto const db = c.getBlue () - bg.getBlue ();
  EXPECT_NEAR (dr, dg, 3);
  EXPECT_NEAR (dg, db, 3);
}

TEST (BodyLookPaint, ADeadZoneIsRedAndHatched)
{
  Painter p;
  auto const image = p.paint (-2.f);
  auto const inside = juce::Rectangle<float> (radius, radius)
                          .withCentre (centre)
                          .toNearestInt ();
  EXPECT_GT (redPixels (image, inside), 0);
  EXPECT_EQ (redPixels (p.paint (2.f), inside), 0) << "a group has no red";

  // Across the disc the hatch goes red, not red, red: stripes, not a fill.
  auto y = juce::roundToInt (centre.y + radius * 0.3f);
  auto changes = 0;
  auto wasRed = false;
  for (int x = juce::roundToInt (centre.x - radius * 0.7f);
       x < juce::roundToInt (centre.x + radius * 0.7f); ++x)
    {
      auto const c = image.getPixelAt (x, y);
      auto const red = c.getRed () > c.getGreen () + 40;
      changes += red != wasRed ? 1 : 0;
      wasRed = red;
    }
  EXPECT_GE (changes, 4);
}

TEST (BodyLookPaint, TheHoldRingFillsTowardsTheRemoval)
{
  Painter p;
  auto const all = juce::Rectangle<int> (side, side);
  auto const none = redPixels (p.paint (2.f, 1.f, 0.f), all);
  auto const half = redPixels (p.paint (2.f, 1.f, 0.5f), all);
  auto const full = redPixels (p.paint (2.f, 1.f, 1.f), all);
  EXPECT_EQ (none, 0);
  EXPECT_GT (half, 0);
  EXPECT_GT (full, half + half / 2);
}

TEST (BodyLookPaint, TheDiscBreathesWithThePulse)
{
  Painter p;
  EXPECT_GT (pixelsAwayFromBackground (p.paint (1.f, 1.6f)),
             pixelsAwayFromBackground (p.paint (1.f, 1.f)));
}
