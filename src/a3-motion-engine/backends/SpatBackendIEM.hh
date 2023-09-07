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

#include "SpatBackend.hh"

#include <JuceHeader.h>

namespace a3
{

class SpatBackendIEM : public SpatBackend
{
public:
  SpatBackendIEM (juce::String address, int basePort);

  void sendChannelPosition (index_t channel, Pos const &pos) override;
  void sendChannelWidth (index_t channel, float width) override;

private:
  juce::String _address;
  int _basePort;

  juce::OSCSender _sender;
};

}
