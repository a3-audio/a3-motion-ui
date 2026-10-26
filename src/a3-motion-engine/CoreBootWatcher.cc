/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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


#include "CoreBootWatcher.hh"

#include "TruthKeeper.hh"

#include <iostream>

namespace a3
{

std::optional<std::string>
bootIdOf (juce::OSCMessage const &message)
{
  if (message.getAddressPattern ().toString () != truthkeeper::announceAddress
      || message.size () != 3 || !message[2].isString ())
    return std::nullopt;

  auto id = message[2].getString ().toStdString ();
  if (id.empty ())
    return std::nullopt;
  return id;
}

CoreBootWatcher::CoreBootWatcher (
    std::function<void (std::string const &)> onBootId)
    : _onBootId (std::move (onBootId))
{
}

CoreBootWatcher::~CoreBootWatcher ()
{
  _receiver.removeListener (this);
  _receiver.disconnect ();
}

bool
CoreBootWatcher::start ()
{
  _socket.setEnablePortReuse (true);
  if (!_socket.bindToPort (truthkeeper::announcePort)
      || !_receiver.connectToSocket (_socket))
    {
      std::cerr << "A3 Motion: cannot listen for Core's boot id on "
                << truthkeeper::announcePort << std::endl;
      return false;
    }
  _receiver.addListener (this);
  return true;
}

void
CoreBootWatcher::handle (juce::OSCMessage const &message)
{
  if (auto const id = bootIdOf (message))
    _onBootId (*id);
}

void
CoreBootWatcher::oscMessageReceived (juce::OSCMessage const &message)
{
  handle (message);
}

}
