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

#include <a3-motion-engine/PilotOrder.hh>
#include <a3-motion-ui/components/AppView.hh>

#include <juce_core/juce_core.h>

#include <optional>

namespace a3
{

/** What an action's Pilot section asks for at a press. FPV: every press
 *  asks -- the order as written when it names a game (\none too); with no
 *  game, its style, \auto when none is written (the music picks); nothing
 *  for \none as a style, which leaves only the floor keys. FULL ignores the
 *  section: the games are played on the floor FPV shows. The motion keys
 *  are not decided here: they fly a ship by its mode (ORBIT), in either
 *  view. */
std::optional<PilotOrder> pilotOrderAtPress (AppView view, PilotOrder const &order);

/** What a press with no figure sends for its ship when a pilot plays with
 *  it: \none, which takes that ship alone out of the pilot's game. */
PilotOrder releasingItsShip ();

/** The readout of a press that left the figure to its style: "CH1 A3 AUTO",
 *  until the figure chosen is known (pilotReadout). */
juce::String pilotStyleReadout (int channel, juce::String const &padName, PilotStyle style);

/** A game as the readouts write it: its word in capitals, but CIRCLE for an
 *  encircle, as the label under the ship says it. */
juce::String readoutWord (PilotGame game);

/** The readout after a press that asked a game: "CH1 A3 GAME FAKEOUT", and
 *  "CH1 A3 NO GAME" for a \none that calls one off. */
juce::String pilotReadout (int channel, juce::String const &padName, PilotGame game);

}
