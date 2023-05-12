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

#include <type_traits>

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <a3-motion-engine/util/Geometry.hh>

using namespace a3;

template <typename ScalarT>
ScalarT
error_bound ()
{
  static_assert (std::is_same_v<ScalarT, float>
                 || std::is_same_v<ScalarT, double>);

  if constexpr (std::is_same_v<ScalarT, float>)
    return 1e-5f;
  if constexpr (std::is_same_v<ScalarT, double>)
    return 2e-14;
}

using xyz_aed_equivalence_list_t = const std::initializer_list<std::pair<
    std::tuple<double, double, double>, std::tuple<double, double, double> > >;

// elevation angle between the diagonal of a cube and the diagonal
// of one of its sides.
const auto elevation_cube
    = std::acos (std::sqrt (2.) / std::sqrt (3.)) * 180 / pi<double> ();

const auto xyz_aed_equivalence_list = xyz_aed_equivalence_list_t{
  // main axes
  { { 1., 0., 0. }, { 0., 0., 1. } },    //
  { { -1., 0., 0. }, { 180., 0., 1. } }, //
  { { 0., 1., 0. }, { 90., 0., 1. } },   //
  { { 0., -1., 0. }, { -90., 0., 1. } }, //
  { { 0., 0., 1. }, { 0., 90., 1. } },   //
  { { 0., 0., -1. }, { 0., -90., 1. } }, //
  // magnitude sanity
  { { 2., 0., 0. }, { 0., 0., 2. } }, //
  // planes through the origin: xy
  { { 1., 1., 0. }, { 45., 0., std::sqrt (2.) } },     //
  { { 1., -1., 0. }, { -45., 0., std::sqrt (2.) } },   //
  { { -1., 1., 0. }, { 135., 0., std::sqrt (2.) } },   //
  { { -1., -1., 0. }, { -135., 0., std::sqrt (2.) } }, //
  // planes through the origin: xz
  { { 1., 0., 1. }, { 0., 45., std::sqrt (2.) } },      //
  { { 1., 0., -1. }, { 0., -45., std::sqrt (2.) } },    //
  { { -1., 0., 1. }, { 180., 45., std::sqrt (2.) } },   //
  { { -1., 0., -1. }, { 180., -45., std::sqrt (2.) } }, //
  // planes through the origin: yz
  { { 0., 1., 1. }, { 90., 45., std::sqrt (2.) } },     //
  { { 0., 1., -1. }, { 90., -45., std::sqrt (2.) } },   //
  { { 0., -1., 1. }, { -90., 45., std::sqrt (2.) } },   //
  { { 0., -1., -1. }, { -90., -45., std::sqrt (2.) } }, //
  // corners of the unit cube
  { { 1., 1., 1. }, { 45., elevation_cube, std::sqrt (3.) } },       //
  { { 1., 1., -1. }, { 45., -elevation_cube, std::sqrt (3.) } },     //
  { { 1., -1., 1. }, { -45., elevation_cube, std::sqrt (3.) } },     //
  { { 1., -1., -1. }, { -45., -elevation_cube, std::sqrt (3.) } },   //
  { { -1., 1., 1. }, { 135., elevation_cube, std::sqrt (3.) } },     //
  { { -1., 1., -1. }, { 135., -elevation_cube, std::sqrt (3.) } },   //
  { { -1., -1., 1. }, { -135., elevation_cube, std::sqrt (3.) } },   //
  { { -1., -1., -1. }, { -135., -elevation_cube, std::sqrt (3.) } }, //
  // origin in the limit from above/below
  { { 0., 0., 0. }, { 0., 90., 0. } },   //
  { { 0., 0., -0. }, { 0., -90., 0. } }, //
};

auto
GenericNear (double const scalar, double const error)
{
  return ::testing::DoubleNear (scalar, error);
}

auto
GenericNear (float const scalar, float const error)
{
  return ::testing::FloatNear (scalar, error);
}

template <typename ScalarT>
auto
SphericalEq (std::tuple<ScalarT, ScalarT, ScalarT> const &expected)
{
  using namespace ::testing;
  const auto [azimuth, elevation, distance] = expected;

  return AllOf (Property (&Position<ScalarT>::azimuth,
                          GenericNear (azimuth, error_bound<ScalarT> ())),
                Property (&Position<ScalarT>::elevation,
                          GenericNear (elevation, error_bound<ScalarT> ())),
                Property (&Position<ScalarT>::distance,
                          GenericNear (distance, error_bound<ScalarT> ())));
}

template <typename ScalarT>
auto
CartesianEq (std::tuple<ScalarT, ScalarT, ScalarT> const &expected)
{
  using namespace ::testing;
  const auto [x, y, z] = expected;
  return AllOf (Property (&Position<ScalarT>::x,
                          GenericNear (x, error_bound<ScalarT> ())),
                Property (&Position<ScalarT>::y,
                          GenericNear (y, error_bound<ScalarT> ())),
                Property (&Position<ScalarT>::z,
                          GenericNear (z, error_bound<ScalarT> ())));
}

template <typename ScalarT>
std::string
xyz_to_aed_error (const std::tuple<ScalarT, ScalarT, ScalarT> &input,
                  const std::tuple<ScalarT, ScalarT, ScalarT> &expected,
                  const Position<ScalarT> &received)
{
  const auto [x, y, z] = input;
  const auto [azimuth, elevation, distance] = expected;
  std::ostringstream ss;
  ss << "FROM INPUT XYZ: " << x << " " << y << " " << z << std::endl;
  ss << "EXPECTED - RECEIVED:" << std::endl
     << "azimuth: " << azimuth << " | " << received.azimuth () << std::endl
     << "elevation: " << elevation << " | " << received.elevation ()
     << std::endl //
     << "distance: " << distance << " | " << received.distance () << std::endl;
  return ss.str ();
}

template <typename ScalarT>
std::string
aed_to_xyz_error (const std::tuple<ScalarT, ScalarT, ScalarT> &input,
                  const std::tuple<ScalarT, ScalarT, ScalarT> &expected,
                  const Position<ScalarT> &received)
{
  const auto [azimuth, elevation, distance] = input;
  const auto [x, y, z] = expected;
  std::ostringstream ss;
  ss << "FROM INPUT AED: " << azimuth << " " << elevation << " " << distance
     << std::endl;
  ss << "EXPECTED - RECEIVED:" << std::endl
     << "x: " << x << " | " << received.x () << std::endl
     << "y: " << y << " | " << received.y () << std::endl //
     << "z: " << z << " | " << received.z () << std::endl;
  return ss.str ();
}

TEST (Position, PosIsFloat)
{
  ASSERT_TRUE ((std::is_same_v<Pos, Position<float> >));
}

template <typename ScalarT>
class PositionTest : public testing::Test
{
};
using ScalarTypes = ::testing::Types<float, double>;
TYPED_TEST_SUITE (PositionTest, ScalarTypes);

TYPED_TEST (PositionTest, ZeroInitialise)
{
  Position<TypeParam> p;
  ASSERT_THAT (p, CartesianEq<TypeParam> ({ 0, 0, 0 }));
}

TYPED_TEST (PositionTest, SetCartesianQueryCartesian)
{
  Position<TypeParam> p;

  auto inputX = TypeParam{ 23. };
  auto inputY = TypeParam{ 17.5 };
  auto inputZ = TypeParam{ 42. };

  p = Position<TypeParam>::fromCartesian (inputX, inputY, inputZ);

  ASSERT_THAT (p, CartesianEq<TypeParam> ({ inputX, inputY, inputZ }));

  inputX = 13.37f;
  inputY = 69.f;
  inputZ = 4.20f;

  p.setX (inputX);
  p.setY (inputY);
  p.setZ (inputZ);

  ASSERT_THAT (p, CartesianEq<TypeParam> ({ inputX, inputY, inputZ }));
}

TYPED_TEST (PositionTest, SetCartesianQuerySpherical)
{
  for (auto const &xyz_aed_equivalence : xyz_aed_equivalence_list)
    {
      const auto [x, y, z] = xyz_aed_equivalence.first;
      auto p = Position<TypeParam>::fromCartesian (x, y, z);
      ASSERT_THAT (p, SphericalEq<TypeParam> (xyz_aed_equivalence.second))
          << xyz_to_aed_error<TypeParam> (xyz_aed_equivalence.first,
                                          xyz_aed_equivalence.second, p);
    }
}

TYPED_TEST (PositionTest, SetSphericalQueryCartesian)
{
  for (auto const &xyz_aed_equivalence : xyz_aed_equivalence_list)
    {
      const auto [azimuth, elevation, distance] = xyz_aed_equivalence.second;
      auto p
          = Position<TypeParam>::fromSpherical (azimuth, elevation, distance);
      ASSERT_THAT (p, CartesianEq<TypeParam> (xyz_aed_equivalence.first))
          << aed_to_xyz_error<TypeParam> (xyz_aed_equivalence.second,
                                          xyz_aed_equivalence.first, p);
    }
}

TYPED_TEST (PositionTest, Addition)
{
  using PosT = Position<TypeParam>;
  PosT p0 = PosT::fromCartesian (2, 5, 1);
  PosT p1 = PosT::fromCartesian (4, 7, -5);

  auto p = p1 + p0;
  ASSERT_THAT (p, CartesianEq<TypeParam> ({ 6, 12, -4 }));

  p += p0;
  ASSERT_THAT (p, CartesianEq<TypeParam> ({ 8, 17, -3 }));
}

TYPED_TEST (PositionTest, Subtraction)
{
  using PosT = Position<TypeParam>;
  PosT p0 = PosT::fromCartesian (2, 5, 1);
  PosT p1 = PosT::fromCartesian (4, 7, -5);

  auto p = p1 - p0;
  ASSERT_THAT (p, CartesianEq<TypeParam> ({ 2, 2, -6 }));

  p -= p0;
  ASSERT_THAT (p, CartesianEq<TypeParam> ({ 0, -3, -7 }));
}
