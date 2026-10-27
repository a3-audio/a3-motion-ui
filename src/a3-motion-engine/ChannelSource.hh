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

#include <array>
#include <string>

namespace a3
{

/** What feeds a channel in engine mode: the analog input, or StemDeck's deck
 *  k. The wire says it with Core's `channel.source` word (0 = analog, k =
 *  deck k, 1 = A ... 4 = D); Core sends it to the desk and to Motion alike.
 *
 *  Motion places sound only for an analog channel: a deck's stems are placed
 *  by Core, so a deck channel sends no position, 3D, FREQ or Q. */
constexpr int analogSource = 0;

/** The truth's `stemdeck.present` deck range (1..4, A..D). A wire value outside
 *  0..maxDecks is not a source Motion knows, and is analog: the safe side
 *  of a mistake is the channel that behaves as it always has. */
constexpr int maxDecks = 4;

constexpr int numSourceChannels = 4;

inline int
sourceFromWire (int wire)
{
  return wire >= 1 && wire <= maxDecks ? wire : analogSource;
}

inline bool
isDeck (int source)
{
  return source != analogSource;
}

/** What a channel's face says instead of its clip's name: empty for analog,
 *  "DECK A" for deck 1, and the deck marked absent while Core does not report
 *  it present. */
inline std::string
deckFaceText (int source, bool present)
{
  if (!isDeck (source))
    return {};

  auto const name = std::string ("DECK ")
                    + static_cast<char> ('A' + (source - 1));
  return present ? name : name + " (absent)";
}

/** What Core has told Motion about the decks, on the message thread only.
 *
 *  A deck not reported present is absent. A Core that has just started knows
 *  nothing of what the last one said, so on a new boot everything goes back to
 *  analog until Core reports `channel.source` again -- the same ruling the
 *  desk follows. */
class ChannelSources
{
public:
  void
  setSource (int channel, int wire)
  {
    if (inRange (channel))
      _source[static_cast<std::size_t> (channel)] = sourceFromWire (wire);
  }

  int
  source (int channel) const
  {
    return inRange (channel) ? _source[static_cast<std::size_t> (channel)]
                             : analogSource;
  }

  /** `deck` is 1-based, as on the wire. */
  void
  setPresent (int deck, bool present)
  {
    if (deck >= 1 && deck <= maxDecks)
      _present[static_cast<std::size_t> (deck - 1)] = present;
  }

  bool
  isPresent (int deck) const
  {
    return deck >= 1 && deck <= maxDecks
           && _present[static_cast<std::size_t> (deck - 1)];
  }

  std::string
  faceText (int channel) const
  {
    auto const s = source (channel);
    return deckFaceText (s, isPresent (s));
  }

  /** Core announced itself with this boot id. True when it is another Core
   *  than the one Motion knew; then everything is back to analog and absent.
   *  The first id Motion hears only records: Core answers the recall with
   *  its sources before the first announcement arrives, and that answer
   *  must stand. An announcement without a boot id (an older Core) says
   *  nothing. */
  bool
  coreAnnounced (std::string const &bootId)
  {
    if (bootId.empty () || bootId == _bootId)
      return false;

    auto const known = !_bootId.empty ();
    _bootId = bootId;
    if (!known)
      return false;

    _source.fill (analogSource);
    _present.fill (false);
    return true;
  }

private:
  static bool
  inRange (int channel)
  {
    return channel >= 0 && channel < numSourceChannels;
  }

  std::array<int, numSourceChannels> _source{};
  std::array<bool, maxDecks> _present{};
  std::string _bootId;
};

}
