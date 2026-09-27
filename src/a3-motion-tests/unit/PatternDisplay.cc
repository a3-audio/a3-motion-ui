/*

  A3 Motion UI
  Copyright (C) 2026 Patric Schmitz

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

#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/PatternLibrary.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>
#include <a3-motion-ui/components/PatternDisplay.hh>

using namespace a3;

namespace
{
juce::File
aLibraryWithACrossAndALine ()
{
  auto const root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("a3-pattern-display");
  root.deleteRecursively ();
  root.getChildFile ("system").createDirectory ();
  root.getChildFile ("clips/user").createDirectory ();

  auto const header = [] (char const *name) {
    return juce::String ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                         "<svg xmlns=\"http://www.w3.org/2000/svg\" "
                         "viewBox=\"-1 -1 2 2\" data-name=\"")
           + name + "\" data-beats=\"8\" data-ppqn=\"128\">";
  };

  root.getChildFile ("system/08_Cross.svg")
      .replaceWithText (header ("Cross")
                        + "<circle cx=\"1.0\" cy=\"0.0\" r=\"0.05\"/>"
                          "<circle cx=\"0.0\" cy=\"-1.0\" r=\"0.05\"/>"
                          "<circle cx=\"-1.0\" cy=\"0.0\" r=\"0.05\"/>"
                          "<circle cx=\"0.0\" cy=\"1.0\" r=\"0.05\"/></svg>");
  root.getChildFile ("system/08_Line.svg")
      .replaceWithText (header ("Line")
                        + "<path d=\"M -0.5 0 L 0.5 0\"/></svg>");
  return root;
}

std::shared_ptr<Pattern>
loaded (PatternLibrary &library, char const *name)
{
  return library.loadPattern (library.indexForName (name));
}
}

// A shape of dots has no line: every step of it is a jump or a hold. What the
// sphere shows of it is its dots, from the file -- whichever route asks, the
// first registration or the refresh after a knob was turned. The refresh used
// to draw it from the ticks, which gave nothing, so Cross, Corner and Bounce
// vanished from the sphere the moment a MOTION knob moved.
TEST (PatternDisplay, AShapeOfDotsIsShownByItsDots)
{
  PatternLibrary library (aLibraryWithACrossAndALine ());
  library.refresh ();
  auto const cross = loaded (library, "Cross");
  ASSERT_NE (cross, nullptr);

  auto const shown = patternDisplayFor (*cross, library);

  EXPECT_EQ (shown.jumpDots.size (), 4u);
  EXPECT_TRUE (shown.path.isEmpty ());
}

TEST (PatternDisplay, AShapeWithALineIsDrawnFromItsTicks)
{
  PatternLibrary library (aLibraryWithACrossAndALine ());
  library.refresh ();
  auto const line = loaded (library, "Line");
  ASSERT_NE (line, nullptr);

  auto const shown = patternDisplayFor (*line, library);

  EXPECT_TRUE (shown.jumpDots.empty ());
  EXPECT_FALSE (shown.path.isEmpty ());
}

// A take has no file yet, so nothing in the library to ask: its ticks.
TEST (PatternDisplay, ATakeIsDrawnFromItsTicks)
{
  PatternLibrary library (aLibraryWithACrossAndALine ());
  library.refresh ();

  Pattern take;
  take.setName ("Rec_nowhere");
  take.resize (4);
  take.setTick (0, Pos::fromCartesian (0.f, 0.f, 0.f));
  take.setTick (1, Pos::fromCartesian (0.1f, 0.f, 0.f));
  take.setTick (2, Pos::fromCartesian (0.2f, 0.f, 0.f));
  take.setTick (3, Pos::fromCartesian (0.3f, 0.f, 0.f));
  take.markComplete ();

  auto const shown = patternDisplayFor (take, library);

  EXPECT_TRUE (shown.jumpDots.empty ());
  EXPECT_FALSE (shown.path.isEmpty ());
}

// The elevation picture draws what the sphere draws, from the same answer: a
// shape of dots as its dots, a line as its line. It drew every clip from its
// ticks, and a shape of jumps and holds is no line at all -- Cross was an
// empty circle there.
TEST (PatternDisplay, TheElevationPictureShowsADotShapeByItsDots)
{
  PatternLibrary library (aLibraryWithACrossAndALine ());
  library.refresh ();
  HeightMapSphere heightMap;
  auto const cross = loaded (library, "Cross");
  ASSERT_NE (cross, nullptr);

  auto const figure = elevationFigureFor (*cross, library, heightMap, {}, 96);

  EXPECT_EQ (figure.dots.size (), 4u);
  EXPECT_TRUE (figure.line.empty ());
}

TEST (PatternDisplay, TheElevationPictureShowsALineByItsLine)
{
  PatternLibrary library (aLibraryWithACrossAndALine ());
  library.refresh ();
  HeightMapSphere heightMap;
  auto const line = loaded (library, "Line");
  ASSERT_NE (line, nullptr);

  auto const figure = elevationFigureFor (*line, library, heightMap, {}, 96);

  EXPECT_TRUE (figure.dots.empty ());
  EXPECT_GE (figure.line.size (), 2u);
  EXPECT_LE (figure.line.size (), 96u);
}

// ── Which clips are drawn (2026-09-27) ─────────────────────────────────────

namespace
{
ClipGrid
none ()
{
  return {};
}

bool
has (std::vector<DrawnClip> const &drawn, index_t channel, index_t slot)
{
  for (auto const &clip : drawn)
    if (clip.channel == channel && clip.slot == slot)
      return true;
  return false;
}
}

// The selected clip is drawn, playing or not: it is the preview of what the
// bar describes. Nothing else that is not playing.
TEST (PatternDisplay, TheSelectedClipIsDrawnWhenItIsNotPlaying)
{
  auto filled = none ();
  filled[1][0] = filled[2][1] = filled[3][0] = true;

  auto const drawn = clipsToDraw (none (), filled, 1, 0);

  ASSERT_EQ (drawn.size (), 1u);
  EXPECT_EQ (drawn[0].channel, 1u);
  EXPECT_EQ (drawn[0].slot, 0u);
  EXPECT_TRUE (drawn[0].selected);
}

// Every playing clip, in whichever slot, and the selected one last so it is
// drawn over them.
TEST (PatternDisplay, PlayingClipsAreDrawnAndTheSelectedOneLast)
{
  auto filled = none ();
  auto running = none ();
  filled[0][1] = running[0][1] = true;
  filled[2][0] = running[2][0] = true;
  filled[1][0] = true;

  auto const drawn = clipsToDraw (running, filled, 1, 0);

  ASSERT_EQ (drawn.size (), 3u);
  EXPECT_TRUE (has (drawn, 0, 1));
  EXPECT_TRUE (has (drawn, 2, 0));
  EXPECT_EQ (drawn.back ().channel, 1u);
  EXPECT_TRUE (drawn.back ().selected);
  EXPECT_FALSE (drawn.front ().selected);
}

TEST (PatternDisplay, ASelectedClipThatPlaysIsDrawnOnce)
{
  auto filled = none ();
  auto running = none ();
  filled[3][1] = running[3][1] = true;

  auto const drawn = clipsToDraw (running, filled, 3, 1);

  ASSERT_EQ (drawn.size (), 1u);
  EXPECT_TRUE (drawn[0].selected);
}

TEST (PatternDisplay, AnEmptySelectedSlotDrawsNothing)
{
  EXPECT_TRUE (clipsToDraw (none (), none (), 0, 0).empty ());
}
