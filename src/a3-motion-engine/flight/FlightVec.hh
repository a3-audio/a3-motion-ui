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

#include <JuceHeader.h>

#include <cmath>

namespace a3
{

/** A point or a velocity on the floor plane, seen from above, with the room's
 *  edge at radius 1.
 *
 *  Not juce::Point, which the plan named: that lives in juce_graphics, and the
 *  engine is headless and links juce_core and juce_osc only (the pattern
 *  generator links nothing else). The names follow juce::Point's, so code on
 *  the UI side reads the same and converts with one line. */
struct Vec2
{
  float x = 0.f;
  float y = 0.f;

  Vec2 &
  operator+= (Vec2 other)
  {
    x += other.x;
    y += other.y;
    return *this;
  }

  Vec2 &
  operator-= (Vec2 other)
  {
    x -= other.x;
    y -= other.y;
    return *this;
  }

  Vec2 &
  operator*= (float factor)
  {
    x *= factor;
    y *= factor;
    return *this;
  }

  friend Vec2 operator+ (Vec2 lhs, Vec2 rhs) { return lhs += rhs; }
  friend Vec2 operator- (Vec2 lhs, Vec2 rhs) { return lhs -= rhs; }
  friend Vec2 operator* (Vec2 v, float factor) { return v *= factor; }
  friend Vec2 operator* (float factor, Vec2 v) { return v *= factor; }
  friend Vec2 operator/ (Vec2 v, float divisor) { return v *= 1.f / divisor; }
  friend Vec2 operator- (Vec2 v) { return { -v.x, -v.y }; }

  friend bool
  operator== (Vec2 lhs, Vec2 rhs)
  {
    return juce::exactlyEqual (lhs.x, rhs.x)
           && juce::exactlyEqual (lhs.y, rhs.y);
  }

  friend bool operator!= (Vec2 lhs, Vec2 rhs) { return !(lhs == rhs); }

  float getDistanceSquaredFromOrigin () const { return x * x + y * y; }
  float getDistanceFromOrigin () const { return std::hypot (x, y); }
  float getDistanceFrom (Vec2 other) const { return (*this - other).getDistanceFromOrigin (); }
  float getDotProduct (Vec2 other) const { return x * other.x + y * other.y; }
};

}
