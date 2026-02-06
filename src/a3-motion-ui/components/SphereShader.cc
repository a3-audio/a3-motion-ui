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

// Channel blobs as coloured light sources
uniform vec4  uBlobPosSize0;  // xy=position, z=size, w=vuLevel
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
    float d = length (uv);
    if (d > 1.0) return -1.0;
    float z = sqrt (1.0 - d * d);
    normal = vec3 (uv, z);
    return z;
}

// ─── head silhouette SDF (nose points UP = +Y in GL) ────────────

float sdCircle (vec2 p, vec2 c, float r)
{
    return length (p - c) - r;
}

float sdBox (vec2 p, vec2 c, vec2 halfSize)
{
    vec2 d = abs (p - c) - halfSize;
    return length (max (d, 0.0)) + min (max (d.x, d.y), 0.0);
}

float headSDF (vec2 uv)
{
    // Scale head to sit well inside the sphere (matching reduceFactorHead ~0.35)
    // Head occupies about 35% of sphere diameter
    float s = 0.6;  // scale factor: smaller = head more centred
    vec2 p = uv / s;
    float cranium = sdCircle (p, vec2 (0.0), 0.38);
    float earL = sdCircle (p, vec2 (-0.39, 0.0), 0.09);
    float earR = sdCircle (p, vec2 ( 0.39, 0.0), 0.09);
    // Nose pointing UP (+Y in GL = up on screen)
    float nose = sdBox (p, vec2 (0.0, 0.42), vec2 (0.045, 0.09));
    float head = min (cranium, min (earL, earR));
    head = min (head, nose);
    return head * s;  // scale distance back
}

// ─── spotlight helpers ──────────────────────────────────────────

float getSpotLevel (int i)
{
    if (i == 0) return uSpotLevel0;
    if (i == 1) return uSpotLevel1;
    if (i == 2) return uSpotLevel2;
    return uSpotLevel3;
}

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

float spotContrib (float spotLevel, float angleDeg, vec2 uv, float dist)
{
    if (spotLevel < 0.001) return 0.0;
    float angleRad = angleDeg * 3.14159265 / 180.0;
    vec2 spotDir = vec2 (cos (angleRad), sin (angleRad));
    vec2 dirFromCentre = normalize (uv);
    float alignment = max (dot (dirFromCentre, spotDir), 0.0);
    alignment = pow (alignment, 2.0);  // wider cone
    float radialFade = 1.0 / (1.0 + (dist - 1.0) * 0.8);  // slower falloff
    return spotLevel * alignment * radialFade * 0.8;
}

float spotSurface (float spotLevel, float angleDeg, vec3 N)
{
    if (spotLevel < 0.001) return 0.0;
    float angleRad = angleDeg * 3.14159265 / 180.0;
    vec3 spotLightDir = normalize (vec3 (cos (angleRad), sin (angleRad), 0.3));
    float spotDot = max (dot (N, spotLightDir), 0.0);
    spotDot = pow (spotDot, 2.0);  // broader light spread
    return spotLevel * spotDot * 0.5;  // stronger contribution
}

// ─── main ───────────────────────────────────────────────────────
void main ()
{
    vec2 pxCoord = vUV * uResolution;
    vec2 uv = (pxCoord - uSphereCentre) / uSphereRadius;
    float dist = length (uv);

    // ── Outside sphere ──────────────────────────────────────────
    if (dist > 1.0)
    {
        vec3 col = vec3 (0.0);
        float alpha = 0.0;

        // Background glow — config-driven colour, falloff, intensity
        if (uGlowLevel > 0.001)
        {
            float glowFalloff = 1.0 / (1.0 + (dist - 1.0) * uBgGlowFalloff);
            glowFalloff *= glowFalloff;
            float ga = uGlowLevel * glowFalloff * uBgGlowIntensity;
            col += uBgGlowColour * ga;
            alpha = max(alpha, ga);
        }

        // Speaker spotlights — magenta-orange beams from speaker positions
        float sa = 0.0;
        sa += spotContrib (uSpotLevel0,  45.0, uv, dist);
        sa += spotContrib (uSpotLevel1, 135.0, uv, dist);
        sa += spotContrib (uSpotLevel2, 225.0, uv, dist);
        sa += spotContrib (uSpotLevel3, 315.0, uv, dist);
        if (sa > 0.001)
        {
            col += uSpotColour * sa;
            alpha = max(alpha, sa * 0.9);
        }

        gl_FragColor = vec4 (col, alpha);
        return;
    }

    // ── On the sphere surface ───────────────────────────────────
    vec3 N;
    float z = sphereIntersect (uv, N);

    vec3 baseCol = vec3 (0.04, 0.04, 0.055);

    // Fresnel rim
    float fresnel = 1.0 - N.z;
    fresnel = pow (fresnel, 3.0);
    vec3 rimColour = vec3 (0.5, 0.55, 0.65);
    float rimStrength = 0.35;

    // Fake environment reflection
    vec3 viewDir = vec3 (0.0, 0.0, 1.0);
    vec3 R = reflect (-viewDir, N);
    float envH = R.x * 0.5 + 0.5;
    float envV = R.y * 0.5 + 0.5;
    vec3 envColour = vec3 (0.02) + vec3 (0.04, 0.05, 0.06) * smoothstep (0.3, 0.7, envH)
                   + vec3 (0.03, 0.03, 0.04) * smoothstep (0.5, 0.9, envV);
    float envStrength = fresnel * 0.6;

    // No white key/fill lights — blobs provide the coloured lighting

    // Blob-driven coloured spotlights on sphere surface
    vec3 blobLight = vec3 (0.0);
    vec3 blobSpec = vec3 (0.0);
    for (int b = 0; b < 4; b++)
    {
        if (float(b) >= uNumBlobs) break;
        vec4 bps = getBlobPosSize (b);
        vec3 bcol = getBlobCol (b);
        if (bps.z < 0.001) continue;

        // Blob position on the 2D plane → treat as a point light above the surface
        vec3 blobLightDir = normalize (vec3 (bps.xy - uv, 0.5));

        // Diffuse contribution
        float bDiff = max (dot (N, blobLightDir), 0.0);
        // Distance attenuation (closer blobs light more)
        float bDist = length (bps.xy - uv);
        float bAtten = 1.0 / (1.0 + bDist * 2.0);
        // VU-driven intensity boost
        float bIntensity = 0.3 + bps.w * 0.7;
        blobLight += bcol * bDiff * bAtten * bIntensity * 0.5;

        // Specular reflection of blob colour on sphere
        vec3 bHalf = normalize (blobLightDir + viewDir);
        float bSpec = pow (max (dot (N, bHalf), 0.0), 40.0);
        blobSpec += bcol * bSpec * bAtten * bIntensity * 0.6;
    }

    // Wireframe (procedural great circles)
    float gridFreq = 8.0;
    float gridLineU = abs (fract (N.x * gridFreq + 0.5) - 0.5);
    float gridLineV = abs (fract (N.y * gridFreq + 0.5) - 0.5);
    float gridLineW = abs (fract (N.z * gridFreq + 0.5) - 0.5);
    float gridLine = min (gridLineU, min (gridLineV, gridLineW));
    float wireframe = 1.0 - smoothstep (0.01, 0.04, gridLine);
    wireframe *= 0.08;
    wireframe *= (1.0 - fresnel * 0.8);

    // Head silhouette
    float headDist = headSDF (uv);
    float headMask = 1.0 - smoothstep (-0.01, 0.02, headDist);
    float headDarken = headMask * 0.2;
    float headEdge = smoothstep (0.0, 0.03, abs (headDist));
    headEdge = (1.0 - headEdge) * 0.15;

    // VU glow on surface — red absorption glow
    float surfaceGlow = uGlowLevel * (1.0 - dist * 0.3) * 0.5;
    vec3 glowOnSurface = uGlowColour * surfaceGlow;

    // Spot light on surface — sphere absorbs into reddish glow
    float ss = 0.0;
    ss += spotSurface (uSpotLevel0,  45.0, N);
    ss += spotSurface (uSpotLevel1, 135.0, N);
    ss += spotSurface (uSpotLevel2, 225.0, N);
    ss += spotSurface (uSpotLevel3, 315.0, N);
    // Incoming magenta-orange light absorbed → shifted to warm red glow
    vec3 absorbedColour = vec3 (0.9, 0.12, 0.06);
    vec3 spotOnSurface = mix (uSpotColour, absorbedColour, 0.7) * ss;

    // Compose
    vec3 colour = baseCol;
    colour += envColour * envStrength;
    colour += rimColour * rimStrength * fresnel;
    colour += blobLight;
    colour += blobSpec;
    colour += vec3 (wireframe);
    colour += glowOnSurface;
    colour += spotOnSurface;
    colour += headEdge * vec3 (0.6, 0.65, 0.7);
    colour -= headDarken * vec3 (1.0);
    colour = max (colour, vec3 (0.0));

    // Anti-aliased edge
    float edgeAA = smoothstep (1.0, 0.99, dist);

    gl_FragColor = vec4 (colour, edgeAA);
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
  _uNumBlobs      = glGetUniformLocation (pid, "uNumBlobs");

  _uBlobPosSize[0] = glGetUniformLocation (pid, "uBlobPosSize0");
  _uBlobPosSize[1] = glGetUniformLocation (pid, "uBlobPosSize1");
  _uBlobPosSize[2] = glGetUniformLocation (pid, "uBlobPosSize2");
  _uBlobPosSize[3] = glGetUniformLocation (pid, "uBlobPosSize3");
  _uBlobCol[0]     = glGetUniformLocation (pid, "uBlobCol0");
  _uBlobCol[1]     = glGetUniformLocation (pid, "uBlobCol1");
  _uBlobCol[2]     = glGetUniformLocation (pid, "uBlobCol2");
  _uBlobCol[3]     = glGetUniformLocation (pid, "uBlobCol3");

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
          vuL = std::clamp (vuL / 0.4f, 0.f, 1.f);  // normalise
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
