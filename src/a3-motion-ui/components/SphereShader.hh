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
  /** The sphere's own corona: a halo outside it, driven by the subwoofer.
   *
   *  It used to light the sphere's skin from within, which is now the beams'
   *  territory — the sphere is translucent, so they pass into it. */
  struct GlowConfig
  {
    float r = 0.9f, g = 0.12f, b = 0.05f;
    float alphaMax = 0.6f;
    float vuMax = 0.2f;
    float curve = 0.4f;
    float falloff = 1.5f;
    float intensity = 0.8f;
  };
  void setGlowConfig (GlowConfig const &cfg);

  struct SpotlightConfig
  {
    float r = 1.0f, g = 0.85f, b = 0.2f;
    float alphaMax = 0.35f;
    float vuMax = 0.2f;
    float curve = 0.4f;
    float speakerRadius = 1.55f;
    float beamConeExp = 6.f;
    float beamFalloff = 0.6f;
    float beamIntensity = 0.8f;
    float widthStart = 0.1f;
    float widthEnd = 0.2929f; // 45 deg — neighbouring cones just touch
    float absorb = 1.5f;      // per unit travelled through the sphere
    float innerIntensity = 0.8f;
  };
  void setSpotlightConfig (SpotlightConfig const &cfg);
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
  GLint _uGlowFalloff = -1;
  GLint _uGlowIntensity = -1;
  GLint _uBgColour = -1;

  GLint _uSpotLevel[4] = { -1, -1, -1, -1 };
  GLint _uBeamTan[4] = { -1, -1, -1, -1 };
  GLint _uSpotColour = -1;
  GLint _uSpeakerRadius = -1;
  GLint _uBeamConeExp = -1;
  GLint _uBeamFalloff = -1;
  GLint _uBeamIntensity = -1;
  GLint _uApertureHalf = -1;
  GLint _uMouthOffset = -1;
  GLint _uBeamAbsorb = -1;
  GLint _uBeamInner = -1;

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
};

} // namespace a3
