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

#include <atomic>
#include <functional>
#include <string>

#include <JuceHeader.h>

namespace a3
{

/** The JUCE side of the truth keeper (TruthKeeper.hh has the decisions):
 *  hears /core/here on the announce port, fetches a fingerprint Motion does
 *  not have off the message thread, checks it, writes the cache whole and
 *  asks the app to restart. A refusal is said once per reason; Motion runs
 *  on. The port is shared with StemDeck on the same machine: both bind it
 *  with port reuse, and both receive the broadcast. */
class TruthKeeperLink
    : private juce::OSCReceiver::Listener<
          juce::OSCReceiver::MessageLoopCallback>
{
public:
  /** `usable(body)` returns why a fetched truth cannot be used, or empty. */
  TruthKeeperLink (std::string own, juce::File cache,
                   std::function<juce::String (juce::String const &body)> usable,
                   std::function<void ()> restart);
  ~TruthKeeperLink () override;

  bool start ();

private:
  void oscMessageReceived (juce::OSCMessage const &message) override;
  void take (juce::String url, juce::String announced);
  void refuse (juce::String const &reason);

  std::string _own;
  juce::File _cache;
  std::function<juce::String (juce::String const &)> _usable;
  std::function<void ()> _restart;
  juce::DatagramSocket _socket{ true };
  juce::OSCReceiver _receiver;
  std::atomic<bool> _busy{ false };
  juce::String _lastReason;
  // Last: destroyed first, so a job it still waits for finds the rest alive.
  juce::ThreadPool _fetcher{ juce::ThreadPoolOptions{}.withNumberOfThreads (1) };

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TruthKeeperLink)
};

}
