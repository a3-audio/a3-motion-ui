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
#include <optional>

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
 *  where it is set once, the three bands under it, the volume under them, the
 *  two keys at the foot.
 *
 *  **fx-send arrived on 2026-09-12**, and the condition written here for it
 *  is what let it in: it used to carry the 3D crossfade, which is what
 *  Motion's own 3d value does, so a fader for it would have been a second
 *  control for one function. Core gave the desk's pot its own job back, and
 *  a beat-synced delay now hangs on that bus — see
 *  issues/a3-core-fx-send-fuehrt-noch-die-3d-funktion.md.
 *
 *  It sits after VOL rather than at the top where the desk has it. On the
 *  desk it is pot 0, above gain; here the top of the strip is the set-once
 *  end and the foot is where the hand goes during a set, and a send is a
 *  gesture, not a setting. Moving it is one line if that turns out wrong.
 *
 *  **One of the mixer's controls is still deliberately not here.** The 3D
 *  key, because Core's boolean moved to `4d`, the key is gone in mixer
 *  hardware v3.2, and the continuous 3d value already sits in the bar's own
 *  4x3 grid — a key would be a second way to a thing that is no longer a
 *  switch.
 */
enum class MixerControl
{
  Gain,
  EqHigh,
  EqMid,
  EqLow,
  Volume,
  FxSend,
  Pfl,
  Fx,
};

constexpr int numMixerControls = 8;

/** Top to bottom on a vertical strip; left to right on the bar's one. */
constexpr std::array<MixerControl, numMixerControls> mixerControlOrder{
  MixerControl::Gain,   MixerControl::EqHigh, MixerControl::EqMid,
  MixerControl::EqLow,  MixerControl::Volume, MixerControl::FxSend,
  MixerControl::Pfl,    MixerControl::Fx,
};

/** What the two MIX pages draw, in the same order: everything but VOL.
 *
 *  VOL is set by dragging the channel's meter -- "das vol pot muss weg" --
 *  so a knob for it beside the meter was the same control twice. It stays in
 *  mixerControlOrder, which the state and the wire are counted by; this is
 *  only what a page lays out. */
constexpr int numMixerFaceControls = 7;
constexpr std::array<MixerControl, numMixerFaceControls> mixerFaceOrder{
  MixerControl::Gain,  MixerControl::EqHigh, MixerControl::EqMid,
  MixerControl::EqLow, MixerControl::FxSend, MixerControl::Pfl,
  MixerControl::Fx,
};

/** Where a control stands on a page, or -1 for one no page draws (VOL). */
constexpr int
faceSlot (MixerControl control)
{
  for (std::size_t i = 0; i < numMixerFaceControls; ++i)
    if (mixerFaceOrder[i] == control)
      return static_cast<int> (i);
  return -1;
}

/** Whether it is a key rather than something turned.
 *
 *  The toggles come last in the order, which is what lets a layout take them
 *  off the end without knowing which they are — a test insists on it. */
constexpr bool
mixerControlIsAToggle (MixerControl control)
{
  return control == MixerControl::Pfl || control == MixerControl::Fx;
}


/** Whether the control's middle means neutral.
 *
 *  An EQ band is cut or boost either side of flat, so its arc grows out of
 *  the middle and which side of flat you are on reads at a glance. Gain and
 *  the volume run from silence upwards and fill from their start, the way a
 *  volume knob anywhere does. */
constexpr bool
fillsFromTheMiddle (MixerControl control)
{
  return control == MixerControl::EqHigh || control == MixerControl::EqMid
         || control == MixerControl::EqLow;
}

/** Where two taps put a control back, or nothing for one that stays where it
 *  was left.
 *
 *  What this guards against is **zero**, not resetting. Zero on GAIN or on VOL
 *  is a mute, and a mute two fingertips away from a control that is dragged
 *  all evening is a way to silence the room by accident -- which is why it is
 *  asked per control rather than done to the whole strip. None of the answers
 *  below quietens anything:
 *
 *  - the three EQ bands go back to **flat**, the middle they already fill
 *    from;
 *  - GAIN goes back to **full**, not to zero;
 *  - SEND goes to **none**, the one place a hand reaches for "none of that"
 *    and means it.
 *
 *  VOL keeps none. It is the fader now and two taps there are handled where
 *  the fader is, so that the master can be left out of it -- full volume on
 *  the master is the one gesture that makes the whole room loud at once.
 *
 *  Nothing for the undecided case rather than a plausible 0.5: a control that
 *  fell through to a number would look decided without being it. Same rule as
 *  channelValueRestPosition, and for the same reason.
 */
constexpr std::optional<float>
mixerControlRestPosition (MixerControl control)
{
  if (fillsFromTheMiddle (control))
    return 0.5f;
  if (control == MixerControl::Gain)
    return 1.f;
  if (control == MixerControl::FxSend)
    return 0.f;
  return {};
}

/** Where a control stands on a device that has just come up.
 *
 *  The same question as the rest position at a different moment, and for two
 *  of the seven the answers are genuinely different:
 *
 *  **GAIN starts at zero and two taps put it at full.** A desk coming up has
 *  to be silent -- nobody knows what is patched into it, and zero is the one
 *  starting point that cannot be wrong in the direction that matters. Two
 *  taps are a hand asking for something, and what a hand asks of a gain is
 *  "back to reference".
 *
 *  **VOL likewise**, though it has no rest position at all: it is the fader,
 *  and the fader's own double tap is where that lives.
 *
 *  Everything else starts where it rests. Keeping the two answers in one
 *  place is what the SEND bug of 2026-09-12 cost: half open in the
 *  constructor, shut here, so a double tap moved a control nobody had
 *  touched. They may differ -- they may not differ *by accident*, which is
 *  why this is a function with a reason rather than a number in a loop.
 */
constexpr float
mixerControlStartPosition (MixerControl control)
{
  if (control == MixerControl::Gain || control == MixerControl::Volume
      || control == MixerControl::FxSend)
    return 0.f;

  if (auto const rest = mixerControlRestPosition (control))
    return *rest;

  return 0.f;
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
    case MixerControl::FxSend:
      return "SEND";
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

/** The same, for the summing section: the phones' blend sits between two ends
 *  and reads as a distance from the middle; everything else is a level. */
constexpr bool
fillsFromTheMiddle (MasterControl control)
{
  return control == MasterControl::PhonesMix;
}

/** Where two taps put a master control, or nothing for one that stays.
 *
 *  Two have an answer. The phones' blend sits between two ends and its middle
 *  *means* something -- half cue, half master. RET is the far end of the
 *  channels' SEND, so it puts back what SEND puts back: **nothing**, take the
 *  effect out. Both ends of the effect path therefore behave the same way,
 *  and both of them in the quiet direction.
 *
 *  BTH and PHN keep none. They are levels, and the only value two taps could
 *  mean on a level is full -- full into a pair of headphones is an ear, full
 *  into the booth wedge is the same gesture the master fader is deliberately
 *  without. The master's volume is that fader and keeps none for that reason:
 *  full there makes the whole room loud at once, and two taps in the dark are
 *  too cheap for it.
 */
constexpr std::optional<float>
masterControlRestPosition (MasterControl control)
{
  if (fillsFromTheMiddle (control))
    return 0.5f;
  if (control == MasterControl::Return)
    return 0.f;
  return {};
}

/** The master's pots, top to bottom: everything but its volume, which is
 *  dragged on the output meters the way a channel's VOL is dragged on its
 *  own meter. Volume stays in masterControlOrder for the state and the wire. */
constexpr int numMasterFaceControls = 4;
constexpr std::array<MasterControl, numMasterFaceControls> masterFaceOrder{
  MasterControl::Booth, MasterControl::PhonesMix, MasterControl::PhonesVolume,
  MasterControl::Return,
};

/** Where a master control stands among the pots, or -1 for the volume. */
constexpr int
masterFaceSlot (MasterControl control)
{
  for (std::size_t i = 0; i < numMasterFaceControls; ++i)
    if (masterFaceOrder[i] == control)
      return static_cast<int> (i);
  return -1;
}

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

/** Where two taps put a filter knob, or nothing for the mode.
 *
 *  The middle for FREQ, chosen over "the open end for whichever mode you are
 *  in": the filter still bites at 0.5, but two taps then mean one thing
 *  whatever the mode switch says, and a reset whose result depends on a
 *  control somewhere else is one you have to look up before you dare use it.
 *  None for RES, which is the unambiguous "take it out".
 *
 *  The mode is a word rather than a value and has nothing to put back.
 */
constexpr std::optional<float>
filterControlRestPosition (FilterControl control)
{
  if (control == FilterControl::Frequency)
    return 0.5f;
  if (control == FilterControl::Resonance)
    return 0.f;
  return {};
}

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

/** Where a control sits in its respective order, or -1 for one not in it.
 *
 *  Three overloads let a caller ask without knowing which of the three lists
 *  a control belongs to — useful where a control's type is determined at
 *  runtime or where mixer state and address lookup want one function name
 *  for all three. */
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
