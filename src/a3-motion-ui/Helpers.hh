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

#include <JuceHeader.h>

#include <a3-motion-engine/util/Geometry.hh>

namespace juce
{
template <>
struct VariantConverter<juce::Colour>
{
  static Colour
  fromVar (const var &v)
  {
    return Colour{ static_cast<juce::uint32> (static_cast<juce::int64> (v)) };
  }
  static var
  toVar (const Colour &colour)
  {
    return static_cast<juce::int64> (colour.getARGB ());
  }
};
}

namespace a3
{

template <typename Scalar>
juce::Point<Scalar>
cartesian2DHOA2JUCE (Position<Scalar> const &posHOA)
{
  return { -posHOA.y (), -posHOA.x () };
}

template <typename Scalar>
Position<Scalar>
cartesian2DJUCE2HOA (juce::Point<Scalar> const &posJUCE)
{
  return Position<Scalar>::fromCartesian (-posJUCE.getY (), -posJUCE.getX (),
                                          0.f);
}

}
