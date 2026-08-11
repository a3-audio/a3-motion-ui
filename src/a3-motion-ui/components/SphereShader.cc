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

#include "SpeakerLightScaling.hh"

#include <cmath>

namespace a3
{

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

uniform float uSpotLevel0;
uniform float uSpotLevel1;
uniform float uSpotLevel2;
uniform float uSpotLevel3;
uniform vec3  uSpotColour;
uniform float uSpeakerRadius;
uniform float uBeamEdge;       // fraction of the half-width that stays flat
uniform float uBeamFalloff;
uniform float uBeamIntensity;
uniform float uApertureAngle;  // half-angle of the band where it leaves the horn
uniform float uWrapAngle;      // and where it meets the sphere — 45 closes the circle
uniform float uWander;         // degrees the centre line wanders
uniform float uWanderTwist;    // detail of the wander around the circle
uniform float uWanderScale;    // and along the radius
uniform float uWanderFlow;     // how fast it creeps
uniform float uBeamRoot;       // density where it leaves the speaker
uniform float uBeamFilament;   // how much of the beam is filament rather than solid
uniform float uApertureHalf;   // half-width of the horn's mouth
uniform float uMouthOffset;    // mouth position ahead of the speaker centre
uniform float uBeamReach;      // how far past the mouth the stub carries

// Energy arriving from each direction, folded into an equirectangular map by
// EnergyMap.cc from the IEM EnergyVisualizer's 426 points
uniform sampler2D uEnergyMap;
uniform vec3  uEnergyColour;
uniform float uEnergyIntensity;
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
uniform vec3  uBlobCol0;
uniform vec3  uBlobCol1;
uniform vec3  uBlobCol2;
uniform vec3  uBlobCol3;
uniform float uNumBlobs;

// ─── helpers ────────────────────────────────────────────────────

float sphereIntersect (vec2 uv, out vec3 normal)
{
    float d2 = dot (uv, uv);
    if (d2 > 1.0) return -1.0;
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

    return vec3 (uv.y, -uv.x, up);
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

// A beam is a band in the annulus between the horn's mouth and the sphere: it
// leaves the speaker narrow and opens to a quarter of the way round by the
// time it arrives, so the four of them close the circle. Its centre line
// wanders with fractal noise, which is what turns a band into a root.
// Mirrors beamWrapHalfAngle() in EnergyMap.cc.
//
// Needs valueNoise(), so it lives below it.
float beamDensity (vec2 point, vec2 spkDir, float level)
{
    if (level <= 0.001) return 0.0;

    float mouthR = uSpeakerRadius - uMouthOffset;
    float d = length (point);
    if (d > mouthR || d < 1.0) return 0.0;   // only in the annulus

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

    float across = 1.0 - smoothstep (halfWidth * uBeamEdge, halfWidth,
                                     abs (dA - wander));
    if (across <= 0.0) return 0.0;

    // Denser where it wraps the sphere than where it leaves the speaker.
    float grip = mix (uBeamRoot, 1.0, eased);

    return level * across * grip;
}

float beamTotal (vec2 p)
{
    return beamDensity (p, vec2 (-0.7071,  0.7071), uSpotLevel0)
         + beamDensity (p, vec2 ( 0.7071,  0.7071), uSpotLevel1)
         + beamDensity (p, vec2 ( 0.7071, -0.7071), uSpotLevel2)
         + beamDensity (p, vec2 (-0.7071, -0.7071), uSpotLevel3);
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
float netFilaments (vec2 uv, float dist, float flow, float reach,
                    float twist, float scale, float sharpness,
                    float octaves, float lacunarity, float gain)
{
    // Inverts netFilamentRadius() in EnergyMap.cc: the filament standing at
    // this radius now is the one that started further along the flow.
    float radial = clamp (dist, 0.0, reach) + uTime * flow;

    // Domain built from the direction, not from an angle. An angle wraps, and
    // the wrap left a seam due west where the filaments failed to meet.
    // Mirrors netDomainPoint() in EnergyMap.cc.
    float len = max (length (uv), 0.000001);
    vec2 n = uv / len;
    vec3 p = vec3 (n * twist, radial * scale);

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
float innerNet (vec2 uv, float dist)
{
    return netFilaments (uv, dist, uNetFlow, 1.6, uNetTwist, uNetScale,
                         uNetSharpness, uNetOctaves, uNetLacunarity, uNetGain);
}

// The beam's own filament texture, so it reads as part of the same weather
// rather than a solid wedge sitting next to it.
float beamTexture (vec2 uv, float dist)
{
    return mix (1.0, innerNet (uv, dist), uBeamFilament);
}

// The glow's net, running the other way and reaching to the screen edge.
float glowNet (vec2 uv, float dist)
{
    return netFilaments (uv, dist, uGlowFlow, uGlowReach, uGlowTwist,
                         uGlowScale, uGlowSharpness, uGlowOctaves,
                         uGlowLacunarity, uGlowGain);
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
        if (uGlowLevel > 0.001)
        {
            vec3 rimDirection = screenToDirection (uvScene / max (dist, 0.001),
                                                   1.0);
            colOut += uGlowColour * uGlowLevel * glowNet (uvScene, dist)
                    * glowEmergence (dist, uGlowRise)
                    * energyAt (rimDirection) * uGlowIntensity;
        }

        // Speaker beams on their way to the sphere, curling as they go
        colOut += uSpotColour * beamTotal (uvScene)
                * beamTexture (uvScene, dist) * uBeamIntensity;


        // Outside, the net rides the beams towards the sphere, so a filament
        // is already visible before it crosses the rim.
        colOut += uSpotColour * innerNet (uvScene, dist)
                * beamTotal (uvScene) * uNetBeamIntensity;

        // Blob outside glow removed — blobs only create reflections on sphere surface
    }

    // ── On the sphere surface contribution ──────────────────────
    vec3 colSurf = vec3 (0.0);
    if (dist < 1.0 + aaWidth)
    {
        vec3 N;
        sphereIntersect (uvScene, N);

        colSurf = vec3 (0.04, 0.04, 0.055);

        // Fresnel rim
        float fresnel = 1.0 - N.z;
        fresnel = fresnel * fresnel * fresnel;
        colSurf += vec3 (0.5, 0.55, 0.65) * 0.35 * fresnel;

        // Fake env reflection (simplified)
        colSurf += vec3 (0.02, 0.025, 0.03) * fresnel * 0.6;

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

        // Wireframe
        float gf = 8.0;
        float gl1 = abs (fract (N.x * gf + 0.5) - 0.5);
        float gl2 = abs (fract (N.y * gf + 0.5) - 0.5);
        float gl3 = abs (fract (N.z * gf + 0.5) - 0.5);
        float wf = 1.0 - smoothstep (0.01, 0.04, min (gl1, min (gl2, gl3)));
        colSurf += vec3 (wf * 0.08 * (1.0 - fresnel * 0.8));

        // Energy arriving from the direction this pixel stands for. This is
        // the whole field, not four loudspeakers, so it carries height as well.
        vec3 dir = screenToDirection (uvScene, dist);
        float energy = energyAt (dir);

        colSurf += uEnergyColour * energy * uEnergyIntensity;

        // The beams stop at the rim. Inside, the net takes over from them:
        // filaments landing where a beam meets the sphere and running in from
        // there, strongest at the rim and thinning out as they travel.
        float net = innerNet (uvScene, dist);
        vec3 rimDir = screenToDirection (uvScene / max (dist, 0.001), 1.0);

        colSurf += uEnergyColour * net * energyAt (rimDir) * dist * dist
                 * uNetIntensity;
    }

    // Blend outside and surface with smooth AA transition
    // Outside: start with solid background, add glow/beams on top
    vec3 colOutFinal = uBgColour + colOut;
    col = mix (colOutFinal, colSurf, surfaceMix);

    // Semi-transparent sphere: alpha < 1 on the sphere surface so
    // blobs on the back side remain partially visible through it.
    // Outside the sphere is fully opaque background.
    float sphereAlpha = 0.75;  // sphere surface transparency
    alpha = mix (1.0, sphereAlpha, surfaceMix);

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
  _uSpotLevel[0]  = glGetUniformLocation (pid, "uSpotLevel0");
  _uSpotLevel[1]  = glGetUniformLocation (pid, "uSpotLevel1");
  _uSpotLevel[2]  = glGetUniformLocation (pid, "uSpotLevel2");
  _uSpotLevel[3]  = glGetUniformLocation (pid, "uSpotLevel3");
  _uSpotColour    = glGetUniformLocation (pid, "uSpotColour");
  _uSpeakerRadius = glGetUniformLocation (pid, "uSpeakerRadius");
  _uBeamEdge      = glGetUniformLocation (pid, "uBeamEdge");
  _uBeamFalloff   = glGetUniformLocation (pid, "uBeamFalloff");
  _uBeamIntensity = glGetUniformLocation (pid, "uBeamIntensity");
  _uApertureAngle = glGetUniformLocation (pid, "uApertureAngle");
  _uWrapAngle = glGetUniformLocation (pid, "uWrapAngle");
  _uWander = glGetUniformLocation (pid, "uWander");
  _uWanderTwist = glGetUniformLocation (pid, "uWanderTwist");
  _uWanderScale = glGetUniformLocation (pid, "uWanderScale");
  _uWanderFlow = glGetUniformLocation (pid, "uWanderFlow");
  _uBeamRoot = glGetUniformLocation (pid, "uBeamRoot");
  _uBeamFilament  = glGetUniformLocation (pid, "uBeamFilament");
  _uApertureHalf  = glGetUniformLocation (pid, "uApertureHalf");
  _uMouthOffset   = glGetUniformLocation (pid, "uMouthOffset");
  _uBeamReach     = glGetUniformLocation (pid, "uBeamReach");
  _uEnergyMap       = glGetUniformLocation (pid, "uEnergyMap");
  _uEnergyColour    = glGetUniformLocation (pid, "uEnergyColour");
  _uEnergyIntensity = glGetUniformLocation (pid, "uEnergyIntensity");
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
  // Blob state + corona uniforms removed — blobs drawn as 2D overlay

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

  // Solid background colour (from LookAndFeel Colours::background)
  if (_uBgColour >= 0)
    {
      // 0xff292f36 → RGB normalized
      glUniform3f (_uBgColour, 0.161f, 0.184f, 0.212f);
    }

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
  if (_uBeamEdge >= 0)
    glUniform1f (_uBeamEdge, _spotCfg.edgeSoftness);
  if (_uBeamFalloff >= 0)
    glUniform1f (_uBeamFalloff, _spotCfg.beamFalloff);
  if (_uBeamIntensity >= 0)
    glUniform1f (_uBeamIntensity, _spotCfg.beamIntensity);
  if (_uApertureAngle >= 0) glUniform1f (_uApertureAngle, _spotCfg.apertureAngle);
  if (_uWrapAngle >= 0) glUniform1f (_uWrapAngle, _spotCfg.wrapAngle);
  if (_uWander >= 0) glUniform1f (_uWander, _spotCfg.wander);
  if (_uWanderTwist >= 0) glUniform1f (_uWanderTwist, _spotCfg.wanderTwist);
  if (_uWanderScale >= 0) glUniform1f (_uWanderScale, _spotCfg.wanderScale);
  if (_uWanderFlow >= 0) glUniform1f (_uWanderFlow, _spotCfg.wanderFlow);
  if (_uBeamRoot >= 0) glUniform1f (_uBeamRoot, _spotCfg.root);
  if (_uBeamFilament >= 0)
    glUniform1f (_uBeamFilament, _spotCfg.filament);
  if (_uApertureHalf >= 0)
    glUniform1f (_uApertureHalf, speakerApertureHalfWidth);
  if (_uMouthOffset >= 0)
    glUniform1f (_uMouthOffset, speakerMouthOffset);
  if (_uBeamReach >= 0)
    glUniform1f (_uBeamReach, _spotCfg.reach);

  // Energy map and net
  if (_uEnergyColour >= 0)
    glUniform3f (_uEnergyColour, _energyCfg.r, _energyCfg.g, _energyCfg.b);
  if (_uEnergyIntensity >= 0)
    glUniform1f (_uEnergyIntensity, _energyCfg.intensity);
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

    }

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

void SphereShader::setNumBlobs (int n)
{ _numBlobs = std::min (n, kMaxBlobs); }

void SphereShader::setGlowConfig (GlowConfig const &c)
{ _glowCfg = c; }


void SphereShader::setEnergyConfig (EnergyConfig const &c)
{ _energyCfg = c; }

void SphereShader::setSpotlightConfig (SpotlightConfig const &c)
{ _spotCfg = c; }

} // namespace a3
