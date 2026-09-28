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

#include <a3-motion-engine/ClipSettings.hh>

#include <JuceHeader.h>

#include <array>
#include <optional>

namespace a3
{

/** What an action button can put on the clip besides its feel: every knob of
 *  the MOTION page and CLIP's direction, end and speed (2026-09-28).
 *
 *  In the order the ACTION page's MOTION tile lays them out -- MOTION's eight
 *  fields two to a row, each field's pair as MOTION shows it, then CLIP's
 *  three. `motionParamOrder` is that order; the enum's values index the
 *  arrays below. */
enum class MotionParam
{
  Spin,
  Rotate,
  Swell,
  Reach,
  StretchX,
  SqueezeX,
  StretchY,
  SqueezeY,
  Sway,
  Elevation,
  ClipTop,
  ClipBottom,
  TiltSweep,
  Tilt,
  RollSweep,
  Roll,
  Speed,
  Direction,
  EndAction,
};

constexpr int numMotionParams = 19;

constexpr std::array<MotionParam, numMotionParams> motionParamOrder{
  MotionParam::Spin,      MotionParam::Rotate,     MotionParam::Swell,
  MotionParam::Reach,     MotionParam::StretchX,   MotionParam::SqueezeX,
  MotionParam::StretchY,  MotionParam::SqueezeY,   MotionParam::Sway,
  MotionParam::Elevation, MotionParam::ClipTop,    MotionParam::ClipBottom,
  MotionParam::TiltSweep, MotionParam::Tilt,       MotionParam::RollSweep,
  MotionParam::Roll,      MotionParam::Speed,      MotionParam::Direction,
  MotionParam::EndAction,
};

/** What a set calls it -- the key a slot's overrides already use for the
 *  same field, so one file format has one word for each value. */
char const *motionParamKey (MotionParam param);

/** What a script calls it (`~spin`, `~base`, `~dir`). */
char const *motionParamScriptName (MotionParam param);

/** The value as the settings hold it: a whole number for a sweep, the speed,
 *  the direction and the end (the last two as the enum's index), otherwise
 *  the float itself. */
float motionValueOf (ClipSettings const &settings, MotionParam param);

/** `settings` with one value put on, held inside its range the way a
 *  script's assignment is held. */
ClipSettings withMotionValue (ClipSettings settings, MotionParam param,
                              float value);

/** The values a button was turned to, and only those: a value nobody turned
 *  is not here, and keeps coming from the script -- the way a feel keeps
 *  only what differs, except that here a turn back to the script's own value
 *  is still the button's. */
struct MotionOverrides
{
  std::array<std::optional<float>, numMotionParams> values;

  bool empty () const;
  std::optional<float> get (MotionParam param) const;
  void set (MotionParam param, float value);
  /** Back to the script -- a double tap. */
  void unset (MotionParam param);
};

bool operator== (MotionOverrides const &a, MotionOverrides const &b);
inline bool
operator!= (MotionOverrides const &a, MotionOverrides const &b)
{
  return !(a == b);
}

/** `settings` with every turned value put on. */
ClipSettings withMotion (ClipSettings settings, MotionOverrides const &motion);

/** Where a value on the tile comes from: the clip (nobody sets it -- shown as
 *  a hint), the script, or a turn on the button. */
enum class MotionSource
{
  Clip,
  Script,
  Button,
};

struct MotionShown
{
  float value = 0.f;
  MotionSource source = MotionSource::Clip;
};

/** What the tile shows for a button, indexed by MotionParam: the value the
 *  button puts on the clip -- or for one it leaves alone, the clip's own --
 *  and where each came from. */
std::array<MotionShown, numMotionParams>
motionShownFor (juce::String const &source, ClipSettings const &base,
                juce::int64 seed, MotionOverrides const &motion);

}
