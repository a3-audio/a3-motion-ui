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

#include "TempoEstimatorIRLS.hh"

#include <JuceHeader.h>

#include <chrono>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_multifit.h>

namespace
{
void
gsl_error_handler (const char *reason, const char *file, int line,
                   int gsl_errno)
{
  juce::ignoreUnused (file);
  juce::ignoreUnused (line);
  juce::ignoreUnused (gsl_errno);
  std::cerr << "GSL error: " << reason << std::endl;
}
}

namespace a3
{

TempoEstimatorIRLS::TempoEstimatorIRLS ()
{
  gsl_set_error_handler (gsl_error_handler);
}

void
TempoEstimatorIRLS::estimateTempo ()
{
  // estimate using GNU scientific library robust linear regression.
  // adapted from
  // http://transit.iut2.upmf-grenoble.fr/doc/gsl-ref-html/Fitting-robust-linear-regression-example.html

  auto const tapTimes = getTapTimes ();
  jassert (tapTimes.size () >= numTapsMin);
  std::cout << "tapTimes.size (): " << tapTimes.size () << std::endl;

  auto const numObservations = tapTimes.size ();
  auto constexpr numParameters = 2; // linear slope and offset

  auto y = gsl_vector_alloc (numObservations);
  for (auto index = 0u; index < numObservations; ++index)
    {
      auto timeMicros = std::chrono::duration_cast<std::chrono::microseconds> (
                            tapTimes[index] - tapTimes[0])
                            .count ();
      gsl_vector_set (y, index, timeMicros);
    }

  auto X = gsl_matrix_alloc (numObservations, numParameters);
  for (auto index = 0u; index < numObservations; ++index)
    {
      gsl_matrix_set (X, index, 0, 1.0);
      gsl_matrix_set (X, index, 1, index);
    }

  auto coefficients = gsl_vector_alloc (numParameters);
  auto covariance = gsl_matrix_alloc (numParameters, numParameters);
  auto workspace = gsl_multifit_robust_alloc (gsl_multifit_robust_default,
                                              numObservations, numParameters);

  auto iterations
      = gsl_multifit_robust (X, y, coefficients, covariance, workspace);
  juce::Logger::writeToLog ("robust fit iterations: "
                            + juce::String (iterations));

  setTempoDeltaT (std::chrono::microseconds (
      (long long)(gsl_vector_get (coefficients, 1))));

  gsl_multifit_robust_free (workspace);
  gsl_matrix_free (covariance);
  gsl_vector_free (coefficients);
  gsl_matrix_free (X);
  gsl_vector_free (y);
}

}
