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
 * Raytraced 3D sphere rendered via a fullscreen quad with a fragment
 * shader.  Features: dark reflective surface, Fresnel rim glow,
 * specular highlights, iso-sphere wireframe hint, head silhouette,
 * VU-driven sphere glow and 4 speaker spotlights.
 *
 * Compatible with OpenGL ES 3.1 / GLSL ES 3.10 (RPi 4 V3D).
 */
class SphereShader
{
public:
  SphereShader ();
  ~SphereShader ();

  /** Call from newOpenGLContextCreated().  Returns true on success. */
  bool initialise (juce::OpenGLContext &context);

  /** Call from openGLContextClosing(). */
  void shutdown ();

  /**
   * Draw the sphere.  Must be called while the GL context is current.
   *
   * @param viewportWidth   viewport width in pixels
   * @param viewportHeight  viewport height in pixels
   * @param sphereRadius    radius of the sphere in normalised coords
   *                        (0..1 fraction of the shorter viewport side)
   * @param sphereCentreX   centre X in normalised coords (0..1)
   * @param sphereCentreY   centre Y in normalised coords (0..1)
   */
  void draw (int viewportWidth, int viewportHeight,
             float sphereRadius, float sphereCentreX, float sphereCentreY);

  // ── VU-driven lighting uniforms ────────────────────────────────
  void setSphereGlow (float peak, float rms);
  void setSpeakerLight (int index, float peak, float rms);

  // ── Glow config (from config.json) ─────────────────────────────
  struct GlowConfig
  {
    float r = 0.9f, g = 0.12f, b = 0.05f;
    float alphaMax = 0.6f;
    float vuMax = 0.2f;
    float curve = 0.4f;
  };
  void setGlowConfig (GlowConfig const &cfg);

  struct SpotlightConfig
  {
    float r = 1.0f, g = 0.85f, b = 0.2f;
    float alphaMax = 0.35f;
    float vuMax = 0.2f;
    float curve = 0.4f;
  };
  void setSpotlightConfig (SpotlightConfig const &cfg);

private:
  static juce::String getVertexShader ();
  static juce::String getFragmentShader ();

  void createQuad ();
  void deleteQuad ();

  std::unique_ptr<juce::OpenGLShaderProgram> _shader;

  // Quad VBO
  GLuint _vbo = 0;

  // Uniform locations (cached after link)
  GLint _uResolution = -1;
  GLint _uSphereRadius = -1;
  GLint _uSphereCentre = -1;

  GLint _uGlowLevel = -1;
  GLint _uGlowColour = -1;

  GLint _uSpotLevel[4] = { -1, -1, -1, -1 };
  GLint _uSpotColour = -1;

  // Attribute location
  GLint _aPos = -1;

  // CPU-side state
  float _glowPeak = 0.f, _glowRms = 0.f;
  float _spotPeak[4]{}, _spotRms[4]{};

  GlowConfig _glowCfg;
  SpotlightConfig _spotCfg;
};

} // namespace a3
