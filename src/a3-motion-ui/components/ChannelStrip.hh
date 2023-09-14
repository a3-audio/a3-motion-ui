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

#include "a3-motion-ui/components/DirectivityComponent.hh"
#include "a3-motion-ui/components/LayoutHints.hh"

namespace a3
{

class Channel;
class ChannelUIState;

class ChannelStrip : public juce::Component
{
public:
  ChannelStrip (ChannelUIState const &channelViewState);

  void resized () override;
  void paint (juce::Graphics &) override;

  // static constexpr int
  // getMinimumHeight ()
  // {
  //   return 200;
  // }

private:
  ChannelUIState const &_uiState;
  DirectivityComponent _directivity;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelStrip)
};

}
