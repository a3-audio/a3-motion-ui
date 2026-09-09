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

  /** At least `fingertipSize` in both dimensions, centred on `cap`. A flat
   *  cap is a smaller *drawn* target than the old square one, and a smaller
   *  drawn target is not a smaller one to touch -- this is what a
   *  TouchControl should bind to instead of `cap` itself, if a fader is ever
   *  drawn into a cell it shares with other controls. Today no TouchControl
   *  does: MixerComponent gives the fader the whole per-control layout cell,
   *  which is already larger than any cap, so `hitArea` is consumed only
   *  indirectly, through faderHeightForThrow(). */
  juce::Rectangle<int> hitArea;

  juce::Rectangle<int> caption;
};

FaderGeometry faderGeometry (juce::Rectangle<int> bounds,
                             ControlMetrics metrics, float fraction);

/** How much clear travel a "real throw" needs, in multiples of the hit
 *  area's own height. One hit-area height of travel is already enough for
 *  the two ends not to overlap; a second one on top of that is the margin
 *  that makes them read as two places a finger can aim at separately,
 *  rather than two hit areas that merely stop touching at the extremes --
 *  that part is a judgement call, and would still be defensible at `1`.
 *
 *  The number that is not a judgement call: at `1` it measurably breaks
 *  `MixerLayout.cc`'s own tests, in a file this one does not otherwise
 *  touch or know about (measured: a volume row of 57px against 99px for
 *  the rows around it, in `MixerLayout.TheVolumeRowIsTallerThanTheRowsAroundIt`).
 *  Public rather than file-local so `BarFader.cc`'s own test suite can pin
 *  the property this guards (see
 *  `BarFader.FaderHeightForThrowGivesAThrowThatClearsItsMargin`) instead of
 *  the coupling living only in another file's test output. Anyone changing
 *  this constant has to re-run `ctest -R MixerLayout` alongside this file's
 *  own suite. */
constexpr float minTravelInHitAreaHeights = 2.f;

/** The shortest cell of this width in which the fader actually moves.
 *
 *  A throw needs length. The track is a share of the *width*, and the cap is
 *  flat — a small share of the track's own height — so a cell that is wide
 *  and short comes out as a cap barely nudging in a track it fills almost
 *  entirely, still a lit block that answers a drag by standing still. A
 *  layout that means to draw a fader has to ask for the height first rather
 *  than discover afterwards that it has drawn one that cannot be operated.
 *
 *  "Moves" is read strictly: the travel has to clear the cap's *hit area*,
 *  not merely its drawn height, or a cell just tall enough to nudge a flat
 *  cap a few pixels would count as a fader nobody's fingertip could actually
 *  land on and follow.
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
