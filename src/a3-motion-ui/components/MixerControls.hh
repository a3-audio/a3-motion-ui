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

#include <array>

namespace a3
{

/** What a channel strip has, and the order it is in.
 *
 *  **This is the authority.** The overlay lays out four strips side by side
 *  and the bar's MIX tab lays out one across its width; both read this and
 *  neither knows the list itself. That is the same reasoning as
 *  functionKeyOrder, where the panel's wiring and the screen's strip are one
 *  list read two ways — two tables would eventually disagree, and the
 *  disagreement shows up as a control that is on one view and not the other.
 *
 *  The order is what a hand coming from a mixer expects: gain at the top
 *  where it is set once, the three bands under it, the fader under them, the
 *  two keys at the foot.
 *
 *  **Two of the mixer's controls are deliberately not here.** The 3D key,
 *  because Core's boolean moved to `4d`, the key is gone in mixer hardware
 *  v3.2, and the continuous 3d value already sits in the bar's own 4x3 grid —
 *  a key would be a second way to a thing that is no longer a switch. And
 *  fx-send, because it currently carries the 3D crossfade, which is what
 *  Motion's own 3d value does: two controls for one function. It comes when
 *  it is the FX send again, not before. See
 *  issues/a3-core-fx-send-fuehrt-noch-die-3d-funktion.md.
 */
enum class MixerControl
{
  Gain,
  EqHigh,
  EqMid,
  EqLow,
  Volume,
  Pfl,
  Fx,
};

constexpr int numMixerControls = 7;

/** Top to bottom on a vertical strip; left to right on the bar's one. */
constexpr std::array<MixerControl, numMixerControls> mixerControlOrder{
  MixerControl::Gain,   MixerControl::EqHigh, MixerControl::EqMid,
  MixerControl::EqLow,  MixerControl::Volume, MixerControl::Pfl,
  MixerControl::Fx,
};

/** Whether it is a key rather than something turned or thrown.
 *
 *  The toggles come last in the order, which is what lets a layout take them
 *  off the end without knowing which they are — a test insists on it. */
constexpr bool
mixerControlIsAToggle (MixerControl control)
{
  return control == MixerControl::Pfl || control == MixerControl::Fx;
}

/** What is written under it. At most four characters: the narrowest a strip
 *  may get is minimumChannelWidth, and a longer word is drawn clipped, which
 *  reads as a fault rather than as an abbreviation. */
constexpr char const *
mixerControlLabel (MixerControl control)
{
  switch (control)
    {
    case MixerControl::Gain:
      return "GAIN";
    case MixerControl::EqHigh:
      return "HIGH";
    case MixerControl::EqMid:
      return "MID";
    case MixerControl::EqLow:
      return "LOW";
    case MixerControl::Volume:
      return "VOL";
    case MixerControl::Pfl:
      return "PFL";
    case MixerControl::Fx:
      return "FX";
    }
  return "";
}

/** The summing section. Not per channel, so its own short list. */
enum class MasterControl
{
  Volume,
  Booth,
  PhonesMix,
  PhonesVolume,
  Return,
};

constexpr int numMasterControls = 5;

constexpr std::array<MasterControl, numMasterControls> masterControlOrder{
  MasterControl::Volume,       MasterControl::Booth,
  MasterControl::PhonesMix,    MasterControl::PhonesVolume,
  MasterControl::Return,
};

constexpr char const *
masterControlLabel (MasterControl control)
{
  switch (control)
    {
    case MasterControl::Volume:
      return "MST";
    case MasterControl::Booth:
      return "BTH";
    case MasterControl::PhonesMix:
      return "MIX";
    case MasterControl::PhonesVolume:
      return "PHN";
    case MasterControl::Return:
      return "RET";
    }
  return "";
}

/** The one filter, for all four channels at once. Mode is a key, the other
 *  two are turned. */
enum class FilterControl
{
  Mode,
  Frequency,
  Resonance,
};

constexpr int numFilterControls = 3;

constexpr std::array<FilterControl, numFilterControls> filterControlOrder{
  FilterControl::Mode,
  FilterControl::Frequency,
  FilterControl::Resonance,
};

constexpr char const *
filterControlLabel (FilterControl control)
{
  switch (control)
    {
    case FilterControl::Mode:
      return "MODE";
    case FilterControl::Frequency:
      return "FREQ";
    case FilterControl::Resonance:
      return "RES";
    }
  return "";
}

/** Position of a control in its respective order array. */
constexpr int
controlSlot (MixerControl control)
{
  for (std::size_t i = 0; i < numMixerControls; ++i)
    if (mixerControlOrder[i] == control)
      return static_cast<int> (i);
  return -1;
}

constexpr int
controlSlot (MasterControl control)
{
  for (std::size_t i = 0; i < numMasterControls; ++i)
    if (masterControlOrder[i] == control)
      return static_cast<int> (i);
  return -1;
}

constexpr int
controlSlot (FilterControl control)
{
  for (std::size_t i = 0; i < numFilterControls; ++i)
    if (filterControlOrder[i] == control)
      return static_cast<int> (i);
  return -1;
}

}
