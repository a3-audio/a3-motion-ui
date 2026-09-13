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

#include "SphereShader.hh"

#include <a3-motion-ui/theme/Theme.hh>

#include "SpeakerLightScaling.hh"

#include <cmath>

namespace a3
{

namespace
{
/** A theme role as a GL colour. The skin states 0..255; GLSL wants 0..1. */
void
setThemeUniform (GLint location, ThemeColour const &colour)
{
  if (location < 0)
    return;

  juce::gl::glUniform3f (location, static_cast<float> (colour.r) / 255.f,
               static_cast<float> (colour.g) / 255.f,
               static_cast<float> (colour.b) / 255.f);
}
}

SphereShader::SphereShader () = default;
SphereShader::~SphereShader () = default;

// ─────────────────────────────────────────────────────────────────
// Vertex shader – trivial fullscreen quad
// ─────────────────────────────────────────────────────────────────
juce::String
SphereShader::getVertexShader ()
{
  return R"(
attribute vec2 aPos;
varying vec2 vUV;

void main ()
{
    vUV = aPos * 0.5 + 0.5;
    gl_Position = vec4 (aPos, 0.0, 1.0);
}
)";
}

// ─────────────────────────────────────────────────────────────────
// Fragment shader – raytraced sphere, head SDF, VU lighting
// GLSL 1.20 compatible (GL 2.1 desktop context on RPi 4 V3D)
// ─────────────────────────────────────────────────────────────────
juce::String
SphereShader::getFragmentShader ()
{
  return R"(
varying vec2 vUV;

uniform vec2  uResolution;
uniform float uSphereRadius;
uniform vec2  uSphereCentre;

// Corona of the whole sphere, driven by the subwoofer
uniform float uGlowLevel;
uniform vec3  uGlowColour;
uniform float uGlowIntensity;
uniform float uGlowFlow;        // negative: the glow's filaments run outwards
uniform float uGlowReach;       // how far the domain still varies
uniform float uGlowRise;        // distance over which they emerge past the rim
uniform float uGlowTwist;
uniform float uGlowScale;
uniform float uGlowSharpness;
uniform float uGlowOctaves;
uniform float uGlowLacunarity;
uniform float uGlowGain;

uniform vec3  uBgColour;  // solid background colour
uniform vec3  uSphereSurface;      // the sphere's own dark body
uniform vec3  uSphereRim;          // the fresnel edge it catches
uniform vec3  uSphereEnvironment;  // the faint room it reflects
uniform vec3  uBoltCoreColour;     // the white a bolt runs at its centre

uniform float uSpotLevel0;
uniform float uSpotLevel1;
uniform float uSpotLevel2;
uniform float uSpotLevel3;
uniform vec3  uSpotColour;
uniform float uSpeakerRadius;
uniform float uBeamEdge;       // fraction of the half-width that stays flat
uniform float uBeamIntensity;
uniform float uApertureAngle;  // half-angle of the band where it leaves the horn
uniform float uWrapAngle;      // and where it meets the sphere — 45 closes the circle
uniform float uWander;         // degrees the centre line wanders
uniform float uWanderTwist;    // detail of the wander around the circle
uniform float uWanderScale;    // and along the radius
uniform float uWanderFlow;     // how fast it creeps
uniform float uBeamRoot;       // density where it leaves the speaker
uniform float uBeamFloor;      // level the band never drops below
uniform float uBeamBleed;      // how far it reaches past the annulus
uniform float uBeamFray;       // how ragged its edge is
uniform float uBeamCover;      // how strongly the band hides the glow
uniform float uBoltWidth;      // angular width of a bolt's core, degrees
uniform float uBoltWander;     // how far its path strays across the band
uniform float uBoltScale;      // how quickly it strays with radius
uniform float uBoltFlow;       // how fast the path creeps
uniform float uBoltRate;       // how often a bolt strikes
uniform float uBoltDuty;       // and how much of the time it is dark
uniform float uBoltCoreExp;    // how tight the white core is
uniform float uBoltCore;       // how bright it runs
uniform float uBoltCount;      // bolts per band
uniform float uBoltReach;      // how far an escaping one carries
uniform float uBoltEscape;     // how many of them escape
uniform float uBoltBranches;   // branches per bolt
uniform float uBoltBranch;     // how hard a branch leaves its trunk
uniform float uMouthOffset;    // mouth position ahead of the speaker centre

// Energy arriving from each direction, folded into an equirectangular map by
// EnergyMap.cc from the IEM EnergyVisualizer's 426 points
uniform sampler2D uEnergyMap;
uniform vec3  uEnergyColour;
uniform float uEnergyIntensity;
// Where the room is being looked at from: how far the eye has come down from
// straight above, and how far round it has walked. Both zero is the overhead
// view the device has always had, and then this costs nothing.
uniform vec2  uCamera;
uniform float uNetIntensity;
uniform float uNetScale;
uniform float uNetSharpness;
uniform float uNetFlow;
uniform float uNetBeamIntensity;  // filaments carried by the beams
uniform float uNetTwist;          // detail running around the circle
uniform float uNetOctaves;        // fractal depth, fractional
uniform float uNetLacunarity;     // how much finer each octave gets
uniform float uNetGain;           // how much quieter each octave gets

uniform float uTime;

// Channel blobs (position, size, VU, colour, state)
uniform vec4  uBlobPosSize0;
uniform vec4  uBlobPosSize1;
uniform vec4  uBlobPosSize2;
uniform vec4  uBlobPosSize3;
/** Per blob, the things the light alone could not carry: how loud it is, how
 *  far an action has it, and a seed so four blobs do not sparkle in step.
 *  x = vuPeak, y = action 0..1, z = seed, w = depth fade (back of the sphere). */
uniform vec4  uBlobState0;
uniform vec4  uBlobState1;
uniform vec4  uBlobState2;
uniform vec4  uBlobState3;
uniform vec4  uBlobTrailA0;
uniform vec4  uBlobTrailA1;
uniform vec4  uBlobTrailA2;
uniform vec4  uBlobTrailA3;
uniform vec4  uBlobTrailB0;
uniform vec4  uBlobTrailB1;
uniform vec4  uBlobTrailB2;
uniform vec4  uBlobTrailB3;
uniform vec4  uBlobTrailC0;
uniform vec4  uBlobTrailC1;
uniform vec4  uBlobTrailC2;
uniform vec4  uBlobTrailC3;
uniform vec4  uBlobTrailD0;
uniform vec4  uBlobTrailD1;
uniform vec4  uBlobTrailD2;
uniform vec4  uBlobTrailD3;

/** What an action wears. Neon violet rather than white: white is what a VU
 *  peak already blends towards, and a signal that borrows another signal's
 *  colour says nothing. The hue alone will not separate it from channel one's
 *  pink -- the flicker is the other half of the message. */
uniform vec3  uActionColour;
// How much of each of the blob's three effects there is: sparkle, bolt, wake.
uniform vec3  uBlobEffects;

// Where each channel's trajectory is. See SphereShader::setLineTexture.
uniform sampler2D uLineMap0;
uniform sampler2D uLineMap1;
uniform sampler2D uLineMap2;
uniform sampler2D uLineMap3;
// Which of the four have anything in them, and how far the maps reach.
uniform vec4  uLineOn;
uniform float uLineExtent;
// How much of each of the line's four effects there is: glow, filaments,
// bolts, and how hot it runs where the blob is.
uniform vec4  uLineEffects;
// The weave: turns over the figure, turns a second along it, how deep it cuts,
// and how many strands it stands for.
uniform vec4  uBraid;

// Where the four speakers are, worked out once per frame on the CPU.
//
// These used to be built per pixel out of asSeen(), which is four sines and
// cosines of the camera's two angles -- times four speakers, times every pixel
// on the display. It cost a quarter of a core on its own and it is the same
// answer for every pixel of a frame, which is the definition of work that
// belongs on the other side.
//
// `Dir` is the bearing the flat beam band leaves on, on the screen. The other
// three are the cabinet's frame in the eye's own terms: where it stands, which
// way its baffle looks, and which way is along its width.
uniform vec2  uSpkDir0;
uniform vec2  uSpkDir1;
uniform vec2  uSpkDir2;
uniform vec2  uSpkDir3;
uniform vec3  uSpkCentre0;
uniform vec3  uSpkCentre1;
uniform vec3  uSpkCentre2;
uniform vec3  uSpkCentre3;
uniform vec3  uSpkNose0;
uniform vec3  uSpkNose1;
uniform vec3  uSpkNose2;
uniform vec3  uSpkNose3;
uniform vec3  uSpkSide0;
uniform vec3  uSpkSide1;
uniform vec3  uSpkSide2;
uniform vec3  uSpkSide3;

uniform vec3  uBlobCol0;
uniform vec3  uBlobCol1;
uniform vec3  uBlobCol2;
uniform vec3  uBlobCol3;
uniform float uNumBlobs;

// ─── helpers ────────────────────────────────────────────────────

float sphereIntersect (vec2 uv, out vec3 normal)
{
    float d2 = dot (uv, uv);
    if (d2 > 1.0)
    {
        // The ray misses, but the antialiasing band outside the silhouette
        // still asks for a normal here. Hand back the rim's own — pointing
        // straight out, z = 0 — so that band blends into the rim colour.
        // Leaving `normal` unwritten left the caller reading an
        // uninitialised value, which drew a hard black ring two pixels wide
        // around the sphere.
        normal = vec3 (normalize (uv), 0.0);
        return -1.0;
    }
    float z = sqrt (1.0 - d2);
    normal = vec3 (uv, z);
    return z;
}

// ─── data accessors (unrolled for GLSL 1.20) ───────────────────

vec4 getBlobPosSize (int i)
{
    if (i == 0) return uBlobPosSize0;
    if (i == 1) return uBlobPosSize1;
    if (i == 2) return uBlobPosSize2;
    return uBlobPosSize3;
}

vec3 getBlobCol (int i)
{
    if (i == 0) return uBlobCol0;
    if (i == 1) return uBlobCol1;
    if (i == 2) return uBlobCol2;
    return uBlobCol3;
}


// Truncated cone leaving the horn's mouth at the mouth's own width. Only ever
// drawn outside the sphere — where it lands, the net takes over. Mirrors
// beamHalfWidthAt() in SpeakerLightScaling.cc.
// How much of the glow's net has emerged from behind the sphere. Mirrors
// glowEmergence() in EnergyMap.cc.
float glowEmergence (float d, float rise)
{
    return smoothstep (1.0, 1.0 + max (rise, 0.0001), d);
}

// Half the chord a view ray traverses, 1 at the centre and 0 at the rim.
float sphereHalfChord (float d)
{
    float c = clamp (d, 0.0, 1.0);
    return sqrt (1.0 - c * c);
}


// ─── energy map and fractal net ─────────────────────────────────

// The display is an orthographic view of the upper hemisphere from above: the
// centre of the disc is straight up, the rim is the horizon. That is exactly
// what the sphere's own normal describes, so the direction is that normal with
// its horizontal axes rearranged to match the way blobs are placed —
// cartesian2DHOA2JUCE maps a position to { -y, -x } with JUCE's y pointing
// down. Mirrors energyDirectionForScreen() in EnergyMap.cc.
vec3 screenToDirection (vec2 uv, float dist)
{
    float r = min (dist, 1.0);
    float up = sqrt (max (0.0, 1.0 - r * r));

    // What the pixel stands for as the eye sees it. The screen's horizontal
    // is the room's y and its vertical the room's x -- cartesian2DHOA2JUCE
    // puts a position at { -y, -x } -- which is where this shuffle comes from.
    vec3 seen = vec3 (uv.y, -uv.x, up);

    if (uCamera.x == 0.0 && uCamera.y == 0.0)
        return seen;

    // Back into the room's own terms: undo the lean, then the walk. The last
    // thing done is the first thing undone, or the room comes back tipped the
    // wrong way. Mirrors asSeenFromInverse() in SphereProjection.
    float cp = cos (-uCamera.x);
    float sp = sin (-uCamera.x);
    vec3 unpitched = vec3 (seen.x * cp + seen.z * sp,
                           seen.y,
                          -seen.x * sp + seen.z * cp);

    float ct = cos (uCamera.y);
    float st = sin (uCamera.y);

    return vec3 (unpitched.x * ct - unpitched.y * st,
                 unpitched.x * st + unpitched.y * ct,
                 unpitched.z);
}

vec2 energyUV (vec3 dir)
{
    float azimuth = atan (dir.y, dir.x);
    float elevation = asin (clamp (dir.z, -1.0, 1.0));

    return vec2 (azimuth / 6.28318531 + 0.5, elevation / 3.14159265 + 0.5);
}

float energyAt (vec3 dir)
{
    return texture2D (uEnergyMap, energyUV (dir)).r;
}

float hash13 (vec3 p)
{
    return fract (sin (dot (p, vec3 (127.1, 311.7, 74.7))) * 43758.5453);
}

float boltAt (float distance, float width)
{
    return width / (abs (distance) + width);
}

float valueNoise (vec3 p)
{
    vec3 i = floor (p);
    vec3 f = fract (p);
    f = f * f * (3.0 - 2.0 * f);

    float n000 = hash13 (i + vec3 (0.0, 0.0, 0.0));
    float n100 = hash13 (i + vec3 (1.0, 0.0, 0.0));
    float n010 = hash13 (i + vec3 (0.0, 1.0, 0.0));
    float n110 = hash13 (i + vec3 (1.0, 1.0, 0.0));
    float n001 = hash13 (i + vec3 (0.0, 0.0, 1.0));
    float n101 = hash13 (i + vec3 (1.0, 0.0, 1.0));
    float n011 = hash13 (i + vec3 (0.0, 1.0, 1.0));
    float n111 = hash13 (i + vec3 (1.0, 1.0, 1.0));

    return mix (mix (mix (n000, n100, f.x), mix (n010, n110, f.x), f.y),
                mix (mix (n001, n101, f.x), mix (n011, n111, f.x), f.y), f.z);
}

vec4 getBlobState (int i)
{
    if (i == 0) return uBlobState0;
    if (i == 1) return uBlobState1;
    if (i == 2) return uBlobState2;
    return uBlobState3;
}

vec4 getBlobTrailA (int i)
{
    if (i == 0) return uBlobTrailA0;
    if (i == 1) return uBlobTrailA1;
    if (i == 2) return uBlobTrailA2;
    return uBlobTrailA3;
}

vec4 getBlobTrailB (int i)
{
    if (i == 0) return uBlobTrailB0;
    if (i == 1) return uBlobTrailB1;
    if (i == 2) return uBlobTrailB2;
    return uBlobTrailB3;
}

vec4 getBlobTrailC (int i)
{
    if (i == 0) return uBlobTrailC0;
    if (i == 1) return uBlobTrailC1;
    if (i == 2) return uBlobTrailC2;
    return uBlobTrailC3;
}

vec4 getBlobTrailD (int i)
{
    if (i == 0) return uBlobTrailD0;
    if (i == 1) return uBlobTrailD1;
    if (i == 2) return uBlobTrailD2;
    return uBlobTrailD3;
}

/** One link of the wake.
 *
 *  The field around the segment a->b, tapering in width and brightness from
 *  one end to the other and billowing across itself as it goes. Bounded at
 *  both ends on purpose: boltAt() never reaches zero, and four of these
 *  running to the edge of the screen is a haze over the whole picture rather
 *  than a trail behind one blob. */
float wakeSegment (vec2 uv, vec2 a, vec2 b, float w0, float w1,
                   float i0, float i1, float seed)
{
    vec2 ab = b - a;
    float len2 = max (dot (ab, ab), 1e-8);
    float t = clamp (dot (uv - a, ab) / len2, 0.0, 1.0);
    vec2 onIt = a + ab * t;
    float w = mix (w0, w1, t);

    // The billow. The centre line is pushed sideways rather than the distance
    // to it being shifted: shifting the distance and taking its modulus draws
    // the two edges of a tube and leaves the middle empty, which is a wire
    // outline following the blob around -- what the first version of this did.
    vec2 across = normalize (vec2 (-ab.y, ab.x) + 1e-6);
    float billow = valueNoise (vec3 (onIt * 26.0, uTime * 0.7 + seed)) - 0.5;
    vec2 centre = onIt + across * billow * w * 1.6;

    float d = length (uv - centre);
    // Squared rather than linear, so the plume has a body and an edge instead
    // of trailing off into a wash the width of the sphere.
    float reach = clamp (1.0 - d / (w * 6.0), 0.0, 1.0);
    return boltAt (d, w * 0.8) * reach * reach * mix (i0, i1, t);
}

/** How near a channel's trajectory this pixel is, 0 away from it to 1 on it.
 *
 *  Sampled rather than computed: a thousand points cannot be handed to a
 *  fragment shader, so the line is rasterised into a small map on the way past
 *  (MotionComponent::lineMapFor) and read back here. Bilinear filtering turns
 *  the stepped cone that was drawn into a smooth enough field. */
float lineNear (vec2 uv, int i)
{
    vec2 t = uv / uLineExtent * 0.5 + 0.5;
    if (t.x < 0.0 || t.x > 1.0 || t.y < 0.0 || t.y > 1.0)
        return 0.0;

    if (i == 0) return texture2D (uLineMap0, t).r;
    if (i == 1) return texture2D (uLineMap1, t).r;
    if (i == 2) return texture2D (uLineMap2, t).r;
    return texture2D (uLineMap3, t).r;
}

/** Where along the figure a pixel is, 0..1, out of the map's second channel.
 *
 *  Only meaningful within the cord itself -- that is the only part of the line
 *  map drawn in pieces -- which is exactly where the weave is wanted. */
float lineArc (vec2 uv, int i)
{
    vec2 t = uv / uLineExtent * 0.5 + 0.5;
    if (t.x < 0.0 || t.x > 1.0 || t.y < 0.0 || t.y > 1.0)
        return 0.0;

    if (i == 0) return texture2D (uLineMap0, t).g;
    if (i == 1) return texture2D (uLineMap1, t).g;
    if (i == 2) return texture2D (uLineMap2, t).g;
    return texture2D (uLineMap3, t).g;
}

/** How the cord is twisted here, 0..1.
 *
 *  A braid of three hairlines cannot be drawn as light at this size: the map
 *  the shader finds the line through is two and a half screen pixels a texel,
 *  and three strands two pixels apart are one texel. What *can* be drawn is
 *  what a braid does to the light -- the maximum of three sines a third of a
 *  turn apart, which crests three times a winding and is the signature of a
 *  twisted cord rather than a round one.
 *
 *  It travels along the line, because a weave that stood still would be a
 *  texture printed on the figure rather than a rope being laid. */
float braidWeaveAt (float u)
{
    if (uBraid.z < 0.001)
        return 0.5;

    float phase = 6.28318531 * (u * uBraid.x + uTime * uBraid.y);
    float strands = max (uBraid.w, 1.0);

    float best = -2.0;
    for (int k = 0; k < 5; k++)
    {
        if (float (k) >= strands) break;
        best = max (best, sin (phase + 6.28318531 * float (k) / strands));
    }

    return 0.5 + 0.5 * best;
}

float lineOn (int i)
{
    if (i == 0) return uLineOn.x;
    if (i == 1) return uLineOn.y;
    if (i == 2) return uLineOn.z;
    return uLineOn.w;
}

/** What the trajectory burns with.
 *
 *  The line itself is still drawn as a vector, over the top, because a vector
 *  is the one thing that can be a crisp line thinner than a pixel. Everything
 *  that *glows* is here, because the opposite is true of glow: JUCE's 2D
 *  context has no additive blend at all, so a stroked "plasma" is a stack of
 *  translucent ribbons and looks like one.
 *
 *  Three things, all built out of the same field:
 *
 *  - **The glow.** Two falloffs of the nearness, a tight bright one and a wide
 *    faint one.
 *  - **The filaments.** Contours of the field *after the lookup has been
 *    pushed around by noise*: a contour of a warped distance field runs
 *    alongside the line, wanders, and -- this is the point -- can never fold
 *    into spokes the way an offset copy of the curve does at a pole, because
 *    it is a level set and not a parallel curve.
 *  - **The bolts.** The same, warped harder and cut sharper, so what is left
 *    is short and bright.
 *
 *  And it is hottest where the sound is: near the channel's own blob it runs
 *  towards white. That is the physical connection the maintainer asked for --
 *  the wire is energised where the thing travelling along it is.
 */
vec3 lineGlow (vec2 uv, int i)
{
    if (lineOn (i) < 0.5)
        return vec3 (0.0);

    float near = lineNear (uv, i);
    if (near < 0.02)
        return vec3 (0.0);

    vec3 col = getBlobCol (i);
    vec4 st = getBlobState (i);
    float vu = clamp (st.x, 0.0, 1.0);
    float seed = st.z;

    // Only near the line. boltAt() never reaches zero -- that long tail is
    // what ties a bolt into the glow around it -- so every term built on it
    // has to be shut off by hand where there is no line, or four channels'
    // worth of tails wash the whole ball white. Which is exactly what the
    // first build of this did.
    float presence = smoothstep (0.04, 0.30, near);

    // Where the sound is on this line, and how far this pixel is from it.
    vec4 ps = getBlobPosSize (i);
    float atBlob = ps.z > 0.001
                 ? smoothstep (0.30, 0.0, length (uv - ps.xy)) * uLineEffects.w
                 : 0.0;

    // How the cord is twisted here. It rides on the core rather than on the
    // whole glow: the outer filaments are the plasma and belong to the field,
    // not to the rope.
    // How deep the twist cuts. Past one it sharpens the wave rather than
    // being handed to mix() as a factor above one -- mix extrapolates, so a
    // weave of 2.2 was swinging the cord's waist by half its width and
    // whitening its core by sixty per cent. That is not a deeper twist, that
    // is a different picture.
    float weave = braidWeaveAt (lineArc (uv, i));
    float swing = clamp (uBraid.z, 0.0, 1.0);
    float gain = 1.0 + max (uBraid.z - 1.0, 0.0);
    weave = clamp (0.5 + (weave - 0.5) * gain, 0.0, 1.0);

    float twist = mix (1.0, 0.78 + 0.44 * weave, swing);

    // The glow. The exponent is what the line's apparent thickness actually
    // is: the vector stroke under it is a single pixel, and everything wider
    // than that is this. Asked to halve the line, halve this first.
    //
    // The weave narrows and widens it a little along its length, which is the
    // scalloped silhouette of a laid rope, and brightens where a strand comes
    // over the top.
    // Divided by what the core is worth, so a full-strength core still reads
    // as one and the weave has somewhere to move it.
    float tight = pow (clamp (near * twist / 0.88, 0.0, 1.0), 9.0)
                * mix (1.0, 0.40 + 1.25 * weave, swing);
    // The pale wash standing off the cord. It reached almost as far as the
    // map does -- an exponent barely above one is nearly the raw field -- and
    // what it read as was a fat light-red line rather than a thin one with
    // air around it. The filaments and the bolts are untouched: those are the
    // parts the maintainer said to keep.
    float wide  = pow (near, 3.0);

    // The filaments. The noise is sampled in scene space and drifts, so they
    // crawl along the line rather than sitting on it.
    vec2 warp = vec2 (valueNoise (vec3 (uv * 7.0, uTime * 0.23 + seed)),
                      valueNoise (vec3 (uv * 7.0 + 19.0, uTime * 0.23 + seed)))
              - 0.5;
    float warped = lineNear (uv + warp * 0.22, i);
    float filament = (boltAt (warped - 0.46, 0.035)
                      + boltAt (warped - 0.70, 0.025) * 0.7)
                   * presence * uLineEffects.y;

    // The bolts: warped harder, cut sharper, and only some of the time.
    vec2 jag = vec2 (valueNoise (vec3 (uv * 23.0, uTime * 1.7 + seed)),
                     valueNoise (vec3 (uv * 23.0 + 41.0, uTime * 1.7 + seed)))
             - 0.5;
    float struck = lineNear (uv + jag * 0.12, i);
    float gate = step (0.55, hash13 (vec3 (floor (uTime * 3.0), seed,
                                           floor (near * 5.0))));
    float bolt = boltAt (struck - 0.88, 0.012) * gate * presence
               * uLineEffects.z;

    // And a little white where a strand rides over the top, so the crest reads
    // as something catching the light rather than as the line simply pulsing.
    vec3 hot = mix (col, uBoltCoreColour,
                    0.15 + 0.55 * atBlob
                        + 0.45 * swing * max (weave - 0.5, 0.0));

    return col * wide * 0.055 * uLineEffects.x
         + hot * tight * 0.45 * uLineEffects.x
         + hot * filament * (0.16 + 0.30 * atBlob + 0.18 * vu)
         + mix (col, uBoltCoreColour, 0.75) * bolt * (0.30 + 0.55 * atBlob);
}

/** The blob itself: a hot core, a corona around it, sparks off it, and a bolt
 *  when the channel spikes.
 *
 *  Drawn here rather than as a flat disc over the top, which is what it was
 *  until 2026-09-13 -- and a disc is a disc: hard-edged, unlit, the same in a
 *  quiet passage as in a drop. Everything below reacts to something.
 *
 *  All of it is additive and procedural. No particle lives anywhere: a spark
 *  is a function of where you are, what time it is and the blob's seed, the
 *  same way the sphere's own bolts already work. Nothing to allocate, nothing
 *  to keep in step, and it costs the same whether one channel plays or four.
 */
vec3 blobLight (vec2 uv, int i)
{
    vec4 ps = getBlobPosSize (i);
    if (ps.z < 0.001)
        return vec3 (0.0);

    vec3 col = getBlobCol (i);
    vec4 st = getBlobState (i);

    float vu     = clamp (st.x, 0.0, 1.0);
    float action = clamp (st.y, 0.0, 1.0);
    float seed   = st.z;
    float depth  = clamp (st.w, 0.0, 1.0);

    vec2 d2 = uv - ps.xy;
    float d = length (d2);
    float r = ps.z;

    // The core. Hot in the middle and gone by the edge -- a body rather than a
    // stamped circle, so it sits *in* the picture instead of on it. Two parts:
    // a body out to the blob's radius and a hard point inside it, because a
    // single soft falloff on top of a neon trajectory reads as a smudge over
    // the line rather than as the thing travelling along it.
    float core = smoothstep (r, r * 0.35, d);
    float pip = smoothstep (r * 0.45, r * 0.12, d);

    // The corona, swelling with the level. It reached only as far as two
    // translucent rings before; this falls off continuously, so a loud channel
    // lights the room around it rather than growing a second outline.
    // How far it reaches was five and a half blob-radii per unit of level,
    // which at a working level is a wash a third of the sphere across with the
    // body lost inside it. A corona is a surround, not a fog.
    float reach = r * (1.9 + 2.4 * vu + 1.6 * action);
    float halo = pow (clamp (1.0 - d / reach, 0.0, 1.0), 3.0);

    // Sparks. A ring of flecks that drift outwards and burn out, thrown harder
    // by level and much harder while an action runs. Procedural: the ring is
    // sampled by angle, and each fleck's life is a fraction of time.
    float sparks = 0.0;
    float sparkGain = (0.35 + 1.6 * vu + 3.0 * action) * uBlobEffects.x;
    if (sparkGain > 0.01 && d < reach * 1.6)
    {
        float ang = atan (d2.y, d2.x);
        // Forty slots around the blob; each holds one fleck at a time.
        float slots = 40.0;
        float turn = ang / 6.28318531 + 0.5;
        float slot = floor (turn * slots);
        float life = fract (uTime * (0.7 + 0.5 * vu) + hash13 (vec3 (slot, seed, 1.0)));
        // Where this fleck has got to, and how bright it still is.
        float travel = r * (0.9 + 3.4 * life);
        float fade = 1.0 - life;
        // Round, and off the grid. Bounded by angle as well as by radius, or a
        // slot is an arc a ninth of the way round the blob and forty of them
        // read as a gear wheel rather than as sparks; the jitter is what keeps
        // the survivors off a perfect ring.
        float jitter = hash13 (vec3 (slot, seed, 5.0)) - 0.5;
        float dAng = (fract (turn * slots) - 0.5 + jitter * 0.7) / slots
                   * 6.28318531 * d;
        float grain = r * 0.11 * (0.3 + fade);
        float fleck = boltAt (d - travel, grain) * boltAt (dAng, grain * 1.3);
        // Only some slots are lit at any moment.
        float lit = step (0.34, hash13 (vec3 (slot, seed, floor (uTime * 6.0 + life))));
        sparks += fleck * fade * fade * lit;
    }

    // The bolt. A short arm that strikes out of the blob on a transient and is
    // gone -- the sphere's own lightning does the same thing with the same
    // helper, so the two read as one weather.
    float bolt = 0.0;
    if (vu > 0.25 && uBlobEffects.y > 0.001)
    {
        float strikeId = floor (uTime * 11.0);
        // Rare enough to be an event. At a threshold of 0.30 one was alight
        // seven frames in ten, which is not lightning, it is a whisker.
        float fires = step (0.66, hash13 (vec3 (seed, strikeId, 3.0))) * step (0.28, vu);
        if (fires > 0.0)
        {
            float ang = hash13 (vec3 (seed, strikeId, 7.0)) * 6.28318531;
            vec2 dir = vec2 (cos (ang), sin (ang));
            float along = dot (d2, dir);
            // Signed. Taken as a modulus it is mirrored about the arm's axis,
            // so a path straying to one side is drawn on both and the bolt
            // comes out as a closed lens rather than as a line.
            float across = dot (d2, vec2 (-dir.y, dir.x));
            // Strays as it travels, the way the sphere's bolts do. Slowly:
            // at twenty-six noise periods over an arm this long the path
            // doubled back on itself every few pixels and drew a string of
            // little wire outlines rather than a bolt.
            float stray = (valueNoise (vec3 (along * 7.0, seed, uTime * 5.0)) - 0.5)
                        * r * 1.2;
            float len = r * (2.5 + 7.0 * vu);
            float within = step (0.0, along) * step (along, len);
            float taper = 1.0 - along / max (len, 0.001);
            bolt += within * boltAt (across - stray, r * 0.07 * taper)
                  * taper * taper * uBlobEffects.y;
        }
    }

    // What it is all painted in. The channel keeps its colour -- that is how
    // four of them stay apart -- and the two things that are *events* carry
    // their own: a peak runs towards the bolt core's white, an action towards
    // the neon the skin names.
    // The channel's colour has to survive the level. It blended 0.85 of the
    // way to white at full VU, which made a loud blob a white dot -- and four
    // white dots are four channels you can no longer tell apart. Only the very
    // middle goes white now, and only when it is genuinely loud.
    vec3 hot   = mix (col, uBoltCoreColour, 0.10 + 0.30 * vu * vu);
    vec3 spark = mix (hot, uActionColour, action);

    // The action ring: one more corona on top, pulsing, in the action colour.
    // It is the flicker that carries the message where the hue cannot -- on
    // channel one the blob is already pink.
    float ring = 0.0;
    if (action > 0.001)
    {
        float pulse = 0.55 + 0.45 * sin (uTime * 31.0 + seed * 6.0);
        float edge = r * (1.7 + 0.5 * pulse);
        ring = boltAt (d - edge, r * 0.22) * pulse * action;
    }

    // A rim just inside the edge, so the body reads as a body at rest rather
    // than as the brightest part of a cloud. It is what the flat disc did well
    // and the first version of this lost.
    float rim = boltAt (d - r * 0.82, r * 0.20);

    // The chemtrail. Four links, each thinner and fainter than the one in
    // front, hung off points that lag the blob -- so it is a wake rather than
    // a second drawing of the line the take already has under it.
    //
    // How far the tail has been left behind is the gate: parked, the four
    // points sit on the blob and there is nothing to draw, and gating on that
    // spread costs a length() rather than a uniform nobody would ever set by
    // hand.
    vec4 ta = getBlobTrailA (i);
    vec4 tb = getBlobTrailB (i);
    vec4 tc = getBlobTrailC (i);
    vec4 td = getBlobTrailD (i);
    float spread = length (td.zw - ps.xy) / max (r, 0.001);
    float trail = 0.0;
    if (spread > 0.35 && uBlobEffects.z > 0.001)
    {
        trail += wakeSegment (uv, ps.xy, ta.xy, r * 0.88, r * 0.79,
                              1.00, 0.87, seed);
        trail += wakeSegment (uv, ta.xy, ta.zw, r * 0.79, r * 0.70,
                              0.87, 0.74, seed + 1.0);
        trail += wakeSegment (uv, ta.zw, tb.xy, r * 0.70, r * 0.61,
                              0.74, 0.62, seed + 2.0);
        trail += wakeSegment (uv, tb.xy, tb.zw, r * 0.61, r * 0.52,
                              0.62, 0.50, seed + 3.0);
        trail += wakeSegment (uv, tb.zw, tc.xy, r * 0.52, r * 0.43,
                              0.50, 0.39, seed + 4.0);
        trail += wakeSegment (uv, tc.xy, tc.zw, r * 0.43, r * 0.34,
                              0.39, 0.28, seed + 5.0);
        trail += wakeSegment (uv, tc.zw, td.xy, r * 0.34, r * 0.25,
                              0.28, 0.18, seed + 6.0);
        trail += wakeSegment (uv, td.xy, td.zw, r * 0.25, r * 0.14,
                              0.18, 0.07, seed + 7.0);
        trail *= smoothstep (0.35, 1.2, spread)
               * (0.75 + 0.85 * vu + 0.7 * action) * uBlobEffects.z;
    }
    // It wears the action too. A trail in the channel's colour while an action
    // runs would leave the one effect that has to be unmistakable saying
    // nothing at the very moment it matters.
    vec3 trailCol = mix (col, uActionColour, action * 0.85);

    vec3 out3 = hot * core * 1.55
              + mix (hot, uBoltCoreColour, 0.18) * pip * 1.40
              + hot * rim * 0.85
              + col * halo * (0.30 + 0.6 * vu)
              + spark * sparks * sparkGain * 0.8
              + mix (col, uBoltCoreColour, 0.55) * bolt * (0.7 + vu)
              + uActionColour * ring * 1.3
              + trailCol * trail * 1.5;

    return out3 * depth;
}

// Lightning. A bolt is a path, not a field: for each radius it sits at some
// angle that wanders with noise, and a pixel's brightness comes from its
// distance to that path -- width/(distance+width), full at the core and
// trailing off without ever quite ending. That tail is what ties the band into
// the net inside and the glow outside; a soft-edged field, however hard it is
// sharpened, stops at a line instead. Mirrors boltFalloff() and boltStrike()
// in EnergyMap.cc.
//
// Returns the bolt field in x and its hottest core in y, so the core can be
// drawn white and the glow in colour.
vec2 bolts (float dA, float d, float halfWidth, float seed, float mouthR)
{
    float width = radians (uBoltWidth);
    float best = 0.0;

    for (int i = 0; i < 8; ++i)
    {
        if (float (i) >= uBoltCount) break;

        float id = seed + float (i) * 31.7;

        // Some bolts stay in the annulus, others break out towards the edge of
        // the screen -- otherwise the band reads as a ring with a hard limit.
        float escapes = step (uBoltEscape, valueNoise (vec3 (id, 9.0, 0.0)));
        float far = mix (mouthR, uBoltReach, escapes);
        if (d > far) continue;

        // Fades in where it reaches into the sphere and out where it ends
        float rad = smoothstep (1.0 - uBeamBleed, 1.0 - uBeamBleed * 0.4, d)
                  * smoothstep (far, far - (far - 1.0) * 0.45, d);

        // Each bolt strikes and is gone rather than sitting there
        float strike = smoothstep (uBoltDuty, 1.0,
                                   valueNoise (vec3 (id, 5.0, uTime * uBoltRate)));
        if (strike <= 0.0) continue;

        // Where this bolt sits across the band, and how it strays on the way
        float lane = (valueNoise (vec3 (id, 0.0, 0.0)) - 0.5) * 1.4;
        float stray = (valueNoise (vec3 (id, d * uBoltScale, uTime * uBoltFlow))
                       - 0.5) * 2.0;
        float trunk = (lane + stray * uBoltWander) * halfWidth;

        best = max (best, boltAt (abs (dA - trunk), width) * strike * rad);

        // Branches share the trunk until they fork, then go their own way.
        // Mirrors boltBranchOffset() in EnergyMap.cc.
        for (int b = 0; b < 3; ++b)
        {
            if (float (b) >= uBoltBranches) break;

            float bid = id + float (b) * 7.13 + 101.0;
            float fork = 1.0 + valueNoise (vec3 (bid, 1.0, 0.0)) * (far - 1.0);
            float away = max (0.0, d - fork) * uBoltBranch;
            float side = (valueNoise (vec3 (bid, 2.0, uTime * uBoltFlow)) - 0.5)
                       * 2.0;

            float path = trunk + side * away * halfWidth;

            // Thinner and dimmer than what they came off
            best = max (best, boltAt (abs (dA - path), width * 0.65)
                              * strike * rad * 0.7);
        }
    }

    return vec2 (best, pow (best, uBoltCoreExp));
}

// A beam is a band in the annulus between the horn's mouth and the sphere: it
// leaves the speaker narrow and opens to a quarter of the way round by the
// time it arrives, so the four of them close the circle. Its centre line
// wanders with fractal noise, which is what turns a band into a root.
// Mirrors beamWrapHalfAngle() in EnergyMap.cc.
//
// Needs valueNoise(), so it lives below it.
// ─── the speakers, as objects in the room ───────────────────────

/** Where a seen direction lands on the screen. Inverts the shuffle
 *  screenToDirection makes on the way in. */
vec2 seenToScreen (vec3 seen)
{
    return vec2 (-seen.y, seen.x);
}

float speakerLevel (int i)
{
    if (i == 0) return uSpotLevel0;
    if (i == 1) return uSpotLevel1;
    if (i == 2) return uSpotLevel2;
    return uSpotLevel3;
}

/** Slab test against a box at the origin. x is where the ray goes in, y where
 *  it comes out; in > out means it missed. */
vec2 boxSpan (vec3 ro, vec3 rd, vec3 halfExtent)
{
    // A ray exactly along a face has a zero in it and 1/0 is an infinity that
    // the min/max below handles correctly -- but only if it is an infinity and
    // not a driver's idea of one. Nudged rather than branched.
    vec3 inv = 1.0 / (rd + (1.0 - abs (sign (rd))) * 0.000001);
    vec3 a = (-halfExtent - ro) * inv;
    vec3 b = ( halfExtent - ro) * inv;
    vec3 lo = min (a, b);
    vec3 hi = max (a, b);
    return vec2 (max (max (lo.x, lo.y), lo.z),
                 min (min (hi.x, hi.y), hi.z));
}

/** Which face of the box a local hit point is on, as a local normal. */
vec3 boxNormal (vec3 local, vec3 halfExtent)
{
    vec3 share = abs (local) / halfExtent;
    if (share.x > share.y && share.x > share.z)
        return vec3 (sign (local.x), 0.0, 0.0);
    if (share.y > share.z)
        return vec3 (0.0, sign (local.y), 0.0);
    return vec3 (0.0, 0.0, sign (local.z));
}

vec2 beamDensity (vec2 point, vec2 spkDir, float level)
{
    float mouthR = uSpeakerRadius - uMouthOffset;
    float d = length (point);

    // Full strength in the annulus, bleeding past both ends so the band runs
    // into the glow's filaments outside and the net's inside instead of
    // sitting between them. Mirrors beamRadialWindow() in EnergyMap.cc.
    float span = max (uBeamBleed, 0.0001);
    float radial = smoothstep (0.0, 1.0, clamp ((mouthR + span - d) / span, 0.0, 1.0))
                 * smoothstep (0.0, 1.0, clamp ((d - (1.0 - span)) / span, 0.0, 1.0));

    // The body stops at the annulus but the bolts do not — escaping ones carry
    // on to the edge of the screen, so this cannot bail out on `radial`.
    if (d < 1.0 - span || d > max (mouthR, uBoltReach)) return vec2 (0.0);

    // How far along the way in, 0 at the mouth and 1 at the sphere.
    float t = clamp ((mouthR - d) / max (mouthR - 1.0, 0.0001), 0.0, 1.0);
    float eased = t * t * (3.0 - 2.0 * t);

    float halfWidth = radians (mix (uApertureAngle, uWrapAngle, eased));

    // Offset from the speaker's own direction, wrapped to +-pi
    float a = atan (point.y, point.x);
    float a0 = atan (spkDir.y, spkDir.x);
    float dA = mod (a - a0 + 9.42477796, 6.28318531) - 3.14159265;

    // The centre line wanders sideways, more the further it has travelled —
    // roots, not spokes. Sampled on the direction so it has no seam.
    vec3 wp = vec3 (cos (a), sin (a), 0.0) * uWanderTwist;
    wp.z = d * uWanderScale - uTime * uWanderFlow;
    float wander = (valueNoise (wp) - 0.5) * radians (uWander) * eased;

    // The edge frays: its own noise eats into the band so it ends in tendrils
    // rather than a clean line.
    vec3 fp = vec3 (cos (a), sin (a), 0.0) * uWanderTwist * 2.0;
    fp.z = d * uWanderScale * 2.0 - uTime * uWanderFlow * 1.7;
    float ragged = halfWidth * (1.0 + (valueNoise (fp) - 0.5) * uBeamFray);

    float across = 1.0 - smoothstep (ragged * uBeamEdge, ragged,
                                     abs (dA - wander));
    if (across <= 0.0) return vec2 (0.0);

    // Denser where it wraps the sphere than where it leaves the speaker.
    float grip = mix (uBeamRoot, 1.0, eased);

    // Never lets go entirely — a silent speaker thins its band instead of
    // dropping it and opening the ring. Relative to the loudest band, so with
    // nothing playing there is no ring. Mirrors beamBandLevel().
    float loudest = max (max (uSpotLevel0, uSpotLevel1),
                         max (uSpotLevel2, uSpotLevel3));
    float lifted = max (level, uBeamFloor * loudest);

    // The envelope is not drawn and does not hide anything — it only says
    // where a bolt may strike, and how brightly. What reaches the screen is
    // the bolts alone, so the glow behind them stays visible between them.
    float envelope = lifted * across * grip * radial;

    float seed = spkDir.x * 13.0 + spkDir.y * 71.0;
    vec2 strike = bolts (dA - wander, d, ragged, seed, mouthR);

    return vec2 (envelope * strike.x, envelope * strike.y);
}

// x is the bolts' coloured glow, y their white core.
/** Which way a speaker lies as the eye sees it, on the screen.
 *
 *  The four directions used to be written in as the screen's own diagonals,
 *  which nails them to the glass: walk round the room and the speakers stay
 *  in the corners of the display while everything else turns. They come from
 *  the room now and are carried through the camera like any other direction.
 *
 *  Normalised, which is what a two-dimensional band can be given: under a
 *  *lean* the speaker moves in off the rim and the annulus this is built on
 *  has no word for that. The cabinets themselves are raytraced and do lean
 *  properly -- the bands are the half of this that is still flat, and they are
 *  meant to be rebuilt next. */
vec2 speakerScreenDir (int i)
{
    if (i == 0) return uSpkDir0;
    if (i == 1) return uSpkDir1;
    if (i == 2) return uSpkDir2;
    return uSpkDir3;
}

vec3 speakerCentre (int i)
{
    if (i == 0) return uSpkCentre0;
    if (i == 1) return uSpkCentre1;
    if (i == 2) return uSpkCentre2;
    return uSpkCentre3;
}

vec3 speakerNose (int i)
{
    if (i == 0) return uSpkNose0;
    if (i == 1) return uSpkNose1;
    if (i == 2) return uSpkNose2;
    return uSpkNose3;
}

vec3 speakerSide (int i)
{
    if (i == 0) return uSpkSide0;
    if (i == 1) return uSpkSide1;
    if (i == 2) return uSpkSide2;
    return uSpkSide3;
}

vec2 beamTotal (vec2 p)
{
    return beamDensity (p, speakerScreenDir (0), uSpotLevel0)
         + beamDensity (p, speakerScreenDir (1), uSpotLevel1)
         + beamDensity (p, speakerScreenDir (2), uSpotLevel2)
         + beamDensity (p, speakerScreenDir (3), uSpotLevel3);
}

// How big a cabinet is, in sphere radii: a little taller than it is wide and
// about as deep, which is the shape of a monitor standing on its end.
const vec3 kCabinetHalf = vec3 (0.085, 0.130, 0.095);



/** The four cabinets, raytraced.
 *
 *  An orthographic view, so a pixel's ray is a straight drop along the eye's
 *  own axis: in the frame the eye sees, that is the point (uv.y, -uv.x) coming
 *  down from far above. Each box is put into that frame whole -- its centre and
 *  its three axes carried through the camera -- and the ray is then taken into
 *  the box's own frame and slab-tested, which is the cheapest correct thing
 *  there is.
 *
 *  Each faces the listener. `depth` comes back as how far along the ray the
 *  nearest one was hit, so the caller can tell a cabinet in front of the ball
 *  from one behind it. */
vec4 speakerBoxes (vec2 uv, out float depth)
{
    vec3 ro = vec3 (uv.y, -uv.x, 4.0);
    vec3 rd = vec3 (0.0, 0.0, -1.0);

    depth = 1000.0;
    vec4 out4 = vec4 (0.0);

    for (int i = 0; i < 4; i++)
    {
        vec3 centre = speakerCentre (i);

        // Almost every pixel is nowhere near any cabinet, and the slab test
        // below is not cheap enough to run for all of them four times over.
        // A box is never further from its own centre than the length of its
        // half-extents, so a circle that wide around where the centre lands on
        // screen rejects the whole display bar a few thousand pixels -- for
        // two subtractions and a dot. It took the frame from eighty-four per
        // cent of a core to what the measurement in the commit says.
        vec2 near = uv - seenToScreen (centre);
        if (dot (near, near) > 0.0625)   // (0.25)^2, the diagonal plus slack
            continue;

        // Its own frame: nose towards the listener, head up, and the third
        // axis from the other two so the set stays right-handed however far
        // the room has been tipped.
        vec3 nose = speakerNose (i);
        vec3 side = speakerSide (i);
        vec3 head = cross (nose, side);

        vec3 toRay = ro - centre;
        vec3 roL = vec3 (dot (toRay, side), dot (toRay, head), dot (toRay, nose));
        vec3 rdL = vec3 (dot (rd, side), dot (rd, head), dot (rd, nose));

        vec2 span = boxSpan (roL, rdL, kCabinetHalf);
        if (span.x > span.y || span.y < 0.0)
            continue;

        float t = max (span.x, 0.0);
        if (t >= depth)
            continue;
        depth = t;

        vec3 hitL = roL + rdL * t;
        vec3 normalL = boxNormal (hitL, kCabinetHalf);
        vec3 normal = normalL.x * side + normalL.y * head + normalL.z * nose;

        // A key from over the listener's shoulder and a little fill, so the
        // top and the front read as two different faces rather than one flat
        // grey. The ball's own surface colour is the ground, brightened: a
        // cabinet the same value as the sphere is a hole in the picture.
        vec3 key = normalize (vec3 (0.35, 0.75, 0.55));
        float lit = 0.30 + 0.70 * max (dot (normal, key), 0.0);
        vec3 body = mix (uSphereSurface, vec3 (1.0), 0.22) * lit;

        // The edge, so a cabinet stands away from whatever is behind it.
        float edge = 1.0 - max (max (abs (hitL.x) / kCabinetHalf.x,
                                     abs (hitL.y) / kCabinetHalf.y),
                                abs (hitL.z) / kCabinetHalf.z);
        body += uSphereRim * smoothstep (0.05, 0.0, edge) * 0.35;

        // The drivers, on the face that looks at the listener. A woofer low
        // and a tweeter high, both sunk into the baffle -- and both lit by
        // what this speaker is being sent, which is the same number the band
        // leaving it is drawn from.
        if (normalL.z > 0.5)
        {
            float level = speakerLevel (i);
            vec2 face = vec2 (hitL.x, hitL.y);

            float woofer = length (face - vec2 (0.0, -0.035)) ;
            float tweeter = length (face - vec2 (0.0, 0.062));

            float inWoofer = smoothstep (0.050, 0.042, woofer);
            float inTweeter = smoothstep (0.022, 0.017, tweeter);
            float cone = max (inWoofer, inTweeter);

            // Sunk: dark in the middle, with a bright ring where the surround
            // meets the baffle.
            body = mix (body, body * 0.35, cone);
            body += uSphereRim * 0.5
                  * (smoothstep (0.056, 0.050, woofer) - inWoofer
                     + smoothstep (0.026, 0.022, tweeter) - inTweeter);
            body += uSpotColour * cone * level * 1.6;
        }

        out4 = vec4 (body, 1.0);
    }

    return out4;
}

// Ridged fractal noise: the filaments are the ridges between noise cells, and
// stacking octaves is what gives them branches within branches.
// Ridged fractal noise: the filaments are the ridges between noise cells, and
// stacking octaves is what gives them branches within branches.
//
// `radial` already carries the flow — the caller forms it, and its sign is what
// decides whether the filaments run in or out. `reach` clamps how far out the
// domain still varies; past it the pattern freezes, so it has to cover
// wherever the filaments are meant to go.
/** The bearing a pixel stands for in the *room*, as a unit vector.
 *
 *  At the identity camera this is exactly normalize(uv): screenToDirection
 *  shuffles the axes to { uv.y, -uv.x, up } and this shuffles them back, so a
 *  device nobody has tilted draws the picture it always drew, to the bit.
 */
vec2 netBearing (vec3 dir)
{
    vec2 bearing = vec2 (-dir.y, dir.x);
    float len = max (length (bearing), 0.000001);
    return bearing / len;
}

float netFilaments (vec2 bearing, float radialAt, float flow, float reach,
                    float twist, float scale, float sharpness,
                    float octaves, float lacunarity, float gain)
{
    // Inverts netFilamentRadius() in EnergyMap.cc: the filament standing at
    // this radius now is the one that started further along the flow.
    float radial = clamp (radialAt, 0.0, reach) + uTime * flow;

    // Domain built from the direction, not from an angle. An angle wraps, and
    // the wrap left a seam due west where the filaments failed to meet.
    // Mirrors netDomainPoint() in EnergyMap.cc.
    //
    // And the *room's* bearing, not the screen's. It used to normalise the
    // screen uv, which nails the whole weave to the display: the ball turns
    // under the camera and the energy stays where it was. Exactly the fault
    // the graticule had before it was rebuilt in the room's terms -- a net on
    // the screen draws the same picture whichever way the room is looked at.
    vec3 p = vec3 (bearing * twist, radial * scale);

    float sum = 0.0;
    float amp = 1.0;
    float norm = 0.0;
    for (int octave = 0; octave < 5; ++octave)
    {
        // Fades the last octave in rather than switching it, so the count can
        // be tuned continuously.
        float active = clamp (octaves - float (octave), 0.0, 1.0);
        if (active <= 0.0) break;

        float ridge = 1.0 - abs (2.0 * valueNoise (p) - 1.0);
        sum += ridge * amp * active;
        norm += amp * active;
        p *= lacunarity;
        amp *= gain;
    }

    return pow (sum / max (norm, 0.0001), sharpness);
}

// The net inside the sphere and along the beams.
/** The net on the sphere. Both of its coordinates are the room's: the bearing
 *  says which filament, and how far the direction lies from straight up says
 *  how far along it. At the identity camera that second one is the screen
 *  distance it used to be, exactly -- screenToDirection builds z as
 *  sqrt(1 - dist^2), so sqrt(1 - z^2) gives the distance back. */
float innerNetAt (vec2 bearing, float radial)
{
    return netFilaments (bearing, radial, uNetFlow, 1.6, uNetTwist,
                         uNetScale, uNetSharpness, uNetOctaves,
                         uNetLacunarity, uNetGain);
}

float innerNet (vec3 dir)
{
    return innerNetAt (netBearing (dir),
                       sqrt (max (0.0, 1.0 - dir.z * dir.z)));
}

/** The glow's net, running the other way and reaching to the screen edge.
 *
 *  Its bearing is the room's like the inner one, but how far along stays the
 *  *screen* distance: these filaments come out from behind the sphere and run
 *  to the corner of the display, which is a thing about the display and has no
 *  direction in the room to be measured against. */
float glowNet (vec3 rimDir, float dist)
{
    return netFilaments (netBearing (rimDir), dist, uGlowFlow, uGlowReach,
                         uGlowTwist, uGlowScale, uGlowSharpness,
                         uGlowOctaves, uGlowLacunarity, uGlowGain);
}

// ─── main ───────────────────────────────────────────────────────
void main ()
{
    vec2 pxCoord = vUV * uResolution;
    vec2 uv = (pxCoord - uSphereCentre) / uSphereRadius;

    vec2 uvScene = uv;
    float dist = length (uvScene);

    vec3 col = vec3 (0.0);
    float alpha = 1.0;

    // AA blend zone: 2 pixels wide in sphere-space for crisp but smooth edge
    float aaWidth = 2.0 / uSphereRadius;
    float surfaceMix = smoothstep (1.0 + aaWidth, 1.0 - aaWidth, dist);

    // ── Outside sphere contribution ─────────────────────────────
    vec3 colOut = vec3 (0.0);
    if (dist > 1.0 - aaWidth)
    {
        // The sphere's own glow, driven by the subwoofer (/vu/4). Filaments
        // coming out from behind the sphere and running to the edge of the
        // screen — the outward counterpart to the net inside, which runs in.
        // Modulated by the energy arriving from this direction, so the spread
        // outside continues what lands inside.
        vec2 band = beamTotal (uvScene);

        // Only the bolts hide anything. They are thin, so the glow behind
        // them stays visible between them rather than sitting under a veil.
        // Mirrors glowVisibility() in EnergyMap.cc.
        float showGlow = 1.0 - clamp (band.x * uBeamCover, 0.0, 1.0);

        if (uGlowLevel > 0.001 && showGlow > 0.0)
        {
            vec3 rimDirection = screenToDirection (uvScene / max (dist, 0.001),
                                                   1.0);
            colOut += uGlowColour * uGlowLevel
                    * glowNet (rimDirection, dist)
                    * glowEmergence (dist, uGlowRise)
                    * energyAt (rimDirection) * uGlowIntensity * showGlow;
        }

        // Speaker bands wrapping the sphere
        colOut += uSpotColour * band.y * uBeamIntensity;

        // The core runs white, the way a bolt does against a sky.
        colOut += uBoltCoreColour * band.y * uBoltCore;


        // Outside, the net rides the band towards the sphere, so a filament is
        // already visible before it crosses the rim. The same bearing the
        // inner net has at the rim, so the filament does not step sideways as
        // it crosses -- and the screen distance to carry it, because out here
        // there is no room direction to measure along.
        colOut += uSpotColour
                * innerNetAt (netBearing (screenToDirection (
                                  uvScene / max (dist, 0.001), 1.0)),
                              dist)
                * band.x * uNetBeamIntensity;

        // Blob outside glow removed — blobs only create reflections on sphere surface
    }

    // ── On the sphere surface contribution ──────────────────────
    vec3 colSurf = vec3 (0.0);
    if (dist < 1.0 + aaWidth)
    {
        vec3 N;
        sphereIntersect (uvScene, N);

        colSurf = uSphereSurface;

        // Fresnel rim
        float fresnel = 1.0 - N.z;
        fresnel = fresnel * fresnel * fresnel;
        colSurf += uSphereRim * 0.35 * fresnel;

        // Fake env reflection (simplified)
        colSurf += uSphereEnvironment * fresnel * 0.6;

        // Blob lighting on surface (combined diffuse + specular, single loop)
        vec3 viewDir = vec3 (0.0, 0.0, 1.0);
        for (int b = 0; b < 4; b++)
        {
            if (float(b) >= uNumBlobs) break;
            vec4 bps = getBlobPosSize (b);
            vec3 bcol = getBlobCol (b);
            if (bps.z < 0.001) continue;

            vec2 blobPos = bps.xy;

            vec3 bld = normalize (vec3 (blobPos - uvScene, 0.5));
            float diff = max (dot (N, bld), 0.0);
            float bd = length (blobPos - uvScene);
            float att = 1.0 / (1.0 + bd * 2.0);
            float inten = 0.3 + bps.w * 0.7;

            // Diffuse
            colSurf += bcol * diff * att * inten * 0.5;

            // Specular (pow 32 via squaring)
            vec3 bh = normalize (bld + viewDir);
            float sp = max (dot (N, bh), 0.0);
            sp = sp * sp; sp = sp * sp; sp = sp * sp; sp = sp * sp; sp = sp * sp; // ^32
            colSurf += bcol * sp * att * inten * 0.5;
        }

        // The direction this pixel stands for, in the room's own terms.
        vec3 dir = screenToDirection (uvScene, dist);

        // The graticule: circles of equal height and lines of equal bearing,
        // every thirty degrees. It used to be a net on the *screen* normal --
        // planes of constant N.x, N.y and N.z -- which draws the same picture
        // whichever way the room is being looked at, and that picture happens
        // to read as a globe seen edge-on. Looking straight down at a room,
        // what says so is rings around the zenith and spokes out of it.
        float lat = asin (clamp (dir.z, -1.0, 1.0));
        float lon = atan (dir.y, dir.x);

        float step30 = 0.52359878;                    // thirty degrees
        float latD = abs (fract (lat / step30 + 0.5) - 0.5) * step30;
        // Weighted by how far round the sphere is at this height: without it
        // the spokes crowd into a blot at the pole, which is the middle of
        // the picture in the view this device is usually in.
        float lonD = abs (fract (lon / step30 + 0.5) - 0.5) * step30
                   * max (cos (lat), 0.02);

        float wf = 1.0 - smoothstep (0.004, 0.014, min (latD, lonD));
        colSurf += vec3 (wf * 0.08 * (1.0 - fresnel * 0.8));

        // Energy arriving from the direction this pixel stands for. This is
        // the whole field, not four loudspeakers, so it carries height as well.
        float energy = energyAt (dir);

        colSurf += uEnergyColour * energy * uEnergyIntensity;

        // The beams stop at the rim. Inside, the net takes over from them:
        // filaments landing where a beam meets the sphere and running in from
        // there, strongest at the rim and thinning out as they travel.
        float net = innerNet (dir);
        vec3 rimDir = screenToDirection (uvScene / max (dist, 0.001), 1.0);

        colSurf += uEnergyColour * net * energyAt (rimDir) * dist * dist
                 * uNetIntensity;
    }

    // Blend outside and surface with smooth AA transition
    // Outside: start with solid background, add glow/beams on top
    vec3 colOutFinal = uBgColour + colOut;
    col = mix (colOutFinal, colSurf, surfaceMix);

    // The blobs go on last and additively. Last, because they are light and
    // light does not get occluded by the glass it shines through -- the sphere
    // is semi-transparent and a blob behind it is dimmed by its own depth fade
    // rather than by being drawn under something.
    // ── the cabinets ────────────────────────────────────────────
    //
    // After the ball, because they are solid and it is glass: one in front
    // covers it outright, one behind shows through dimmed the way anything
    // behind the ball does. The blobs stay last -- they are light, and light
    // is not occluded by what it shines through.
    float boxOpaque = 0.0;
    {
        float boxDepth;
        vec4 box = speakerBoxes (uvScene, boxDepth);
        if (box.a > 0.0)
        {
            // Where the ball is, and how far along the ray. The ray falls from
            // z = 4, so the near side of the unit sphere is that much less its
            // own height.
            float ballDepth = surfaceMix > 0.0
                            ? 4.0 - sqrt (max (0.0, 1.0 - dist * dist))
                            : 1000.0;

            float inFront = step (boxDepth, ballDepth);
            float through = mix (0.30, 1.0, inFront);

            col = mix (col, box.rgb, box.a * through);
            // Carried past the sphere's own alpha, which is assigned further
            // down and would otherwise wipe this out.
            boxOpaque = box.a * inFront;
        }
    }

    vec3 blobs = vec3 (0.0);
    for (int b = 0; b < 4; b++)
    {
        if (float(b) >= uNumBlobs) break;
        blobs += lineGlow (uvScene, b);
        blobs += blobLight (uvScene, b);
    }
    col += blobs;

    // Semi-transparent sphere: alpha < 1 on the sphere surface so
    // blobs on the back side remain partially visible through it.
    // Outside the sphere is fully opaque background.
    float sphereAlpha = 0.75;  // sphere surface transparency
    alpha = mix (1.0, sphereAlpha, surfaceMix);

    // A cabinet in front of the ball is solid: the glass behind it is not seen
    // through it.
    alpha = clamp (alpha + boxOpaque, 0.0, 1.0);

    // But light is not seen *through*. The sphere's alpha is what lets a blob
    // on the far side show at all, and it was also quietly taking a quarter
    // off every blob on the near one: a core computed at full came out of the
    // blend at 0.75 -- a grey dot where a bright one was meant to be. Where a
    // blob is bright the pixel belongs to the blob.
    alpha = clamp (alpha + max (blobs.r, max (blobs.g, blobs.b)), 0.0, 1.0);

    gl_FragColor = vec4 (col, alpha);
}
)";
}

// ─────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────
bool
SphereShader::initialise (juce::OpenGLContext &context)
{
  using namespace juce::gl;

  _shader = std::make_unique<juce::OpenGLShaderProgram> (context);

  if (!_shader->addVertexShader (getVertexShader ()))
    {
      juce::Logger::writeToLog (
          "SphereShader vertex error: " + _shader->getLastError ());
      _shader.reset ();
      return false;
    }

  if (!_shader->addFragmentShader (getFragmentShader ()))
    {
      juce::Logger::writeToLog (
          "SphereShader fragment error: " + _shader->getLastError ());
      _shader.reset ();
      return false;
    }

  if (!_shader->link ())
    {
      juce::Logger::writeToLog (
          "SphereShader link error: " + _shader->getLastError ());
      _shader.reset ();
      return false;
    }

  auto pid = _shader->getProgramID ();
  _uResolution    = glGetUniformLocation (pid, "uResolution");
  _uSphereRadius  = glGetUniformLocation (pid, "uSphereRadius");
  _uSphereCentre  = glGetUniformLocation (pid, "uSphereCentre");
  _uGlowLevel     = glGetUniformLocation (pid, "uGlowLevel");
  _uGlowColour    = glGetUniformLocation (pid, "uGlowColour");
  _uGlowFlow = glGetUniformLocation (pid, "uGlowFlow");
  _uGlowReach = glGetUniformLocation (pid, "uGlowReach");
  _uGlowRise = glGetUniformLocation (pid, "uGlowRise");
  _uGlowTwist = glGetUniformLocation (pid, "uGlowTwist");
  _uGlowScale = glGetUniformLocation (pid, "uGlowScale");
  _uGlowSharpness = glGetUniformLocation (pid, "uGlowSharpness");
  _uGlowOctaves = glGetUniformLocation (pid, "uGlowOctaves");
  _uGlowLacunarity = glGetUniformLocation (pid, "uGlowLacunarity");
  _uGlowGain = glGetUniformLocation (pid, "uGlowGain");
  _uGlowIntensity = glGetUniformLocation (pid, "uGlowIntensity");
  _uBgColour      = glGetUniformLocation (pid, "uBgColour");
  _uSphereSurface = glGetUniformLocation (pid, "uSphereSurface");
  _uSphereRim     = glGetUniformLocation (pid, "uSphereRim");
  _uSphereEnvironment = glGetUniformLocation (pid, "uSphereEnvironment");
  _uBoltCoreColour = glGetUniformLocation (pid, "uBoltCoreColour");
  _uSpotLevel[0]  = glGetUniformLocation (pid, "uSpotLevel0");
  _uSpotLevel[1]  = glGetUniformLocation (pid, "uSpotLevel1");
  _uSpotLevel[2]  = glGetUniformLocation (pid, "uSpotLevel2");
  _uSpotLevel[3]  = glGetUniformLocation (pid, "uSpotLevel3");
  _uSpotColour    = glGetUniformLocation (pid, "uSpotColour");
  _uSpeakerRadius = glGetUniformLocation (pid, "uSpeakerRadius");
  _uBeamEdge      = glGetUniformLocation (pid, "uBeamEdge");
  _uBeamIntensity = glGetUniformLocation (pid, "uBeamIntensity");
  _uApertureAngle = glGetUniformLocation (pid, "uApertureAngle");
  _uWrapAngle = glGetUniformLocation (pid, "uWrapAngle");
  _uWander = glGetUniformLocation (pid, "uWander");
  _uWanderTwist = glGetUniformLocation (pid, "uWanderTwist");
  _uWanderScale = glGetUniformLocation (pid, "uWanderScale");
  _uWanderFlow = glGetUniformLocation (pid, "uWanderFlow");
  _uBeamRoot = glGetUniformLocation (pid, "uBeamRoot");
  _uBeamFray = glGetUniformLocation (pid, "uBeamFray");
  _uBeamCover = glGetUniformLocation (pid, "uBeamCover");
  _uBoltWidth = glGetUniformLocation (pid, "uBoltWidth");
  _uBoltWander = glGetUniformLocation (pid, "uBoltWander");
  _uBoltScale = glGetUniformLocation (pid, "uBoltScale");
  _uBoltFlow = glGetUniformLocation (pid, "uBoltFlow");
  _uBoltRate = glGetUniformLocation (pid, "uBoltRate");
  _uBoltDuty = glGetUniformLocation (pid, "uBoltDuty");
  _uBoltCoreExp = glGetUniformLocation (pid, "uBoltCoreExp");
  _uBoltCore = glGetUniformLocation (pid, "uBoltCore");
  _uBoltCount = glGetUniformLocation (pid, "uBoltCount");
  _uBoltReach = glGetUniformLocation (pid, "uBoltReach");
  _uBoltEscape = glGetUniformLocation (pid, "uBoltEscape");
  _uBoltBranches = glGetUniformLocation (pid, "uBoltBranches");
  _uBoltBranch = glGetUniformLocation (pid, "uBoltBranch");
  _uBeamBleed = glGetUniformLocation (pid, "uBeamBleed");
  _uBeamFloor = glGetUniformLocation (pid, "uBeamFloor");
  _uMouthOffset   = glGetUniformLocation (pid, "uMouthOffset");
  _uEnergyMap       = glGetUniformLocation (pid, "uEnergyMap");
  _uEnergyColour    = glGetUniformLocation (pid, "uEnergyColour");
  _uEnergyIntensity = glGetUniformLocation (pid, "uEnergyIntensity");
  _uCamera          = glGetUniformLocation (pid, "uCamera");
  _uNetIntensity    = glGetUniformLocation (pid, "uNetIntensity");
  _uNetScale        = glGetUniformLocation (pid, "uNetScale");
  _uNetSharpness    = glGetUniformLocation (pid, "uNetSharpness");
  _uNetFlow         = glGetUniformLocation (pid, "uNetFlow");
  _uNetBeamIntensity = glGetUniformLocation (pid, "uNetBeamIntensity");
  _uNetGain = glGetUniformLocation (pid, "uNetGain");
  _uNetLacunarity = glGetUniformLocation (pid, "uNetLacunarity");
  _uNetOctaves = glGetUniformLocation (pid, "uNetOctaves");
  _uNetTwist = glGetUniformLocation (pid, "uNetTwist");
  _uTime            = glGetUniformLocation (pid, "uTime");
  _uNumBlobs      = glGetUniformLocation (pid, "uNumBlobs");

  _uBlobPosSize[0] = glGetUniformLocation (pid, "uBlobPosSize0");
  _uBlobPosSize[1] = glGetUniformLocation (pid, "uBlobPosSize1");
  _uBlobPosSize[2] = glGetUniformLocation (pid, "uBlobPosSize2");
  _uBlobPosSize[3] = glGetUniformLocation (pid, "uBlobPosSize3");
  _uBlobCol[0]     = glGetUniformLocation (pid, "uBlobCol0");
  _uBlobCol[1]     = glGetUniformLocation (pid, "uBlobCol1");
  _uBlobCol[2]     = glGetUniformLocation (pid, "uBlobCol2");
  _uBlobCol[3]     = glGetUniformLocation (pid, "uBlobCol3");
  _uBlobState[0]  = glGetUniformLocation (pid, "uBlobState0");
  _uBlobState[1]  = glGetUniformLocation (pid, "uBlobState1");
  _uBlobState[2]  = glGetUniformLocation (pid, "uBlobState2");
  _uBlobState[3]  = glGetUniformLocation (pid, "uBlobState3");
  _uBlobTrailA[0] = glGetUniformLocation (pid, "uBlobTrailA0");
  _uBlobTrailA[1] = glGetUniformLocation (pid, "uBlobTrailA1");
  _uBlobTrailA[2] = glGetUniformLocation (pid, "uBlobTrailA2");
  _uBlobTrailA[3] = glGetUniformLocation (pid, "uBlobTrailA3");
  _uBlobTrailB[0] = glGetUniformLocation (pid, "uBlobTrailB0");
  _uBlobTrailB[1] = glGetUniformLocation (pid, "uBlobTrailB1");
  _uBlobTrailB[2] = glGetUniformLocation (pid, "uBlobTrailB2");
  _uBlobTrailB[3] = glGetUniformLocation (pid, "uBlobTrailB3");
  _uBlobTrailC[0] = glGetUniformLocation (pid, "uBlobTrailC0");
  _uBlobTrailC[1] = glGetUniformLocation (pid, "uBlobTrailC1");
  _uBlobTrailC[2] = glGetUniformLocation (pid, "uBlobTrailC2");
  _uBlobTrailC[3] = glGetUniformLocation (pid, "uBlobTrailC3");
  _uBlobTrailD[0] = glGetUniformLocation (pid, "uBlobTrailD0");
  _uBlobTrailD[1] = glGetUniformLocation (pid, "uBlobTrailD1");
  _uBlobTrailD[2] = glGetUniformLocation (pid, "uBlobTrailD2");
  _uBlobTrailD[3] = glGetUniformLocation (pid, "uBlobTrailD3");
  _uActionColour  = glGetUniformLocation (pid, "uActionColour");
  _uBlobEffects   = glGetUniformLocation (pid, "uBlobEffects");
  _uLineMap[0]    = glGetUniformLocation (pid, "uLineMap0");
  _uLineMap[1]    = glGetUniformLocation (pid, "uLineMap1");
  _uLineMap[2]    = glGetUniformLocation (pid, "uLineMap2");
  _uLineMap[3]    = glGetUniformLocation (pid, "uLineMap3");
  _uLineOn        = glGetUniformLocation (pid, "uLineOn");
  _uLineExtent    = glGetUniformLocation (pid, "uLineExtent");
  _uLineEffects   = glGetUniformLocation (pid, "uLineEffects");
  _uBraid         = glGetUniformLocation (pid, "uBraid");
  _uSpkDir[0]     = glGetUniformLocation (pid, "uSpkDir0");
  _uSpkDir[1]     = glGetUniformLocation (pid, "uSpkDir1");
  _uSpkDir[2]     = glGetUniformLocation (pid, "uSpkDir2");
  _uSpkDir[3]     = glGetUniformLocation (pid, "uSpkDir3");
  _uSpkCentre[0]  = glGetUniformLocation (pid, "uSpkCentre0");
  _uSpkCentre[1]  = glGetUniformLocation (pid, "uSpkCentre1");
  _uSpkCentre[2]  = glGetUniformLocation (pid, "uSpkCentre2");
  _uSpkCentre[3]  = glGetUniformLocation (pid, "uSpkCentre3");
  _uSpkNose[0]    = glGetUniformLocation (pid, "uSpkNose0");
  _uSpkNose[1]    = glGetUniformLocation (pid, "uSpkNose1");
  _uSpkNose[2]    = glGetUniformLocation (pid, "uSpkNose2");
  _uSpkNose[3]    = glGetUniformLocation (pid, "uSpkNose3");
  _uSpkSide[0]    = glGetUniformLocation (pid, "uSpkSide0");
  _uSpkSide[1]    = glGetUniformLocation (pid, "uSpkSide1");
  _uSpkSide[2]    = glGetUniformLocation (pid, "uSpkSide2");
  _uSpkSide[3]    = glGetUniformLocation (pid, "uSpkSide3");

  _aPos = glGetAttribLocation (pid, "aPos");

  createQuad ();
  return true;
}

void
SphereShader::shutdown ()
{
  deleteQuad ();
  _shader.reset ();
}

// ─────────────────────────────────────────────────────────────────
// Fullscreen quad geometry (no VAO – GL 2.1)
// ─────────────────────────────────────────────────────────────────
void
SphereShader::createQuad ()
{
  using namespace juce::gl;
  static const float quadVerts[] = { -1, -1, 1, -1, -1, 1, 1, 1 };
  glGenBuffers (1, &_vbo);
  glBindBuffer (GL_ARRAY_BUFFER, _vbo);
  glBufferData (GL_ARRAY_BUFFER, sizeof (quadVerts), quadVerts,
                GL_STATIC_DRAW);
  glBindBuffer (GL_ARRAY_BUFFER, 0);
}

void
SphereShader::deleteQuad ()
{
  using namespace juce::gl;
  if (_vbo)
    {
      glDeleteBuffers (1, &_vbo);
      _vbo = 0;
    }
}

// ─────────────────────────────────────────────────────────────────
// Draw
// ─────────────────────────────────────────────────────────────────
void
SphereShader::draw (int viewportWidth, int viewportHeight,
                    float sphereRadius, float sphereCentreX,
                    float sphereCentreY)
{
  using namespace juce::gl;

  if (!_shader)
    return;

  _shader->use ();

  if (_uResolution >= 0)
    glUniform2f (_uResolution, float (viewportWidth), float (viewportHeight));
  if (_uSphereRadius >= 0)
    glUniform1f (_uSphereRadius, sphereRadius);
  if (_uSphereCentre >= 0)
    glUniform2f (_uSphereCentre, sphereCentreX, sphereCentreY);

  // Sphere corona — rms only. Mixing the peak in was what made the old
  // background glow flicker, and the corona is a mood, not a transient.
  {
    float s = speakerLightLevel (_glowRms, _glowCfg.vuMax, _glowCfg.curve);
    if (_uGlowLevel >= 0)     glUniform1f (_uGlowLevel, s);
    if (_uGlowColour >= 0)    glUniform3f (_uGlowColour, _glowCfg.r, _glowCfg.g, _glowCfg.b);
    if (_uGlowFlow >= 0) glUniform1f (_uGlowFlow, _glowCfg.netFlow);
    if (_uGlowReach >= 0) glUniform1f (_uGlowReach, _glowCfg.netReach);
    if (_uGlowRise >= 0) glUniform1f (_uGlowRise, _glowCfg.netRise);
    if (_uGlowTwist >= 0) glUniform1f (_uGlowTwist, _glowCfg.netTwist);
    if (_uGlowScale >= 0) glUniform1f (_uGlowScale, _glowCfg.netScale);
    if (_uGlowSharpness >= 0) glUniform1f (_uGlowSharpness, _glowCfg.netSharpness);
    if (_uGlowOctaves >= 0) glUniform1f (_uGlowOctaves, _glowCfg.netOctaves);
    if (_uGlowLacunarity >= 0) glUniform1f (_uGlowLacunarity, _glowCfg.netLacunarity);
    if (_uGlowGain >= 0) glUniform1f (_uGlowGain, _glowCfg.netGain);
    if (_uGlowIntensity >= 0) glUniform1f (_uGlowIntensity, _glowCfg.intensity);
  }

  // The colours the sphere is made of, straight from the theme. Set every
  // frame, so a skin reload reaches the shader without a restart — the same
  // path the tuning uniforms above already take.
  setThemeUniform (_uBgColour, theme ().background);
  setThemeUniform (_uSphereSurface, theme ().sphereSurface);
  setThemeUniform (_uSphereRim, theme ().sphereRim);
  setThemeUniform (_uSphereEnvironment, theme ().sphereEnvironment);
  setThemeUniform (_uBoltCoreColour, theme ().boltCore);

  // Speaker beams. The band's shape no longer depends on level — that drives
  // brightness alone now.
  for (int i = 0; i < 4; ++i)
    {
      float s = speakerLightLevel (_spotRms[i], _spotCfg.vuMax, _spotCfg.curve);
      if (_uSpotLevel[i] >= 0) glUniform1f (_uSpotLevel[i], s);
    }
  if (_uSpotColour >= 0)
    glUniform3f (_uSpotColour, _spotCfg.r, _spotCfg.g, _spotCfg.b);
  if (_uSpeakerRadius >= 0)
    glUniform1f (_uSpeakerRadius, _spotCfg.speakerRadius);

  uploadSpeakerFrames ();
  if (_uBeamEdge >= 0)
    glUniform1f (_uBeamEdge, _spotCfg.edgeSoftness);
  if (_uBeamIntensity >= 0)
    glUniform1f (_uBeamIntensity, _spotCfg.beamIntensity);
  if (_uApertureAngle >= 0) glUniform1f (_uApertureAngle, _spotCfg.apertureAngle);
  if (_uWrapAngle >= 0) glUniform1f (_uWrapAngle, _spotCfg.wrapAngle);
  if (_uWander >= 0) glUniform1f (_uWander, _spotCfg.wander);
  if (_uWanderTwist >= 0) glUniform1f (_uWanderTwist, _spotCfg.wanderTwist);
  if (_uWanderScale >= 0) glUniform1f (_uWanderScale, _spotCfg.wanderScale);
  if (_uWanderFlow >= 0) glUniform1f (_uWanderFlow, _spotCfg.wanderFlow);
  if (_uBeamRoot >= 0) glUniform1f (_uBeamRoot, _spotCfg.root);
  if (_uBeamFloor >= 0) glUniform1f (_uBeamFloor, _spotCfg.levelFloor);
  if (_uBeamBleed >= 0) glUniform1f (_uBeamBleed, _spotCfg.bleed);
  if (_uBeamFray >= 0) glUniform1f (_uBeamFray, _spotCfg.fray);
  if (_uBeamCover >= 0) glUniform1f (_uBeamCover, _spotCfg.cover);
  if (_uBoltWidth >= 0) glUniform1f (_uBoltWidth, _spotCfg.boltWidth);
  if (_uBoltWander >= 0) glUniform1f (_uBoltWander, _spotCfg.boltWander);
  if (_uBoltScale >= 0) glUniform1f (_uBoltScale, _spotCfg.boltScale);
  if (_uBoltFlow >= 0) glUniform1f (_uBoltFlow, _spotCfg.boltFlow);
  if (_uBoltRate >= 0) glUniform1f (_uBoltRate, _spotCfg.boltRate);
  if (_uBoltDuty >= 0) glUniform1f (_uBoltDuty, _spotCfg.boltDuty);
  if (_uBoltCoreExp >= 0) glUniform1f (_uBoltCoreExp, _spotCfg.boltCoreExp);
  if (_uBoltCore >= 0) glUniform1f (_uBoltCore, _spotCfg.boltCore);
  if (_uBoltCount >= 0) glUniform1f (_uBoltCount, _spotCfg.boltCount);
  if (_uBoltReach >= 0) glUniform1f (_uBoltReach, _spotCfg.boltReach);
  if (_uBoltEscape >= 0) glUniform1f (_uBoltEscape, _spotCfg.boltEscape);
  if (_uBoltBranches >= 0) glUniform1f (_uBoltBranches, _spotCfg.boltBranches);
  if (_uBoltBranch >= 0) glUniform1f (_uBoltBranch, _spotCfg.boltBranch);
  if (_uMouthOffset >= 0)
    glUniform1f (_uMouthOffset, speakerMouthOffset);

  // Energy map and net
  if (_uEnergyColour >= 0)
    glUniform3f (_uEnergyColour, _energyCfg.r, _energyCfg.g, _energyCfg.b);
  if (_uEnergyIntensity >= 0)
    glUniform1f (_uEnergyIntensity, _energyCfg.intensity);
  if (_uCamera >= 0)
    glUniform2f (_uCamera, _camera.pitch, _camera.turn);
  if (_uNetIntensity >= 0)
    glUniform1f (_uNetIntensity, _energyCfg.netIntensity);
  if (_uNetScale >= 0)
    glUniform1f (_uNetScale, _energyCfg.netScale);
  if (_uNetSharpness >= 0)
    glUniform1f (_uNetSharpness, _energyCfg.netSharpness);
  if (_uNetFlow >= 0)
    glUniform1f (_uNetFlow, _energyCfg.netFlow);
  if (_uNetBeamIntensity >= 0)
    glUniform1f (_uNetBeamIntensity, _energyCfg.netBeamIntensity);
  if (_uNetGain >= 0)
    glUniform1f (_uNetGain, _energyCfg.netGain);
  if (_uNetLacunarity >= 0)
    glUniform1f (_uNetLacunarity, _energyCfg.netLacunarity);
  if (_uNetOctaves >= 0)
    glUniform1f (_uNetOctaves, _energyCfg.netOctaves);
  if (_uNetTwist >= 0)
    glUniform1f (_uNetTwist, _energyCfg.netTwist);
  if (_uTime >= 0)
    glUniform1f (_uTime, _time);

  if (_uEnergyMap >= 0)
    {
      glActiveTexture (GL_TEXTURE0);
      glBindTexture (GL_TEXTURE_2D, _energyTexture);
      glUniform1i (_uEnergyMap, 0);
    }

  // Blobs
  if (_uNumBlobs >= 0)
    glUniform1f (_uNumBlobs, static_cast<float> (_numBlobs));
  for (int i = 0; i < kMaxBlobs; ++i)
    {
      auto const &b = _blobs[i];
      float vuL = 0.f;
      if (b.visible)
        {
          float peak = b.vuPeak, rms = b.vuRms;
          vuL = std::max (rms, peak * 0.8f);
          vuL = std::clamp (vuL / 0.4f, 0.f, 1.f);
          vuL = std::pow (vuL, 0.6f);
        }
      if (_uBlobPosSize[i] >= 0)
        glUniform4f (_uBlobPosSize[i],
                     b.visible ? b.x : 999.f,
                     b.visible ? b.y : 999.f,
                     b.visible ? b.size : 0.f,
                     vuL);
      if (_uBlobCol[i] >= 0)
        glUniform3f (_uBlobCol[i], b.r, b.g, b.b);

      // The seed keeps four blobs from sparkling in step. From the index
      // rather than from a clock, so a channel's own flecks stay its own
      // across a restart instead of shuffling every time the app comes up.
      if (_uBlobState[i] >= 0)
        glUniform4f (_uBlobState[i], b.vuPeak, b.action,
                     1.7f + static_cast<float> (i) * 3.1f,
                     b.visible ? b.depthFade : 0.f);

      // An invisible blob's wake is collapsed onto the blob rather than left
      // where it last was: the shader gates the trail on how far the tail has
      // been left behind, and a stale tail would go on burning under a channel
      // that has stopped.
      auto const tx = [&b] (int k) { return b.visible ? b.trailX[k] : 999.f; };
      auto const ty = [&b] (int k) { return b.visible ? b.trailY[k] : 999.f; };
      if (_uBlobTrailA[i] >= 0)
        glUniform4f (_uBlobTrailA[i], tx (0), ty (0), tx (1), ty (1));
      if (_uBlobTrailB[i] >= 0)
        glUniform4f (_uBlobTrailB[i], tx (2), ty (2), tx (3), ty (3));
      if (_uBlobTrailC[i] >= 0)
        glUniform4f (_uBlobTrailC[i], tx (4), ty (4), tx (5), ty (5));
      if (_uBlobTrailD[i] >= 0)
        glUniform4f (_uBlobTrailD[i], tx (6), ty (6), tx (7), ty (7));
    }

  // The line maps, on units one to four -- nought is the energy map's.
  {
    float on[kMaxBlobs] = { 0.f, 0.f, 0.f, 0.f };
    for (int i = 0; i < kMaxBlobs; ++i)
      {
        if (_uLineMap[i] < 0)
          continue;
        glActiveTexture (GL_TEXTURE1 + static_cast<GLenum> (i));
        glBindTexture (GL_TEXTURE_2D, _lineTexture[i]);
        glUniform1i (_uLineMap[i], 1 + i);
        on[i] = _lineTexture[i] != 0 ? 1.f : 0.f;
      }
    glActiveTexture (GL_TEXTURE0);

    if (_uLineOn >= 0)
      glUniform4f (_uLineOn, on[0], on[1], on[2], on[3]);
    if (_uLineExtent >= 0)
      glUniform1f (_uLineExtent, _lineExtent);
    if (_uLineEffects >= 0)
      glUniform4f (_uLineEffects, theme ().lineGlow, theme ().lineFilament,
                   theme ().lineBolt, theme ().lineHeat);
    if (_uBraid >= 0)
      glUniform4f (_uBraid, theme ().braidTurns, theme ().braidSpin,
                   theme ().braidWeave, theme ().braidStrands);
  }

  setThemeUniform (_uActionColour, theme ().blobAction);
  if (_uBlobEffects >= 0)
    glUniform3f (_uBlobEffects, theme ().blobSparkle, theme ().blobBolt,
                 theme ().blobTrail);

  // Draw fullscreen quad (with alpha blending for semi-transparent sphere)
  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glBindBuffer (GL_ARRAY_BUFFER, _vbo);
  if (_aPos >= 0)
    {
      glEnableVertexAttribArray (GLuint (_aPos));
      glVertexAttribPointer (GLuint (_aPos), 2, GL_FLOAT, GL_FALSE,
                             2 * sizeof (float), nullptr);
    }

  glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);

  if (_aPos >= 0)
    glDisableVertexAttribArray (GLuint (_aPos));
  glBindBuffer (GL_ARRAY_BUFFER, 0);
  glUseProgram (0);
}

// ─────────────────────────────────────────────────────────────────
// Setters
// ─────────────────────────────────────────────────────────────────
void SphereShader::setSphereGlow (float peak, float rms)
{ _glowPeak = peak; _glowRms = rms; }

void SphereShader::setSpeakerLight (int i, float peak, float rms)
{ if (i >= 0 && i < 4) { _spotPeak[i] = peak; _spotRms[i] = rms; } }

void SphereShader::setBlob (int i, BlobData const &d)
{ if (i >= 0 && i < kMaxBlobs) _blobs[i] = d; }

void
SphereShader::uploadSpeakerFrames ()
{
  using namespace juce::gl;

  // Where the four of them stand, once per frame.
  //
  // Bearings first: the four screen diagonals beamTotal was built on, read
  // back into the room -- uv = (-d.y, d.x), so d = (uv.y, -uv.x, 0) -- which
  // keeps the numbering and the places exactly what they were, and
  // uSpotLevel0..3 still belonging to the same corners.
  static constexpr float k = 0.70710678f;
  static constexpr float bearings[kMaxBlobs][2]
      = { { k, k }, { k, -k }, { -k, -k }, { -k, k } };

  // How far *below* the horizon a cabinet stands, in radians.
  //
  // On the horizon they are geometrically right and unreadable: the device's
  // own view is from straight overhead, and from there a speaker beside you is
  // its top panel -- four grey diamonds, which is very nearly what the SVG
  // arrows they replaced were. Lifting them and aiming down at the listener
  // makes it worse, not better: the eye is then behind them and sees the back.
  // Set below the ear and angled up -- floor monitors around a listening
  // position -- the baffle turns towards the overhead view, so the drivers are
  // in sight from the view the device actually ships in, and the geometry is
  // still a room somebody could build.
  static constexpr float drop = 0.42f;

  auto const level = std::cos (drop);
  auto const sink = std::sin (drop);

  for (int i = 0; i < kMaxBlobs; ++i)
    {
      auto const bx = bearings[i][0];
      auto const by = bearings[i][1];

      auto const seen = [this] (float x, float y, float z) {
        return asSeenFrom (Pos::fromCartesian (x, y, z), _camera);
      };

      // The band leaving it is still a flat annulus and belongs to the ring on
      // the horizon, so its bearing comes from there rather than from the
      // sunken cabinet.
      auto const onRim = seen (bx, by, 0.f);
      auto const dir
          = juce::Point<float> (-onRim.y (), onRim.x ());
      auto const length = std::max (dir.getDistanceFromOrigin (), 1e-6f);

      auto const radius = _spotCfg.speakerRadius;
      auto const centre
          = seen (bx * level * radius, by * level * radius, -sink * radius);
      auto const nose = seen (-bx * level, -by * level, sink);
      // Along its width, taken from the bearing rather than from the world's
      // up: a cabinet angled steeply has a nose near the vertical, and a cross
      // product against up would collapse there.
      auto const side = seen (-by, bx, 0.f);

      if (_uSpkDir[i] >= 0)
        glUniform2f (_uSpkDir[i], dir.x / length, dir.y / length);
      if (_uSpkCentre[i] >= 0)
        glUniform3f (_uSpkCentre[i], centre.x (), centre.y (), centre.z ());
      if (_uSpkNose[i] >= 0)
        glUniform3f (_uSpkNose[i], nose.x (), nose.y (), nose.z ());
      if (_uSpkSide[i] >= 0)
        glUniform3f (_uSpkSide[i], side.x (), side.y (), side.z ());
    }
}

void SphereShader::setLineTexture (int channel, unsigned int textureID)
{
  if (channel >= 0 && channel < kMaxBlobs)
    _lineTexture[channel] = textureID;
}

void SphereShader::setNumBlobs (int n)
{ _numBlobs = std::min (n, kMaxBlobs); }

void SphereShader::setGlowConfig (GlowConfig const &c)
{ _glowCfg = c; }


void SphereShader::setEnergyConfig (EnergyConfig const &c)
{ _energyCfg = c; }

void SphereShader::setSpotlightConfig (SpotlightConfig const &c)
{ _spotCfg = c; }

} // namespace a3
