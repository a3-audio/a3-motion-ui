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

#include "GameLabel.hh"

#include <a3-motion-ui/components/fpv/BodyLook.hh>
#include <a3-motion-ui/theme/Theme.hh>

#include <algorithm>

namespace a3
{

namespace
{
float
textWidth (juce::Font const &font, juce::String const &text)
{
  juce::GlyphArrangement glyphs;
  glyphs.addLineOfText (font, text, 0.f, 0.f);
  return glyphs.getBoundingBox (0, -1, true).getWidth ();
}

juce::Font
fontOfHeight (float height)
{
  return juce::Font (juce::FontOptions (height));
}

/** `start` moved so that `length` fits inside [low, high]; the low edge wins
 *  when it cannot. */
float
keptInside (float start, float length, float low, float high)
{
  return std::max (low, std::min (start, high - length));
}
}

juce::String
gameWord (PilotGame game)
{
  switch (game)
    {
    case PilotGame::FakeOut:
      return "FAKE-OUT";
    case PilotGame::Formation:
      return "FORMATION";
    case PilotGame::HideAndSeek:
      return "HIDE";
    case PilotGame::CallAndResponse:
      return "CALL";
    case PilotGame::None:
      return {};
    }
  return {};
}

GameInk
gameInk (bool byPilot)
{
  if (!byPilot)
    return { 1.f, 0.f, false };
  return { theme ().alphaSecondary, theme ().alphaMuted, true };
}

bool
sameGame (std::optional<ShipGame> const &a, std::optional<ShipGame> const &b)
{
  if (a.has_value () != b.has_value ())
    return false;
  return !a || (a->game == b->game && a->leader == b->leader && a->byPilot == b->byPilot);
}

HeldGame
heldGame (std::optional<ShipGame> const &now, std::optional<ShipGame> const &last,
          double endedAtBeat, double beats, int beatsPerBar)
{
  if (now && now->game != PilotGame::None)
    return { now, 1.f };
  if (!last || last->game == PilotGame::None || beatsPerBar <= 0)
    return {};
  auto const elapsedBars = (beats - endedAtBeat) / beatsPerBar;
  if (!(elapsedBars >= 0.0) || elapsedBars >= 1.0)
    return {};
  return { last, static_cast<float> (1.0 - elapsedBars) };
}

std::string
gameStartLine (int channel, ShipGame const &game)
{
  auto const target = game.target == noBodyId ? juce::String ("none") : bodyLabel (game.target);
  return (juce::String ("A3 Motion: game start ch") + juce::String (channel + 1) + " "
          + gameWord (game.game) + " by " + (game.byPilot ? "pilot" : "dj") + " leader ch"
          + juce::String (game.leader + 1) + " target " + target)
      .toStdString ();
}

std::string
gameEndLine (int channel, PilotGame game)
{
  return (juce::String ("A3 Motion: game end ch") + juce::String (channel + 1) + " "
          + gameWord (game))
      .toStdString ();
}

std::array<std::optional<int>, fpvShips>
gameLines (std::array<std::optional<ShipGame>, fpvShips> const &games,
           FlightBodies const &bodies)
{
  std::array<std::optional<int>, fpvShips> lines{};
  for (size_t ship = 0; ship < games.size (); ++ship)
    {
      if (!games[ship] || games[ship]->game == PilotGame::None
          || games[ship]->leader != static_cast<int> (ship)
          || games[ship]->target == noBodyId)
        continue;
      for (auto i = 0; i < bodies.count; ++i)
        if (bodies.body[static_cast<size_t> (i)].id == games[ship]->target)
          lines[ship] = games[ship]->target;
    }
  return lines;
}

juce::Rectangle<float>
gameLabelBox (juce::Point<float> shipCentre, float shipLength, juce::Point<float> size,
              juce::Rectangle<float> floor)
{
  auto const x = shipCentre.x - size.x / 2.f;
  auto const y = shipCentre.y + shipLength * gameDropOfShip;
  return { keptInside (x, size.x, floor.getX (), floor.getRight ()),
           keptInside (y, size.y, floor.getY (), floor.getBottom ()), size.x, size.y };
}

void
paintGameLabel (juce::Graphics &g, GameLabelPaint const &label)
{
  auto const word = gameWord (label.game);
  if (word.isEmpty () || label.alpha <= 0.f)
    return;

  auto const ink = gameInk (label.byPilot);
  auto const wordFont = fontOfHeight (label.fontHeight * gameWordOfFont);
  auto const tagFont = fontOfHeight (wordFont.getHeight () * gameTagOfWord);
  auto const gap = wordFont.getHeight () * gameGapOfWord;
  auto const wordWidth = textWidth (wordFont, word);
  auto const tagWidth = ink.showsTag ? textWidth (tagFont, "AUTO") : 0.f;
  auto const width = wordWidth + (ink.showsTag ? gap + tagWidth : 0.f);

  auto const box = gameLabelBox (label.shipCentre, label.shipLength,
                                 { width, wordFont.getHeight () }, label.floor);

  g.setFont (wordFont);
  g.setColour (label.colour.withMultipliedAlpha (ink.word * label.alpha));
  g.drawText (word, box.withWidth (wordWidth), juce::Justification::centredLeft, false);

  if (!ink.showsTag)
    return;
  g.setFont (tagFont);
  g.setColour (label.colour.withMultipliedAlpha (ink.tag * label.alpha));
  g.drawText ("AUTO", box.withTrimmedLeft (wordWidth + gap), juce::Justification::centredLeft,
              false);
}

}
