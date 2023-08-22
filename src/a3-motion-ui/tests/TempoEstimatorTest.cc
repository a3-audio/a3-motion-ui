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

#include "TempoEstimatorTest.hh"

#include <a3-motion-engine/tempo/TempoEstimator.hh>
#include <a3-motion-engine/tempo/TempoEstimatorIRLS.hh>
#include <a3-motion-engine/tempo/TempoEstimatorLast.hh>
#include <a3-motion-engine/tempo/TempoEstimatorMean.hh>
#include <a3-motion-engine/tempo/TempoEstimatorMeanSelective.hh>

namespace a3
{

TempoEstimatorTest::TempoEstimatorTest ()
{
  _vectorEstimatorTests.push_back (
      { "mean", std::make_unique<a3::TempoEstimatorMean> () });
  _vectorEstimatorTests.push_back (
      { "sel4", std::make_unique<a3::TempoEstimatorMeanSelective> (4) });
  _vectorEstimatorTests.push_back (
      { "irls", std::make_unique<a3::TempoEstimatorIRLS> () });

  // EstimatorTest{ "last", std::make_unique<a3::TempoEstimatorLast> () },
  // EstimatorTest{ "sel3",
  //                std::make_unique<a3::TempoEstimatorMeanSelective> (3)
  //                },
  // EstimatorTest{ "sel5",
  //                std::make_unique<a3::TempoEstimatorMeanSelective> (5)
  //                },
  // EstimatorTest{ "sel6",
  //                std::make_unique<a3::TempoEstimatorMeanSelective> (6)
  //                },

  _outTimings.open ("timings.csv");
  _outTimings.precision (10);

  for (auto index = 0u; index < _vectorEstimatorTests.size (); ++index)
    {
      _outTimings << _vectorEstimatorTests[index].name;
      if (index != _vectorEstimatorTests.size () - 1)
        _outTimings << ",";
    }
  _outTimings << std::endl;
}

TempoEstimatorTest::~TempoEstimatorTest ()
{
  _outTimings.close ();
}

void
TempoEstimatorTest::valueChanged (juce::Value &value)
{
  bool writeNewLine = false;
  for (auto index = 0u; index < _vectorEstimatorTests.size (); ++index)
    {
      auto &estimatorTest = _vectorEstimatorTests[index];
      if (estimatorTest.estimator->tap (value.getValue ())
          == TempoEstimator::TapResult::TempoAvailable)
        {
          writeNewLine = true;

          _outTimings << estimatorTest.estimator->getTempoBPM ();
          if (index != _vectorEstimatorTests.size () - 1)
            _outTimings << ",";
        }
    }
  if (writeNewLine)
    _outTimings << std::endl;
}
}
