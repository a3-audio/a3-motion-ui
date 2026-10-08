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
#include <a3-motion-ui/theme/TransportLook.hh>

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

namespace
{
int
rgbDistance (juce::Colour a, juce::Colour b)
{
  return std::abs (a.getRed () - b.getRed ())
         + std::abs (a.getGreen () - b.getGreen ())
         + std::abs (a.getBlue () - b.getBlue ());
}
}

// Probe: just inside the strip's top-left corner, past the rounded corner and
// before the header text, which starts a padding in. The strip stands on the
// skin's ground, so wearing its colour means: nearer the channel than the
// ground is.
TEST (FpvStripsPaint, EachStripWearsItsChannelsColour)
{
  Fixture f;
  auto const image = f.paint ();
  auto const ground = toColour (theme ().background);
  for (int ch : { 0, 2 })
    {
      auto const &whole = f.strips.strips ()[static_cast<size_t> (ch)].whole;
      auto const inset = whole.reduced (whole.getHeight () / 8).getTopLeft ();
      auto const pixel = image.getPixelAt (inset.x, inset.y);
      auto const want = toColour (theme ().channel[ch]);
      EXPECT_LT (rgbDistance (pixel, want), rgbDistance (ground, want)) << ch;
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

// The row used to be see-through, and what showed through between the
// strips was JUCE's stock window grey, which no skin can reach -- every
// skin's FPV stood on the same grey (F1 in the stemdeck-look audit).
TEST (FpvStripsPaint, TheRowStandsOnTheSkinsGround)
{
  Fixture f;
  EXPECT_TRUE (f.strips.isOpaque ());
  auto const image = f.paint ();
  auto const &left = f.strips.strips ()[0].whole;
  auto const &right = f.strips.strips ()[1].whole;
  ASSERT_LT (left.getRight (), right.getX ()) << "the probe needs a gap";
  auto const gap = (left.getRight () + right.getX ()) / 2;
  EXPECT_EQ (image.getPixelAt (gap, left.getCentreY ()),
             toColour (theme ().background));
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

/** Pixels within a small distance of the theme's text colour. */
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
  c[2].escortMass = FlightTuning{}.crowdMass;
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

namespace
{
/** CIE L*, from the relative luminance the contrast rules use. */
float
lightness (juce::Colour colour)
{
  auto const y = relativeLuminance (colour);
  return y > 0.008856f ? 116.f * std::cbrt (y) - 16.f : 903.3f * y;
}

/** A pixel of a key's face: just inside its left edge, clear of the hairline
 *  and of the word, which is centred. */
juce::Point<int>
faceOf (juce::Rectangle<float> key)
{
  return { juce::roundToInt (key.getX () + key.getWidth () / 10.f),
           juce::roundToInt (key.getCentreY ()) };
}
}

// The mode is a key, as on StemDeck: CLIP the idle face, ORBIT the lifted
// "current" face. Which mode, the ground says before the word does.
TEST (FpvStripsPaint, TheOrbitKeyIsLiftedAndTheClipKeyIsNot)
{
  Fixture f;
  auto const at = faceOf (fpvModeKey (f.strips.strips ()[0].header));
  auto const clip = f.paint ().getPixelAt (at.x, at.y);
  f.strips.setChannels (orbiting ());
  auto const orbit = f.paint ().getPixelAt (at.x, at.y);
  EXPECT_EQ (clip, toColour (theme ().surfaceRaised));
  EXPECT_GT (lightness (orbit), lightness (clip));
}

TEST (FpvStripsPaint, ThePlateIsLeftAndTheModeKeyRight)
{
  Fixture f;
  auto const &header = f.strips.strips ()[0].header;
  auto const plate = fpvHeaderPlate (header);
  auto const key = fpvModeKey (header);
  EXPECT_TRUE (header.toFloat ().contains (plate));
  EXPECT_TRUE (header.toFloat ().contains (key));
  EXPECT_LT (plate.getRight (), key.getX ());
  EXPECT_GT (key.getX (), static_cast<float> (header.getCentreX ()));
}

// The strip is a card of the skin's own surface: opaque, so it looks the same
// in every skin's FPV, and with no channel wash -- the plate says whose it is.
TEST (FpvStripsPaint, TheStripIsACardOfTheSkinsSurface)
{
  Fixture f;
  auto const image = f.paint ();
  for (auto const &strip : f.strips.strips ())
    {
      auto const x = strip.whole.getX ()
                     + juce::roundToInt (theme ().padding / 2.f);
      EXPECT_EQ (image.getPixelAt (x, strip.instruments.getCentreY ()),
                 toColour (theme ().surface));
    }
}

TEST (FpvStripsPaint, ThePlateWearsTheChannelsColourAtMostAsLightAsACaption)
{
  Fixture f;
  auto const image = f.paint ();
  for (size_t ch = 0; ch < 4; ++ch)
    {
      auto const plate = fpvHeaderPlate (f.strips.strips ()[ch].header);
      auto const at = faceOf (plate);
      auto const want = fpvPlateColour (toColour (theme ().channel[ch]));
      EXPECT_EQ (image.getPixelAt (at.x, at.y), want) << ch;
    }
}

TEST (FpvStripsPaint, ABrightChannelsPlateIsDimmedToTheCaptionsLightness)
{
  auto const before = theme ();
  auto muted = before;
  muted.textMuted = { 150, 151, 166 }; // quiet-indigo-2's captions, L* 63
  setTheme (muted);

  auto const caption = lightness (toColour (theme ().textMuted));
  auto const yellow = juce::Colour (247, 208, 2);
  ASSERT_GT (lightness (yellow), caption);
  auto const plate = fpvPlateColour (yellow);
  EXPECT_NEAR (lightness (plate), caption, 1.f);
  EXPECT_NEAR (plate.getHue (), yellow.getHue (), 0.01f) << "same hue";

  setTheme (before);
}

TEST (FpvStripsPaint, ADarkerChannelsPlateIsItsOwnColour)
{
  auto const crimson = juce::Colour (40, 4, 18);
  ASSERT_LT (lightness (crimson), lightness (toColour (theme ().textMuted)));
  EXPECT_EQ (fpvPlateColour (crimson), crimson);
}

// Black on a light plate, the skin's text colour on a dark one: whichever of
// the two reads better, so no channel colour leaves its number unreadable.
TEST (FpvStripsPaint, ThePlateInkReadsOnEveryChannel)
{
  for (size_t ch = 0; ch < 4; ++ch)
    {
      auto const plate = fpvPlateColour (toColour (theme ().channel[ch]));
      EXPECT_GE (contrastRatio (fpvInkOn (plate), plate), minimumInkContrast)
          << ch;
    }
}

// Play state as a key, as StemDeck's: lit in play's colour while the clip
// runs, the idle face while it does not.
TEST (FpvStripsPaint, ThePlayKeyLightsWhileItsClipRuns)
{
  Fixture f;
  auto const image = f.paint ();
  auto const running = faceOf (fpvPlayKey (f.strips.strips ()[0].clip));
  auto const idle = faceOf (fpvPlayKey (f.strips.strips ()[1].clip));
  EXPECT_EQ (image.getPixelAt (running.x, running.y),
             transportColour (TransportKey::PlayPause));
  EXPECT_EQ (image.getPixelAt (idle.x, idle.y),
             toColour (theme ().surfaceRaised));
}

TEST (FpvStripsPaint, ThePlayKeyIsASquareAtTheRowsEnd)
{
  Fixture f;
  auto const &clip = f.strips.strips ()[0].clip;
  auto const key = fpvPlayKey (clip);
  EXPECT_TRUE (clip.toFloat ().contains (key));
  EXPECT_FLOAT_EQ (key.getWidth (), key.getHeight ());
  EXPECT_GT (key.getX (), static_cast<float> (clip.getCentreX ()));
}

// A bar's empty part is a recess in the skin's ground, the fader slot of a
// desk, rather than a white wash.
TEST (FpvStripsPaint, AnEmptySlotIsTheSkinsWell)
{
  Fixture f;
  auto const image = f.paint ();
  auto const &inst = f.strips.strips ()[2].instruments; // pots 1, 0, 0
  auto const yFreq = inst.getY () + inst.getHeight () / 2;
  auto const xRight = inst.getRight () - inst.getWidth () / 20;
  EXPECT_EQ (image.getPixelAt (xRight, yFreq), toColour (theme ().background));
}

// A skin whose ground and surface are one grey still shows where an empty
// bar is: the slot carries a hairline, as a key does.
TEST (FpvStripsPaint, AnEmptySlotCanBeFoundOnAFlatSkin)
{
  auto const before = theme ();
  auto flat = before;
  flat.background = flat.surface = { 12, 12, 16 };
  setTheme (flat);
  {
    Fixture f;
    auto const image = f.paint ();
    auto const &inst = f.strips.strips ()[1].instruments; // pots all 0
    auto const third = inst.getHeight () / 3;
    auto const ground = toColour (theme ().surface);
    auto marked = 0;
    for (int y = inst.getY () + third; y < inst.getY () + 2 * third; ++y)
      for (int x = inst.getCentreX (); x < inst.getRight () - inst.getWidth () / 10; ++x)
        marked += image.getPixelAt (x, y) != ground ? 1 : 0;
    EXPECT_GT (marked, 0);
  }
  setTheme (before);
}

// The meter is the desk's display meter: silent, it is an empty well with
// its yellow and red marks beside it.
TEST (FpvStripsPaint, ASilentMeterShowsItsWellAndMarks)
{
  Fixture f;
  f.strips.channelLevel = [] (int) { return VuReading{}; };
  auto const image = f.paint ();
  auto const &m = f.strips.strips ()[1].meter;
  auto marks = 0;
  for (int y = m.getY (); y < m.getBottom (); ++y)
    for (int x = m.getX (); x < m.getRight (); ++x)
      marks += image.getPixelAt (x, y) == toColour (theme ().textMuted) ? 1 : 0;
  EXPECT_GT (marks, 0);
}

// The look this was built for, in its own skin -- the soft channel set and
// StemDeck's greys -- painted in each state the row can be in.
TEST (FpvStripsPaint, ItPaintsInTheStemdeckSkin)
{
  auto const before = theme ();
  auto const file = juce::File (A3_CONFIG_JSON_PATH)
                        .getParentDirectory ()
                        .getChildFile ("skins")
                        .getChildFile ("stemdeck.json");
  ASSERT_TRUE (file.existsAsFile ());
  setTheme (loadTheme (juce::JSON::parse (file)));
  {
    Fixture f;
    f.strips.setBounds (0, 0, 768, 330);
    f.strips.setChannels (fourChannels ());
    writeSnapshot (f.paint (), "fpv-strips-stemdeck.png");
    f.strips.setChannels (orbiting ());
    writeSnapshot (f.paint (), "fpv-strips-stemdeck-orbit.png");
    // -5 dBFS with a held -2: the desk shows six LEDs, the top one yellow,
    // and the held red seventh.
    f.strips.channelLevel = [] (int) {
      return VuReading{ std::pow (10.f, -5.f / 20.f), std::pow (10.f, -2.f / 20.f) };
    };
    writeSnapshot (f.paint (), "fpv-strips-stemdeck-minus5.png");
    f.strips.channelLevel = [] (int) { return VuReading{}; };
    auto rest = fourChannels ();
    for (auto &channel : rest)
      {
        channel.playing = false;
        channel.pots = {};
      }
    f.strips.setChannels (rest);
    writeSnapshot (f.paint (), "fpv-strips-stemdeck-rest.png");

    for (size_t ch = 0; ch < 4; ++ch)
      {
        auto const plate = fpvPlateColour (toColour (theme ().channel[ch]));
        EXPECT_GE (contrastRatio (fpvInkOn (plate), plate), 4.5f) << ch;
      }
  }
  setTheme (before);
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
  light[2].escortMass = FlightTuning{}.groupMass;
  f.strips.setChannels (light);
  auto const &row = f.strips.strips ()[2].clip;
  auto const small = textColouredPixels (f.paint (), row);
  auto heavy = orbiting ();
  heavy[2].escortMass = FlightTuning{}.hotspotMass;
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
