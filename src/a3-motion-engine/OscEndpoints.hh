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

#include <JuceHeader.h>

namespace a3
{

/** Where this device sends, read out of `config.json`'s `oscSender` block.
 *
 *  Two destinations on one host, and which is which is the whole point of
 *  this struct existing rather than an int being read where it is needed.
 *  `corePort` is A3 Core: the spatial position, and everything the mixer
 *  turns. `beatclockPort` is the beat-analyzer, a different process, which
 *  is told the beat and the clock mode and nothing else.
 *
 *  Getting that backwards costs nothing visible. OSC over UDP never
 *  answers, so a message sent to the wrong one of the two is discarded in
 *  silence — which is how fifteen mixer addresses spent a branch going to
 *  the beat-analyzer while every control on screen moved as though it had
 *  worked. `OscEndpoints` is what the test suite can point at to say the
 *  mixer and the spatial data agree about where Core is. */
struct OscEndpoints
{
  juce::String host{ "127.0.0.1" };

  /** A3 Core, which listens for the channel and master addresses. */
  int corePort{ 9000 };

  /** The beat-analyzer. Falls back to corePort for a config written before
   *  the clock had a port of its own. */
  int beatclockPort{ 9000 };
};

OscEndpoints loadOscEndpoints (juce::var const &config);

}
