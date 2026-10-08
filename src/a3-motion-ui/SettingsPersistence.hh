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

#include <a3-motion-engine/RecMode.hh>

// For numSpeedButtons: how many speed keys the bar has is the bar's to say,
// and what is on them is this file's.
#include <a3-motion-ui/components/ClipSettingsLayout.hh>
// For the camera's limits, which a stored view is held to.
#include <a3-motion-ui/components/SphereProjection.hh>

#include <array>

namespace a3
{

/** Persisted UI preference — a small JSON
 *  file alongside config.json, read once at startup and rewritten whenever
 *  one of these actually changes, so they survive app restarts. */
struct AppSettings
{
  int clockMode = 0;

  /** What a recording pass writes where the finger is not. Touch is what the
   *  device did before this was a choice, so a settings file that predates it
   *  keeps behaving exactly as it did. */
  RecMode recMode = RecMode::Touch;

  /** What the Shape section's four speed keys carry, as powers of two of a
   *  bar, left to right. Assignable by dragging a key, which is why they are
   *  values rather than a table.
   *
   *  Here *and* in the set, since 2026-09-25: the four belong to how a set is
   *  played, so a set brings its own (Session::speedButtonLog2). This copy is
   *  what the device comes back with, and a set older than that leaves it
   *  alone.
   *
   *  A fresh device starts on four paths (2026-10-08): on a one-bar take they
   *  lap in 4 bars, 2, 1 and 2 beats, all under the speed past which a room
   *  cannot tell which way a sound turns -- see
   *  .claude/notes/auditory-motion-research.md, B1. A settings file that
   *  predates the keys being assignable gets legacySpeedButtonLog2 instead,
   *  so that device behaves exactly as it did before. */
  static constexpr std::array<int, numSpeedButtons> legacySpeedButtonLog2
      = { 0, -3, -4, -6 };
  std::array<int, numSpeedButtons> speedButtonLog2 = { 2, 1, 0, -1 };

  /** Lets Save write over the instrument's own clips -- how the factory clips
   *  are maintained. Off by default and in every file written before it, so no
   *  device starts in it by accident. */
  bool developerMode = false;

  /** The skin the status bar's CLEAN key goes back to -- see
   *  theme/CleanSkin.hh. Empty until the key has been used. */
  juce::String skinBeforeClean;

  /** Where the sphere is looked at from and how close (2026-09-26): the lean
   *  from straight above, the walk round, the zoom. Straight above and
   *  unzoomed in every file written before, which is the view the device
   *  always had. */
  float cameraPitch = 0.f;
  float cameraTurn = 0.f;
  float cameraZoom = 1.f;

  /** Which of its two things each encoder is on, on MOTION and on REC
   *  (2026-09-27), as encoderClicksMask() writes it. None clicked in a file
   *  from before. */
  int encoderClicksMotion = 0;
  int encoderClicksRecord = 0;

  /** The device came down in FPV and comes back up in it. */
  bool fpvView = false;
};

/** Returns defaults if the file doesn't exist or fails to parse as JSON. */
AppSettings loadSettings (juce::File const &file);

void saveSettings (juce::File const &file, AppSettings const &settings);

}
