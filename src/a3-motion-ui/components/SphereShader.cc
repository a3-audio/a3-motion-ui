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

uniform float uGlowLevel;
uniform vec3  uGlowColour;

uniform vec3  uBgGlowColour;
uniform float uBgGlowFalloff;
uniform float uBgGlowIntensity;
uniform vec3  uBgColour;  // solid background colour

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
        // Background glow (VU-driven from subwoofer /vu/4)
        if (uGlowLevel > 0.001)
        {
            float gf = 1.0 / (1.0 + (dist - 1.0) * uBgGlowFalloff);
            colOut += uBgGlowColour * uGlowLevel * gf * gf * uBgGlowIntensity;
        }

        // Speaker beams — dynamic VU-driven cones from speakers
        // Width grows with VU level, absorbed at sphere edge
        float spkR = uSpeakerRadius;
        vec2 dirN = uvScene / dist;

        // Unrolled: GLSL 1.20 has no local arrays
        // Speaker 0: 135°
        if (uSpotLevel0 > 0.001) {
            vec2 sd = vec2 (-0.7071, 0.7071);
            vec2 tp = uvScene - sd * spkR;
            float tpL = length (tp);
            float al = max (dot (tp / max(tpL, 0.001), -sd), 0.0);
            float bw = 0.3 + uSpotLevel0 * 0.4;
            float cone = pow (smoothstep (1.0 - bw, 1.0, al), uBeamConeExp);
            float glow = pow (smoothstep (1.0 - bw * 1.5, 1.0, al), uBeamConeExp) * 0.3;
            float df = 1.0 / (1.0 + tpL / spkR * uBeamFalloff);
            colOut += uSpotColour * uSpotLevel0 * (cone + glow) * df * uBeamIntensity;
        }
        // Speaker 1: 45°
        if (uSpotLevel1 > 0.001) {
            vec2 sd = vec2 ( 0.7071, 0.7071);
            vec2 tp = uvScene - sd * spkR;
            float tpL = length (tp);
            float al = max (dot (tp / max(tpL, 0.001), -sd), 0.0);
            float bw = 0.3 + uSpotLevel1 * 0.4;
            float cone = pow (smoothstep (1.0 - bw, 1.0, al), uBeamConeExp);
            float glow = pow (smoothstep (1.0 - bw * 1.5, 1.0, al), uBeamConeExp) * 0.3;
            float df = 1.0 / (1.0 + tpL / spkR * uBeamFalloff);
            colOut += uSpotColour * uSpotLevel1 * (cone + glow) * df * uBeamIntensity;
        }
        // Speaker 2: 315°
        if (uSpotLevel2 > 0.001) {
            vec2 sd = vec2 ( 0.7071,-0.7071);
            vec2 tp = uvScene - sd * spkR;
            float tpL = length (tp);
            float al = max (dot (tp / max(tpL, 0.001), -sd), 0.0);
            float bw = 0.3 + uSpotLevel2 * 0.4;
            float cone = pow (smoothstep (1.0 - bw, 1.0, al), uBeamConeExp);
            float glow = pow (smoothstep (1.0 - bw * 1.5, 1.0, al), uBeamConeExp) * 0.3;
            float df = 1.0 / (1.0 + tpL / spkR * uBeamFalloff);
            colOut += uSpotColour * uSpotLevel2 * (cone + glow) * df * uBeamIntensity;
        }
        // Speaker 3: 225°
        if (uSpotLevel3 > 0.001) {
            vec2 sd = vec2 (-0.7071,-0.7071);
            vec2 tp = uvScene - sd * spkR;
            float tpL = length (tp);
            float al = max (dot (tp / max(tpL, 0.001), -sd), 0.0);
            float bw = 0.3 + uSpotLevel3 * 0.4;
            float cone = pow (smoothstep (1.0 - bw, 1.0, al), uBeamConeExp);
            float glow = pow (smoothstep (1.0 - bw * 1.5, 1.0, al), uBeamConeExp) * 0.3;
            float df = 1.0 / (1.0 + tpL / spkR * uBeamFalloff);
            colOut += uSpotColour * uSpotLevel3 * (cone + glow) * df * uBeamIntensity;
        }

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

        // Surface glow
        colSurf += uGlowColour * uGlowLevel * (1.0 - dist * 0.3) * 0.5;

        // Speaker beams flowing INTO sphere — from rim towards centre
        // Each speaker lights its quadrant, strongest at rim, fading inward
        // Blends with sphereGlow colour for seamless fusion
        vec2 uvN = (length(uvScene) > 0.001) ? uvScene / length(uvScene) : vec2(0.0);
        float rimFade = dist * dist; // stronger at rim (dist→1), fades to centre (dist→0)

        float q0 = max (dot (uvN, vec2 (-0.7071, 0.7071)), 0.0); // 135°
        float q1 = max (dot (uvN, vec2 ( 0.7071, 0.7071)), 0.0); // 45°
        float q2 = max (dot (uvN, vec2 ( 0.7071,-0.7071)), 0.0); // 315°
        float q3 = max (dot (uvN, vec2 (-0.7071,-0.7071)), 0.0); // 225°

        // Directional beam per quadrant (q^2 for ~90° spread)
        float surfBeam = uSpotLevel0 * q0 * q0 + uSpotLevel1 * q1 * q1
                       + uSpotLevel2 * q2 * q2 + uSpotLevel3 * q3 * q3;

        // Blend speaker colour into sphereGlow colour towards centre
        vec3 beamCol = mix (uGlowColour, uSpotColour, rimFade);
        colSurf += beamCol * surfBeam * rimFade * 0.35;
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
  _uGlowLevel       = glGetUniformLocation (pid, "uGlowLevel");
  _uGlowColour      = glGetUniformLocation (pid, "uGlowColour");
  _uBgGlowColour    = glGetUniformLocation (pid, "uBgGlowColour");
  _uBgGlowFalloff   = glGetUniformLocation (pid, "uBgGlowFalloff");
  _uBgGlowIntensity = glGetUniformLocation (pid, "uBgGlowIntensity");
  _uBgColour        = glGetUniformLocation (pid, "uBgColour");
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

  // Solid background colour (from LookAndFeel Colours::background)
  if (_uBgColour >= 0)
    {
      // 0xff292f36 → RGB normalized
      glUniform3f (_uBgColour, 0.161f, 0.184f, 0.212f);
    }

  // Speaker spotlights
  for (int i = 0; i < 4; ++i)
    {
      float s = speakerLightLevel (_spotRms[i], _spotCfg.vuMax, _spotCfg.curve);
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

void SphereShader::setBackgroundGlowConfig (BackgroundGlowConfig const &c)
{ _bgGlowCfg = c; }

void SphereShader::setSpotlightConfig (SpotlightConfig const &c)
{ _spotCfg = c; }

} // namespace a3
