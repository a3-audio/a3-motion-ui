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

#include <optional>

namespace a3
{

/** The CLEAN key on the status bar: one tap to a skin that shows the
 *  trajectories, the blobs and a faint glow in the speakers and nothing else,
 *  one tap back to whatever was running before.
 *
 *  A skin rather than a switch over the skins, because a skin can already
 *  turn every one of those effects down to nothing -- "ein skin sollte all
 *  diese werte eh schon schalten können". What the key adds is only the way
 *  back: which skin to return to is the one thing the menu would make you
 *  remember. */
inline constexpr char const *cleanSkinName = "clean";

/** Where a tap on the key goes: the skin to apply, and the skin to come back
 *  to on the next tap. */
struct CleanSkinToggle
{
  juce::String apply;
  juce::String remember;
};

/** What a tap does, given the running skin, the one remembered from the last
 *  tap and the skins on the device. Nothing when there is no clean skin to go
 *  to -- the key is greyed out then, and a tap that did something anyway would
 *  be a lie.
 *
 *  Back goes to the remembered skin, or to `default` when that one has gone
 *  (deleted or renamed in the editor since), or to the first skin there is. */
std::optional<CleanSkinToggle>
toggleCleanSkin (juce::String const &active, juce::String const &remembered,
                 juce::StringArray const &available);

}
