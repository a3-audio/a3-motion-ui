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

#include "TruthKeeperLink.hh"
#include "TruthKeeper.hh"

#include <iostream>
#include <utility>

namespace a3
{

TruthKeeperLink::TruthKeeperLink (
    juce::File cache, std::function<juce::String (juce::String const &)> usable,
    std::function<void ()> restart)
    : _cache (std::move (cache)), _usable (std::move (usable)),
      _restart (std::move (restart))
{
}

TruthKeeperLink::~TruthKeeperLink ()
{
  stopTimer ();
  _receiver.removeListener (this);
  _receiver.disconnect ();
  _fetcher.removeAllJobs (true, 6000);
}

bool
TruthKeeperLink::start ()
{
  // Shared with StemDeck on this machine: both reuse the port, both hear it.
  _socket.setEnablePortReuse (true);
  if (!_socket.bindToPort (truthkeeper::announcePort)
      || !_receiver.connectToSocket (_socket))
    {
      std::cerr << "A3 Motion: cannot listen for Core's truth on "
                << truthkeeper::announcePort << std::endl;
      return false;
    }
  _receiver.addListener (this);
  return true;
}

void
TruthKeeperLink::waitForCore (std::string candidate, std::function<void ()> open)
{
  _wait.emplace (std::move (candidate));
  _open = std::move (open);
  startTimer (truthkeeper::startupWaitMs);
}

void
TruthKeeperLink::follow (std::string own)
{
  _own = std::move (own);
}

bool
TruthKeeperLink::waiting () const
{
  return _wait.has_value () && !_wait->isOpen ();
}

void
TruthKeeperLink::oscMessageReceived (juce::OSCMessage const &message)
{
  if (message.getAddressPattern ().toString () != truthkeeper::announceAddress
      || message.size () != 2 || !message[0].isString ()
      || !message[1].isString ())
    return;
  auto const url = message[0].getString ();
  auto const announced = message[1].getString ();
  if (waiting ())
    {
      apply (_wait->announced (announced.toStdString ()), url, announced);
      return;
    }
  if (truthkeeper::needsFetch (announced.toStdString (), _own))
    fetch (url, announced);
}

void
TruthKeeperLink::timerCallback ()
{
  stopTimer ();
  if (!waiting ())
    return;
  std::cerr << "A3 Motion: no word from Core in "
            << truthkeeper::startupWaitMs / 1000
            << " s, opening on the truth on disk" << std::endl;
  apply (_wait->timedOut (), {}, {});
}

void
TruthKeeperLink::apply (truthkeeper::StartupStep next, juce::String const &url,
                        juce::String const &announced)
{
  if (next == truthkeeper::StartupStep::fetch)
    {
      fetch (url, announced);
      return;
    }
  if (next != truthkeeper::StartupStep::open)
    return;
  stopTimer ();
  if (auto const open = std::exchange (_open, nullptr))
    open ();
}

void
TruthKeeperLink::fetch (juce::String const &url, juce::String const &announced)
{
  if (_busy.exchange (true))
    return;
  _fetcher.addJob ([this, url, announced] {
    auto const written = take (url, announced);
    _busy = false;
    juce::MessageManager::callAsync (
        [link = juce::WeakReference<TruthKeeperLink> (this), written] {
          if (auto *alive = link.get ())
            alive->finished (written);
        });
  });
}

void
TruthKeeperLink::finished (bool written)
{
  if (waiting ())
    {
      std::cerr << (written ? "A3 Motion: took Core's truth, opening on it"
                            : "A3 Motion: opening on the truth on disk")
                << std::endl;
      apply (_wait->fetched (), {}, {});
      return;
    }
  if (!written)
    return;
  std::cerr << "A3 Motion: Core announced another truth, restarting on it"
            << std::endl;
  _restart ();
}

bool
TruthKeeperLink::take (juce::String url, juce::String announced)
{
  juce::StringPairArray headers;
  int status = 0;
  auto stream = juce::URL (url).createInputStream (
      juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
          .withConnectionTimeoutMs (5000)
          .withResponseHeaders (&headers)
          .withStatusCode (&status));
  if (stream == nullptr || status != 200)
    {
      refuse ("fetch failed (status " + juce::String (status) + ")");
      return false;
    }
  juce::MemoryBlock body;
  stream->readIntoMemoryBlock (body);
  auto const hash = juce::SHA256 (body).toHexString ();
  if (!truthkeeper::verified (hash.toStdString (),
                              headers["X-A3-Truth"].toStdString (),
                              announced.toStdString ()))
    {
      refuse ("body, header and announcement do not agree");
      return false;
    }
  if (auto const why = _usable (body.toString ()); why.isNotEmpty ())
    {
      refuse (why);
      return false;
    }
  _cache.getParentDirectory ().createDirectory ();
  juce::TemporaryFile temporary (_cache);
  if (!temporary.getFile ().replaceWithData (body.getData (), body.getSize ())
      || !temporary.overwriteTargetFileWithTemporary ())
    {
      refuse ("cannot write " + _cache.getFullPathName ());
      return false;
    }
  return true;
}

void
TruthKeeperLink::refuse (juce::String const &reason)
{
  // Said once per reason, not every 2 s while Core keeps announcing it.
  if (reason == _lastReason)
    return;
  _lastReason = reason;
  std::cerr << "A3 Motion: Core's truth refused: " << reason << std::endl;
}

}
