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
#include <a3-motion-engine/flight/PilotLevel.hh>

#include <juce_core/juce_core.h>

namespace a3
{

/** What the level key wears. */
struct PilotKeyFace
{
  /** PILOTS (off), HINT, FLY. */
  juce::String word;
  /** The lifted face: the pilots are doing something. */
  bool on = false;
  /** FPV only: in FULL the games are called off and the key cannot act. */
  bool available = true;
  /** FLY: the word in the notice colour -- someone else is driving, as the
   *  PIO clock's key says it. */
  bool notice = false;
};

PilotKeyFace pilotKeyFace (PilotLevel level, bool fpv);

/** The level after a DJ's tap in FPV. At FLY, firing a game (a `\none`
 *  too), or firing anything at a ship a pilot is playing with, takes over:
 *  the pilots step back to HINT, which ends their games, and FLY is one
 *  deliberate press of the key away. Below FLY a tap changes no level. */
PilotLevel levelAfterTap (PilotLevel level, bool firesAGame, bool shipInAPilotsGame);

/** "-- PILOTS OFF", "-- PILOTS HINT", "-- PILOTS FLY". */
juce::String pilotLevelReadout (PilotLevel level);

/** "CH2 FLY FAKEOUT": a pilot started a game, on its leader's channel. */
juce::String pilotGameReadout (int channel, PilotGame game);

}
