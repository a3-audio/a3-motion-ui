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

#include "StandaloneApp.hh"

#include <a3-motion-engine/UserConfig.hh>

#ifdef A3_AUDIO_ENGINE_ENABLED
#include "A3MotionAudioProcessor.hh"

#include <cstdlib>
#endif

namespace a3
{

StandaloneApp::~StandaloneApp () {}

void
StandaloneApp::initialise (juce::String const &commandLine)
{
  juce::ignoreUnused (commandLine);

  setupFileLogger ();

  auto appNameVer = getApplicationName () + " " + getApplicationVersion ();
  juce::Logger::writeToLog (appNameVer);

  if (juce::JSON::parse (juce::File::getCurrentWorkingDirectory ()
                             .getChildFile ("config/config.json")
                             .loadFileAsString (),
                         userConfig)
          .failed ())
    {
      throw std::runtime_error ("could not parse config/config.json");
    }

  // splash = std::make_unique<SplashScreen> (
  //     appNameVer,
  //     ImageFileFormat::loadFrom (File ("./resources/a3_logo-dark.png")),
  //     true /* useDropShadow */);

  // auto waitDurationMs = 2000;
  // Time::waitForMillisecondCounter (Time::getMillisecondCounter ()
  //                                  + waitDurationMs);
  // splash = nullptr;
  // splash->deleteAfterDelay (RelativeTime::seconds (3),
  //                           true /* removeOnMouseClick */);

  _mainWindow = std::make_unique<MainWindow> (getApplicationName ());

  // The panel the device runs on, in the orientation it hangs in: 768x1024,
  // portrait, because the screen is mounted rotated (i3 sets `--rotate right`
  // on HDMI-2; see also /etc/X11/xorg.conf.d/99-ilitek-rotation.conf, which
  // turns the touches with it).
  //
  // It asked for 450x768 before -- neither the panel's size nor its shape.
  // On the rig that never showed, because i3 tiles the window to the whole
  // workspace a moment later and JUCE lays out again at the size it is given.
  // It shows when nobody tiles it: on a bare X, on a desk, and at a cold boot
  // in the seconds before i3 has the window. Then this number *is* the window,
  // and a window 450 wide on a 768-wide panel is a layout squeezed into a bit
  // over half the room it has.
  _mainWindow->setBounds (0, 0, 768, 1024);
  _mainWindow->setVisible (true);

#ifdef A3_AUDIO_ENGINE_ENABLED
  // After the window, not before: the config parse above and the UI's own
  // construction can throw, and neither should leave an open audio device
  // behind. The processor does not depend on the window, so nothing needs
  // it earlier.
  startAudio ();
#endif
}

#ifdef A3_AUDIO_ENGINE_ENABLED
namespace
{
juce::String
environmentValue (char const *name)
{
  auto const *value = std::getenv (name);
  return value != nullptr ? juce::String (value) : juce::String ();
}
}

void
StandaloneApp::startAudio ()
{
  // The processor exists only to render audio. The editor is never created
  // from it: the performer sees our own MainWindow, which is built above and
  // knows nothing of this processor.
  _processor.reset (createPluginFilter ());
  _player.setProcessor (_processor.get ());
  openAudioDevice ();
  _deviceManager.addAudioCallback (&_player);
}

void
StandaloneApp::openAudioDevice ()
{
  // Chosen by environment until plan 2 brings a selector in the UI.
  auto const requestedType = environmentValue ("A3_AUDIO_DEVICE_TYPE");
  auto const requestedOutput = environmentValue ("A3_AUDIO_OUTPUT_DEVICE");

  if (requestedType.isNotEmpty ())
    {
      // setCurrentAudioDeviceType only knows the types a scan has found.
      _deviceManager.getAvailableDeviceTypes ();
      _deviceManager.setCurrentAudioDeviceType (requestedType, true);
    }

  juce::AudioDeviceManager::AudioDeviceSetup setup;
  setup.outputDeviceName = requestedOutput;

  auto const error = _deviceManager.initialise (
      A3MotionAudioProcessor::numInputs, A3MotionAudioProcessor::numOutputs,
      nullptr, true, {}, requestedOutput.isNotEmpty () ? &setup : nullptr);

  // initialise() moves on to another type when the requested one has no
  // devices (JACK without a running server falls back to ALSA). Noise on an
  // output nobody asked for is worse than none, so that is refused.
  auto const openedType = _deviceManager.getCurrentAudioDeviceType ();
  if (requestedType.isNotEmpty () && openedType != requestedType)
    {
      juce::StringArray available;
      for (auto *type : _deviceManager.getAvailableDeviceTypes ())
        available.add (type->getTypeName ());

      juce::Logger::writeToLog ("audio: device type \"" + requestedType
                                + "\" not available (have: "
                                + available.joinIntoString (", ")
                                + "), no audio device opened");
      _deviceManager.closeAudioDevice ();
      return;
    }

  if (error.isNotEmpty ())
    juce::Logger::writeToLog ("audio: opening the device failed: " + error);

  auto *device = _deviceManager.getCurrentAudioDevice ();
  if (device == nullptr)
    {
      juce::Logger::writeToLog ("audio: no audio device open");
      return;
    }

  juce::Logger::writeToLog (
      "audio: opened " + openedType + " device \"" + device->getName ()
      + "\", " + juce::String (device->getActiveInputChannels ().countNumberOfSetBits ())
      + " in / "
      + juce::String (device->getActiveOutputChannels ().countNumberOfSetBits ())
      + " out, " + juce::String (device->getCurrentSampleRate ()) + " Hz, "
      + juce::String (device->getCurrentBufferSizeSamples ()) + " samples");
}

void
StandaloneApp::stopAudio ()
{
  // Strictly the reverse of startAudio().
  _deviceManager.removeAudioCallback (&_player);
  _player.setProcessor (nullptr);
  _deviceManager.closeAudioDevice ();
  _processor = nullptr;
}
#endif

void
StandaloneApp::setupFileLogger ()
{
  auto fileExecutable = juce::File::getSpecialLocation (
      juce::File::SpecialLocationType::currentExecutableFile);
  auto filenameLog = fileExecutable.getFileNameWithoutExtension () + ".log";

  _logger = std::make_unique<juce::FileLogger> (
      fileExecutable.getParentDirectory ().getChildFile (filenameLog),
      "a3-motion-ui debug log", 0);
  juce::Logger::setCurrentLogger (_logger.get ());
}

void
StandaloneApp::shutdown ()
{
#ifdef A3_AUDIO_ENGINE_ENABLED
  stopAudio ();
#endif

  userConfig = juce::var{};

  // explicit deletion of MainWindow to capture tear-down messages
  // with our logger
  _mainWindow = nullptr;
  juce::Logger::setCurrentLogger (nullptr);
}

juce::String const
StandaloneApp::getApplicationName ()
{
  return "A3 Motion UI";
}

juce::String const
StandaloneApp::getApplicationVersion ()
{
  return "0.0.0";
}

void
StandaloneApp::systemRequestedQuit ()
{
  juce::Logger::writeToLog ("systemRequestedQuit()");
}

}

// this generates boilerplate code to launch our app class:
START_JUCE_APPLICATION (a3::StandaloneApp)
