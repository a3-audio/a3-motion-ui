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

#include "A3MotionAudioProcessor.hh"
#include "A3MotionEditor.hh"

namespace
{
}

namespace a3
{

A3MotionAudioProcessor::A3MotionAudioProcessor ()
    : AudioProcessor (
        BusesProperties ().withInput ("Input", AudioChannelSet::stereo ())),
      namePlugin ("A3 Motion UI")
{
  auto useFileLogger = false;
  if (useFileLogger)
    {
      auto const filenameLog = String ("a3-motion-ui.log");
      auto fileExecutable = File::getSpecialLocation (
          File::SpecialLocationType::currentExecutableFile);
      fileLogger = std::make_unique<FileLogger> (
          fileExecutable.getParentDirectory ().getChildFile (filenameLog),
          "A3 Motion UI Log", 0);
      Logger::setCurrentLogger (fileLogger.get ());
    }
}

A3MotionAudioProcessor::~A3MotionAudioProcessor ()
{
  // Logger::writeToLog ("~A3MotionAudioProcessor");
  Logger::setCurrentLogger (nullptr);
}

const String
A3MotionAudioProcessor::getName () const
{
  // Logger::writeToLog("getName : " + namePlugin);
  return namePlugin;
}

bool
A3MotionAudioProcessor::acceptsMidi () const
{
  // Logger::writeToLog("acceptsMidi");
  return true;
}

bool
A3MotionAudioProcessor::producesMidi () const
{
  // Logger::writeToLog("producesMidi");
  return true;
}

bool
A3MotionAudioProcessor::isMidiEffect () const
{
  //    Logger::writeToLog("isMidiEffect");
  return true;
}

double
A3MotionAudioProcessor::getTailLengthSeconds () const
{
  //    Logger::writeToLog("getTailLengthSeconds");
  return 0.0;
}

int
A3MotionAudioProcessor::getNumPrograms ()
{
  // Logger::writeToLog("getNumPrograms");
  return 1; // NB: some hosts don't cope very well if you tell
            // them there are 0 programs, so this should be at
            // least 1, even if you're not really implementing
            // programs.
}

int
A3MotionAudioProcessor::getCurrentProgram ()
{
  // Logger::writeToLog("getCurrentProgram");
  return 0;
}

void
A3MotionAudioProcessor::setCurrentProgram (int index)
{
  // Logger::writeToLog("setCurrentProgram");
}

const String
A3MotionAudioProcessor::getProgramName (int index)
{
  // Logger::writeToLog("getProgramName");
  return {};
}

void
A3MotionAudioProcessor::changeProgramName (int index, const String &newName)
{
  // Logger::writeToLog("changeProgramName");
}

void
A3MotionAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
  // Logger::writeToLog("prepareToPlay");
}

void
A3MotionAudioProcessor::releaseResources ()
{
  // Logger::writeToLog("releaseResources");
}

bool
A3MotionAudioProcessor::isBusesLayoutSupported (
    const BusesLayout &layouts) const
{
  // Logger::writeToLog("isBusesLayoutSupported");
  return true;
}

void
A3MotionAudioProcessor::processBlock (AudioBuffer<float> &buffer,
                                      MidiBuffer &midiMessages)
{
  auto mainInputOutput = getBusBuffer (buffer, true, 0);

  // add a hopefully inaudible float epsilon here to circumvent VST3
  // plugin auto-suspend (tested in Bitwig). This is ugly, let's
  // hope we can switch to CLAP soon or find a saner solution by
  // flagging the host that we don't want to get suspended via the
  // JUCE API.
  // for (auto j = 0; j < buffer.getNumSamples(); ++j)
  //     for (auto i = 0; i < mainInputOutput.getNumChannels(); ++i)
  //         *mainInputOutput.getWritePointer (i, j) =
  //             *mainInputOutput.getReadPointer (i, j) +
  //             std::numeric_limits<float>::epsilon();
}

bool
A3MotionAudioProcessor::hasEditor () const
{
  // Logger::writeToLog("hasEditor");
  return true;
}

AudioProcessorEditor *
A3MotionAudioProcessor::createEditor ()
{
  // Logger::writeToLog("createEditor");
  return new A3MotionEditor (*this);
}

void
A3MotionAudioProcessor::getStateInformation (MemoryBlock &destData)
{
  // Logger::writeToLog("getStateInformation");
}

void
A3MotionAudioProcessor::setStateInformation (const void *data, int sizeInBytes)
{
  // Logger::writeToLog("setStateInformation");
}

} // namespace a3 end

// This creates new instances of the plugin..
AudioProcessor *JUCE_CALLTYPE
createPluginFilter ()
{
  // Logger::writeToLog("createPluginFilter");
  return new a3::A3MotionAudioProcessor ();
}
