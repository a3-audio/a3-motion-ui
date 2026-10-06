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

#include <functional>
#include <optional>

namespace a3
{

/** Which input source an event came from: juce::MouseInputSource's type and
 *  index, without the source itself, so the rule below can be tested. */
struct SourceKey
{
  enum Type
  {
    mouse,
    touch,
    pen
  };

  int type = mouse;
  int index = 0;

  constexpr bool operator== (SourceKey const &other) const
  {
    return type == other.type && index == other.index;
  }
};

/** A gesture belongs to the source that started it (#64).
 *
 *  On the device every finger arrives twice -- as a touch and, a couple of
 *  milliseconds later, as X's emulated mouse at the same point -- and a
 *  juce::Slider treats each source's press as a new gesture and each source's
 *  movement as a step. Fed both, a knob is pulled back and forth between two
 *  streams of the same finger. This keeps the first source and ignores every
 *  other until that one lets go. */
class FirstSourceOnly
{
public:
  /** A source went down. True when it leads the gesture now. */
  bool
  press (SourceKey source)
  {
    if (!_leader)
      _leader = source;
    return *_leader == source;
  }

  /** Whether events from this source move the control. */
  bool
  follows (SourceKey source) const
  {
    return _leader && *_leader == source;
  }

  /** A source went up. True when that ended the gesture. */
  bool
  release (SourceKey source)
  {
    if (!follows (source))
      return false;
    _leader.reset ();
    return true;
  }

  /** Drop the leader if it is no longer down -- its release was lost. */
  void
  forgetIfNotDown (std::function<bool (SourceKey)> const &isDown)
  {
    if (_leader && !isDown (*_leader))
      _leader.reset ();
  }

private:
  std::optional<SourceKey> _leader;
};

}
