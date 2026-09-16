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
#include <a3-motion-ui/components/SphereProjection.hh>

namespace a3
{

/**
 * Full-scene 3D renderer via a fullscreen-quad fragment shader.
 *
 * Raytraces: a dark reflective sphere with a head silhouette; four speaker
 * cabinets standing in the room, facing the listener, each with a woofer and a
 * tweeter that light with what the speaker is being sent; and the 4 channel
 * blobs -- each a core, a rim, a VU corona, procedural sparks, a bolt on a
 * transient, a wake behind it and a neon ring while an action runs.
 *
 * Drawn flat over the top, not raytraced: the volumetric speaker bands, which
 * are a two-dimensional annulus in screen polar coordinates. They follow a
 * walk round the room and cannot follow a lean, and rebuilding them in three
 * dimensions is the next piece of this.
 *
 * This block has been wrong twice, both times in the same way. It claimed the
 * blobs were raytraced while a flat 2D ellipse over the top was what you
 * actually saw, and the 2D layer was removed to "uncover" a 3D one that had
 * never been written -- which cost the maintainer a sphere with no blobs on
 * it. Then it went on claiming "4 speaker boxes" for a whole session after
 * that paragraph was written, while the speakers were still four SVG arrows
 * pinned to the corners of the display.
 *
 * So, twice over: if you are about to remove a layer because this header says
 * something else draws it, make the replacement draw first and look at it.
 *
 * GLSL 1.20 compatible (GL 2.1 desktop).
 */
class SphereShader
{
public:
  static constexpr int kMaxBlobs = 4;

  SphereShader ();
  ~SphereShader ();

  /** Where the room is looked at from. Handed to the shader so the ball, its
   *  graticule and the energy it carries turn with the trajectories drawn
   *  over them -- a picture whose halves disagree about the view is worse
   *  than one that cannot turn at all. */
  void setCamera (SphereCamera const &camera) { _camera = camera; }
  SphereCamera getCamera () const { return _camera; }

  /** Call from newOpenGLContextCreated().  Returns true on success. */
  bool initialise (juce::OpenGLContext &context);

  /** Call from openGLContextClosing(). */
  void shutdown ();

  /**
   * Draw everything.  Must be called while the GL context is current.
   */
  void draw (int viewportWidth, int viewportHeight,
             float sphereRadius, float sphereCentreX, float sphereCentreY);

  // ── VU-driven lighting ─────────────────────────────────────────
  void setSphereGlow (float peak, float rms);
  void setSpeakerLight (int index, float peak, float rms);

  // ── Blob data (set each frame before draw) ─────────────────────
  struct BlobData
  {
    float x = 0.f, y = 0.f;          // normalised position (-1..1)
    float r = 0.f, g = 0.f, b = 0.f; // colour
    float size = 0.f;                 // radius in sphere-normalised units
    float vuPeak = 0.f;
    float corona = 1.9f;  // how far the corona reaches, in blob radii
    float vuRms = 0.f;
    /** How far an action has this channel, 0..1. Not whether a finger is down:
     *  the engine puts a clip's settings back when the accent's envelope has
     *  finished falling, and that is when the blob stops wearing it. */
    float action = 0.f;
    /** Dimmed on the back of the semi-transparent sphere, so depth reads as
     *  depth rather than as everything being equally bright. */
    float depthFade = 1.f;
    /** Where the blob has just been: four points that lag it, nearest first.
     *
     *  A wake, not a copy of the trajectory. The line under it already says
     *  exactly where the take goes; what is missing is the sense that
     *  something heavy is travelling along it, and a plume that cuts the
     *  corners says that where one tracing them would only be a second line.
     *  The lag is the caller's -- see MotionComponent::advanceBlobTrails(). */
    float trailX[8]{}, trailY[8]{};
    bool visible = false;
    bool grabbed = false;
  };
  void setBlob (int index, BlobData const &data);
  void setNumBlobs (int n);

  // Note: CoronaConfig now lives in MotionComponent (2D blob overlay)

  // ── Config ─────────────────────────────────────────────────────
  /** The sphere's own glow, driven by the subwoofer: filaments coming out
   *  from behind it and running to the edge of the screen.
   *
   *  It was a smooth halo hugging the rim before, and before that it lit the
   *  sphere's skin from within — which is the beams' and the net's territory
   *  now. `netFlow` is negative here: these run outwards, against the net
   *  inside. */
  struct GlowConfig
  {
    float r = 0.9f, g = 0.12f, b = 0.05f;
    float alphaMax = 0.6f;
    float vuMax = 0.2f;
    float curve = 0.4f;
    float intensity = 0.8f;
    float netFlow = -0.18f;
    float netReach = 2.6f;
    float netRise = 0.25f;
    float netTwist = 9.f;
    float netScale = 7.f;
    float netSharpness = 6.f;
    float netOctaves = 3.f;
    float netLacunarity = 2.f;
    float netGain = 0.5f;
    float attack = 0.05f;      // envelope, seconds
    float decay = 1.2f;
  };
  void setGlowConfig (GlowConfig const &cfg);

  struct SpotlightConfig
  {
    float r = 1.0f, g = 0.85f, b = 0.2f;
    float alphaMax = 0.35f;
    float vuMax = 0.2f;
    float curve = 0.4f;
    float speakerRadius = 1.55f;
    float edgeSoftness = 0.7f;
    float beamIntensity = 0.8f;
    float apertureAngle = 6.f; // half-angle where the band leaves the horn
    float wrapAngle = 45.f;    // and where it meets the sphere
    float wander = 14.f;       // degrees the centre line wanders
    float wanderTwist = 5.f;
    float wanderScale = 4.f;
    float wanderFlow = 0.08f;
    float root = 0.35f;        // density where it leaves the speaker
    float levelFloor = 0.25f;  // level the band never drops below
    float bleed = 0.22f;       // how far it reaches past the annulus
    float fray = 0.8f;         // how ragged its edge is
    float cover = 3.f;         // how strongly the band hides the glow
    float boltWidth = 0.9f;    // angular width of a bolt's core, degrees
    float boltThin = 0.3f;     // what a silent speaker's bolt is worth
    float beamGate = 0.004f;   // level below which the room is silent
    float boltWander = 0.55f;  // how far its path strays across the band
    float boltScale = 6.f;     // how quickly it strays with radius
    float boltFlow = 0.5f;     // how fast the path creeps
    float boltRate = 1.4f;     // how often a bolt strikes
    float boltDuty = 0.55f;    // and how much of the time it is dark
    float boltCoreExp = 5.f;   // how tight the white core is
    float boltCore = 0.9f;     // how bright it runs
    float boltCount = 6.f;     // bolts the loudest speaker draws
    float boltFewest = 2.f;    // bolts the quietest speaker keeps
    float boltDim = 0.45f;     // how far a quiet speaker dims
    float floorLevel = 1.f;    // how strongly the dance floor shows
    float floorThrough = 0.55f; // how much of it shows through the ball
    float floorDark = 0.38f;   // how far it darkens what is behind it
    float floorBeams = 0.7f;   // how strongly the beams cross it
    float boltReach = 2.4f;    // how far an escaping one carries
    float boltEscape = 0.55f;  // how many of them escape
    float boltBranches = 2.f;  // branches per bolt
    float boltBranch = 1.6f;   // how hard a branch leaves its trunk
  };
  void setSpotlightConfig (SpotlightConfig const &cfg);

  /** Energy arriving from each direction, from the IEM EnergyVisualizer, plus
   *  the fractal net drawn on top of it. */
  struct EnergyConfig
  {
    float r = 1.f, g = 1.f, b = 1.f;
    float intensity = 1.f;
    float netIntensity = 0.8f;
    float netScale = 6.f;
    float netSharpness = 8.f;
    float netFlow = 0.15f;
    float netBeamIntensity = 1.5f;
    float netTwist = 9.f;
    float netOctaves = 3.f;
    float netLacunarity = 2.f;
    float netGain = 0.5f;
    float attack = 0.05f;      // envelope, seconds
    float decay = 1.2f;
  };
  void setEnergyConfig (EnergyConfig const &cfg);

  /** Texture holding the equirectangular energy map, owned by the caller. */
  void setEnergyTexture (unsigned int textureID) { _energyTexture = textureID; }

  /** A picture of where one channel's trajectory is, owned by the caller, or
   *  0 for a channel with nothing playing.
   *
   *  A point is four uniforms; a curve of a thousand points is not, and
   *  without it a fragment has no way of knowing how far it is from the line.
   *  The map carries nearness -- 1 on the line, falling away from it -- and
   *  everything the trajectory glows with is built from that field: the glow
   *  itself, the filaments (which are contours of the field after it has been
   *  warped by noise, so they wander along the line and can never fold the
   *  way an offset copy of the curve does), and the bolts. */
  void setLineTexture (int channel, unsigned int textureID);
  /** How far the line map reaches, in sphere radii. */
  void setLineExtent (float extent) { _lineExtent = extent; }

  /** Seconds since start, for the net's drift. */
  void setTime (float seconds) { _time = seconds; }
  float getSpeakerRadius () const { return _spotCfg.speakerRadius; }

private:
  static juce::String getVertexShader ();
  static juce::String getFragmentShader ();

  void createQuad ();
  void deleteQuad ();

  std::unique_ptr<juce::OpenGLShaderProgram> _shader;

  GLuint _vbo = 0;

  // Uniform locations
  GLint _uResolution = -1;
  GLint _uSphereRadius = -1;
  GLint _uSphereCentre = -1;

  GLint _uGlowLevel = -1;
  GLint _uGlowColour = -1;
  GLint _uGlowFlow = -1;
  GLint _uGlowReach = -1;
  GLint _uGlowRise = -1;
  GLint _uGlowTwist = -1;
  GLint _uGlowScale = -1;
  GLint _uGlowSharpness = -1;
  GLint _uGlowOctaves = -1;
  GLint _uGlowLacunarity = -1;
  GLint _uGlowGain = -1;
  GLint _uGlowIntensity = -1;
  GLint _uBgColour = -1;

  GLint _uSpotLevel[4] = { -1, -1, -1, -1 };
  GLint _uSpotColour = -1;
  GLint _uSpeakerRadius = -1;
  GLint _uBeamEdge = -1;
  GLint _uBeamIntensity = -1;
  GLint _uApertureAngle = -1;
  GLint _uWrapAngle = -1;
  GLint _uWander = -1;
  GLint _uWanderTwist = -1;
  GLint _uWanderScale = -1;
  GLint _uWanderFlow = -1;
  GLint _uBeamRoot = -1;
  GLint _uBeamFloor = -1;
  GLint _uBeamBleed = -1;
  GLint _uBeamFray = -1;
  GLint _uBeamCover = -1;
  GLint _uBeamMinAnnulus = -1;
  GLint _uBeamDepthSoft = -1;
  GLint _uBeamGate = -1;
  GLint _uBoltWidth = -1;
  GLint _uBoltThin = -1;
  GLint _uBoltWander = -1;
  GLint _uBoltScale = -1;
  GLint _uBoltFlow = -1;
  GLint _uBoltRate = -1;
  GLint _uBoltDuty = -1;
  GLint _uBoltCoreExp = -1;
  GLint _uBoltCore = -1;
  GLint _uSphereSurface = -1;
  GLint _uSphereRim = -1;
  GLint _uSphereEnvironment = -1;
  GLint _uBoltCoreColour = -1;
  GLint _uBoltCount = -1;
  GLint _uBoltFewest = -1;
  GLint _uBoltDim = -1;
  GLint _uBoltReach = -1;
  GLint _uBoltEscape = -1;
  GLint _uBoltBranches = -1;
  GLint _uBoltBranch = -1;
  GLint _uMouthOffset = -1;

  /** Where the room is being looked at from. The overhead view is the
   *  default and costs nothing -- the shader short-circuits on it. */
  SphereCamera _camera = defaultCamera ();

  GLint _uEnergyMap = -1;
  GLint _uEnergyColour = -1;
  GLint _uEnergyIntensity = -1;
  GLint _uCamera = -1;
  GLint _uNetIntensity = -1;
  GLint _uNetScale = -1;
  GLint _uNetSharpness = -1;
  GLint _uNetFlow = -1;
  GLint _uNetBeamIntensity = -1;
  GLint _uNetTwist = -1;
  GLint _uNetOctaves = -1;
  GLint _uNetLacunarity = -1;
  GLint _uNetGain = -1;
  GLint _uTime = -1;

  // Blob uniforms (position+colour kept for lighting on sphere surface)
  GLint _uBlobPosSize[kMaxBlobs] = {};  // vec4: x, y, size, vuLevel
  GLint _uBlobCol[kMaxBlobs] = {};      // vec3: r, g, b
  GLint _uBlobState[kMaxBlobs] = {};
  GLint _uBlobCorona[kMaxBlobs] = {};    // vec4: vu, action, seed, depth
  // The wake, two points to a vec4 -- separate uniforms rather than one array
  // because a uniform array in GLSL 1.20 may only be indexed by a
  // constant-index-expression, and the blob index here is a function argument.
  GLint _uBlobTrailA[kMaxBlobs] = {};   // vec4: t0.xy, t1.xy
  GLint _uBlobTrailB[kMaxBlobs] = {};   // vec4: t2.xy, t3.xy
  GLint _uBlobTrailC[kMaxBlobs] = {};   // vec4: t4.xy, t5.xy
  GLint _uBlobTrailD[kMaxBlobs] = {};   // vec4: t6.xy, t7.xy
  GLint _uActionColour = -1;
  GLint _uBlobEffects = -1;

  GLint _uLineMap[kMaxBlobs] = {};
  GLint _uLineOn = -1;
  GLint _uLineExtent = -1;
  GLint _uLineFarSide = -1;
  GLint _uLineEffects = -1;
  GLint _uBraid = -1;

  GLint _uStackTop = -1;
  GLint _uStackSub = -1;
  GLint _uStackTopMid = -1;
  GLint _uStackSubMid = -1;
  GLint _uStackReach = -1;
  GLint _uStackSubCount = -1;
  GLint _uStackOne = -1;
  GLint _uStackSplay = -1;
  GLint _uFloorZ = -1;
  GLint _uFloorReach = -1;
  GLint _uFloorLevel = -1;
  GLint _uFloorThrough = -1;
  GLint _uFloorDark = -1;
  GLint _uFloorBeams = -1;
  GLint _uFloorGrazeDir = -1;
  GLint _uRoomUp = -1;
  GLint _uSpkSeed[kMaxBlobs] = {};
  GLint _uSpkCentre[kMaxBlobs] = {};
  GLint _uSpkNose[kMaxBlobs] = {};
  GLint _uSpkSide[kMaxBlobs] = {};
  /** Where the four cabinets stand as the eye sees them. Once a frame: it is
   *  the same answer for every pixel, and worked out per pixel it cost a
   *  quarter of a core in sines and cosines. */
  void uploadStackGeometry ();
  void uploadSpeakerFrames ();
  unsigned int _lineTexture[kMaxBlobs] = {};
  float _lineExtent = 1.3f;

  GLint _uNumBlobs = -1;

  GLint _aPos = -1;

  // CPU-side state
  float _glowPeak = 0.f, _glowRms = 0.f;
  float _spotPeak[4]{}, _spotRms[4]{};
  BlobData _blobs[kMaxBlobs];
  int _numBlobs = 0;

  GlowConfig _glowCfg;
  SpotlightConfig _spotCfg;
  EnergyConfig _energyCfg;
  unsigned int _energyTexture = 0;
  float _time = 0.f;
};

} // namespace a3
