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

#include <a3-motion-ui/components/ChannelFooter.hh>
#include <a3-motion-ui/components/ChannelHeader.hh>
#include <a3-motion-ui/components/ChannelViewState.hh>
#include <a3-motion-ui/components/LookAndFeel.hh>

namespace a3
{
class TempoEstimator;
class TempoEstimatorTest;

class StatusBar;
class MotionComponent;
class InputOutputAdapter;

class A3MotionUIComponent : public juce::Component,
                            public juce::Value::Listener

{
public:
  A3MotionUIComponent (unsigned int const numChannels);
  ~A3MotionUIComponent ();

  void paint (juce::Graphics &g) override;
  void resized () override;

  float getMinimumWidth () const;
  float getMinimumHeight () const;

  void valueChanged (juce::Value &value) override;

private:
  void createChannelsUI ();
  void createHardwareInterface ();

  MotionEngine _engine;
  std::unique_ptr<TempoEstimatorTest> _tempoEstimatorTest;

  LookAndFeel_A3 _lookAndFeel;
  std::vector<std::unique_ptr<ChannelViewState> > _viewStates;
  std::vector<std::unique_ptr<ChannelHeader> > _headers;
  std::vector<std::unique_ptr<ChannelFooter> > _footers;
  std::unique_ptr<MotionComponent> _motionComponent;
  std::unique_ptr<StatusBar> _statusBar;

  std::unique_ptr<InputOutputAdapter> _ioAdapter;

  bool const _drawHeaders = false;

  juce::Value _tempoBPM;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (A3MotionUIComponent)
};

}
