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


#pragma once

#include <functional>
#include <optional>
#include <string>

#include <JuceHeader.h>

namespace a3
{

/** The boot id of the Core that sent this /core/here, if it carries one: the
 *  third string after url and fingerprint (an older Core sends two). */
std::optional<std::string> bootIdOf (juce::OSCMessage const &message);

/** Hears Core's announcements for their boot id alone, on the port the truth
 *  keeper uses too (both bind it with port reuse). It is separate from the
 *  keeper on purpose: the keeper is not there when $A3_OSC_TRUTH is set or
 *  when it cannot listen, and a new Core must be noticed regardless. The
 *  callback runs on the message thread. */
class CoreBootWatcher
    : private juce::OSCReceiver::Listener<
          juce::OSCReceiver::MessageLoopCallback>
{
public:
  explicit CoreBootWatcher (std::function<void (std::string const &)> onBootId);
  ~CoreBootWatcher () override;

  bool start ();

  /** What a received announcement does; public for tests. */
  void handle (juce::OSCMessage const &message);

private:
  void oscMessageReceived (juce::OSCMessage const &message) override;

  std::function<void (std::string const &)> _onBootId;
  juce::DatagramSocket _socket{ true };
  juce::OSCReceiver _receiver;
};

}
