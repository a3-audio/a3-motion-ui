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

#include "FpvFloor.hh"

#include <a3-motion-ui/components/fpv/BodyLook.hh>

namespace a3
{

bool
onTheFloor (std::optional<Vec2> floorPoint)
{
  return floorPoint && floorPoint->getDistanceFromOrigin () <= 1.f;
}

std::optional<int>
bodyUnderFinger (std::array<BodyOnScreen, maxFlightBodies> const &bodies,
                 int count, juce::Point<float> at)
{
  std::optional<int> hit;
  auto nearest = 0.f;
  for (auto i = 0; i < std::min (count, maxFlightBodies); ++i)
    {
      auto const &body = bodies[static_cast<size_t> (i)];
      auto const distance = at.getDistanceFrom (body.centre);
      if (distance > body.hitRadius || (hit && distance >= nearest))
        continue;
      hit = body.id;
      nearest = distance;
    }
  return hit;
}

void
FpvEscorts::set (int channel, int bodyId)
{
  if (channel < 0 || channel >= fpvShips)
    return;
  _escort[static_cast<size_t> (channel)] = bodyId;
}

int
FpvEscorts::of (int channel) const
{
  if (channel < 0 || channel >= fpvShips)
    return noBodyId;
  return _escort[static_cast<size_t> (channel)];
}

std::array<bool, fpvShips>
FpvEscorts::forget (int bodyId)
{
  std::array<bool, fpvShips> changed{};
  for (auto ch = 0u; ch < _escort.size (); ++ch)
    {
      if (bodyId == noBodyId || _escort[ch] != bodyId)
        continue;
      _escort[ch] = noBodyId;
      changed[ch] = true;
    }
  return changed;
}

namespace
{
FlightBody const *
bodyWithId (FlightBodies const &bodies, int id)
{
  if (id == noBodyId)
    return nullptr;
  for (auto i = 0; i < std::min (bodies.count, maxFlightBodies); ++i)
    if (bodies.body[static_cast<size_t> (i)].id == id)
      return &bodies.body[static_cast<size_t> (i)];
  return nullptr;
}

bool
isDeadZone (FlightBody const &body)
{
  return bodyRole (body.mass) == BodyRole::Repel;
}
}

std::array<bool, fpvShips>
FpvEscorts::dropDeadZones (FlightBodies const &bodies)
{
  std::array<bool, fpvShips> changed{};
  for (auto ch = 0u; ch < _escort.size (); ++ch)
    {
      auto const *body = bodyWithId (bodies, _escort[ch]);
      if (body == nullptr || !isDeadZone (*body))
        continue;
      _escort[ch] = noBodyId;
      changed[ch] = true;
    }
  return changed;
}

EscortView
escortView (int escortId, bool orbit, FlightBodies const &bodies)
{
  if (!orbit)
    return {};
  auto const *body = bodyWithId (bodies, escortId);
  if (body == nullptr || isDeadZone (*body))
    return {};
  return { escortId, body->mass };
}

void
FpvPageHold::press (int channel)
{
  _channel = channel;
  _tapped = false;
  _body.reset ();
}

bool
FpvPageHold::isHeld () const
{
  return _channel.has_value ();
}

void
FpvPageHold::tap (std::optional<int> bodyId)
{
  if (!_channel)
    return;
  _tapped = true;
  _body = bodyId;
}

void
FpvPageHold::bodyRemoved (int bodyId)
{
  if (_body == std::optional<int>{ bodyId })
    _body.reset ();
}

std::optional<int>
FpvPageHold::tappedBody () const
{
  return _body;
}

std::optional<PageOutcome>
FpvPageHold::release (int channel)
{
  if (_channel != std::optional<int>{ channel })
    return std::nullopt;
  auto const outcome = fpvPagePress (_tapped, _body);
  clear ();
  return outcome;
}

void
FpvPageHold::clear ()
{
  _channel.reset ();
  _tapped = false;
  _body.reset ();
}

juce::String
fpvPageReadout (int channel, PageOutcome outcome, bool orbitNow, int bodyId,
                FlightClip clip)
{
  auto const ch = "CH" + juce::String (channel + 1) + " ";
  switch (outcome)
    {
    case PageOutcome::Escort:
      return ch + juce::String::fromUTF8 ("\xe2\x86\x92 ") + bodyLabel (bodyId);
    case PageOutcome::Patrol:
      return ch + "PATROL";
    case PageOutcome::Toggle:
    case PageOutcome::None:
      break;
    }
  if (!orbitNow)
    return ch + "CLIP";
  switch (clip)
    {
    case FlightClip::None:
      return ch + juce::String::fromUTF8 ("ORBIT \xc2\xb7 no clip");
    case FlightClip::Stopped:
      return ch + juce::String::fromUTF8 ("ORBIT \xc2\xb7 stopped");
    case FlightClip::Running:
      break;
    }
  return ch + "ORBIT";
}

FlightClip
flightClipOf (Pattern const *clip)
{
  if (clip == nullptr || clip->getStatus () == Pattern::Status::Empty)
    return FlightClip::None;
  return passIsRunning (*clip) ? FlightClip::Running : FlightClip::Stopped;
}

PageOutcome
flownOutcome (PageOutcome outcome, int bodyId, FlightBodies const &bodies)
{
  if (outcome != PageOutcome::Escort)
    return outcome;
  return escortView (bodyId, true, bodies).escort == noBodyId
             ? PageOutcome::Patrol
             : outcome;
}

juce::String
padPressReadout (int channel, juce::String const &padName, AppView view,
                 bool orbit, bool playOrPagePad)
{
  auto const ch = "CH" + juce::String (channel + 1) + " ";
  if (view == AppView::Full && orbit && playOrPagePad)
    return ch + "ORBIT";
  return ch + padName;
}

}
