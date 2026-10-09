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

#pragma once

#include <a3-motion-engine/flight/FlightField.hh>
#include <a3-motion-engine/flight/PilotGames.hh>
#include <a3-motion-ui/components/fpv/FpvFloor.hh>

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <optional>

namespace a3
{

/** "FAKE-OUT", "FORMATION", "HIDE", "CALL": what the DJ reads under a ship
 *  that plays a game. Empty for no game, which draws nothing. */
juce::String gameWord (PilotGame game);

/** How strongly a game's word and its AUTO tag are inked. A game the DJ
 *  started with a pad is in full ink with no tag; one a pilot started on its
 *  own is dimmer and says so. */
struct GameInk
{
  float word = 1.f;
  float tag = 0.f;
  bool showsTag = false;
};
GameInk gameInk (bool byPilot);

/** Per ship, the group its game is played against, for the ships that get a
 *  line to it: only the game's leader, so a formation of four draws one line
 *  and not four. Nothing for a game without a target or whose group is gone. */
std::array<std::optional<int>, fpvShips>
gameLines (std::array<std::optional<ShipGame>, fpvShips> const &games,
           FlightBodies const &bodies);

/** Where a label of `size` goes: centred under a ship `shipLength` long, and
 *  moved back inside `floor` when it would stick out. */
juce::Rectangle<float> gameLabelBox (juce::Point<float> shipCentre, float shipLength,
                                     juce::Point<float> size,
                                     juce::Rectangle<float> floor);

constexpr float gameWordOfFont = 0.8f;  // the word, against the body font
constexpr float gameTagOfWord = 0.7f;   // the AUTO tag, against the word
constexpr float gameGapOfWord = 0.4f;   // the space between word and tag
constexpr float gameDashOfStroke = 4.f; // dash and gap of the leader's line, in strokes
constexpr float gameDropOfShip = 0.5f;  // the label starts this far below the centre, in ship lengths

/** One label to paint, in pixels. */
struct GameLabelPaint
{
  juce::Point<float> shipCentre;
  float shipLength = 0.f;
  juce::Colour colour;       // the ship's channel colour
  PilotGame game = PilotGame::None;
  bool byPilot = false;
  float fontHeight = 1.f;    // the body font; the word is gameWordOfFont of it
  juce::Rectangle<float> floor;
};

void paintGameLabel (juce::Graphics &g, GameLabelPaint const &label);

}
