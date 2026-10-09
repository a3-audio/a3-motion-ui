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
  if (word.isEmpty ())
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
  g.setColour (label.colour.withMultipliedAlpha (ink.word));
  g.drawText (word, box.withWidth (wordWidth), juce::Justification::centredLeft, false);

  if (!ink.showsTag)
    return;
  g.setFont (tagFont);
  g.setColour (label.colour.withMultipliedAlpha (ink.tag));
  g.drawText ("AUTO", box.withTrimmedLeft (wordWidth + gap), juce::Justification::centredLeft,
              false);
}

}
