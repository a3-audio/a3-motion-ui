#include <type_traits>

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <a3-motion-engine/util/Geometry.hh>

using namespace a3;

TEST (Position, PosIsFloat)
{
  ASSERT_TRUE ((std::is_same_v<Pos, Position<float> >));
}

TEST (Position, ZeroInitialise)
{
  Pos p;

  ASSERT_FLOAT_EQ (p.x (), 0.f);
  ASSERT_FLOAT_EQ (p.y (), 0.f);
  ASSERT_FLOAT_EQ (p.z (), 0.f);
}

TEST (Position, SetCartesianQueryCartesian)
{
  Pos p;

  auto inputX = 23.f;
  auto inputY = 17.5f;
  auto inputZ = 42.f;

  p.setX (inputX);
  p.setY (inputY);
  p.setZ (inputZ);

  ASSERT_FLOAT_EQ (p.x (), inputX);
  ASSERT_FLOAT_EQ (p.y (), inputY);
  ASSERT_FLOAT_EQ (p.z (), inputZ);

  inputX = 13.37f;
  inputY = 69.f;
  inputZ = 4.20f;

  p = Pos::fromCartesian (inputX, inputY, inputZ);

  ASSERT_FLOAT_EQ (p.x (), inputX);
  ASSERT_FLOAT_EQ (p.y (), inputY);
  ASSERT_FLOAT_EQ (p.z (), inputZ);
}

using xyz_aed_equivalence_list_t = const std::initializer_list<std::pair<
    std::tuple<double, double, double>, std::tuple<double, double, double> > >;
const auto xyz_aed_equivalence_list = xyz_aed_equivalence_list_t{
  { { 1.f, 0.f, 0.f }, { 0.f, 0.f, 1.f } },                  //
  { { 2.f, 0.f, 0.f }, { 0.f, 0.f, 2.f } },                  //
  { { 1.f, 1.f, 0.f }, { 45.f, 0.f, std::sqrt (2.f) } },     //
  { { 1.f, -1.f, 0.f }, { -45.f, 0.f, std::sqrt (2.f) } },   //
  { { 0.f, 1.f, 0.f }, { 90.f, 0.f, 1.f } },                 //
  { { 0.f, -1.f, 0.f }, { -90.f, 0.f, 1.f } },               //
  { { -1.f, 1.f, 0.f }, { 135.f, 0.f, std::sqrt (2.f) } },   //
  { { -1.f, -1.f, 0.f }, { -135.f, 0.f, std::sqrt (2.f) } }, //
  { { -1.f, 0.f, 0.f }, { 180.f, 0.f, 1.f } },               //
  // { { 1.f, 1.f, 1.f }, { 45.f, 45.f, std::sqrt (3.f) } },    //
};

auto
AzimuthElevationDistanceEq (std::tuple<double, double, double> const &expected)
{
  using namespace ::testing;
  const auto [azimuth, elevation, distance] = expected;
  return AllOf (Property (&Pos::azimuth, FloatEq (azimuth)),
                Property (&Pos::elevation, FloatEq (elevation)),
                Property (&Pos::distance, FloatEq (distance)));
}

std::string
xyz_to_aed_error (const std::tuple<double, double, double> &input,
                  const std::tuple<double, double, double> &expected,
                  const Pos &received)
{
  const auto [x, y, z] = input;
  const auto [azimuth, elevation, distance] = expected;
  std::ostringstream ss;
  ss << "FROM INPUT XYZ: " << x << " " << y << " " << z << std::endl;
  ss << "EXPECTED - RECEIVED:" << std::endl
     << "azimuth: " << azimuth << " - " << received.azimuth () << std::endl
     << "elevation: " << elevation << " - " << received.elevation ()
     << std::endl //
     << "distance: " << distance << " - " << received.distance () << std::endl;
  return ss.str ();
}

TEST (Position, SetCartesianQuerySpherical)
{
  for (auto const &xyz_aed_equivalence : xyz_aed_equivalence_list)
    {
      const auto [x, y, z] = xyz_aed_equivalence.first;
      auto p = Pos::fromCartesian (x, y, z);
      ASSERT_THAT (p, AzimuthElevationDistanceEq (xyz_aed_equivalence.second))
          << xyz_to_aed_error (xyz_aed_equivalence.first,
                               xyz_aed_equivalence.second, p);
    }
}
