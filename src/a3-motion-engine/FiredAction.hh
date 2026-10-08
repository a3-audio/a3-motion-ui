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

#include <a3-motion-engine/ActionScript.hh>
#include <a3-motion-engine/PilotOrder.hh>
#include <a3-motion-engine/flight/FlightMotion.hh>

namespace a3
{

/** What one press of an action button does (FPV): the settings the
 *  clip wears for the accent, what a flying ship does meanwhile, and what the
 *  channel's pilot is asked to play. Worked out by one run of the script, so
 *  one throw of the dice lands all three. */
struct FiredAction
{
  ClipSettings settings;
  FlightMotion flight;
  PilotOrder pilot;
};

/** The Motion keys the script assigned, as a flying ship reads them; a key
 *  the script left commented is not set, whatever the clip has. */
FlightMotion flightMotionFrom (ActionScriptResult const &result);

/** resolveActionAt (source, base, seed, feel), and the ship's and the pilot's
 *  share of the same run. */
FiredAction fireActionAt (juce::String const &source, ClipSettings const &base,
                          juce::int64 seed, ActionFeel const &feel);

}
