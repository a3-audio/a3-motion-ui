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

#include <a3-motion-engine/util/Types.hh>

#include <JuceHeader.h>

#include <vector>

namespace a3
{

/** One direction of the IEM EnergyVisualizer's grid.
 *
 *  The plugin sends `/EnergyVisualizer/RMS` with one float per grid point, in
 *  the order of its own coordinate file — so the order here is the plugin's,
 *  not ours. */
struct EnergyDirection
{
  float azimuthDegrees = 0.f;
  float elevationDegrees = 0.f;
};

std::vector<EnergyDirection> loadEnergyGrid (juce::File const &jsonFile);

// Equirectangular map the grid is folded into, small enough to rebuild every
// update and to upload as a texture without stalling the render thread.
constexpr int energyMapWidth = 64;
constexpr int energyMapHeight = 32;
constexpr int energyMapTexelCount = energyMapWidth * energyMapHeight;

// 426 points over a sphere sit roughly 10 degrees apart, so this is about one
// grid spacing: enough to close the gaps between points without washing
// neighbouring directions into each other.
constexpr float energyMapSpreadDegrees = 12.f;

/** Direction a point of the sphere display stands for.
 *
 *  The display is an orthographic view of the upper hemisphere from above:
 *  the centre of the disc is straight up, the rim is the horizon. That is what
 *  the shader's own sphere normal already describes, and what puts the blobs
 *  on the rim rather than half way out.
 *
 *  `x`/`y` are in sphere-normalised screen coordinates with y pointing up.
 *  Energy from below the horizon has nowhere to go in this projection and is
 *  not shown.
 *
 *  Azimuth is the one the blobs are sent out with — Position::azimuth() as it
 *  reaches /StereoEncoder/azimuth — so the energy lands where the blob for
 *  that direction sits: 0 at the top of the disc, running anticlockwise. The
 *  speaker icons are not a reference for this; drawSpeakers() places them by
 *  raw angle, without the HOA-to-JUCE conversion the blobs go through. */
EnergyDirection energyDirectionForScreen (float x, float y);

/** Radius at which a given filament of a net stands at a given time.
 *
 *  `flow` is signed. Positive runs the filaments inwards — the direction sound
 *  arrives from — which is what the net inside the sphere does. Negative runs
 *  them outwards, which is what the glow outside it does. The shader picks the
 *  filament sitting at a pixel by inverting this. */
float netFilamentRadius (float filamentCoordinate, float time, float flow);

// How far out the screen corner sits, in sphere radii. The component is
// 768 x ~734 px with the sphere at reduceFactorCircle 0.64, so a radius of
// 734 * 0.64 / 2 = 235 px against a corner distance of hypot(384, 367) = 531.
constexpr float screenCornerDistance = 2.26f;

/** How much of the glow's net has emerged from behind the sphere at a given
 *  distance from the centre.
 *
 *  Zero on and inside the rim, rising to one over `rise` beyond it, so the
 *  filaments look like they come out from behind the sphere rather than
 *  sprouting from its edge. */
float glowEmergence (float distanceFromCentre, float rise);

/** The bearing a place on the display stands for, as a unit vector.
 *
 *  **The room's bearing, not the screen's**, and that distinction is the whole
 *  of it. The net used to normalise the screen coordinate, which nails the
 *  weave to the display: turn the camera and the ball rotates under a pattern
 *  that stays where it was. Exactly the fault the graticule had before it was
 *  rebuilt in the room's terms -- a net on the screen draws the same picture
 *  whichever way the room is looked at.
 *
 *  `direction` is what the pixel stands for in the room, which is what
 *  SphereProjection's asSeenFromInverse() gives back. At the identity camera
 *  this returns the screen bearing exactly: the direction has its axes
 *  shuffled to { y, -x, up } on the way in, and this shuffles them back.
 *  Mirrors netBearing() in SphereShader.cc. */
struct NetBearing
{
  float x = 0.f;
  float y = 0.f;
};
NetBearing netBearingForDirection (Pos const &direction);

/** Point in the noise domain a place on the display maps to.
 *
 *  Built from the direction vector rather than from an azimuth angle: an angle
 *  wraps, and the wrap put a visible seam due west where the filaments failed
 *  to meet. `twist` sets how much detail runs around the circle, `scale` how
 *  much runs along the radius.
 *
 *  `x`/`y` are the **room's** bearing -- see netBearingForDirection(). They are
 *  normalised here, so handing in an unnormalised one is fine. */
struct NetDomainPoint
{
  float x, y, z;
};

NetDomainPoint netDomainPoint (float x, float y, float radial, float twist,
                               float scale);

/** Arc of the sphere's rim a beam covers, in degrees.
 *
 *  With four speakers, 90 degrees each is what closes the circle and makes the
 *  net look like it leaves the loudspeakers rather than appearing at the rim.
 *  `angleDegrees` is the beam's angle off its own axis. */
float beamRimCoverageDegrees (float angleDegrees, float speakerRadius);

/** Half the angle a beam's band spans about the sphere centre, in degrees, at
 *  a given distance from it.
 *
 *  The beams are bands in the annulus between the horn's mouth and the sphere
 *  rather than cones aimed at its centre: narrow where they leave the speaker,
 *  opening to `wrapAngle` by the time they arrive. At 45 degrees the four of
 *  them close the circle and the sphere ends up enclosed. Held at both ends —
 *  there is no band behind the mouth and none inside the sphere. */
float beamWrapHalfAngle (float distanceFromCentre, float mouthRadius,
                         float apertureAngleDegrees, float wrapAngleDegrees);

/** The angle between what a pixel stands for and where a speaker stands,
 *  measured in the room.
 *
 *  This is the beam's *across* coordinate, and the whole of what makes the
 *  bands three-dimensional. They used to take it as a difference of screen
 *  azimuths about the centre of the display, which nails the band to the
 *  glass: the annulus it was built on has no word for a speaker that is not on
 *  the rim, and under a lean every speaker comes in off it. Both directions
 *  here have already been through the camera, so a lean moves the speaker and
 *  the band goes with it.
 *
 *  Zero on the speaker's own bearing, pi at the far side of the room. Mirrors
 *  beamSpread() in SphereShader.cc. */
float beamSpreadAngle (Pos const &pixelDirection, Pos const &speakerDirection);

/** Where a speaker's mouth lands on the screen, as a distance from the
 *  sphere's centre.
 *
 *  The mouth radius used to be one number for all four — speakerRadius less
 *  the horn's offset — which is true only while every cabinet sits on the rim.
 *  Leaned over, each one lands somewhere else, and a band that still started
 *  at the shared radius left its own speaker behind.
 *
 *  `speakerSeen` is the cabinet's centre as the eye sees it, already scaled by
 *  the speaker radius. Held off the sphere: a cabinet crossing the silhouette
 *  would otherwise ask for an annulus with no room in it. */
float beamMouthRadiusSeen (Pos const &speakerSeen);

/** Whether there is any sound in the room at all, 0 to 1.
 *
 *  The bands are a thing the sound does. Once the level drove thickness rather
 *  than brightness, nothing was left to say "silence": a bolt at level zero is
 *  still drawn, merely thin, and a hairline at full brightness is exactly as
 *  visible as a thick one. So four speakers went on throwing bolts around a
 *  room with nothing playing in it.
 *
 *  Taken from the *loudest* of the four rather than from each speaker's own
 *  level, which is the same reasoning beamBandLevel() already carries: a quiet
 *  speaker beside a loud one is part of a picture and keeps its hairline;
 *  silence everywhere is not a picture. `gate` is where the room counts as
 *  silent — an anlage with a noise floor needs it above zero.
 *
 *  Mirrors beamAlive() in SphereShader.cc. */
float beamAliveness (float loudestLevel, float gate);

/** A speaker's share of the loudest one, 0 to 1.
 *
 *  What the bands are asked is *where* the sound is, and that is a question
 *  about the four speakers relative to each other. Read absolutely it is a
 *  question about the volume knob instead: measured at the rig with one
 *  channel plainly playing, the loudest speaker sat at 2.4% of the scale
 *  (vuMax 0.2 against an rms of 0.009), so all four bolts sat on the boltThin
 *  floor and were 4% apart. Nothing about the picture said which speaker the
 *  sound was coming out of.
 *
 *  `beamBandLevel()` already works this way — its floor is a share of the
 *  loudest, not an absolute — so this is that reasoning carried through to the
 *  thickness. Whether anything is playing at all stays an absolute question
 *  and belongs to beamAliveness().
 *
 *  Mirrors beamShare() in SphereShader.cc. */
float beamShare (float liftedLevel, float loudestLevel);

/** How many bolts a speaker draws, out of its share of the loudest.
 *
 *  The count is what the eye actually reads. Width was doing all the work and
 *  it cannot: measured at the rig, a share of 0.28 against 1.0 is a bolt core
 *  of 1.3 screen pixels against 2.6, and both of those are hairlines — while
 *  all four speakers drew the same seven bolts, which is the thing you can
 *  count without looking closely.
 *
 *  Never none: a speaker that is merely quiet is still part of the picture,
 *  and one that dropped to nothing would open a hole in the ring. Silence
 *  everywhere is beamAliveness()'s job.
 *
 *  Mirrors the count in beamDensity(). */
float beamBoltCount (float share, float maxCount, float minCount);

/** How brightly a speaker's bolts are drawn, out of its share.
 *
 *  Relative, so it cannot repeat the mistake that started this: brightness
 *  driven by an *absolute* level made a band vanish at the levels it spends
 *  most of its time at. The loudest speaker is always at full, whatever the
 *  room's volume; the others fall back to `floorPart` of that.
 *
 *  Mirrors the brightness term in beamDensity(). */
float beamBrightness (float share, float floorPart);

/** How wide a bolt's core runs at a given level, in degrees.
 *
 *  The level is a *thickness* now, not a brightness. Driving brightness with
 *  it meant a band faded out exactly where it was needed most — quiet is most
 *  of the time, and a bolt you can only make out with some imagination is not
 *  a bolt. A quiet speaker draws the same bolts as a loud one, hairline thin;
 *  a loud one swells them.
 *
 *  `thin` is what a silent speaker's bolt is worth as a share of the full
 *  width. Mirrors boltWidthAt() in SphereShader.cc. */
float boltWidthAtLevel (float baseWidthDegrees, float level, float thin);

/** The seed a speaker's bolts are dealt from.
 *
 *  Taken from where the cabinet stands in the *room*, so a bolt belongs to a
 *  loudspeaker rather than to a place on the glass. Read off the screen it
 *  moved as the eye moved, which dealt every speaker a fresh set of bolts on
 *  every frame of a turn — and, since a screen bearing is not a unit vector
 *  once the speaker radius is in it, reached seeds the old normalised bearing
 *  never did. Four of them escaped the annulus at once and crossed the whole
 *  display. */
float beamBoltSeed (Pos const &speakerRoomDirection);

/** How much of a speaker's band reaches the eye.
 *
 *  A cabinet that has gone round behind the ball takes its band with it — the
 *  one thing a flat annulus could never say, since on the screen a speaker
 *  behind the sphere and one in front of it sit in the same place.
 *
 *  1 in front, falling to 0 for a cabinet both inside the silhouette and past
 *  the sphere's centre along the view axis. */
float beamDepthVisibility (Pos const &speakerSeen, float softness);

/** A band's level held up to a share of the loudest band's, so a silent
 *  speaker thins its band rather than losing it and opening the ring around
 *  the sphere.
 *
 *  Relative rather than absolute: with nothing playing at all there is no
 *  ring, which an absolute floor would have left standing. */
float beamBandLevel (float level, float floor, float loudestLevel);

/** How much of the glow shows through where a band covers it.
 *
 *  Both live in the annulus outside the sphere, so they would otherwise add
 *  up into a wash. The band wins. */
float glowVisibility (float bandDensity, float cover);

/** How much of the band survives at a given distance from the sphere centre.
 *
 *  Full strength in the annulus between the sphere and the horn's mouth, then
 *  bleeding past both ends over `bleed` — outwards into the glow's filaments,
 *  inwards into the net's — so the band joins those two rather than sitting
 *  between them as a separate object. */
float beamRadialWindow (float distanceFromCentre, float mouthRadius,
                        float bleed);

/** Brightness of a bolt at a given distance from its path.
 *
 *  `width / (distance + width)`: full at the core, half a width out, and
 *  trailing off without ever quite reaching zero. That tail is what ties the
 *  band into the net inside and the glow outside — a smoothstep edge stops
 *  dead and leaves a line, which is what a field of ridged noise gives you
 *  however hard you sharpen it. */
float boltFalloff (float distance, float width);

/** Whether a bolt is striking, from its own noise and a duty cycle.
 *
 *  Zero below `duty`, so a bolt is absent most of the time and the band reads
 *  as strikes rather than as a texture that happens to flicker. */
float boltStrike (float noise, float duty);

/** How far a branch has left its trunk at a given distance from the sphere.
 *
 *  Zero until the fork, growing after it. A branch that diverges from the
 *  start is just a second bolt running alongside; one that shares its trunk
 *  first is what reads as lightning. */
float boltBranchOffset (float distanceFromCentre, float forkRadius,
                        float spread);

/** Index of the texel a direction falls in. */
int energyMapTexel (float azimuthDegrees, float elevationDegrees);

/** Folds the grid's 426 values into the equirectangular map.
 *
 *  The weights are a spherical Gaussian, resolved once at construction — a
 *  nearest-point lookup instead would show the grid as facets. `spreadDegrees`
 *  is how far a single point's energy reaches; too small leaves holes between
 *  grid points, too large smears the whole sphere into one colour. */
class EnergyMapProjection
{
public:
  EnergyMapProjection (std::vector<EnergyDirection> const &grid,
                       float spreadDegrees);

  /** `gridValues` holds one value per grid point, `map` receives
   *  energyMapTexelCount values. Both are caller-owned. */
  void project (float const *gridValues, float *map) const;

private:
  struct Contribution
  {
    int gridIndex;
    float weight;
  };

  // One run of contributions per texel, indexed by _offsets[texel] ..
  // _offsets[texel + 1].
  std::vector<Contribution> _contributions;
  std::vector<int> _offsets;
};

}
