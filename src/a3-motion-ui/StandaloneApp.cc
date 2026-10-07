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
#include "AppPaths.hh"

#include <a3-motion-engine/OscTruth.hh>
#include <a3-motion-engine/TruthKeeper.hh>
#include <a3-motion-engine/UserConfig.hh>

#include <iostream>

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

  // Before anything else is set up, because a crash is a poor way to say
  // "there is no display yet". See issue #15.
  //
  // JUCE does not refuse on its own: LinuxComponentPeer's constructor returns
  // early when X cannot be reached (juce_Windowing_linux.cpp:51), leaving a
  // peer with no window and a null repainter behind it, and the next thing
  // that touches it segfaults. The unit is then `failed` with nothing in the
  // journal saying why, and the rig runs without a surface.
  //
  // Displays::findDisplays() leaves the list empty when the X display could
  // not be opened (juce_Windowing_linux.cpp:699), so an empty list is the
  // public way to ask what the internal isX11Available() answers.
  //
  // Note for anyone testing this: unsetting DISPLAY does *not* produce the
  // condition. JUCE falls back to ":0.0" when DISPLAY is empty
  // (juce_XWindowSystem_linux.cpp:3377), so on a machine with a session
  // running it simply connects to it. Point DISPLAY at a server that is not
  // there instead -- see smoke-test/scripts/no-display-exits-cleanly.sh.
  //
  // Asking here is also ahead of the OSC senders and receivers: on the start
  // that was reported they were already bound, which is why the log ends on a
  // connection line and reads as if the network were at fault.
  if (juce::Desktop::getInstance ().getDisplays ().displays.isEmpty ())
    {
      auto const refusal
          = juce::String ("No display could be reached. Is X running, and is "
                          "DISPLAY set? Refusing to start rather than putting "
                          "a window up that cannot exist.");

      // To both, and for different readers. writeToLog() goes to the log file
      // in ~/.local/state/a3-motion, which is where the app's own history lives;
      // stderr is what systemd puts in the journal, which is where somebody
      // looks when the unit is `failed`.
      juce::Logger::writeToLog (refusal);
      std::cerr << refusal << std::endl;

      setApplicationReturnValue (1);
      quit ();
      return;
    }

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

  // The truth first, then the window, once (a3-system#74): a window opened on
  // the cached truth used to quit seconds later when Core announced another.
  waitForCoresTruth ();
}

void
StandaloneApp::openWindow ()
{
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
}


void
StandaloneApp::setupFileLogger ()
{
  // In the user's state folder, not beside the executable: the package's
  // binary is in /usr/bin (2026-10-08).
  auto const home = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                        .getFullPathName ();
  _logger = std::make_unique<juce::FileLogger> (
      logFile (home, juce::SystemStats::getEnvironmentVariable ("XDG_STATE_HOME", {})),
      "a3-motion-ui debug log", 0);
  juce::Logger::setCurrentLogger (_logger.get ());
}

void
StandaloneApp::shutdown ()
{
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
StandaloneApp::waitForCoresTruth ()
{
  // With $A3_OSC_TRUTH set, that file wins at every start: following Core
  // would restart Motion into the same file forever.
  auto const overridden
      = juce::SystemStats::getEnvironmentVariable ("A3_OSC_TRUTH", {});
  if (!truthkeeper::followsCore (overridden.toRawUTF8 ()))
    {
      std::cerr << "A3 Motion: A3_OSC_TRUTH is set: not following Core's truth"
                << std::endl;
      openWindow ();
      return;
    }
  auto const home
      = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
  _truthKeeper = std::make_unique<TruthKeeperLink> (
      juce::File (truthkeeper::cachePath (home.getFullPathName ().toStdString ())),
      [] (juce::String const &body) { return unusableOscTruth (body); },
      [this] {
        // As on a missing display: a clean quit with 1, so systemd starts
        // Motion again (Restart=on-failure) and its state is saved.
        setApplicationReturnValue (1);
        quit ();
      });
  if (!_truthKeeper->start ())
    {
      openWindow ();
      return;
    }
  // Decided before anything loads the truth: the window's first
  // installedOscTruth () then reads whatever the wait left in the cache.
  _truthKeeper->waitForCore (oscTruthFileDigest (oscTruthFile ()).toStdString (), [this] {
    openWindow ();
    // The loaded bytes' digest, not a second read of a file StemDeck may
    // rewrite meanwhile: from here on Motion follows what it runs on.
    _truthKeeper->follow (installedOscTruth ().digest ().toStdString ());
  });
}

void
StandaloneApp::systemRequestedQuit ()
{
  juce::Logger::writeToLog ("systemRequestedQuit()");
}

}

// this generates boilerplate code to launch our app class:
START_JUCE_APPLICATION (a3::StandaloneApp)
