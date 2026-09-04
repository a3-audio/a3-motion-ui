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

#include <a3-motion-engine/PatternFile.hh>
#include <a3-motion-ui/Helpers.hh>
#include <a3-motion-ui/components/TrajectoryIcon.hh>

#include <cmath>

using namespace a3;

namespace
{

juce::File
systemPatternDir ()
{
  return juce::File (A3_PATTERN_SYSTEM_DIR);
}

/** The longest straight hop the icon's outline makes, next to its typical one.
 *  A stroke that travels has small, even hops; a chord laid across a shape is a
 *  single long one, which is exactly what a stray line is. */
std::pair<float, float>
hopLengths (juce::Path const &path)
{
  std::vector<float> hops;
  juce::Path::Iterator it (path);
  juce::Point<float> previous;
  bool have = false;

  while (it.next ())
    {
      juce::Point<float> here;
      switch (it.elementType)
        {
        case juce::Path::Iterator::startNewSubPath:
          previous = { it.x1, it.y1 };
          have = true;
          continue;
        case juce::Path::Iterator::lineTo: here = { it.x1, it.y1 }; break;
        case juce::Path::Iterator::quadraticTo: here = { it.x2, it.y2 }; break;
        case juce::Path::Iterator::cubicTo: here = { it.x3, it.y3 }; break;
        default: continue;
        }

      if (have)
        hops.push_back (here.getDistanceFrom (previous));
      previous = here;
      have = true;
    }

  if (hops.empty ())
    return { 0.f, 0.f };

  auto sorted = hops;
  std::sort (sorted.begin (), sorted.end ());
  return { sorted[sorted.size () / 2], sorted.back () };
}

// Every shipped shape draws as itself. The pictogram in the Shape section
// showed the roses with a straight line running out of them that is in none of
// the files, so the line is made on the way to the screen.
TEST (SystemPatternIcons, NoShippedShapeGrowsAStrayLine)
{
  auto const files
      = systemPatternDir ().findChildFiles (juce::File::findFiles, false, "*.svg");
  ASSERT_FALSE (files.isEmpty ()) << systemPatternDir ().getFullPathName ();

  for (auto const &file : files)
    {
      auto const peeked = PatternFile::peek (file);
      auto const path = svgDToPath (peeked.pathData);
      if (path.isEmpty ())
        continue; // a shape made of dots has no outline to check

      auto const icon = trajectoryIconFromPath (path, peeked.jumpDots);
      auto const [typical, longest] = hopLengths (icon.path);

      if (typical <= 0.f)
        continue;

      EXPECT_LT (longest, typical * 6.f)
          << file.getFileName () << ": one hop of " << longest
          << " against a typical " << typical;
    }
}

// And the same shape built from a loaded pattern's ticks, which is what the
// Shape section falls back to when the library cannot name the clip.
TEST (SystemPatternIcons, NoShippedShapeGrowsAStrayLineFromItsTicks)
{
  auto const files
      = systemPatternDir ().findChildFiles (juce::File::findFiles, false, "*.svg");
  ASSERT_FALSE (files.isEmpty ());

  for (auto const &file : files)
    {
      auto const pattern = PatternFile::load (file);
      if (!pattern)
        continue;

      auto const icon = trajectoryIconFromTicks (pattern->getTicks ().positions);
      if (icon.path.isEmpty ())
        continue;

      auto const [typical, longest] = hopLengths (icon.path);
      if (typical <= 0.f)
        continue;

      EXPECT_LT (longest, typical * 6.f)
          << file.getFileName () << ": one hop of " << longest
          << " against a typical " << typical;
    }
}

}

// A take that is still being played in, as the Shape section's back sees it:
// the ring is already the full length, the hand has drawn part of the way
// round, and every tick past the write head holds the last position it was
// given. That tail is the whole difficulty — it is valid data, so nothing
// upstream marks it as missing, and it is motionless, so the icon has to
// decide what a mostly-motionless ring means.
namespace
{
std::vector<Pos>
takeInProgress (size_t numTicks, size_t written)
{
  std::vector<Pos> ticks;
  ticks.reserve (numTicks);

  for (size_t i = 0; i < written; ++i)
    {
      auto const a = juce::MathConstants<float>::twoPi
                     * static_cast<float> (i) / static_cast<float> (numTicks);
      ticks.push_back (Pos::fromCartesian (std::cos (a) * 0.6f,
                                           std::sin (a) * 0.6f, 0.5f));
    }

  // The write head has not come round yet; the rest holds where it left off.
  while (ticks.size () < numTicks)
    ticks.push_back (ticks.back ());

  return ticks;
}
}

TEST (TrajectoryIcon, ATakeStillBeingPlayedInHasAnIcon)
{
  // Three quarters round of a 512-tick ring: what you are looking at a couple
  // of seconds into a take. If this has no icon the Shape section shows an
  // empty box for the whole recording, which is the one moment it is meant to
  // be showing something.
  auto const data = trajectoryIconFromTicks (takeInProgress (512, 400));

  EXPECT_TRUE (data.hasIcon);
  EXPECT_FALSE (data.path.isEmpty ());
}

/** What the icon actually has to show. hasIcon on its own says only that the
 *  builder thought it had something, and the whole of this bug was that it
 *  thought so while holding neither a line nor a dot. */
bool
hasSomethingToDraw (TrajectoryIconData const &data)
{
  return !data.path.isEmpty () || !data.jumpDots.empty ();
}

TEST (TrajectoryIcon, ATakeJustStartedIsDrawnAsTheLineItIs)
{
  // A few ticks in: what the Shape section shows for the first second of every
  // take. The unwritten remainder is motionless, so counting held ticks says
  // "tapped" by a landslide -- and a tap take with one plateau yields no dots
  // worth keeping, so the icon came out claiming to have something and having
  // nothing at all.
  auto const data = trajectoryIconFromTicks (takeInProgress (512, 8));

  EXPECT_TRUE (data.hasIcon);
  EXPECT_TRUE (hasSomethingToDraw (data));
  EXPECT_FALSE (data.hasJumpDots)
      << "the hand is drawing, not tapping -- the tail is simply not recorded yet";
}

TEST (TrajectoryIcon, ATakePartWayInIsDrawnAsTheLineItIs)
{
  auto const data = trajectoryIconFromTicks (takeInProgress (512, 120));

  EXPECT_TRUE (hasSomethingToDraw (data));
  EXPECT_FALSE (data.hasJumpDots);
}

TEST (TrajectoryIcon, AnIconNeverClaimsToHaveWhatItCannotDraw)
{
  // The invariant the section relies on: if hasIcon is true something appears.
  // Checked across the whole of a take rather than at one stage, because the
  // stage it failed at was simply the one nobody had looked at.
  for (size_t written : { size_t{ 2 }, size_t{ 8 }, size_t{ 40 },
                          size_t{ 120 }, size_t{ 300 }, size_t{ 512 } })
    {
      auto const data = trajectoryIconFromTicks (takeInProgress (512, written));
      if (data.hasIcon)
        EXPECT_TRUE (hasSomethingToDraw (data))
            << "written = " << written;
    }
}

// ── Where the shipped shapes sit ─────────────────────────────────────────

namespace
{
/** The middle of a shape, weighted by how much of it is where -- so a stretch
 *  that happens to be sampled densely does not drag the answer towards
 *  itself. */
juce::Point<float>
pathCentroid (std::vector<Pos> const &ticks)
{
  double sx = 0.0, sy = 0.0, total = 0.0;

  for (size_t i = 0; i < ticks.size (); ++i)
    {
      auto const &a = ticks[i];
      auto const &b = ticks[(i + 1) % ticks.size ()];
      if (!a.isValid () || !b.isValid ())
        continue;

      auto const w = std::hypot (b.x () - a.x (), b.y () - a.y ());
      sx += (a.x () + b.x ()) * 0.5 * w;
      sy += (a.y () + b.y ()) * 0.5 * w;
      total += w;
    }

  if (total <= 0.0)
    return {};

  return { static_cast<float> (sx / total), static_cast<float> (sy / total) };
}

/** Shapes that are meant to sit to one side, and why. Not a list of things to
 *  fix later -- a list of things that would be wrong if they were centred. */
bool
isDeliberatelyOffCentre (juce::String const &name)
{
  // Arc and Petal are one-sided by construction: a stroke across part of the
  // room, not a figure around it. Orbit is a Kepler ellipse with the listener
  // at a focus rather than the middle, which is the whole idea -- the sound
  // comes close and goes far. Random is random.
  return name.contains ("Arc") || name.contains ("Petal")
         || name.contains ("Orbit") || name.contains ("Random");
}
}

TEST (SystemPatternIcons, EveryShippedShapeSitsWhereItShould)
{
  // Rotation turns about the origin, because the origin is the middle of the
  // room. A shape whose own middle is somewhere else swings round instead of
  // turning in place -- it wobbles. This was true of sixteen of these files,
  // because saving them recentred them on their bounding box.
  auto const files = systemPatternDir ().findChildFiles (
      juce::File::findFiles, false, "*.svg");
  ASSERT_FALSE (files.isEmpty ());

  for (auto const &file : files)
    {
      auto const pattern = PatternFile::load (file);
      ASSERT_NE (pattern, nullptr) << file.getFileName ();

      auto const ticks = pattern->getTicks ().positions;
      auto const centre = pathCentroid (ticks);

      float meanRadius = 0.f;
      int n = 0;
      for (auto const &p : ticks)
        {
          if (!p.isValid ())
            continue;
          meanRadius += std::hypot (p.x () - centre.x, p.y () - centre.y);
          ++n;
        }
      ASSERT_GT (n, 0) << file.getFileName ();
      meanRadius /= static_cast<float> (n);
      ASSERT_GT (meanRadius, 0.f) << file.getFileName ();

      auto const offset = std::hypot (centre.x, centre.y) / meanRadius;

      if (isDeliberatelyOffCentre (file.getFileName ()))
        continue;

      EXPECT_LT (offset, 0.07f)
          << file.getFileName () << " sits " << (offset * 100.f)
          << "% of its own radius off the middle of the room";
    }
}
