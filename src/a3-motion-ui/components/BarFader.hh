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

#include <juce_gui_basics/juce_gui_basics.h>

#include <a3-motion-ui/components/ClipSettingsLayout.hh>

namespace a3
{

/** Where a fader's parts are. Public so it can be tested and so a hit area
 *  can be put on the same rectangle the picture is drawn in — the lesson
 *  ClipSettingsLayout stands on: paint() draws into the layout and resized()
 *  puts the TouchControls on it, so the picture and the target cannot
 *  disagree. */
struct FaderGeometry
{
  juce::Rectangle<int> track;
  juce::Rectangle<int> cap;
  juce::Rectangle<int> caption;
};

FaderGeometry faderGeometry (juce::Rectangle<int> bounds,
                             ControlMetrics metrics, float fraction);

/** The shortest cell of this width in which the fader actually moves.
 *
 *  A throw needs length. The track is a share of the *width*, and the cap is
 *  square on the track, so a cell that is wide and short comes out as a cap
 *  filling its own track with a pixel of travel under it — a lit block that
 *  answers a drag by standing still. A layout that means to draw a fader has
 *  to ask for the height first rather than discover afterwards that it has
 *  drawn one that cannot be operated.
 *
 *  "Moves" is read strictly: the travel has to be at least as long as the cap
 *  is tall, so the throw is a stretch a finger can aim along rather than a
 *  couple of pixels that technically differ.
 *
 *  Answered by walking faderGeometry() rather than by solving its constants
 *  a second time — the caption's height is a share under a floor, so the
 *  closed form has two branches and a crossover, and two answers to one
 *  question are one edit apart from disagreeing.
 *
 *  @returns 0 when no height up to `maxHeight` will do, which is the caller's
 *           cue to say its area is too small rather than to draw a pot. */
int faderHeightForThrow (int width, int maxHeight, ControlMetrics metrics);

/** The bar's fader: a track, a cap, a caption under it.
 *
 *  A free function beside paintBarKnob and for the same reason — the mixer
 *  overlay and the bar's MIX tab draw the same fader, and two of them would
 *  eventually be two slightly different faders.
 *
 *  A vertical throw rather than a knob, because that is the one gesture every
 *  hand in a booth already has, and because the volume would otherwise be the
 *  only control here that looks like everything else while being the thing
 *  reached for most.
 *
 *  @param fraction 0..1 up the track.
 */
void paintBarFader (juce::Graphics &g, juce::Rectangle<int> bounds,
                    ControlMetrics metrics, juce::Colour channelColour,
                    juce::String const &label, float fraction, bool isActive,
                    bool isSelected);

}
