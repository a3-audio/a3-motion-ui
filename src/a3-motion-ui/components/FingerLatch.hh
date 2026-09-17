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

namespace a3
{

/** Which finger leads a list that several hit areas share.
 *
 *  "scroll mit einem oder zwei fingern." Two fingers on a list land on two hit
 *  areas, or both on one, and each scrolled it: the list ran at double speed,
 *  or jumped as the second finger restarted the drag the first was making.
 *  The first finger down leads until it comes up; any other is ignored.
 *
 *  Not for controls that are meant to be held two at a time -- the channel
 *  grid's knobs are. Only hit areas given a latch take part.
 */
class FingerLatch
{
public:
  /** A finger went down. True if it leads (now or already). */
  bool
  claim (int source)
  {
    if (_leader < 0)
      _leader = source;
    return _leader == source;
  }

  /** The leading finger, or -1. */
  int leader () const { return _leader; }

  bool
  leads (int source) const
  {
    return _leader == source;
  }

  /** A finger came up; only the leader's frees the list. */
  void
  release (int source)
  {
    if (_leader == source)
      _leader = -1;
  }

  enum Group
  {
    /** Every scrollable area of the menu pages and the strips beside them. */
    menuList
  };

  static FingerLatch &
  forGroup (Group group)
  {
    static FingerLatch latches[1];
    return latches[static_cast<int> (group)];
  }

private:
  int _leader = -1;
};

}
