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

#include "RecordingName.hh"

namespace a3
{

juce::String
recordingBaseName (juce::Time const &when)
{
  return "Rec_" + when.formatted ("%y%m%d");
}

juce::String
freeRecordingName (juce::String const &base,
                   std::function<bool (juce::String const &)> const &taken)
{
  for (auto n = 1; n < 1000; ++n)
    {
      auto const candidate
          = base + "_" + juce::String (n).paddedLeft ('0', 2);
      if (!taken (candidate))
        return candidate;
    }

  // A thousand takes in one day is not a session, it is a fault somewhere
  // else. Rather than overwrite, fall back to something that cannot collide.
  return base + "_" + juce::String (juce::Time::currentTimeMillis ());
}

}
