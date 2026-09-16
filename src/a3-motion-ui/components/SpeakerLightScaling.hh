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

#include <cmath>

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
// ── The stack, in metres ────────────────────────────────────────────────
//
// Each position is a tower: three Res 2 side by side as a cluster, over three
// F218 subs, about three metres tall. Read off the reference render the
// maintainer supplied (`fone08-*.jpg`), not from a datasheet — correct the
// numbers here if the real ones differ, everything downstream is derived.
//
// Two things that render settled, both of which the first attempt had wrong:
// a Res 2 is **portrait**, half as wide as it is tall, and the three of them
// stand **beside** each other rather than stacked. The subs are the wide part
// and carry the cluster; the tower narrows as it goes up.
// Wide enough that three of them cover the subs completely: the cluster is
// the lid of the tower, not a box sitting on top of one.
constexpr float subWidthM_ = 1.65f;
constexpr float resWidthM = subWidthM_ / 3.f;
constexpr float resHeightM = 1.00f;
constexpr float resDepthM = 0.62f;
constexpr int resPerStack = 3;      // side by side

constexpr float subWidthM = subWidthM_;
constexpr float subHeightM = 0.66f;
constexpr float subDepthM = 0.90f;
constexpr int subPerStack = 4;      // one on top of the next

/** How many sphere radii a metre is.
 *
 *  The one place the picture is tied to the room. Everything physical — the
 *  towers, the floor, the listener — is written in metres and scaled through
 *  here, so the drawing keeps one set of proportions instead of several that
 *  happen to look right next to each other. */
constexpr float metrePerSphereRadius = speakerIconSize * 0.42f;

/** Ear height of a standing listener, in metres.
 *
 *  **Zero elevation is ear height.** The sphere is centred on a person, not on
 *  the middle of the room — so the floor is not somewhere below, it is exactly
 *  this far below, and the towers stand on it rather than hanging at whatever
 *  depth looked right.
 *
 *  Before this the floor sat at -sin(drop) * speakerRadius, which works out at
 *  4.9 m under the listener: a cellar, not a dance floor, and it made every
 *  other measurement a number nothing could check. */
constexpr float earHeightM = 1.6f;

/** Where the floor is, in sphere radii — negative, below the listener. */
constexpr float speakerFloorZ = -earHeightM * metrePerSphereRadius;

constexpr float floorReach = 1.35f;

/** How far below the horizon a cabinet stands, in radians. Mirrors `drop` in
 *  SphereShader's uploadSpeakerFrames(). */
constexpr float speakerDropRad = 0.42f;

/** The cluster is as wide as its three cabinets together, and one tall. */
constexpr float clusterWidthM = resPerStack * resWidthM;
constexpr float clusterHeightM = resHeightM;

constexpr float stackHeightM = clusterHeightM + subPerStack * subHeightM;

/** Where the tops meet the subs, as a share of the tower's height. */
constexpr float stackSubShare = (subPerStack * subHeightM) / stackHeightM;

constexpr float speakerMouthOffset = 18.f / 100.f * speakerIconSize;

/** The narrowest annulus a band is ever given, in sphere radii.
 *
 *  A cabinet leaned in over the silhouette has no annulus at all, and the
 *  band's own arithmetic divides by that span. Near enough what a cabinet on
 *  the rim gets, deliberately: a band that thinned to nothing as its speaker
 *  came in would make the clamp the thing that takes bands away, and that job
 *  belongs to beamDepthVisibility() — which knows whether the cabinet went
 *  behind the ball or is standing in front of it in plain sight. */
constexpr float beamMinimumAnnulus = 0.22f;

/** How softly a band goes behind the sphere, in sphere radii.
 *
 *  Wide enough that a cabinet crossing the silhouette arrives rather than
 *  snaps: a band blinking out as its speaker passes the rim reads as a fault
 *  in the drawing rather than as depth. */
constexpr float beamDepthSoftness = 0.35f;

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

/** Whether the speaker icons still fit on screen at a given sphere scale.
 *
 *  They are drawn at `speakerRadius` plus their own half-diagonal, in the same
 *  normalised space the sphere is scaled in, so the two cannot be chosen
 *  independently: past a point the icons run off the shorter edge and are
 *  clipped. */
bool speakerIconsFitOnScreen (float sphereScale, float speakerRadius);

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
