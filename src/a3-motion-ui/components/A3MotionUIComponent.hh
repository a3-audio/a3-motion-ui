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

#include <array>
#include <string>
#include <vector>

#include <a3-motion-engine/MotionEngine.hh>
#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/PatternLibrary.hh>

#include <a3-motion-ui/components/LookAndFeel.hh>
#include <a3-motion-ui/io/AsyncOSCSender.hh>
#include <a3-motion-ui/io/InputOutputAdapter.hh>

namespace a3
{
class TempoEstimator;
class TempoEstimatorTest;

class MotionComponent;

class FilterDisplay;
class LoopLengthDisplay;
class PadRowDisplay;
class StatusBar;
class ChannelStrip;
class ChannelUIState;
class Pattern;

class A3MotionUIComponent : public juce::Component,
                            public juce::Value::Listener,
                            public juce::MessageListener,
                            public juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>

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
  
  // OSC Receiver
  void oscMessageReceived (const juce::OSCMessage &message) override;
  void oscBundleReceived (const juce::OSCBundle &bundle) override;

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
  float getLengthBeats (index_t channel) const;
  std::vector<int> _lengthsBarLog2;
  static constexpr auto lengthBarMinLog2 = -2;
  static constexpr auto lengthBarMaxLog2 = 3;

  void createMainUI ();
  std::unique_ptr<MotionComponent> _motionComponent;
  std::unique_ptr<StatusBar> _statusBar;
  TempoClock::PointerT _statusBarCallbackHandle;
  std::unique_ptr<FilterDisplay> _filterDisplay;
  std::unique_ptr<LoopLengthDisplay> _loopLengthDisplay;

  using Button = InputOutputAdapter::Button;
  constexpr bool runsOnHardware ();
  void createHardwareInterface ();
  void blankLEDs ();
  void handlePadPress (index_t channel, index_t pad);
  bool isButtonPressed (Button button);
  std::unique_ptr<InputOutputAdapter> _ioAdapter;

  void initializePatterns ();
  std::vector<std::vector<std::shared_ptr<Pattern> > > _patterns;

  // Pattern library: manages system/ and user/ pattern files
  std::unique_ptr<PatternLibrary> _patternLibrary;

  // Pad row display (trajectory option bar)
  static auto constexpr numPadRows = 4u;
  void createPadRowDisplays ();
  std::vector<std::unique_ptr<PadRowDisplay> > _padRowDisplays;

  // Encoder navigation state
  static auto constexpr loopLengthRowIndex = -1;
  enum class EncoderLevel { RowSelect, OptionEdit };
  std::array<EncoderLevel, 4> _encoderLevel;
  std::array<int, 4> _encoderSelectedRow;
  void handleEncoderIncrement (index_t channel, int increment);
  void handleEncoderPress (index_t channel);
  void updatePadRowLabel (index_t channel, index_t pad);
  void showTrajectoryPreview (index_t channel, index_t pad);
  void clearTrajectoryPreview (index_t channel);
  void setPreviewWithDisplayData (std::shared_ptr<Pattern> const &pattern);
  int trajectoryNameToIndex (std::string const &name) const;
  std::shared_ptr<Pattern> createPatternForIndex (int index, index_t channel);
  void saveRecordedPattern (std::shared_ptr<Pattern> const &pattern,
                            index_t channel, index_t pad);

  // OSC Receiver for beat clock (port 7771)
  juce::OSCReceiver _oscReceiver;
  // OSC Receiver for VU meters (port 7772)
  juce::OSCReceiver _oscReceiverVU;
  
  // Async OSC Sender for beatclock output (non-blocking, dedicated thread)
  AsyncOSCSender _oscSender;
  
  // Direct OSC Sender for time-critical tap messages (bypasses async queue)
  juce::OSCSender _tapSender;
  
  // ClockMode toggle state
  bool _clockMode = false;
  float _internalBPM = 0.f;  // saved INT tempo for restore after EXT mode
  
  // Record button long-press detection
  juce::int64 _recordButtonPressTime = 0;
  bool _recordButtonLongPress = false;
  static constexpr juce::int64 longPressThresholdMs = 300;
  
  // Tap button long-press detection (for trajectory preview)
  bool _tapButtonLongPress = false;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (A3MotionUIComponent)
};

}
