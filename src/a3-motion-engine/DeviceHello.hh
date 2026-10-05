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

#include <JuceHeader.h>

#include "OscAddresses.hh"

namespace a3
{

// Motion tells Core who it is and which truth it speaks (/device/hello), like
// the desk and StemDeck: once at start, then every 30 s for as long as it
// runs. Not once only -- a Core restarted after it would never hear it again
// (the desk, 2026-10-01). Core follows the Motion that said hello last, so
// this is also how a Motion on another machine takes over.
//
// Pure; the caller passes the clock and sends.
class HelloSchedule
{
public:
  static constexpr double kEveryMillis = 30. * 1000.;

  bool
  due (double nowMillis) const
  {
    return !_said || nowMillis - _last >= kEveryMillis;
  }

  void
  said (double nowMillis)
  {
    _said = true;
    _last = nowMillis;
  }

private:
  bool _said{ false };
  double _last{ 0. };
};

/** Motion's hello: its name and the sha256 of the truth it loaded
 *  (OscTruth::digest). That is Core's fingerprint when the file is the body
 *  Core served, which is the copy Motion keeps and starts from. */
inline juce::OSCMessage
helloMessage (OscAddresses const &addresses, juce::String const &truthDigest)
{
  return juce::OSCMessage (juce::OSCAddressPattern (addresses.deviceHello),
                           juce::String ("motion"), truthDigest);
}

}
