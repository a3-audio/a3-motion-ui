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
#include <optional>
#include <string>

#include <JuceHeader.h>

#include "TruthKeeper.hh"

namespace a3
{

/** The JUCE side of the truth keeper (TruthKeeper.hh has the decisions):
 *  hears /core/here on the announce port and fetches a fingerprint Motion
 *  does not have off the message thread, checks it and writes the cache
 *  whole. At start-up (waitForCore) the window waits for that, up to
 *  truthkeeper::startupWaitMs, and opens once; from follow() on, a changed
 *  truth asks the app to restart. A refusal is said once per reason. The
 *  port is shared with StemDeck on the same machine: both bind it with port
 *  reuse, and both receive the broadcast. */
class TruthKeeperLink
    : private juce::OSCReceiver::Listener<
          juce::OSCReceiver::MessageLoopCallback>,
      private juce::Timer
{
public:
  /** `usable(body)` returns why a fetched truth cannot be used, or empty. */
  TruthKeeperLink (juce::File cache,
                   std::function<juce::String (juce::String const &body)> usable,
                   std::function<void ()> restart);
  ~TruthKeeperLink () override;

  bool start ();
  /** Start-up: call `open` once -- on Core's truth, or after the wait. */
  void waitForCore (std::string candidate, std::function<void ()> open);
  /** From now on, the truth Motion runs on is `own`; another one restarts. */
  void follow (std::string own);

private:
  void oscMessageReceived (juce::OSCMessage const &message) override;
  void timerCallback () override;
  bool waiting () const;
  void apply (truthkeeper::StartupStep next, juce::String const &url,
              juce::String const &announced);
  void fetch (juce::String const &url, juce::String const &announced);
  bool take (juce::String url, juce::String announced);
  void finished (bool written);
  void refuse (juce::String const &reason);

  std::string _own;
  std::optional<truthkeeper::StartupWait> _wait;
  std::function<void ()> _open;
  juce::File _cache;
  std::function<juce::String (juce::String const &)> _usable;
  std::function<void ()> _restart;
  juce::DatagramSocket _socket{ true };
  juce::OSCReceiver _receiver;
  std::atomic<bool> _busy{ false };
  juce::String _lastReason;
  JUCE_DECLARE_WEAK_REFERENCEABLE (TruthKeeperLink)
  // Last: destroyed first, so a job it still waits for finds the rest alive.
  juce::ThreadPool _fetcher{ juce::ThreadPoolOptions{}.withNumberOfThreads (1) };

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TruthKeeperLink)
};

}
