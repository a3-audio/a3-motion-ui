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

#include "PendingTakes.hh"

#include <a3-motion-engine/Pattern.hh>

namespace a3
{

PendingTakes::PendingTakes (std::size_t numChannels, std::size_t numSlots)
    : _numSlots (numSlots), _before (numChannels * numSlots)
{
}

bool
PendingTakes::inRange (index_t channel, index_t slot) const
{
  return slot < _numSlots && channel * _numSlots + slot < _before.size ();
}

void
PendingTakes::begin (index_t channel, index_t slot, SlotContent before)
{
  if (!inRange (channel, slot))
    return;

  auto &entry = _before[channel * _numSlots + slot];
  if (!entry.has_value ())
    entry = std::move (before);
}

bool
PendingTakes::isPending (index_t channel, index_t slot) const
{
  return inRange (channel, slot)
         && _before[channel * _numSlots + slot].has_value ();
}

bool
PendingTakes::offersKeys (index_t channel, index_t slot,
                          bool takeUnderway) const
{
  return !takeUnderway && isPending (channel, slot);
}

SlotContent
PendingTakes::forSet (index_t channel, index_t slot,
                      SlotContent current) const
{
  if (!isPending (channel, slot))
    return current;

  return *_before[channel * _numSlots + slot];
}

SlotContent
PendingTakes::resolve (index_t channel, index_t slot)
{
  if (!isPending (channel, slot))
    return {};

  auto &entry = _before[channel * _numSlots + slot];
  auto back = std::move (*entry);
  entry.reset ();

  if (isDiscardArmed (channel, slot))
    disarm ();

  return back;
}

void
PendingTakes::clear (index_t channel, index_t slot)
{
  resolve (channel, slot);
}

void
PendingTakes::clearAll ()
{
  for (auto &entry : _before)
    entry.reset ();
  disarm ();
}

bool
PendingTakes::pressDiscard (index_t channel, index_t slot)
{
  if (!isPending (channel, slot))
    return false;

  if (isDiscardArmed (channel, slot))
    {
      disarm ();
      return true;
    }

  _armed = std::make_pair (channel, slot);
  return false;
}

bool
PendingTakes::isDiscardArmed (index_t channel, index_t slot) const
{
  return _armed.has_value () && _armed->first == channel
         && _armed->second == slot;
}

void
PendingTakes::disarm ()
{
  _armed.reset ();
}

void
PendingTakes::renamePattern (juce::String const &from, juce::String const &to)
{
  for (auto &before : _before)
    if (before.has_value () && before->pattern
        && juce::String (before->pattern->getName ()) == from)
      before->pattern->setName (to.toStdString ());
}

void
PendingTakes::moveClipFile (juce::File const &from, juce::File const &to)
{
  for (auto &before : _before)
    if (before.has_value () && before->clipFile == from)
      before->clipFile = to;
}

}
