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

uniform float uGlowLevel;
uniform vec3  uGlowColour;

uniform vec3  uBgGlowColour;
uniform float uBgGlowFalloff;
uniform float uBgGlowIntensity;

uniform float uSpotLevel0;
uniform float uSpotLevel1;
uniform float uSpotLevel2;
uniform float uSpotLevel3;
uniform vec3  uSpotColour;
uniform float uSpeakerRadius;
uniform float uBeamConeExp;
uniform float uBeamFalloff;
uniform float uBeamIntensity;

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

// ─── head silhouette SDF ────────────────────────────────────────

float headSDF (vec2 uv)
{
    float s = 0.6;
    vec2 p = uv / s;
    float cranium = length (p) - 0.38;
    float earL = length (p - vec2 (-0.39, 0.0)) - 0.09;
    float earR = length (p - vec2 ( 0.39, 0.0)) - 0.09;
    vec2 nd = abs (p - vec2 (0.0, 0.42)) - vec2 (0.045, 0.09);
    float nose = length (max (nd, 0.0)) + min (max (nd.x, nd.y), 0.0);
    return min (min (cranium, min (earL, earR)), nose) * s;
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

// ─── main ───────────────────────────────────────────────────────
void main ()
{
    vec2 pxCoord = vUV * uResolution;
    vec2 uv = (pxCoord - uSphereCentre) / uSphereRadius;
    float dist = length (uv);

    vec3 col = vec3 (0.0);
    float alpha = 1.0;

    // AA blend zone: 3 pixels wide in sphere-space
    float aaWidth = 3.0 / uSphereRadius;
    float surfaceMix = smoothstep (1.0 + aaWidth, 1.0 - aaWidth, dist);

    // ── Outside sphere contribution ─────────────────────────────
    vec3 colOut = vec3 (0.0);
    if (dist > 1.0 - aaWidth)
    {
        // Background glow (VU-driven from subwoofer /vu/4)
        if (uGlowLevel > 0.001)
        {
            float gf = 1.0 / (1.0 + (dist - 1.0) * uBgGlowFalloff);
            colOut += uBgGlowColour * uGlowLevel * gf * gf * uBgGlowIntensity;
        }

        // Speaker beams — rays FROM speaker positions towards sphere
        // Each speaker sits at speakerRadius along its diagonal.
        // The beam is a line segment from speaker to sphere edge;
        // pixel brightness = exp falloff from distance to that segment.
        float spkR = uSpeakerRadius;
        vec2 spkDir0 = vec2 (-0.7071, 0.7071); // 135°
        vec2 spkDir1 = vec2 ( 0.7071, 0.7071); // 45°
        vec2 spkDir2 = vec2 ( 0.7071,-0.7071); // 315°
        vec2 spkDir3 = vec2 (-0.7071,-0.7071); // 225°

        // For each speaker: project pixel onto segment, get perpendicular distance
        // Segment: from spkPos (at spkR) towards origin, ending at sphere edge (dist=1)
        float beamSum = 0.0;
        for (int s = 0; s < 4; s++)
        {
            vec2 sDir = (s == 0) ? spkDir0 : (s == 1) ? spkDir1
                      : (s == 2) ? spkDir2 : spkDir3;
            float sLvl = (s == 0) ? uSpotLevel0 : (s == 1) ? uSpotLevel1
                       : (s == 2) ? uSpotLevel2 : uSpotLevel3;
            if (sLvl < 0.001) continue;

            vec2 spkPos = sDir * spkR;
            vec2 toOrigin = -sDir;  // direction from speaker towards centre
            // Project pixel onto beam axis
            vec2 toPixel = uv - spkPos;
            float along = dot (toPixel, toOrigin);
            float segLen = spkR - 1.0; // speaker to sphere edge
            float t = clamp (along / segLen, 0.0, 1.0);
            vec2 closest = spkPos + toOrigin * along;
            float perpDist = length (uv - closest);

            // Beam width: narrow near sphere, wider near speaker
            float beamWidth = 0.04 + (1.0 - t) * 0.06;
            float beam = exp (-perpDist * perpDist / (beamWidth * beamWidth * 2.0));
            // Fade along beam: brightest near speaker, absorbed near sphere
            float alongFade = smoothstep (0.0, 0.15, t) * smoothstep (1.0, 0.7, t);
            beamSum += sLvl * beam * alongFade;
        }
        colOut += uSpotColour * beamSum * uBeamIntensity;

        // Blob outside glow
        vec2 dirN = uv / dist;
        for (int b = 0; b < 4; b++)
        {
            if (float(b) >= uNumBlobs) break;
            vec4 bps = getBlobPosSize (b);
            if (bps.w < 0.001) continue;
            vec2 toBlobDir = normalize (bps.xy);
            float align = max (dot (dirN, toBlobDir), 0.0);
            align = align * align * align;
            float rf = 1.0 / (1.0 + (dist - 1.0) * 1.2);
            colOut += getBlobCol (b) * bps.w * align * rf * 0.6;
        }
    }

    // ── On the sphere surface contribution ──────────────────────
    vec3 colSurf = vec3 (0.0);
    if (dist < 1.0 + aaWidth)
    {
        vec3 N;
        sphereIntersect (uv, N);

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

            vec3 bld = normalize (vec3 (bps.xy - uv, 0.5));
            float diff = max (dot (N, bld), 0.0);
            float bd = length (bps.xy - uv);
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

        // Head silhouette
        float hd = headSDF (uv);
        float headEdge = (1.0 - smoothstep (0.0, 0.03, abs (hd))) * 0.15;
        colSurf += headEdge * vec3 (0.6, 0.65, 0.7);
        colSurf -= (1.0 - smoothstep (-0.01, 0.02, hd)) * 0.2;
        colSurf = max (colSurf, vec3 (0.0));

        // Surface glow
        colSurf += uGlowColour * uGlowLevel * (1.0 - dist * 0.3) * 0.5;

        // Speaker VU on surface (omnidirectional)
        float ssOmni = (uSpotLevel0 + uSpotLevel1 + uSpotLevel2 + uSpotLevel3) * 0.1;
        colSurf += vec3 (0.9, 0.12, 0.06) * ssOmni;
    }

    // Blend outside and surface with smooth AA transition
    col = mix (colOut, colSurf, surfaceMix);

    gl_FragColor = vec4 (col, 1.0);
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
  _uGlowLevel       = glGetUniformLocation (pid, "uGlowLevel");
  _uGlowColour      = glGetUniformLocation (pid, "uGlowColour");
  _uBgGlowColour    = glGetUniformLocation (pid, "uBgGlowColour");
  _uBgGlowFalloff   = glGetUniformLocation (pid, "uBgGlowFalloff");
  _uBgGlowIntensity = glGetUniformLocation (pid, "uBgGlowIntensity");
  _uSpotLevel[0]  = glGetUniformLocation (pid, "uSpotLevel0");
  _uSpotLevel[1]  = glGetUniformLocation (pid, "uSpotLevel1");
  _uSpotLevel[2]  = glGetUniformLocation (pid, "uSpotLevel2");
  _uSpotLevel[3]  = glGetUniformLocation (pid, "uSpotLevel3");
  _uSpotColour    = glGetUniformLocation (pid, "uSpotColour");
  _uSpeakerRadius = glGetUniformLocation (pid, "uSpeakerRadius");
  _uBeamConeExp   = glGetUniformLocation (pid, "uBeamConeExp");
  _uBeamFalloff   = glGetUniformLocation (pid, "uBeamFalloff");
  _uBeamIntensity = glGetUniformLocation (pid, "uBeamIntensity");
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

  // Sphere glow
  {
    float lvl = std::max (_glowRms, _glowPeak * 0.8f);
    float n = std::clamp (lvl / _glowCfg.vuMax, 0.f, 1.f);
    float s = std::pow (n, _glowCfg.curve);
    if (_uGlowLevel >= 0)  glUniform1f (_uGlowLevel, s);
    if (_uGlowColour >= 0) glUniform3f (_uGlowColour, _glowCfg.r, _glowCfg.g, _glowCfg.b);
  }

  // Background glow
  if (_uBgGlowColour >= 0)
    glUniform3f (_uBgGlowColour, _bgGlowCfg.r, _bgGlowCfg.g, _bgGlowCfg.b);
  if (_uBgGlowFalloff >= 0)
    glUniform1f (_uBgGlowFalloff, _bgGlowCfg.falloff);
  if (_uBgGlowIntensity >= 0)
    glUniform1f (_uBgGlowIntensity, _bgGlowCfg.intensity);

  // Speaker spotlights
  for (int i = 0; i < 4; ++i)
    {
      float lvl = std::max (_spotRms[i], _spotPeak[i] * 0.8f);
      float n = std::clamp (lvl / _spotCfg.vuMax, 0.f, 1.f);
      float s = std::pow (n, _spotCfg.curve);
      if (_uSpotLevel[i] >= 0) glUniform1f (_uSpotLevel[i], s);
    }
  if (_uSpotColour >= 0)
    glUniform3f (_uSpotColour, _spotCfg.r, _spotCfg.g, _spotCfg.b);
  if (_uSpeakerRadius >= 0)
    glUniform1f (_uSpeakerRadius, _spotCfg.speakerRadius);
  if (_uBeamConeExp >= 0)
    glUniform1f (_uBeamConeExp, _spotCfg.beamConeExp);
  if (_uBeamFalloff >= 0)
    glUniform1f (_uBeamFalloff, _spotCfg.beamFalloff);
  if (_uBeamIntensity >= 0)
    glUniform1f (_uBeamIntensity, _spotCfg.beamIntensity);

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

  // Draw fullscreen quad
  glDisable (GL_BLEND);

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

void SphereShader::setBackgroundGlowConfig (BackgroundGlowConfig const &c)
{ _bgGlowCfg = c; }

void SphereShader::setSpotlightConfig (SpotlightConfig const &c)
{ _spotCfg = c; }

} // namespace a3
