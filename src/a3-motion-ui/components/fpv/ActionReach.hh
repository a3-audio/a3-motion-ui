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

/** What an action's Pilot section asks for at a press: in FPV the whole
 *  order goes to the channel's pilot; FULL ignores the section, because the
 *  games are played on the floor FPV shows. Nothing when the script names no
 *  game. The motion keys are not decided here: they fly a ship by its mode
 *  (ORBIT), in either view. */
std::optional<PilotOrder> pilotOrderAtPress (AppView view, PilotOrder const &order);

/** The readout after a press that asked a game: "CH1 A3 GAME FAKEOUT", and
 *  "CH1 A3 NO GAME" for a \none that calls one off. */
juce::String pilotReadout (int channel, juce::String const &padName, PilotGame game);

}
