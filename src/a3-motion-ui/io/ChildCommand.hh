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
/** Runs a command to its end and hands back what it printed.
 *
 *  Every child is reaped: juce::ChildProcess does not wait for its child
 *  when it goes out of scope, and an unreaped child stays a zombie until
 *  this program exits. The bar's workspace poll started one i3-msg every
 *  500 ms and filled the service's pids.max in nine hours (38 314
 *  zombies); after that no fork worked at all.
 *
 *  A child that has not finished after `timeoutMs` is killed and reaped.
 *  Empty when the command could not be started. */
juce::String runCommand (juce::StringArray const &command,
                         int timeoutMs = 1000);
}
