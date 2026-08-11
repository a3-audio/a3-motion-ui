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

namespace a3
{

/** Brightness of one speaker beam, 0..1, from its VU rms.
 *
 *  Deliberately rms-only: the peak on this rig carries a crest factor around
 *  12, so a peak-driven beam chases transients and never holds still long
 *  enough to be compared against its neighbours. Peaks are the right input for
 *  motion, not for a steady brightness — see
 *  `.claude/notes/speaker-waveform-visualisation.md`.
 *
 *  `curve` is the perceptual exponent and is the only thing that sets how far
 *  apart two speakers read: scaling both by `vuMax` cancels out of the ratio
 *  ((a/m)^c / (b/m)^c == (a/b)^c), so `vuMax` shifts overall brightness while
 *  `curve` alone controls contrast between speakers. */
float speakerLightLevel (float vuRms, float vuMax, float curve);

/** One frame of an asymmetric exponential envelope on a beam's level.
 *
 *  Time constants are in seconds, not per-frame coefficients, so the feel does
 *  not shift with the frame rate. Rising uses `attackSeconds`, falling uses
 *  `decaySeconds`; a decay much slower than the attack is what lets the beams
 *  hold still long enough to be compared against each other. */
float speakerLightEnvelope (float current, float target, float attackSeconds,
                            float decaySeconds, float dt);

/** Half-angle of a beam cone, in degrees, for a given width value.
 *
 *  A cone reaches `acos(1 - width)` off-axis. With the speakers 90 degrees
 *  apart, a half-angle of 45 degrees is the point where neighbouring cones
 *  just touch. */
float beamHalfAngleDegrees (float width);

// ── Beam geometry, in units of the sphere radius ────────────────────────
//
// Taken from `resources/speaker.svg`, which is drawn into a box of
// `speakerSize` (see MotionComponent::drawSpeakers). Its horn is a trapezoid
// whose mouth spans y -16..16 of the 100-unit viewBox, at x = 18. The beam has
// to leave that mouth at exactly that width — anything else looks like it grew
// through the loudspeaker instead of out of it.
constexpr float speakerIconSize = 0.28f;
constexpr float speakerApertureHalfWidth = 16.f / 100.f * speakerIconSize;
constexpr float speakerMouthOffset = 18.f / 100.f * speakerIconSize;

/** Distance from the sphere centre to the horn's mouth. */
constexpr float
speakerMouthRadius (float speakerRadius)
{
  return speakerRadius - speakerMouthOffset;
}

/** Cone width for an angle measured off the beam's axis — the inverse of
 *  beamHalfAngleDegrees(). */
float coneWidthFromAngle (float angleDegrees);

/** Angle off the axis a beam opens to at a given level, between the angle it
 *  holds when silent and the one it reaches at full level. */
float beamAngleAtLevel (float level, float quietAngleDegrees,
                        float loudAngleDegrees);

/** Spread of a beam per unit of travel, for a given cone width. */
float beamSpreadTangent (float width);

/** Brightness across the beam at `offset` from its axis.
 *
 *  Flat across the beam and soft only at its edge, so the mouth is lit across
 *  its full width. A profile that peaks on the axis instead makes the beam
 *  look like it starts as a point no matter how wide it actually is.
 *  `edgeSoftness` is the fraction of the half-width that stays flat. */
float beamProfile (float offset, float halfWidth, float edgeSoftness);

/** Half-width of the beam at `axialDistance` beyond the mouth.
 *
 *  Zero behind the mouth: the beam starts at the horn, not at the speaker's
 *  centre point. */
float beamHalfWidthAt (float axialDistance, float apertureHalfWidth,
                       float spreadTangent);

/** Distance a ray from the mouth has travelled inside the sphere by the time
 *  it reaches a point, in beam-local coordinates: `axialDistance` along the
 *  beam axis, `perpendicularOffset` across it, with the mouth at the origin
 *  and the sphere centred `mouthRadius` down the axis.
 *
 *  Zero for rays that pass beside the sphere. The sphere is translucent, so
 *  this feeds absorption rather than an occlusion test. */
float beamPathInsideSphere (float axialDistance, float perpendicularOffset,
                            float mouthRadius);

/** Share of a beam surviving after travelling `pathLength` through the
 *  sphere. This is what keeps the far side from lighting up as brightly as the
 *  near side. */
float beamAbsorption (float pathLength, float coefficient);

/** Half the chord a view ray traverses through the sphere at a given distance
 *  from the centre, 1 at the centre and 0 at the rim.
 *
 *  Weight for the volume lighting: the speakers all sit in the z=0 plane, so a
 *  view ray along z crosses the beams' densest plane at its own position and
 *  one sample per speaker suffices — no raymarching. */
float sphereHalfChord (float distanceFromCentre);

}
