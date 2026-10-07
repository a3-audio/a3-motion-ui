/*

  A3 Motion UI
  Copyright (C) 2026 Patric Schmitz

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

#include <a3-motion-engine/backends/SpatBackend.hh>

#include <atomic>
#include <memory>

namespace a3
{

/** A backend that counts what an engine would send and sends nothing.
 *
 *  Every engine a test builds takes one. The engine's own default backend
 *  aims at Core as the truth names it -- on the rig, the live one (#67) --
 *  and the test runner refuses it. */
class OfflineBackend : public SpatBackend
{
public:
  void
  sendPosition (index_t, Pos const &) override
  {
    ++positionsSent;
  }
  void sendPot1 (index_t, float) override { ++potsSent; }
  void sendPot2 (index_t, float) override { ++potsSent; }
  void sendPot3 (index_t, float) override { ++potsSent; }

  // Written on the command queue's thread, read on the test's.
  std::atomic<int> positionsSent{ 0 };
  std::atomic<int> potsSent{ 0 };

protected:
  void addressesChanged (OscAddresses const &) override {}
};

inline std::unique_ptr<SpatBackend>
offlineBackend ()
{
  return std::make_unique<OfflineBackend> ();
}

}
