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

#include <JuceHeader.h>

#include <a3-motion-ui/components/LookAndFeel.hh>
#include <a3-motion-ui/components/fpv/FpvStrips.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

using namespace a3;

namespace
{
std::array<FpvChannel, 4>
fourChannels ()
{
  auto const colour = [] (size_t i) { return toColour (theme ().channel[i]); };
  std::array<FpvChannel, 4> c{};
  c[0] = { colour (0), "Closing Sunset", true, { 0.5f, 0.5f, 0.f } };
  c[1] = { colour (1), "", false, {} };
  c[2] = { colour (2), "Move", true, { 1.f, 0.f, 0.f } };
  c[3] = { colour (3), "", false, {} };
  return c;
}

struct Fixture
{
  Fixture ()
  {
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    strips.setBounds (0, 0, 1280, 250);
    strips.setChannels (fourChannels ());
    strips.channelLevel = [] (int) { return VuReading{ 0.8f, 0.9f }; };
  }

  ~Fixture () { juce::LookAndFeel::setDefaultLookAndFeel (nullptr); }

  juce::Image paint ()
  {
    return strips.createComponentSnapshot (strips.getLocalBounds ());
  }

  LookAndFeel_A3 lookAndFeel;
  FpvStrips strips;
};
}

TEST (FpvStripsPaint, ItPaints)
{
  Fixture f;
  auto const image = f.paint ();
  EXPECT_TRUE (image.isValid ());

  auto const dir
      = juce::SystemStats::getEnvironmentVariable ("A3_SNAPSHOT_DIR", {});
  if (dir.isNotEmpty ())
    {
      auto file = juce::File (dir).getChildFile ("fpv-strips.png");
      file.deleteFile ();
      juce::FileOutputStream out (file);
      juce::PNGImageFormat ().writeImageToStream (image, out);
    }
}

TEST (FpvStripsPaint, TheStripsAreTheLayoutsRowAtOrigin)
{
  Fixture f;
  EXPECT_EQ (f.strips.strips ()[0].whole.getX (), 0);
  EXPECT_EQ (f.strips.strips ()[3].whole.getRight (), 1280);
  EXPECT_EQ (f.strips.strips ()[0].whole.getHeight (), 250);
}

// Probe: just inside the strip's top-left corner, past the rounded corner and
// before the header text, which starts a padding in.
TEST (FpvStripsPaint, EachStripWearsItsChannelsColour)
{
  Fixture f;
  auto const image = f.paint ();
  for (int ch : { 0, 2 })
    {
      auto const &whole = f.strips.strips ()[static_cast<size_t> (ch)].whole;
      auto const inset = whole.reduced (whole.getHeight () / 8).getTopLeft ();
      auto const pixel = image.getPixelAt (inset.x, inset.y);
      auto const want = toColour (theme ().channel[ch]);
      if (want.getRed () > want.getBlue ())
        EXPECT_GT (pixel.getRed (), pixel.getBlue ()) << ch;
      else
        EXPECT_GT (pixel.getBlue (), pixel.getRed ()) << ch;
    }
}

// Probe rule: the 3D bar is the first third of the instruments section, FREQ
// the second; the far right end of a bar is track unless its pot is full.
TEST (FpvStripsPaint, AFullPotFillsItsBarAndAnEmptyOneDoesNot)
{
  Fixture f;
  auto const image = f.paint ();
  auto const &inst = f.strips.strips ()[2].instruments; // pots 1, 0, 0
  auto const y3d = inst.getY () + inst.getHeight () / 6;
  auto const yFreq = inst.getY () + inst.getHeight () / 2;
  auto const xRight = inst.getRight () - inst.getWidth () / 20;
  auto const full = image.getPixelAt (xRight, y3d);
  auto const empty = image.getPixelAt (xRight, yFreq);
  EXPECT_NE (full, empty);
  auto const want = toColour (theme ().channel[2]);
  auto const distance = [&] (juce::Colour c) {
    return std::abs (c.getRed () - want.getRed ())
           + std::abs (c.getGreen () - want.getGreen ())
           + std::abs (c.getBlue () - want.getBlue ());
  };
  EXPECT_LT (distance (full), distance (empty));
}

TEST (FpvStripsPaint, TheMeterShowsItsSignal)
{
  Fixture f;
  auto const loud = f.paint ();
  f.strips.channelLevel = [] (int) { return VuReading{}; };
  auto const silent = f.paint ();
  auto const &m = f.strips.strips ()[1].meter;
  auto differ = 0;
  for (int y = m.getY (); y < m.getBottom (); ++y)
    for (int x = m.getX (); x < m.getRight (); ++x)
      differ += loud.getPixelAt (x, y) != silent.getPixelAt (x, y) ? 1 : 0;
  EXPECT_GT (differ, 0);
}

TEST (FpvStripsPaint, ASkinChangeRepaintsWithoutCrashing)
{
  Fixture f;
  f.paint ();
  f.strips.applyTheme ();
  EXPECT_TRUE (f.paint ().isValid ());
}

TEST (FpvStripsPaint, NoLevelSourceStillPaints)
{
  Fixture f;
  f.strips.channelLevel = nullptr;
  EXPECT_TRUE (f.paint ().isValid ());
}

namespace
{
int
pixelsThatDiffer (juce::Image const &a, juce::Image const &b,
                  juce::Rectangle<int> area)
{
  auto differ = 0;
  for (int y = area.getY (); y < area.getBottom (); ++y)
    for (int x = area.getX (); x < area.getRight (); ++x)
      differ += a.getPixelAt (x, y) != b.getPixelAt (x, y) ? 1 : 0;
  return differ;
}

/** Pixels within a small distance of the theme's text colour: what a filled
 *  pill covers and a hollow one only outlines. */
int
textColouredPixels (juce::Image const &image, juce::Rectangle<int> area)
{
  auto const want = toColour (theme ().textPrimary);
  auto count = 0;
  for (int y = area.getY (); y < area.getBottom (); ++y)
    for (int x = area.getX (); x < area.getRight (); ++x)
      {
        auto const c = image.getPixelAt (x, y);
        auto const distance = std::abs (c.getRed () - want.getRed ())
                              + std::abs (c.getGreen () - want.getGreen ())
                              + std::abs (c.getBlue () - want.getBlue ());
        count += distance < 60 ? 1 : 0;
      }
  return count;
}

void
writeSnapshot (juce::Image const &image, char const *name)
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

std::array<FpvChannel, 4>
orbiting ()
{
  auto c = fourChannels ();
  c[0].orbit = true;              // patrol
  c[2].orbit = true;              // escorting G3, a crowd
  c[2].escort = 2;
  c[2].escortMass = 2.f;
  return c;
}
}

TEST (FpvStripsPaint, AnOrbitStripPaintsDifferentlyFromAClipStrip)
{
  Fixture f;
  auto const clip = f.paint ();
  f.strips.setChannels (orbiting ());
  auto const orbit = f.paint ();
  writeSnapshot (orbit, "fpv-strips-orbit.png");

  EXPECT_GT (pixelsThatDiffer (clip, orbit, f.strips.strips ()[0].header), 0);
  EXPECT_EQ (pixelsThatDiffer (clip, orbit, f.strips.strips ()[1].header), 0)
      << "a channel left on CLIP keeps its header";
}

// The mode is a shape before it is a word: CLIP a hollow pill, ORBIT a
// filled one.
TEST (FpvStripsPaint, TheOrbitPillIsFilledAndTheClipPillIsHollow)
{
  Fixture f;
  auto const pill = fpvModePill (f.strips.strips ()[0].header).getSmallestIntegerContainer ();
  ASSERT_FALSE (pill.isEmpty ());
  auto const clip = textColouredPixels (f.paint (), pill);
  f.strips.setChannels (orbiting ());
  auto const orbit = textColouredPixels (f.paint (), pill);
  EXPECT_GT (orbit, 2 * clip) << "clip " << clip << ", orbit " << orbit;
}

TEST (FpvStripsPaint, ThePillSitsInTheHeadersRightHalf)
{
  Fixture f;
  auto const &header = f.strips.strips ()[0].header;
  auto const pill = fpvModePill (header);
  EXPECT_TRUE (header.toFloat ().contains (pill));
  EXPECT_GT (pill.getX (), static_cast<float> (header.getCentreX ()));
}

TEST (FpvStripsPaint, TheEscortRowDiffersFromPatrol)
{
  Fixture f;
  f.strips.setChannels (orbiting ());
  auto const escort = f.paint ();
  auto patrol = orbiting ();
  patrol[2].escort = -1;
  f.strips.setChannels (patrol);
  auto const patrolling = f.paint ();
  writeSnapshot (patrolling, "fpv-strips-patrol.png");
  EXPECT_GT (pixelsThatDiffer (escort, patrolling, f.strips.strips ()[2].clip), 0);
}

// Whom a ship escorts is drawn as the disc the floor shows, sized by weight.
TEST (FpvStripsPaint, AHeavierEscortIsABiggerDisc)
{
  Fixture f;
  auto light = orbiting ();
  light[2].escortMass = 1.f;
  f.strips.setChannels (light);
  auto const &row = f.strips.strips ()[2].clip;
  auto const small = textColouredPixels (f.paint (), row);
  auto heavy = orbiting ();
  heavy[2].escortMass = 3.f;
  f.strips.setChannels (heavy);
  auto const big = textColouredPixels (f.paint (), row);
  EXPECT_GT (big, small);
}

TEST (FpvStripsPaint, AClipStripShowsNoEscort)
{
  Fixture f;
  auto const plain = f.paint ();
  auto c = fourChannels ();
  c[2].escort = 1; // stale target on a CLIP channel
  f.strips.setChannels (c);
  EXPECT_EQ (pixelsThatDiffer (plain, f.paint (), f.strips.strips ()[2].whole), 0);
}

TEST (FpvStripsPaint, ItStillPaintsAfterASkinChange)
{
  Fixture f;
  f.strips.setChannels (orbiting ());
  f.paint ();
  f.strips.applyTheme ();
  EXPECT_TRUE (f.paint ().isValid ());
}
