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

#include "a3-motion-engine/tempo/TempoClock.hh"
#include <JuceHeader.h>

#include <vector>

#include <a3-motion-engine/MotionEngine.hh>
#include <a3-motion-engine/Pattern.hh>

#include <a3-motion-ui/components/LookAndFeel.hh>
#include <a3-motion-ui/io/InputOutputAdapter.hh>

namespace a3
{
class TempoEstimator;
class TempoEstimatorTest;

class MotionComponent;

class StatusBar;
class ChannelStrip;
class ChannelUIState;
class Pattern;

class A3MotionUIComponent : public juce::Component,
                            public juce::Value::Listener,
                            public juce::MessageListener

{
public:
  A3MotionUIComponent (unsigned int const numChannels);
  ~A3MotionUIComponent ();

  void paint (juce::Graphics &g) override;
  void resized () override;

  float getMinimumWidth () const;
  float getMinimumHeight () const;

  void valueChanged (juce::Value &value) override;
  void handleMessage (juce::Message const &message) override;

private:
  static auto constexpr numPages = 4u;

  std::unique_ptr<HeightMap> _heightMap;
  MotionEngine _engine;

  void tickCallback (Measure measure);
  void padLEDCallback (int step);
  juce::Colour scheduledForIdleLEDColour (int step,
                                          Pattern::Status statusLast);

  Measure _now;
  juce::Value _valueBPM;
  TempoClock::PointerT _tickCallbackHandle;
  TempoClock::PointerT _padLEDCallbackHandle;
  static auto constexpr stepsPerBeatPadLEDs = 4;
  static auto constexpr ticksPerStepPadLEDs
      = TempoClock::getTicksPerBeat () / stepsPerBeatPadLEDs;
  int _stepsLED = 0;

  std::unique_ptr<TempoEstimatorTest> _tempoEstimatorTest;

  LookAndFeel_A3 _lookAndFeel;

  void createChannelsUI ();
  std::vector<std::unique_ptr<ChannelStrip> > _channelStrips;
  std::vector<std::unique_ptr<ChannelUIState> > _channelUIStates;

  void handleLengthIncrement (index_t channel, int increment);
  int getLengthBeats (index_t channel) const;
  std::vector<int> _lengthsBarLog2;
  static constexpr auto lengthBarMinLog2 = -2;
  static constexpr auto lengthBarMaxLog2 = 4;

  void createMainUI ();
  std::unique_ptr<MotionComponent> _motionComponent;
  std::unique_ptr<StatusBar> _statusBar;
  TempoClock::PointerT _statusBarCallbackHandle;

  using Button = InputOutputAdapter::Button;
  constexpr bool runsOnHardware ();
  void createHardwareInterface ();
  void blankLEDs ();
  void handlePadPress (index_t channel, index_t pad);
  bool isButtonPressed (Button button);
  std::unique_ptr<InputOutputAdapter> _ioAdapter;

  void initializePatterns ();
  std::vector<std::vector<std::shared_ptr<Pattern> > > _patterns;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (A3MotionUIComponent)
};

}
