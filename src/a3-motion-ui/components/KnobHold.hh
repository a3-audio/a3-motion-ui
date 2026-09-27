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

#pragma once

#include <a3-motion-engine/KnobLanes.hh>

#include <a3-motion-ui/components/ClipKnobs.hh>

#include <array>
#include <optional>

namespace a3
{

/** Where a recordable knob stands in the clip bar: its section and its place
 *  in it. */
struct KnobPlace
{
  int section = 0;
  int sub = 0;
};

constexpr KnobPlace
placeOf (Knob knob)
{
  switch (knob)
    {
    case Knob::Rotate: return { motionSection, 0 };
    case Knob::Spin: return { motionSection, 1 };
    case Knob::Reach: return { motionSection, 2 };
    case Knob::Swell: return { motionSection, 3 };
    case Knob::SqueezeX: return { motionSection, 4 };
    case Knob::StretchX: return { motionSection, 5 };
    case Knob::SqueezeY: return { motionSection, 6 };
    case Knob::StretchY: return { motionSection, 7 };
    case Knob::ClipBottom: return { elevationSection, 0 };
    case Knob::ClipTop: return { elevationSection, 1 };
    case Knob::Sway: return { elevationSection, 2 };
    case Knob::Elevation: return { elevationSection, 3 };
    }
  return {};
}

/** The knob at a place, if a take records it. */
constexpr std::optional<Knob>
knobAt (int section, int sub)
{
  for (int k = 0; k < numKnobs; ++k)
    {
      auto const place = placeOf (static_cast<Knob> (k));
      if (place.section == section && place.sub == sub)
        return static_cast<Knob> (k);
    }
  return {};
}

/** Whether a hand is on a knob: a finger on the screen from the press to the
 *  lift, or an encoder turned in the last moment -- an encoder has no touch,
 *  so its turning is the only sign of a hand. What a take records, and where
 *  the hand wins over a lane. */
class KnobHold
{
public:
  static constexpr double encoderHoldMs = 400.0;

  void press (Knob knob);
  void release (Knob knob);
  void nudge (Knob knob, double nowMs);
  bool isHeld (Knob knob, double nowMs) const;
  void clear ();

private:
  std::array<bool, numKnobs> _pressed{};
  std::array<std::optional<double>, numKnobs> _nudgedAt{};
};

}
