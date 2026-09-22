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

#ifdef A3_AUDIO_ENGINE_ENABLED
#include <a3-audio-engine/ChunkedRender.hh>
#include <a3-audio-engine/OutputOrder.hh>
#include <a3-audio-engine/SpeakerTest.hh>
#endif

namespace a3
{

class A3MotionAudioProcessor : public juce::AudioProcessor
{
public:
  A3MotionAudioProcessor ();
  ~A3MotionAudioProcessor ();

  void prepareToPlay (double sampleRate, int samplesPerBlock) override;
  void releaseResources () override;

  bool isBusesLayoutSupported (const BusesLayout &layouts) const override;

  void processBlock (juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

  juce::AudioProcessorEditor *createEditor () override;
  bool hasEditor () const override;

  const juce::String getName () const override;

  bool acceptsMidi () const override;
  bool producesMidi () const override;
  bool isMidiEffect () const override;

  double getTailLengthSeconds () const override;

  int getNumPrograms () override;
  int getCurrentProgram () override;
  void setCurrentProgram (int index) override;
  const juce::String getProgramName (int index) override;
  void changeProgramName (int index, const juce::String &newName) override;

  void getStateInformation (juce::MemoryBlock &destData) override;
  void setStateInformation (const void *data, int sizeInBytes) override;

#ifdef A3_AUDIO_ENGINE_ENABLED
  // Public so the host in StandaloneApp asks the device for exactly these.
  static constexpr int numInputs = 4;
  static constexpr int numOutputs = 12;  // up to 7.1.4
#endif

private:
  // A member function, not a free function: BusesProperties is protected on
  // juce::AudioProcessor, and only a member (or a derived class's member) may
  // name it.
  static BusesProperties busesForThisBuild ();

  juce::String const _namePlugin;

  std::unique_ptr<juce::FileLogger> _fileLogger;

#ifdef A3_AUDIO_ENGINE_ENABLED
  void renderAudio (juce::AudioBuffer<float> &buffer);

  juce::AudioBuffer<float> _layoutBuffer;
  OutputOrder _outputOrder{ numOutputs };
  // Only while A3_SPEAKER_TEST is set: until the output list exists (plan 2)
  // this is the one way to start it.
  std::unique_ptr<SpeakerTest> _speakerTest;
#endif

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (A3MotionAudioProcessor)
};

}
