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

// 4 speaker spot levels as separate floats (can't loop-index vec4 reliably)
uniform float uSpotLevel0;
uniform float uSpotLevel1;
uniform float uSpotLevel2;
uniform float uSpotLevel3;
uniform vec3  uSpotColour;

// ─── helpers ────────────────────────────────────────────────────

float sphereIntersect (vec2 uv, out vec3 normal)
{
    float d = length (uv);
    if (d > 1.0) return -1.0;
    float z = sqrt (1.0 - d * d);
    normal = vec3 (uv, z);
    return z;
}

// ─── head silhouette SDF ────────────────────────────────────────

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
    float cranium = sdCircle (uv, vec2 (0.0), 0.38);
    float earL = sdCircle (uv, vec2 (-0.39, 0.0), 0.09);
    float earR = sdCircle (uv, vec2 ( 0.39, 0.0), 0.09);
    float nose = sdBox (uv, vec2 (0.0, -0.40), vec2 (0.045, 0.09));
    float head = min (cranium, min (earL, earR));
    head = min (head, nose);
    return head;
}

// ─── spotlight helper ───────────────────────────────────────────

float spotContrib (float spotLevel, float angleDeg, vec2 uv, float dist)
{
    if (spotLevel < 0.001) return 0.0;
    float angleRad = angleDeg * 3.14159265 / 180.0;
    vec2 spotDir = vec2 (cos (angleRad), sin (angleRad));
    vec2 dirFromCentre = normalize (uv);
    float alignment = max (dot (dirFromCentre, spotDir), 0.0);
    alignment = pow (alignment, 4.0);
    float radialFade = 1.0 / (1.0 + (dist - 1.0) * 1.5);
    return spotLevel * alignment * radialFade * 0.5;
}

float spotSurface (float spotLevel, float angleDeg, vec3 N)
{
    if (spotLevel < 0.001) return 0.0;
    float angleRad = angleDeg * 3.14159265 / 180.0;
    vec3 spotLightDir = normalize (vec3 (cos (angleRad), sin (angleRad), 0.3));
    float spotDot = max (dot (N, spotLightDir), 0.0);
    spotDot = pow (spotDot, 3.0);
    return spotLevel * spotDot * 0.25;
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
        vec4 col = vec4 (0.0);

        // Sphere glow
        if (uGlowLevel > 0.001)
        {
            float glowFalloff = 1.0 / (1.0 + (dist - 1.0) * 2.5);
            glowFalloff *= glowFalloff;
            float glowAlpha = uGlowLevel * glowFalloff * 0.7;
            col += vec4 (uGlowColour * glowAlpha, glowAlpha);
        }

        // Speaker spotlights outside sphere
        float sa = 0.0;
        sa += spotContrib (uSpotLevel0,  45.0, uv, dist);
        sa += spotContrib (uSpotLevel1, 135.0, uv, dist);
        sa += spotContrib (uSpotLevel2, 225.0, uv, dist);
        sa += spotContrib (uSpotLevel3, 315.0, uv, dist);
        col += vec4 (uSpotColour * sa, sa);

        gl_FragColor = col;
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

    // Specular (key light upper-left)
    vec3 lightDir = normalize (vec3 (-0.5, -0.6, 0.8));
    vec3 halfVec = normalize (lightDir + viewDir);
    float spec = pow (max (dot (N, halfVec), 0.0), 80.0);
    vec3 specColour = vec3 (0.85, 0.9, 1.0);

    // Secondary specular (fill)
    vec3 lightDir2 = normalize (vec3 (0.3, -0.4, 0.6));
    vec3 halfVec2 = normalize (lightDir2 + viewDir);
    float spec2 = pow (max (dot (N, halfVec2), 0.0), 30.0);
    vec3 specColour2 = vec3 (0.6, 0.65, 0.75);

    // Diffuse
    float diffuse = max (dot (N, lightDir), 0.0) * 0.08;

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

    // VU glow on surface
    float surfaceGlow = uGlowLevel * (1.0 - dist * 0.5) * 0.4;
    vec3 glowOnSurface = uGlowColour * surfaceGlow;

    // Spot light on surface
    float ss = 0.0;
    ss += spotSurface (uSpotLevel0,  45.0, N);
    ss += spotSurface (uSpotLevel1, 135.0, N);
    ss += spotSurface (uSpotLevel2, 225.0, N);
    ss += spotSurface (uSpotLevel3, 315.0, N);
    vec3 spotOnSurface = uSpotColour * ss;

    // Compose
    vec3 colour = baseCol;
    colour += diffuse * vec3 (1.0);
    colour += envColour * envStrength;
    colour += rimColour * rimStrength * fresnel;
    colour += specColour * spec * 0.7;
    colour += specColour2 * spec2 * 0.2;
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

  auto vertSrc = getVertexShader ();
  auto fragSrc = getFragmentShader ();

  if (!_shader->addVertexShader (vertSrc))
    {
      DBG ("SphereShader vertex compile error: " + _shader->getLastError ());
      _shader.reset ();
      return false;
    }

  if (!_shader->addFragmentShader (fragSrc))
    {
      DBG ("SphereShader fragment compile error: " + _shader->getLastError ());
      _shader.reset ();
      return false;
    }

  if (!_shader->link ())
    {
      DBG ("SphereShader link error: " + _shader->getLastError ());
      _shader.reset ();
      return false;
    }

  // Cache uniform locations
  auto pid = _shader->getProgramID ();
  _uResolution   = glGetUniformLocation (pid, "uResolution");
  _uSphereRadius = glGetUniformLocation (pid, "uSphereRadius");
  _uSphereCentre = glGetUniformLocation (pid, "uSphereCentre");
  _uGlowLevel    = glGetUniformLocation (pid, "uGlowLevel");
  _uGlowColour   = glGetUniformLocation (pid, "uGlowColour");
  _uSpotLevel[0] = glGetUniformLocation (pid, "uSpotLevel0");
  _uSpotLevel[1] = glGetUniformLocation (pid, "uSpotLevel1");
  _uSpotLevel[2] = glGetUniformLocation (pid, "uSpotLevel2");
  _uSpotLevel[3] = glGetUniformLocation (pid, "uSpotLevel3");
  _uSpotColour   = glGetUniformLocation (pid, "uSpotColour");
  _aPos          = glGetAttribLocation (pid, "aPos");

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
// Fullscreen quad geometry
// ─────────────────────────────────────────────────────────────────
void
SphereShader::createQuad ()
{
  using namespace juce::gl;

  // clang-format off
  static const float quadVertices[] = {
    -1.f, -1.f,
     1.f, -1.f,
    -1.f,  1.f,
     1.f,  1.f,
  };
  // clang-format on

  glGenBuffers (1, &_vbo);
  glBindBuffer (GL_ARRAY_BUFFER, _vbo);
  glBufferData (GL_ARRAY_BUFFER, sizeof (quadVertices), quadVertices,
                GL_STATIC_DRAW);
  glBindBuffer (GL_ARRAY_BUFFER, 0);
}

void
SphereShader::deleteQuad ()
{
  using namespace juce::gl;

  if (_vbo != 0)
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

  // Set uniforms
  if (_uResolution >= 0)
    glUniform2f (_uResolution, static_cast<float> (viewportWidth),
                 static_cast<float> (viewportHeight));

  if (_uSphereRadius >= 0)
    glUniform1f (_uSphereRadius, sphereRadius);

  if (_uSphereCentre >= 0)
    glUniform2f (_uSphereCentre, sphereCentreX, sphereCentreY);

  // Sphere glow
  {
    float level = std::max (_glowRms, _glowPeak * 0.8f);
    float norm = std::clamp (level / _glowCfg.vuMax, 0.f, 1.f);
    float scaled = std::pow (norm, _glowCfg.curve);
    if (_uGlowLevel >= 0)
      glUniform1f (_uGlowLevel, scaled);
    if (_uGlowColour >= 0)
      glUniform3f (_uGlowColour, _glowCfg.r, _glowCfg.g, _glowCfg.b);
  }

  // Speaker spotlights
  {
    float levels[4];
    for (int i = 0; i < 4; ++i)
      {
        float lvl = std::max (_spotRms[i], _spotPeak[i] * 0.8f);
        float norm = std::clamp (lvl / _spotCfg.vuMax, 0.f, 1.f);
        levels[i] = std::pow (norm, _spotCfg.curve);
      }
    for (int i = 0; i < 4; ++i)
      if (_uSpotLevel[i] >= 0)
        glUniform1f (_uSpotLevel[i], levels[i]);
    if (_uSpotColour >= 0)
      glUniform3f (_uSpotColour, _spotCfg.r, _spotCfg.g, _spotCfg.b);
  }

  // Draw
  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glBindBuffer (GL_ARRAY_BUFFER, _vbo);
  if (_aPos >= 0)
    {
      glEnableVertexAttribArray (static_cast<GLuint> (_aPos));
      glVertexAttribPointer (static_cast<GLuint> (_aPos), 2, GL_FLOAT,
                             GL_FALSE, 2 * sizeof (float), nullptr);
    }

  glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);

  if (_aPos >= 0)
    glDisableVertexAttribArray (static_cast<GLuint> (_aPos));
  glBindBuffer (GL_ARRAY_BUFFER, 0);

  glUseProgram (0);
}

// ─────────────────────────────────────────────────────────────────
// Setters
// ─────────────────────────────────────────────────────────────────
void
SphereShader::setSphereGlow (float peak, float rms)
{
  _glowPeak = peak;
  _glowRms = rms;
}

void
SphereShader::setSpeakerLight (int index, float peak, float rms)
{
  if (index >= 0 && index < 4)
    {
      _spotPeak[index] = peak;
      _spotRms[index] = rms;
    }
}

void
SphereShader::setGlowConfig (GlowConfig const &cfg)
{
  _glowCfg = cfg;
}

void
SphereShader::setSpotlightConfig (SpotlightConfig const &cfg)
{
  _spotCfg = cfg;
}

} // namespace a3
