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

#include <fstream>

#include <JuceHeader.h>

namespace a3
{

class TempoEstimator;

class TempoEstimatorTest : public juce::Value::Listener
{
public:
  TempoEstimatorTest ();
  ~TempoEstimatorTest ();

  void valueChanged (juce::Value &value) override;

private:
  struct EstimatorTest
  {
    juce::String name;
    std::unique_ptr<a3::TempoEstimator> estimator;
  };

  std::vector<EstimatorTest> _vectorEstimatorTests;
  std::ofstream _outTimings;
};

}
