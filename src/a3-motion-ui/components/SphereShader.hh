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

namespace a3
{

/**
 * Full-scene 3D renderer via a fullscreen-quad fragment shader.
 * Raytraces: dark reflective sphere with head silhouette, 4 speaker
 * boxes, volumetric speaker light beams, and up to 4 channel blobs
 * as lit 3D spheres with VU corona glow.
 *
 * GLSL 1.20 compatible (GL 2.1 desktop on RPi 4 V3D).
 */
class SphereShader
{
public:
  static constexpr int kMaxBlobs = 4;

  SphereShader ();
  ~SphereShader ();

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
    float vuRms = 0.f;
    bool visible = false;
    bool grabbed = false;
    bool highlighted = false;
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
    float beamFalloff = 0.6f;
    float beamIntensity = 0.8f;
    float reach = 0.25f; // how far past the mouth the stub carries
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
    float boltWander = 0.55f;  // how far its path strays across the band
    float boltScale = 6.f;     // how quickly it strays with radius
    float boltFlow = 0.5f;     // how fast the path creeps
    float boltRate = 1.4f;     // how often a bolt strikes
    float boltDuty = 0.55f;    // and how much of the time it is dark
    float boltCoreExp = 5.f;   // how tight the white core is
    float boltCore = 0.9f;     // how bright it runs
    float arcBase = 0.35f;     // band brightness where no vein crosses
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
  };
  void setEnergyConfig (EnergyConfig const &cfg);

  /** Texture holding the equirectangular energy map, owned by the caller. */
  void setEnergyTexture (unsigned int textureID) { _energyTexture = textureID; }

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
  GLint _uBeamFalloff = -1;
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
  GLint _uArcBase = -1;
  GLint _uBoltWidth = -1;
  GLint _uBoltWander = -1;
  GLint _uBoltScale = -1;
  GLint _uBoltFlow = -1;
  GLint _uBoltRate = -1;
  GLint _uBoltDuty = -1;
  GLint _uBoltCoreExp = -1;
  GLint _uBoltCore = -1;
  GLint _uApertureHalf = -1;
  GLint _uMouthOffset = -1;
  GLint _uBeamReach = -1;

  GLint _uEnergyMap = -1;
  GLint _uEnergyColour = -1;
  GLint _uEnergyIntensity = -1;
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
  GLint _uNumBlobs = -1;
  // Note: blob disc + corona are drawn as 2D overlay by MotionComponent

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
