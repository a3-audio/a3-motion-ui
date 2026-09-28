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

#include "ActionMotionKnobs.hh"

#include <a3-motion-ui/components/ClipSettingsCaptions.hh>
#include <a3-motion-ui/components/ClipSettingsLayout.hh>

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{
/** Where a knob of the tile stands on the MOTION page, as the bar numbers
 *  its sections and subs (see EncoderMap.cc). */
struct PageKnob
{
  int section;
  int sub;
};

PageKnob
pageKnobOf (MotionParam param)
{
  switch (param)
    {
    case MotionParam::Rotate: return { motionSection, 0 };
    case MotionParam::Spin: return { motionSection, 1 };
    case MotionParam::Reach: return { motionSection, 2 };
    case MotionParam::Swell: return { motionSection, 3 };
    case MotionParam::SqueezeX: return { motionSection, 4 };
    case MotionParam::StretchX: return { motionSection, 5 };
    case MotionParam::SqueezeY: return { motionSection, 6 };
    case MotionParam::StretchY: return { motionSection, 7 };
    case MotionParam::Tilt: return { motionSection, 10 };
    case MotionParam::TiltSweep: return { motionSection, 11 };
    case MotionParam::Roll: return { motionSection, 12 };
    case MotionParam::RollSweep: return { motionSection, 13 };
    case MotionParam::ClipBottom: return { elevationSection, 0 };
    case MotionParam::ClipTop: return { elevationSection, 1 };
    case MotionParam::Sway: return { elevationSection, 2 };
    case MotionParam::Elevation: return { elevationSection, 3 };
    case MotionParam::Speed:
    case MotionParam::Direction:
    case MotionParam::EndAction:
      return { -1, -1 };
    }
  return { -1, -1 };
}

int
cycled (float value, int increment, int count)
{
  auto const at = static_cast<int> (std::lround (value));
  return ((at + increment) % count + count) % count;
}
}

bool
motionParamIsAField (MotionParam param)
{
  return pageKnobOf (param).section < 0;
}

ClipKnobSpec
motionTileKnobSpec (MotionParam param)
{
  auto const at = pageKnobOf (param);
  if (at.section == motionSection)
    return motionKnobSpec (at.sub);
  if (at.section == elevationSection)
    return elevationKnobSpec (at.sub);
  return {};
}

char const *
motionParamCaption (MotionParam param)
{
  switch (param)
    {
    case MotionParam::Speed: return caption::speed;
    case MotionParam::Direction: return caption::direction;
    case MotionParam::EndAction: return caption::endAction;
    case MotionParam::Spin:
    case MotionParam::Rotate:
    case MotionParam::Swell:
    case MotionParam::Reach:
    case MotionParam::StretchX:
    case MotionParam::SqueezeX:
    case MotionParam::StretchY:
    case MotionParam::SqueezeY:
    case MotionParam::Sway:
    case MotionParam::Elevation:
    case MotionParam::ClipTop:
    case MotionParam::ClipBottom:
    case MotionParam::TiltSweep:
    case MotionParam::Tilt:
    case MotionParam::RollSweep:
    case MotionParam::Roll:
      return motionTileKnobSpec (param).label;
    }
  return "";
}

float
motionKnobValue (MotionParam param, float value)
{
  return param == MotionParam::Elevation ? knobForElevationBase (value)
                                         : value;
}

float
motionValueForKnob (MotionParam param, double knob, float clipTop,
                    float clipBottom)
{
  auto const level = static_cast<float> (knob);
  if (param == MotionParam::Elevation)
    return elevationBaseForKnob (level, clipTop, clipBottom);
  if (motionTileKnobSpec (param).interval > 0.0)
    return static_cast<float> (std::lround (knob));
  return level;
}

float
steppedMotionValue (MotionParam param, float value, int increment)
{
  switch (param)
    {
    case MotionParam::Speed:
      return static_cast<float> (draggedSpeedLog2 (
          static_cast<int> (std::lround (value)), increment));
    case MotionParam::Direction:
      return static_cast<float> (
          cycled (value, increment, value::numDirections));
    case MotionParam::EndAction:
      return static_cast<float> (
          cycled (value, increment, value::numEndActions));
    case MotionParam::Spin:
    case MotionParam::Rotate:
    case MotionParam::Swell:
    case MotionParam::Reach:
    case MotionParam::StretchX:
    case MotionParam::SqueezeX:
    case MotionParam::StretchY:
    case MotionParam::SqueezeY:
    case MotionParam::Sway:
    case MotionParam::Elevation:
    case MotionParam::ClipTop:
    case MotionParam::ClipBottom:
    case MotionParam::TiltSweep:
    case MotionParam::Tilt:
    case MotionParam::RollSweep:
    case MotionParam::Roll:
      return value;
    }
  return value;
}

}
