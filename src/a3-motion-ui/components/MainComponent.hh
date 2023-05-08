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

#include <vector>

#include <a3-motion-engine/MotionEngine.hh>

#include <a3-motion-ui/LookAndFeel.hh>
#include <a3-motion-ui/components/ChannelFooter.hh>
#include <a3-motion-ui/components/ChannelHeader.hh>
#include <a3-motion-ui/components/MotionComponent.hh>

namespace a3
{

class MainComponent : public juce::Component
{
public:
  MainComponent (int const numChannels);
  ~MainComponent ();

  void paint (juce::Graphics &g) override;
  void resized () override;

  float getMinimumWidth () const;
  float getMinimumHeight () const;

private:
  void createChannelsUI ();

  MotionEngine engine;

  // UI
  LookAndFeel_A3 lookAndFeel;
  std::vector<std::unique_ptr<ChannelHeader> > headers;
  std::vector<std::unique_ptr<ChannelFooter> > footers;
  MotionComponent motionComp;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

}
