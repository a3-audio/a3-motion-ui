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

#include <a3-motion-engine/ActionScript.hh>
#include <a3-motion-engine/ScriptLine.hh>

using namespace a3;

// ACTION writes into the script (2026-09-29): one line changes, the rest of
// the file stays as the person wrote it.

TEST (ScriptLine, ALiveLineGetsTheNewValueAndKeepsItsComment)
{
  auto const out = setScriptLine ("~spin = 3;               // -8..8   bars\n",
                                  "spin", "4");
  EXPECT_EQ (out, "~spin = 4;               // -8..8   bars\n");
}

TEST (ScriptLine, ACommentedLineIsUncommented)
{
  auto const out = setScriptLine ("//~spin = 0;             // -8..8   bars\n",
                                  "spin", "-2");
  EXPECT_EQ (out, "~spin = -2;              // -8..8   bars\n");
}

TEST (ScriptLine, EveryOtherLineStaysByteForByte)
{
  juce::String const source = "// my note\n~reach = 0.5;  // wide\n"
                              "~spin = 3;   // turn\n\n// end\n";
  auto const out = setScriptLine (source, "spin", "5");
  EXPECT_EQ (out, "// my note\n~reach = 0.5;  // wide\n"
                  "~spin = 5;               // turn\n\n// end\n");
}

TEST (ScriptLine, AnExpressionBecomesTheNumber)
{
  auto const out = setScriptLine ("~spin = rrand(-4, 4);    // -8..8\n",
                                  "spin", "2");
  EXPECT_EQ (out, "~spin = 2;               // -8..8\n");
}

TEST (ScriptLine, AMissingLineGoesIntoItsSection)
{
  juce::String const source = "// ---- Accent ---------------------------------"
                              "---------------------------\n"
                              "~attack = 2;\n"
                              "\n"
                              "// ---- Other -----\n";
  auto const out = setScriptLine (source, "decay", "4");
  auto const lines = juce::StringArray::fromLines (out);
  EXPECT_TRUE (lines[2].startsWith ("~decay = 4;")) << out;
  EXPECT_EQ (lines[3], "") << "the blank line that ends the section stays";
}

TEST (ScriptLine, AMissingLineWithoutItsSectionGoesAtTheEnd)
{
  auto const out = setScriptLine ("~spin = 1;\n", "decay", "4");
  EXPECT_TRUE (out.startsWith ("~spin = 1;\n~decay = 4;")) << out;
  EXPECT_TRUE (out.endsWithChar ('\n'));
}

TEST (ScriptLine, TheLastOfTwoLinesIsTheOneChanged)
{
  auto const out = setScriptLine ("~spin = 1;\n~spin = 2;\n", "spin", "7");
  EXPECT_EQ (out, "~spin = 1;\n~spin = 7;\n");
}

TEST (ScriptLine, ALiveLineWinsOverACommentedOne)
{
  auto const out = setScriptLine ("~spin = 1;\n//~spin = 0;\n", "spin", "7");
  EXPECT_EQ (out, "~spin = 7;\n//~spin = 0;\n");
}

TEST (ScriptLine, ANameInsideAnotherLinesCommentIsNotALine)
{
  auto const out = setScriptLine ("~reach = 1;  // like ~spin = 3\n", "spin", "2");
  EXPECT_TRUE (out.startsWith ("~reach = 1;  // like ~spin = 3\n")) << out;
  EXPECT_TRUE (out.contains ("~spin = 2;"));
}

TEST (ScriptLine, AClipLineIsLeftAlone)
{
  auto const out = setScriptLine ("~clip = \"Peak Anthem\";\n~clipTop = 0;\n",
                                  "clipTop", "0.25");
  EXPECT_EQ (out, "~clip = \"Peak Anthem\";\n~clipTop = 0.25;\n");
}

TEST (ScriptLine, UnsetCommentsTheLineOut)
{
  auto const out = unsetScriptLine ("~spin = 4;               // -8..8\n", "spin");
  EXPECT_EQ (out, "//~spin = 4;             // -8..8\n");
}

TEST (ScriptLine, UnsetOfAMissingOrCommentedLineChangesNothing)
{
  juce::String const source = "//~spin = 4;  // x\n~reach = 1;\n";
  EXPECT_EQ (unsetScriptLine (source, "spin"), source);
  EXPECT_EQ (unsetScriptLine (source, "decay"), source);
}

TEST (ScriptLine, TheSettingIsWrittenAsAScriptWritesIt)
{
  ClipSettings s;
  s.spin = 4;
  s.reach = 0.25f;
  s.actMode = ActMode::Hold;
  EXPECT_EQ (writtenSettingFor (s, "spin"), "4");
  EXPECT_EQ (writtenSettingFor (s, "reach"), "0.25");
  EXPECT_EQ (writtenSettingFor (s, "act"), "\\hold");
  EXPECT_EQ (writtenSettingFor (s, "nonsense"), "");
}

// Round trip: what is written reads back as the same value.
TEST (ScriptLine, AWrittenValueReadsBack)
{
  ClipSettings s;
  s.envelopeMax = 0.35f;
  auto const source = setScriptLine (actionScriptTemplate (), "envelopeMax",
                                     writtenSettingFor (s, "envelopeMax"));
  auto const result = runActionScript (source, ClipSettings{}, 1);
  EXPECT_TRUE (result.errors.isEmpty ()) << result.errors.joinIntoString ("; ");
  EXPECT_FLOAT_EQ (result.settings.envelopeMax, 0.35f);
}

// Every value the page can write reads back as itself, from the template.
TEST (ScriptLine, EveryNameRoundTrips)
{
  ClipSettings s;
  s.spin = -3;
  s.reach = -0.5f;
  s.envelopeAttack = 5;
  s.freqMax = 0.4f;
  s.actMode = ActMode::Hold;
  int checked = 0;
  for (auto const &name : actionScriptNames ())
    {
      auto const written = writtenSettingFor (s, name);
      if (written.isEmpty ())
        continue; // then/clip: not settings
      ++checked;
      auto const source = setScriptLine (actionScriptTemplate (), name, written);
      auto const result = runActionScript (source, ClipSettings{}, 1);
      EXPECT_TRUE (result.errors.isEmpty ()) << name;
      EXPECT_EQ (writtenSettingFor (result.settings, name), written) << name;
    }
  EXPECT_GT (checked, 20) << "every setting, not none";
}

// Review 2026-09-29: prose that mentions a name is not a commented-out line.
// Only `//~name` is -- README.scd and scripts that document their names this
// way would otherwise lose a line of prose to a knob turn.
TEST (ScriptLine, AProseCommentNamingALineIsLeftAlone)
{
  auto const out = setScriptLine ("// ~spin is how fast it turns\n", "spin", "3");
  EXPECT_TRUE (out.startsWith ("// ~spin is how fast it turns\n")) << out;
  EXPECT_TRUE (out.contains ("~spin = 3;")) << out;
}
