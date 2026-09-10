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
#include <functional>

#include <juce_core/juce_core.h>

#include <a3-motion-engine/Config.hh>
#include <a3-motion-ui/components/MixerControls.hh>

namespace a3
{

/** The mixer's values, and where each of them came from.
 *
 *  **Two ways in, and the difference is the whole provision for the
 *  listening half of this feature.** A value from a finger is set *and* sent;
 *  a value from the other end of the wire is set and *not* sent. Without the
 *  split, Core reports a value, Motion sets it, Motion sends it, and Core
 *  reports it again — which is the loop a3_core_echo.py was written to
 *  suppress, rebuilt from this side. It costs nothing now and cannot be
 *  retrofitted later without touching every caller.
 *
 *  **Nothing is sent until something is touched.** Coming up is exactly when
 *  nothing is known, and a device that pulled four gains to its own defaults
 *  while starting could ruin an evening in progress.
 *
 *  Where a value *starts* follows the same thought: everything in the middle
 *  of its travel, except gain, volume and the master volume, which start at
 *  zero. Half of somebody's system is a guess; silence is the one starting
 *  point that cannot be wrong in the direction that matters. */
class MixerState
{
public:
  MixerState ();

  float channelValue (int channel, MixerControl control) const;
  bool channelToggle (int channel, MixerControl control) const;
  float masterValue (MasterControl control) const;
  float filterValue (FilterControl control) const;

  /** What the filter's mode key is asking, rather than the number it is held
   *  as. The word that goes on the wire is the sender's business. */
  bool filterIsHighPass () const;

  void setChannelFromTouch (int channel, MixerControl control, float value);
  void setChannelFromPeer (int channel, MixerControl control, float value);
  void setMasterFromTouch (MasterControl control, float value);
  void setMasterFromPeer (MasterControl control, float value);
  void setFilterFromTouch (FilterControl control, float value);
  void setFilterFromPeer (FilterControl control, float value);

  /** Where a touched value goes. The address is composed by whoever installs
   *  this — the state holds values, not a protocol. Unset until then, and a
   *  state with nobody listening simply holds. */
  std::function<void (juce::String const &address, float value)> onSend;

  /** The address a control is sent on. Installed alongside `onSend` for the
   *  same reason: the addresses are configurable and live in OscAddresses. */
  std::function<juce::String (int channel, MixerControl)> channelAddress;
  std::function<juce::String (MasterControl)> masterAddress;
  std::function<juce::String (FilterControl)> filterAddress;

private:
  static bool channelIsInRange (int channel);
  void send (juce::String const &address, float value);

  std::array<std::array<float, numMixerControls>,
             static_cast<std::size_t> (numChannelsInitial)>
      _channel;
  std::array<float, numMasterControls> _master;
  std::array<float, numFilterControls> _filter;
};

}
