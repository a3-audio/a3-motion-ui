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

#include "MixerState.hh"

namespace a3
{

namespace
{
// The tables' own positions, not a second search over them -- see
// controlSlot in MixerControls.hh. Wrapped only to spare every call site the
// cast: the arrays are indexed by size_t and the table answers in int.
//
// The jmax is a belt and braces, not error handling. controlSlot returns -1
// for a control that is not in its order array, and every enumerator is in
// its array -- MixerControls' EveryControlAppearsExactlyOnce is what enforces
// that, since nothing in C++ can. So -1 cannot arrive here; if it ever did,
// clamping to 0 would quietly address the wrong control, which is why this
// says so rather than looking like a handled case.
template <typename Control>
std::size_t
slotOf (Control control)
{
  return static_cast<std::size_t> (juce::jmax (0, controlSlot (control)));
}

float
clamped (float value)
{
  return juce::jlimit (0.f, 1.f, value);
}
}

MixerState::MixerState ()
{
  for (auto &channel : _channel)
    for (std::size_t i = 0; i < static_cast<std::size_t> (numMixerControls);
         ++i)
      {
        auto const control = mixerControlOrder[i];
        auto const silent = control == MixerControl::Gain
                            || control == MixerControl::Volume;
        channel[i] = mixerControlIsAToggle (control) ? 0.f
                     : silent                        ? 0.f
                                                     : 0.5f;
      }

  _master.fill (0.5f);
  _master[slotOf (MasterControl::Volume)] = 0.f;

  _filter.fill (0.5f);
  _filter[slotOf (FilterControl::Mode)] = 0.f;
}

bool
MixerState::channelIsInRange (int channel)
{
  return channel >= 0 && channel < numChannelsInitial;
}

float
MixerState::channelValue (int channel, MixerControl control) const
{
  if (!channelIsInRange (channel))
    return 0.f;
  return _channel[static_cast<std::size_t> (channel)][slotOf (control)];
}

bool
MixerState::channelToggle (int channel, MixerControl control) const
{
  return channelValue (channel, control) > 0.5f;
}

float
MixerState::masterValue (MasterControl control) const
{
  return _master[slotOf (control)];
}

float
MixerState::filterValue (FilterControl control) const
{
  return _filter[slotOf (control)];
}

bool
MixerState::filterIsHighPass () const
{
  return filterValue (FilterControl::Mode) > 0.5f;
}

void
MixerState::send (juce::String const &address, float value)
{
  if (onSend && address.isNotEmpty ())
    onSend (address, value);
}

void
MixerState::setChannelFromPeer (int channel, MixerControl control,
                                float value)
{
  if (!channelIsInRange (channel))
    return;
  _channel[static_cast<std::size_t> (channel)][slotOf (control)]
      = clamped (value);
}

void
MixerState::setChannelFromTouch (int channel, MixerControl control,
                                 float value)
{
  if (!channelIsInRange (channel))
    return;
  setChannelFromPeer (channel, control, value);
  send (channelAddress ? channelAddress (channel, control) : juce::String (),
        channelValue (channel, control));
}

void
MixerState::setMasterFromPeer (MasterControl control, float value)
{
  _master[slotOf (control)] = clamped (value);
}

void
MixerState::setMasterFromTouch (MasterControl control, float value)
{
  setMasterFromPeer (control, value);
  send (masterAddress ? masterAddress (control) : juce::String (),
        masterValue (control));
}

void
MixerState::setFilterFromPeer (FilterControl control, float value)
{
  _filter[slotOf (control)] = clamped (value);
}

void
MixerState::setFilterFromTouch (FilterControl control, float value)
{
  setFilterFromPeer (control, value);
  send (filterAddress ? filterAddress (control) : juce::String (),
        filterValue (control));
}

}
