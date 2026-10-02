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

#include <optional>

#include <JuceHeader.h>

namespace a3
{

/** The one truth for OSC vocabulary, ports and IPs -- its reader.
 *
 *  a3-core ships /usr/share/a3/a3-osc.json, and every device reads its
 *  addresses and ports from it (decided 2026-09-30). This is Motion's copy of
 *  the reader a3-core has in Python (a3_osc.py): the same questions, the same
 *  answers. It holds no defaults of its own; a fact the file does not have
 *  comes back as nothing. */
class OscTruth
{
public:
  struct Endpoint
  {
    juce::String host;
    int port{ -1 };
  };

  bool isValid () const { return _error.isEmpty (); }
  juce::String const &error () const { return _error; }

  /** The IP of a named host, or empty. */
  juce::String host (juce::String const &name) const;

  /** Where `program` listens as `role`, or -1. */
  int port (juce::String const &program, juce::String const &role) const;

  /** Where to send to reach `program` as `role`. A listener on every
   *  interface ("any") is reached locally. */
  std::optional<Endpoint> endpoint (juce::String const &program,
                                    juce::String const &role) const;

  /** The address pattern for `key` ("channel.volume" ->
   *  "/channel/{ch}/volume"), or empty. */
  juce::String pattern (juce::String const &key) const;

  /** The number a meter is sent under, counted from 1 -- its place in the
   *  channel map's `vu_meters` -- or 0 if the map does not have it. */
  int vuNumber (juce::String const &meterName) const;

private:
  friend OscTruth parseOscTruth (juce::String const &json);
  friend OscTruth loadOscTruth (juce::File const &file);

  juce::var listenerFor (juce::String const &program,
                         juce::String const &role) const;

  juce::var _data;
  juce::String _error;
};

OscTruth parseOscTruth (juce::String const &json);
OscTruth loadOscTruth (juce::File const &file);

/** $A3_OSC_TRUTH if set, else the installed /usr/share/a3/a3-osc.json. */
juce::File oscTruthFile ();

/** The truth Motion reads (spec truth-from-core, step 3): `override`
 *  ($A3_OSC_TRUTH), else what Core last served in `home`/.cache/a3 if it
 *  reads as a truth, else the package's file. */
juce::File oscTruthFileFrom (char const *override, juce::File const &home);

/** The truth this process runs on: oscTruthFile(), read once. */
OscTruth const &installedOscTruth ();

}
