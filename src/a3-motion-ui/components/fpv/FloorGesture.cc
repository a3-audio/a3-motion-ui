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

#include "FloorGesture.hh"

namespace a3
{

void
FloorGesture::down (juce::Point<float> at, double ms, std::optional<int> body)
{
  _state = State::Undecided;
  _start = at;
  _downMs = ms;
  _body = body;
}

FloorAction
FloorGesture::move (juce::Point<float> at, double)
{
  if (_state == State::Undecided
      && at.getDistanceFrom (_start) > slopOfBlob * _blobDiameter)
    _state = _body ? State::Drag : State::Camera;

  if (_state == State::Drag)
    return FloorAction::Drag;
  if (_state == State::Camera)
    return FloorAction::Camera;
  return FloorAction::None;
}

FloorAction
FloorGesture::up (juce::Point<float>, double ms)
{
  auto const undecided = _state == State::Undecided;
  _state = State::Idle;
  if (!undecided)
    return FloorAction::None;

  if (!_body)
    return heldLongEnough (ms) ? FloorAction::None : FloorAction::Place;
  return heldLongEnough (ms) ? FloorAction::Remove : FloorAction::CycleWeight;
}

FloorAction
FloorGesture::held (double ms)
{
  if (_state != State::Undecided || !_body || !heldLongEnough (ms))
    return FloorAction::None;

  _state = State::Finished;
  return FloorAction::Remove;
}

void
FloorGesture::cancel ()
{
  if (_state != State::Idle)
    _state = State::Finished;
}

std::optional<int>
FloorGesture::body () const
{
  return _body;
}

float
FloorGesture::holdProgress (double ms) const
{
  if (_state != State::Undecided || !_body)
    return 0.f;
  return static_cast<float> (juce::jlimit (0., 1., (ms - _downMs) / longPressMs));
}

void
FloorGesture::setBlobDiameter (float pixels)
{
  _blobDiameter = pixels;
}

bool
FloorGesture::heldLongEnough (double ms) const
{
  return ms - _downMs >= longPressMs;
}

}
