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

#include <a3-motion-engine/OscAddresses.hh>
#include <a3-motion-engine/util/Types.hh>

#include <atomic>
#include <mutex>

namespace a3
{

class SpatBackend
{
public:
  virtual ~SpatBackend (){};
  virtual void sendPosition (index_t channel, Pos const &pos) = 0;
  virtual void sendPot1 (index_t channel, float pot1) = 0;
  virtual void sendPot2 (index_t channel, float pot2) = 0;

  /** New addresses, handed over from the message thread. Stored, not
   *  applied — sending happens on the command queue's thread, and that is
   *  where the cached patterns may be rebuilt. */
  void setAddresses (OscAddresses const &addresses);

  /** Called once per drain by the sending thread, before anything is sent.
   *  Costs one atomic load when nothing changed; the lock is taken only
   *  when something did. */
  void applyPendingAddresses ();

  // Both are short enough to live here, and there is no SpatBackend.cc.

protected:
  /** Rebuild whatever the backend caches. Called on the sending thread,
   *  and once by the subclass's own constructor. */
  virtual void addressesChanged (OscAddresses const &addresses) = 0;

private:
  std::mutex _pendingMutex;
  OscAddresses _pending;
  std::atomic<bool> _pendingSet{ false };
};

inline void
SpatBackend::setAddresses (OscAddresses const &addresses)
{
  {
    std::lock_guard<std::mutex> lock{ _pendingMutex };
    _pending = addresses;
  }
  _pendingSet.store (true, std::memory_order_release);
}

inline void
SpatBackend::applyPendingAddresses ()
{
  if (!_pendingSet.load (std::memory_order_acquire))
    return;

  OscAddresses addresses;
  {
    std::lock_guard<std::mutex> lock{ _pendingMutex };
    addresses = _pending;
    _pendingSet.store (false, std::memory_order_relaxed);
  }

  addressesChanged (addresses);
}

}
