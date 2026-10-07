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

#include <a3-motion-ui/components/FirstSourceOnly.hh>

#include <juce_graphics/juce_graphics.h>

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace a3
{

/** The fingers on the sphere in camera mode, and where each was last seen.
 *  One turns the view; two pinch the zoom.
 *
 *  Only sources of the kind that touched first count: on the device X sends
 *  every finger a second time as an emulated mouse, and keyed by source alone
 *  (9a04236) one finger was two and the view would not turn. The same rule as
 *  FirstSourceOnly, widened from one source to one kind, so that two real
 *  touches still pinch. */
class CameraFingers
{
public:
  /** A source went down. True when it counts as a camera finger. */
  bool press (SourceKey source, juce::Point<float> at);

  /** A source moved. True when it is a camera finger. */
  bool move (SourceKey source, juce::Point<float> at);

  /** A source went up. True when it was a camera finger. */
  bool release (SourceKey source);

  /** Drop the fingers that are no longer down -- their release was lost. */
  void forgetIfNotDown (std::function<bool (SourceKey)> const &isDown);

  void clear ();

  std::size_t count () const;

  /** The distance between the two fingers of a pinch; 0 unless there are
   *  exactly two. */
  float pinchDistance () const;

private:
  using Finger = std::pair<SourceKey, juce::Point<float> >;

  Finger *find (SourceKey source);

  std::vector<Finger> _down;
};

}
