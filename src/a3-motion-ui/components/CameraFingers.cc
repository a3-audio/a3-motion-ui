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

#include "CameraFingers.hh"

#include <a3-motion-ui/components/RemotePointer.hh>

#include <algorithm>

namespace a3
{

namespace
{
bool
isRemotePointer (SourceKey source)
{
  return source.type == SourceKey::touch && source.index == remoteTouchIndex;
}

// Fingers of one gesture come from one kind of source on one glass: the
// pro-DJ's remote pointer is a touch too, but never a second finger to the
// DJ's -- with it, a remote tap during a camera drag was a pinch.
bool
sameGesture (SourceKey a, SourceKey b)
{
  return a.type == b.type && isRemotePointer (a) == isRemotePointer (b);
}
}

CameraFingers::Finger *
CameraFingers::find (SourceKey source)
{
  auto const it = std::find_if (
      _down.begin (), _down.end (),
      [source] (Finger const &finger) { return finger.first == source; });
  return it == _down.end () ? nullptr : &*it;
}

bool
CameraFingers::press (SourceKey source, juce::Point<float> at)
{
  if (auto *const finger = find (source))
    {
      finger->second = at;
      return true;
    }

  // On the device every finger arrives twice, as a touch and as X's emulated
  // mouse at the same point. Counted as two, one finger was a pinch. So the
  // kind of source that touched first holds the sphere -- every finger of it,
  // for a pinch -- and the other kinds wait until its last one is up.
  if (!_down.empty () && !sameGesture (_down.front ().first, source))
    return false;

  _down.emplace_back (source, at);
  return true;
}

bool
CameraFingers::move (SourceKey source, juce::Point<float> at)
{
  auto *const finger = find (source);
  if (finger == nullptr)
    return false;
  finger->second = at;
  return true;
}

bool
CameraFingers::release (SourceKey source)
{
  auto const gone = std::remove_if (
      _down.begin (), _down.end (),
      [source] (Finger const &finger) { return finger.first == source; });
  auto const released = gone != _down.end ();
  _down.erase (gone, _down.end ());
  return released;
}

void
CameraFingers::forgetIfNotDown (
    std::function<bool (SourceKey)> const &isDown)
{
  _down.erase (std::remove_if (_down.begin (), _down.end (),
                               [&isDown] (Finger const &finger) {
                                 return !isDown (finger.first);
                               }),
               _down.end ());
}

void
CameraFingers::clear ()
{
  _down.clear ();
}

std::size_t
CameraFingers::count () const
{
  return _down.size ();
}

float
CameraFingers::pinchDistance () const
{
  if (_down.size () != 2)
    return 0.f;
  return _down[0].second.getDistanceFrom (_down[1].second);
}

}
