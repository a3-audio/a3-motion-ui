/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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

#include <optional>

#include <a3-motion-engine/flight/FlightTuning.hh>
#include <a3-motion-ui/theme/Theme.hh>

namespace a3
{

/** How a group on the floor looks. Size is weight; colour is neutral for a
 *  group and red only for a dead zone, since hue on this screen already
 *  means a channel and a group is nobody's channel. */
enum class BodyRole
{
  Attract,
  Repel,
};

BodyRole bodyRole (float mass);

/** textPrimary for a group, danger for a dead zone. Never accent: the
 *  accent is close to a channel's colour on more than one skin. */
ThemeColour bodyColour (BodyRole role);

/** How much bigger a weight is drawn than a group: the square root of its
 *  mass over a group's, so a hotspot (three groups) is not three times a
 *  group but reads as heavier. A dead zone is drawn a crowd's size. */
float bodyWeightScale (float mass, FlightTuning const &tuning);

/** bodyRadiusOfBlob * bodyWeightScale(mass) * blob. */
float bodyRadius (float mass, float blobDiameter, FlightTuning const &tuning);

/** The disc swells with drawnPulse, but less than the pull, so a beat reads
 *  as a breath and not as a jump. */
float bodyPulseScale (float pulse);

/** Where a finger finds the body: as drawn, but never less than a
 *  fingertip across. */
float bodyHitRadius (float mass, float blobDiameter, float fingertip,
                     FlightTuning const &tuning);

/** "G1".."G8": the body id counted from one. */
juce::String bodyLabel (int id);

constexpr float bodyRadiusOfBlob = 0.6f;
constexpr float bodyPulseOfGravity = 0.5f;

/** One body to paint, in whatever units the Graphics is in (the sphere's 2D
 *  pass draws with the sphere's radius as 1). */
struct BodyPaint
{
  juce::Point<float> centre;
  float radius = 0.f;       // bodyRadius, before the pulse
  float mass = 1.f;
  juce::String label;
  float pulse = 1.f;        // drawnPulse now
  float ringRadius = 0.f;   // capture radius or dead-zone clearance; 0 = none
  float holdProgress = 0.f; // 0..1 towards removal (FloorGesture)
  float stroke = 1.f;       // a line, in the Graphics' units
  float fontHeight = 1.f;   // the label, in the Graphics' units
  /** Where a group's blob has its top: the label stands on it. Unset, the
   *  label sits in the middle of the mark (a dead zone). */
  std::optional<juce::Point<float> > labelAbove;
  /** The group is hidden (the ball stands in front): its label dims. */
  bool hidden = false;
};

/** The box a label standing on `above` is written in: centred over it, its
 *  foot on it. */
juce::Rectangle<float> bodyLabelBox (juce::Point<float> above, float fontHeight);

/** A group: its label and a faint capture ring; its body is the blob the
 *  sphere shader stands on the floor. A dead zone: a red, hatched disc with
 *  its clearance ring -- the only red and the only hatched thing on the
 *  floor. A held body gets a red ring filling clockwise from the top towards
 *  its removal. */
void paintBody (juce::Graphics &g, BodyPaint const &body);

}
