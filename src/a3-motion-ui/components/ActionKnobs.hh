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

#pragma once

#include <a3-motion-engine/Envelope.hh>

namespace a3
{

/** What one of the ACTION page's nine knobs is: its scale, where two taps
 *  put it back, and whether it counts whole steps.
 *
 *  Its own table rather than a chain of ifs in the page, so the scale a knob
 *  is dragged on and the value the engine is told cannot drift apart -- and
 *  so it can be checked without building the page, which drags the browser,
 *  the script editor and the engine in with it.
 *
 *  The order is the page's: attack, decay, ceiling, three times over (accent,
 *  cutoff, resonance). */
struct ActionKnobSpec
{
  double max = envelopeMaxStep;
  /** 0 for a continuous value, 1 for whole steps. */
  double interval = 1.0;
  double resetTo = envelopeMaxStep / 2;
};

/** Whether this one is a row's ceiling rather than one of its two stages. */
constexpr bool
actionKnobIsACeiling (int control)
{
  return control % 3 == 2;
}

constexpr ActionKnobSpec
actionKnobSpec (int control)
{
  if (!actionKnobIsACeiling (control))
    return {};

  // Half for the accent's ceiling; off for a filter's -- a sweep nobody asked
  // for is a sweep in the middle of a set.
  return { 1.0, 0.0, control == 2 ? 0.5 : 0.0 };
}

}
