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

#include "ActionMotion.hh"

#include <a3-motion-engine/ActionScript.hh>
#include <a3-motion-engine/TempoLfo.hh>

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{
size_t
indexOf (MotionParam param)
{
  return static_cast<size_t> (param);
}

int
wholeStep (float value, int lo, int hi)
{
  return std::clamp (static_cast<int> (std::lround (value)), lo, hi);
}

int
sweepStep (float value)
{
  return wholeStep (value, -lfoMaxStep, lfoMaxStep);
}

float
bipolar (float value)
{
  return std::clamp (value, -1.f, 1.f);
}

float
unit (float value)
{
  return std::clamp (value, 0.f, 1.f);
}

constexpr int lastDirection = static_cast<int> (PlayDirection::Random);
constexpr int lastEndAction = static_cast<int> (EndAction::Pause);
}

char const *
motionParamKey (MotionParam param)
{
  switch (param)
    {
    case MotionParam::Spin: return "spin";
    case MotionParam::Rotate: return "rotate";
    case MotionParam::Swell: return "swell";
    case MotionParam::Reach: return "reach";
    case MotionParam::StretchX: return "strX";
    case MotionParam::SqueezeX: return "sqzX";
    case MotionParam::StretchY: return "strY";
    case MotionParam::SqueezeY: return "sqzY";
    case MotionParam::Sway: return "sway";
    case MotionParam::Elevation: return "elevationBase";
    case MotionParam::ClipTop: return "clipTop";
    case MotionParam::ClipBottom: return "clipBottom";
    case MotionParam::TiltSweep: return "tswp";
    case MotionParam::Tilt: return "tilt";
    case MotionParam::RollSweep: return "rswp";
    case MotionParam::Roll: return "roll";
    case MotionParam::Speed: return "speed";
    case MotionParam::Direction: return "direction";
    case MotionParam::EndAction: return "endAction";
    }
  return "";
}

char const *
motionParamScriptName (MotionParam param)
{
  switch (param)
    {
    case MotionParam::Elevation: return "base";
    case MotionParam::Speed: return "speedLog2";
    case MotionParam::Direction: return "dir";
    case MotionParam::EndAction: return "end";
    case MotionParam::Spin:
    case MotionParam::Rotate:
    case MotionParam::Swell:
    case MotionParam::Reach:
    case MotionParam::StretchX:
    case MotionParam::SqueezeX:
    case MotionParam::StretchY:
    case MotionParam::SqueezeY:
    case MotionParam::Sway:
    case MotionParam::ClipTop:
    case MotionParam::ClipBottom:
    case MotionParam::TiltSweep:
    case MotionParam::Tilt:
    case MotionParam::RollSweep:
    case MotionParam::Roll:
      // The set's word and the script's are the same for these.
      return motionParamKey (param);
    }
  return "";
}

float
motionValueOf (ClipSettings const &s, MotionParam param)
{
  switch (param)
    {
    case MotionParam::Spin: return static_cast<float> (s.spin);
    case MotionParam::Rotate: return s.rotate;
    case MotionParam::Swell: return static_cast<float> (s.reachLfo);
    case MotionParam::Reach: return s.reach;
    case MotionParam::StretchX: return static_cast<float> (s.squeezeXLfo);
    case MotionParam::SqueezeX: return s.squeezeX;
    case MotionParam::StretchY: return static_cast<float> (s.squeezeYLfo);
    case MotionParam::SqueezeY: return s.squeezeY;
    case MotionParam::Sway: return static_cast<float> (s.elevationLfo);
    case MotionParam::Elevation: return s.elevationBase;
    case MotionParam::ClipTop: return s.clipTop;
    case MotionParam::ClipBottom: return s.clipBottom;
    case MotionParam::TiltSweep: return static_cast<float> (s.tiltLfo);
    case MotionParam::Tilt: return s.tilt;
    case MotionParam::RollSweep: return static_cast<float> (s.rollLfo);
    case MotionParam::Roll: return s.roll;
    case MotionParam::Speed: return static_cast<float> (s.speedLog2);
    case MotionParam::Direction: return static_cast<float> (s.direction);
    case MotionParam::EndAction: return static_cast<float> (s.endAction);
    }
  return 0.f;
}

ClipSettings
withMotionValue (ClipSettings s, MotionParam param, float value)
{
  switch (param)
    {
    case MotionParam::Spin: s.spin = sweepStep (value); break;
    case MotionParam::Rotate:
      // A turn comes round to itself, as a script's rotate does.
      s.rotate = value - std::floor (value);
      break;
    case MotionParam::Swell: s.reachLfo = sweepStep (value); break;
    case MotionParam::Reach: s.reach = bipolar (value); break;
    case MotionParam::StretchX: s.squeezeXLfo = sweepStep (value); break;
    case MotionParam::SqueezeX: s.squeezeX = bipolar (value); break;
    case MotionParam::StretchY: s.squeezeYLfo = sweepStep (value); break;
    case MotionParam::SqueezeY: s.squeezeY = bipolar (value); break;
    case MotionParam::Sway: s.elevationLfo = sweepStep (value); break;
    case MotionParam::Elevation: s.elevationBase = unit (value); break;
    case MotionParam::ClipTop: s.clipTop = unit (value); break;
    case MotionParam::ClipBottom: s.clipBottom = unit (value); break;
    case MotionParam::TiltSweep: s.tiltLfo = sweepStep (value); break;
    case MotionParam::Tilt: s.tilt = bipolar (value); break;
    case MotionParam::RollSweep: s.rollLfo = sweepStep (value); break;
    case MotionParam::Roll: s.roll = bipolar (value); break;
    case MotionParam::Speed:
      s.speedLog2 = wholeStep (value, speedLog2Min, speedLog2Max);
      break;
    case MotionParam::Direction:
      s.direction = static_cast<PlayDirection> (
          wholeStep (value, 0, lastDirection));
      break;
    case MotionParam::EndAction:
      s.endAction
          = static_cast<EndAction> (wholeStep (value, 0, lastEndAction));
      break;
    }
  return s;
}

bool
MotionOverrides::empty () const
{
  return std::none_of (values.begin (), values.end (),
                       [] (auto const &v) { return v.has_value (); });
}

std::optional<float>
MotionOverrides::get (MotionParam param) const
{
  return values[indexOf (param)];
}

void
MotionOverrides::set (MotionParam param, float value)
{
  values[indexOf (param)] = value;
}

void
MotionOverrides::unset (MotionParam param)
{
  values[indexOf (param)].reset ();
}

bool
operator== (MotionOverrides const &a, MotionOverrides const &b)
{
  return a.values == b.values;
}

ClipSettings
withMotion (ClipSettings settings, MotionOverrides const &motion)
{
  for (auto const param : motionParamOrder)
    if (auto const value = motion.get (param))
      settings = withMotionValue (settings, param, *value);
  return settings;
}

std::array<MotionShown, numMotionParams>
motionShownFor (juce::String const &source, ClipSettings const &base,
                juce::int64 seed, MotionOverrides const &motion)
{
  auto const script = runActionScript (source, base, seed);
  auto const resolved = withMotion (script.settings, motion);

  std::array<MotionShown, numMotionParams> shown{};
  for (auto const param : motionParamOrder)
    {
      auto &at = shown[indexOf (param)];
      at.value = motionValueOf (resolved, param);
      at.source = motion.get (param).has_value () ? MotionSource::Button
                  : script.assigned.contains (motionParamScriptName (param))
                      ? MotionSource::Script
                      : MotionSource::Clip;
    }
  return shown;
}

}
