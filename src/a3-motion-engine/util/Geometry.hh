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

#include <cmath>

#include <JuceHeader.h>
#include <limits>

namespace a3
{

template <typename ScalarT>
class Position;

template <typename ScalarT>
constexpr ScalarT pi ();

template <>
constexpr float
pi ()
{
  return 3.14159265f;
}

template <>
constexpr double
pi ()
{
  return 3.141592653589793;
}

template <typename ScalarT>
int
sign (ScalarT const &val)
{
  return (ScalarT (0) < val) - (val < ScalarT (0));
}

// // currently unused
// template <typename ScalarT>
// int
// dot (const Position<ScalarT> &lhs, const Position<ScalarT> &rhs)
// {
//   return lhs.x () * rhs.x () + //
//          lhs.y () * rhs.y () + //
//          lhs.z () * rhs.z ();
// }

template <typename ScalarT>
class Position
{
public:
  static const Position invalid;

  Position ();

  // Cartesian coordinates
  void setX (ScalarT const &x);
  void setY (ScalarT const &y);
  void setZ (ScalarT const &z);

  ScalarT x () const;
  ScalarT y () const;
  ScalarT z () const;

  static Position fromCartesian (ScalarT const &x, //
                                 ScalarT const &y, //
                                 ScalarT const &z);
  // template <typename ArrayT>
  // static Position fromCartesian (ArrayT const &xyz);

  // Spherical coordinates
  void setAzimuth (ScalarT const &azimuth);
  void setElevation (ScalarT const &elevation);
  void setDistance (ScalarT const &distance);

  ScalarT azimuth () const;
  ScalarT elevation () const;
  ScalarT distance () const;

  static Position fromSpherical (ScalarT const &azimuth,   //
                                 ScalarT const &elevation, //
                                 ScalarT const &distance);

  Position &
  operator+= (const Position &rhs)
  {
    _x += rhs._x;
    _y += rhs._y;
    _z += rhs._z;
    return *this;
  }

  friend Position
  operator+ (Position lhs, const Position &rhs)
  {
    lhs += rhs;
    return lhs;
  }

  Position &
  operator-= (Position const &rhs)
  {
    _x -= rhs._x;
    _y -= rhs._y;
    _z -= rhs._z;
    return *this;
  }

  friend Position
  operator- (Position lhs, Position const &rhs)
  {
    lhs -= rhs;
    return lhs;
  }

  friend bool
  operator== (Position const &lhs, Position const &rhs)
  {
    jassert (
        !(std::isnan (lhs._x) || std::isnan (lhs._y) || std::isnan (lhs._z)));
    jassert (
        !(std::isnan (rhs._x) || std::isnan (rhs._y) || std::isnan (rhs._z)));
    return juce::exactlyEqual (lhs._x, rhs._x) && //
           juce::exactlyEqual (lhs._y, rhs._y) && //
           juce::exactlyEqual (lhs._z, rhs._z);
  }

  friend bool
  operator!= (Position const &lhs, Position const &rhs)
  {
    return !(lhs == rhs);
  }

  bool
  isValid () const
  {
    return !(std::isnan (_x) || std::isnan (_y) || std::isnan (_z));
  }

private:
  ScalarT _x, _y, _z;
};

template <typename ScalarT>
Position<ScalarT>::Position () : _x (0), _y (0), _z (0)
{
}

template <typename ScalarT>
void
Position<ScalarT>::setX (ScalarT const &x)
{
  _x = x;
}

template <typename ScalarT>
void
Position<ScalarT>::setY (ScalarT const &y)
{
  _y = y;
}

template <typename ScalarT>
void
Position<ScalarT>::setZ (ScalarT const &z)
{
  _z = z;
}

template <typename ScalarT>
ScalarT
Position<ScalarT>::x () const
{
  return _x;
}

template <typename ScalarT>
ScalarT
Position<ScalarT>::y () const
{
  return _y;
}

template <typename ScalarT>
ScalarT
Position<ScalarT>::z () const
{
  return _z;
}

template <typename ScalarT>
Position<ScalarT>
Position<ScalarT>::fromCartesian (ScalarT const &x, //
                                  ScalarT const &y, //
                                  ScalarT const &z)
{
  Position<ScalarT> p;
  p._x = x;
  p._y = y;
  p._z = z;
  return p;
}

template <typename ScalarT>
ScalarT
Position<ScalarT>::azimuth () const
{
  return std::atan2 (_y, _x) * 180 / pi<ScalarT> ();
}

template <typename ScalarT>
ScalarT
Position<ScalarT>::elevation () const
{
  auto const inclination
      = std::atan2 (std::sqrt (_x * _x + _y * _y), _z) * 180 / pi<ScalarT> ();
  return 90 - inclination;
}

template <typename ScalarT>
ScalarT
Position<ScalarT>::distance () const
{
  return std::sqrt (_x * _x + _y * _y + _z * _z);
}

template <typename ScalarT>
Position<ScalarT>
Position<ScalarT>::fromSpherical (ScalarT const &azimuth,
                                  ScalarT const &elevation,
                                  ScalarT const &distance)
{
  auto a = azimuth * pi<ScalarT> () / 180;
  auto e = elevation * pi<ScalarT> () / 180;

  Position<ScalarT> p;
  p._x = std::cos (a) * std::cos (e) * distance;
  p._y = std::sin (a) * std::cos (e) * distance;
  p._z = std::sin (e) * distance;

  return p;
}

template <typename ScalarT>
const Position<ScalarT> Position<ScalarT>::invalid
    = Position<ScalarT>::fromCartesian (
        std::numeric_limits<ScalarT>::signaling_NaN (),
        std::numeric_limits<ScalarT>::signaling_NaN (),
        std::numeric_limits<ScalarT>::signaling_NaN ());

}
