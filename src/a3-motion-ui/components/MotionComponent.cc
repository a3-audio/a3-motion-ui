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

#include "MotionComponent.hh"

#include <a3-motion-engine/MotionEngine.hh>
#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/UserConfig.hh>
#include <a3-motion-engine/elevation/HeightMap.hh>

#include <a3-motion-ui/components/ChannelUIState.hh>
#include <a3-motion-ui/components/LookAndFeel.hh>
#include <a3-motion-ui/components/SphereShader.hh>

namespace
{

/* Minimal GLSL 1.20 shader for compositing a texture over the framebuffer
 * with alpha blending.  Replaces the old GLContextGraphics approach which
 * called juce::createOpenGLGraphicsContext and clobbered the shader output.
 */
static const char *blitVertSrc = R"(
#version 120
attribute vec2 aPos;
varying   vec2 vUV;
void main() {
  vUV = aPos * 0.5 + 0.5;             // no Y flip — JUCE FBO is standard GL
  gl_Position = vec4(aPos, 0.0, 1.0);
})";

static const char *blitFragSrc = R"(
#version 120
uniform sampler2D uTex;
varying vec2 vUV;
void main() {
  gl_FragColor = texture2D(uTex, vUV);
})";

// struct VertexUV
// {
//   float position[3];
//   float texCoord[2];
// };

// std::unique_ptr<juce::OpenGLShaderProgram::Attribute>
// createAttribute (juce::OpenGLShaderProgram &shader, const char
// *attributeName)
// {
//   using namespace ::juce::gl;

//   if (glGetAttribLocation (shader.getProgramID (), attributeName) < 0)
//     return nullptr;

//   return std::make_unique<juce::OpenGLShaderProgram::Attribute> (
//       shader, attributeName);
// }

// std::unique_ptr<juce::OpenGLShaderProgram::Uniform>
// createUniform (juce::OpenGLShaderProgram &shader, const char *uniformName)
// {
//   using namespace ::juce::gl;

//   if (glGetUniformLocation (shader.getProgramID (), uniformName) < 0)
//     return nullptr;

//   return std::make_unique<juce::OpenGLShaderProgram::Uniform> (shader,
//                                                                uniformName);
// }

// relative to the (square) component extents. Speakers are drawn at
// speakerRadius (config.json's speakerLight.speakerRadius, default 1.55,
// plus their own icon half-diagonal, speakerSize/2*sqrt(2) ≈ 0.20 — see
// drawCircle()) in this same normalized space via the same transform, so
// this factor also controls how close they sit to the component's edge.
// 0.9 pushed their icons past the component's shorter-side edge (clipped,
// not "fully in picture"); 0.57 keeps their full icon — not just their
// centre point — within that edge (0.57 * (1.55+0.20)/2 ≈ 0.50), while
// still noticeably bigger than the original 0.55.
auto constexpr reduceFactorCircle = .57f;
auto constexpr reduceFactorHead = .35f;
auto constexpr reduceFactorBlobs = 0.05f;

auto constexpr activeAreaAroundBlobFactor = 3.f;
auto constexpr blobHighlightFactor = 1.1f;

}

namespace a3
{

namespace
{
juce::File
visualConfigFile ()
{
  return juce::File::getCurrentWorkingDirectory ().getChildFile (
      "config/config.json");
}
}


/* ── BlitResources member methods ─────────────────────────────── */

void MotionComponent::BlitResources::create ()
{
  using namespace juce::gl;
  // Compile vertex shader
  GLuint vs = glCreateShader (GL_VERTEX_SHADER);
  glShaderSource (vs, 1, &blitVertSrc, nullptr);
  glCompileShader (vs);
  {
    GLint ok = 0;
    glGetShaderiv (vs, GL_COMPILE_STATUS, &ok);
    if (!ok)
      {
        char buf[512];
        glGetShaderInfoLog (vs, sizeof (buf), nullptr, buf);
        DBG ("BlitResources: vertex shader compile error: " << buf);
        glDeleteShader (vs);
        return;
      }
  }
  // Compile fragment shader
  GLuint fs = glCreateShader (GL_FRAGMENT_SHADER);
  glShaderSource (fs, 1, &blitFragSrc, nullptr);
  glCompileShader (fs);
  {
    GLint ok = 0;
    glGetShaderiv (fs, GL_COMPILE_STATUS, &ok);
    if (!ok)
      {
        char buf[512];
        glGetShaderInfoLog (fs, sizeof (buf), nullptr, buf);
        DBG ("BlitResources: fragment shader compile error: " << buf);
        glDeleteShader (vs);
        glDeleteShader (fs);
        return;
      }
  }
  // Link
  program = glCreateProgram ();
  glAttachShader (program, vs);
  glAttachShader (program, fs);
  glLinkProgram (program);
  glDeleteShader (vs);
  glDeleteShader (fs);
  {
    GLint ok = 0;
    glGetProgramiv (program, GL_LINK_STATUS, &ok);
    if (!ok)
      {
        char buf[512];
        glGetProgramInfoLog (program, sizeof (buf), nullptr, buf);
        DBG ("BlitResources: program link error: " << buf);
        glDeleteProgram (program);
        program = 0;
        return;
      }
  }

  aPos = glGetAttribLocation (program, "aPos");
  uTex = glGetUniformLocation (program, "uTex");

  // Fullscreen quad VBO
  static const float quad[] = { -1, -1, 1, -1, -1, 1, 1, 1 };
  glGenBuffers (1, &vbo);
  glBindBuffer (GL_ARRAY_BUFFER, vbo);
  glBufferData (GL_ARRAY_BUFFER, sizeof (quad), quad, GL_STATIC_DRAW);
  glBindBuffer (GL_ARRAY_BUFFER, 0);

  valid = true;
}

void MotionComponent::BlitResources::destroy ()
{
  using namespace juce::gl;
  if (vbo)     { glDeleteBuffers (1, &vbo); vbo = 0; }
  if (program) { glDeleteProgram (program);  program = 0; }
  valid = false;
}

void MotionComponent::BlitResources::blit (unsigned int textureID,
                                           int vpW, int vpH) const
{
  using namespace juce::gl;
  if (!valid || !textureID) return;

  glViewport (0, 0, vpW, vpH);
  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glUseProgram (program);
  glActiveTexture (GL_TEXTURE0);
  glBindTexture (GL_TEXTURE_2D, textureID);
  if (uTex >= 0) glUniform1i (uTex, 0);

  glBindBuffer (GL_ARRAY_BUFFER, vbo);
  if (aPos >= 0)
    {
      glEnableVertexAttribArray (GLuint (aPos));
      glVertexAttribPointer (GLuint (aPos), 2, GL_FLOAT, GL_FALSE,
                             2 * sizeof (float), nullptr);
    }

  glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);

  if (aPos >= 0) glDisableVertexAttribArray (GLuint (aPos));
  glBindBuffer (GL_ARRAY_BUFFER, 0);
  glBindTexture (GL_TEXTURE_2D, 0);
  glUseProgram (0);
  glDisable (GL_BLEND);
}

/* ── MotionComponent ──────────────────────────────────────────── */

MotionComponent::MotionComponent (
    MotionEngine &engine,
    std::vector<std::unique_ptr<ChannelUIState> > &uiStates)
    : _engine (engine), _uiStates (uiStates)
{
  _glContext.setOpenGLVersionRequired (
      juce::OpenGLContext::OpenGLVersion::defaultGLVersion);
  _glContext.setRenderer (this);
  _glContext.setContinuousRepainting (true);
  _glContext.setComponentPaintingEnabled (true);
  _glContext.attachTo (*this);

  // @TODO: compile as binary resources into executable
  _imageIsoSphere = juce::ImageFileFormat::loadFrom (
      juce::File::getCurrentWorkingDirectory ().getChildFile (
          "resources/iso-sphere-wireframe.png"));
  _drawableHead = juce::Drawable::createFromSVGFile (
      juce::File::getCurrentWorkingDirectory ().getChildFile (
          "resources/head.svg"));
  _drawableSpeaker = juce::Drawable::createFromSVGFile (
      juce::File::getCurrentWorkingDirectory ().getChildFile (
          "resources/speaker.svg"));

  // start disocclusion / animation timer at 30 Hz
  // (GL renders at 60 Hz vsync, 30 Hz is enough for blob push-away)
  startTimerHz (30);
}

MotionComponent::~MotionComponent ()
{
  stopTimer ();
  _glContext.detach ();
}

// void
// MotionComponent::paint (juce::Graphics &g)
// {
//   juce::ignoreUnused (g);
//   jassert (false); // we do all 2D drawing in the OpenGL render thread
// }

void
MotionComponent::resized ()
{
  auto lock = std::lock_guard<std::mutex> (_mutexBounds);
  _bounds = getLocalBounds ();
}

void
MotionComponent::mouseMove (const juce::MouseEvent &event)
{
  // Skip highlight computation while dragging — saves 4× position lookups per move
  if (!_grabbedIndex.has_value ())
    updateChannelBlobHighlight (event.getPosition ().toFloat ());
}

void
MotionComponent::timerCallback ()
{
  if (_grabbedIndex.has_value ())
    disoccludeBlobs ();
}

void
MotionComponent::setPreviewPattern (std::shared_ptr<Pattern> pattern,
                                    juce::Path displayPath,
                                    std::vector<std::pair<float,float>> jumpDots)
{
  jassert (pattern != nullptr);
  std::lock_guard<std::mutex> guard (_mutexPreview);
  _patternsPreview[pattern] = { std::move (displayPath), std::move (jumpDots) };
}

void
MotionComponent::unsetPreviewPattern (std::shared_ptr<Pattern> pattern)
{
  jassert (pattern != nullptr);
  std::lock_guard<std::mutex> guard (_mutexPreview);
  _patternsPreview.erase (pattern);
}

void
MotionComponent::setPatternDisplayData (std::shared_ptr<Pattern> pattern,
                                        juce::Path displayPath,
                                        std::vector<std::pair<float,float>> jumpDots)
{
  jassert (pattern != nullptr);
  std::lock_guard<std::mutex> guard (_mutexDisplayData);
  _patternsDisplayData[pattern] = { std::move (displayPath), std::move (jumpDots) };
}

void
MotionComponent::removePatternDisplayData (std::shared_ptr<Pattern> pattern)
{
  jassert (pattern != nullptr);
  std::lock_guard<std::mutex> guard (_mutexDisplayData);
  _patternsDisplayData.erase (pattern);
}

void
MotionComponent::setBackgroundColour (juce::Colour const &colour)
{
  _backgroundColourPacked.store (colour.getARGB (), std::memory_order_relaxed);
}

void
MotionComponent::setRenderingPaused (bool paused)
{
  _glContext.setContinuousRepainting (!paused);
}

void
MotionComponent::setSphereGlow (float peak, float rms)
{
  _vuSphereGlowPeak = peak;
  _vuSphereGlowRms = rms;
}

void
MotionComponent::setSpeakerLight (int speakerIndex, float peak, float rms)
{
  if (speakerIndex >= 0 && speakerIndex < 4)
    {
      _vuSpeakerPeak[speakerIndex] = peak;
      _vuSpeakerRms[speakerIndex] = rms;
    }
}

void
MotionComponent::disoccludeBlobs ()
{
  jassert (_grabbedIndex.has_value ());

  auto const posGrabbed = _engine.getChannelPosition (_grabbedIndex.value ());
  jassert (posGrabbed.isValid ());
  auto const posGrabbedPixel = normalizedToLocal2DPosition (posGrabbed);

  for (auto channel = 0u; channel < _engine.getNumChannels (); ++channel)
    {
      if (!_uiStates[channel]->grabbed)
        {
          auto position = _engine.getChannelPosition (channel);
          if (!position.isValid ())
            continue;

          auto posPixel = normalizedToLocal2DPosition (position);
          auto const distance = posPixel.getDistanceFrom (posGrabbedPixel);

          if (distance < getActiveDistanceInPixel ())
            { // push out onto circumference
              // juce::Logger::writeToLog ("disoccluding point "
              //                           + juce::String (channelIndex));
              auto offset = posPixel - posGrabbedPixel;
              offset *= (getActiveDistanceInPixel () + 1.f)
                        / offset.getDistanceFromOrigin ();

              posPixel = posGrabbedPixel + offset;
            }
          else if (posPixel.getDistanceFrom (_uiStates[channel]->posAnchor)
                   > 1.f)
            { // snap back by projection onto circle
              // borrowing math from:
              // https://www.geometrictools.com/Documentation/IntersectionLine2Circle2.pdf
              auto R = getActiveDistanceInPixel ();

              auto C = posGrabbedPixel;
              auto P = posPixel;

              jassert (_uiStates[channel]->posAnchor.isFinite ());
              if (!_uiStates[channel]->posAnchor.isFinite ())
                {
                  _uiStates[channel]->posAnchor = posPixel;
                }

              auto Pa = _uiStates[channel]->posAnchor;

              auto D = Pa - posPixel;

              auto Delta = P - C;
              auto D_dot_Delta = D.getDotProduct (Delta);

              auto delta
                  = D_dot_Delta * D_dot_Delta
                    - D.getDistanceSquaredFromOrigin ()
                          * (Delta.getDistanceSquaredFromOrigin () - R * R);

              auto t = .01f; // default: snap back with exponential
                             // smoothing
              int numValid = 0;
              float validTs[2];
              if (delta > 0.f)
                {
                  auto t0 = -(D_dot_Delta - std::sqrt (delta))
                            / D.getDistanceSquaredFromOrigin ();
                  auto t1 = -(D_dot_Delta + std::sqrt (delta))
                            / D.getDistanceSquaredFromOrigin ();

                  auto constexpr eps = 0.001f;
                  if (t0 >= -eps && t0 <= 1.f + eps)
                    validTs[numValid++] = t0;
                  if (t1 >= -eps && t1 <= 1.f + eps)
                    validTs[numValid++] = t1;

                  // Pick the t that moves P closest to its target
                  float bestDist = std::numeric_limits<float>::max ();
                  for (int ti = 0; ti < numValid; ++ti)
                    {
                      auto d = P.getDistanceSquaredFrom (P + validTs[ti] * D);
                      if (d < bestDist)
                        {
                          bestDist = d;
                          t = validTs[ti];
                        }
                    }
                }

              posPixel = P + t * D;

              if (numValid > 0)
                {
                  // after projecting shift outwards to induce slipping
                  auto Drot = juce::Point<float> (-D.y, D.x);
                  Drot /= Drot.getDistanceFromOrigin ();
                  if ((P - C).getDotProduct (Drot) < 0.f)
                    Drot *= -1.f;
                  posPixel += .25f * Drot;
                }
            }

          _engine.setChannel2DPosition (
              channel, localToNormalized2DPosition (posPixel));
        }
    }
}

void
MotionComponent::mouseDown (const juce::MouseEvent &event)
{
  for (auto channel = 0u; channel < _engine.getNumChannels (); ++channel)
    _uiStates[channel]->grabbed = false;

  if (_engine.isRecording ())
    {
      auto const posPixel = event.getPosition ().toFloat ();
      auto const posHOA = localToNormalized2DPosition (posPixel);
      _engine.setRecording2DPosition (posHOA);
    }
  else
    {
      auto closestIndex = getClosestBlobIndexWithinRadius (
          event.getPosition ().toFloat (), getActiveDistanceInPixel ());
      if (closestIndex.has_value ())
        {
          auto const index = closestIndex.value ();
          _uiStates[index]->grabbed = true;

          // Recover the raw 2D position via the exact inverse mapping
          // (unambiguous between front/back hemisphere) rather than
          // inverting the on-screen (orthographic) position, which would
          // be ambiguous whenever the blob is currently on the back of
          // the sphere and could snap it to the front on grab. No Pattern
          // is in scope here (this is a live/manual grab, not a clip) — use
          // whatever clip is currently playing on the channel, if any.
          auto const playing = _engine.getPlayingPattern (index);
          auto const params
              = playing ? playing->getElevationParams () : ElevationParams{};
          auto const posRaw2D = _engine.getHeightMap ().mapTo2D (
              _engine.getChannelPosition (index), params);
          _uiStates[index]->grabOffset
              = normalizedToLocal2DPosition (posRaw2D)
                - event.getPosition ().toFloat ();
          _grabbedIndex = index;

          // disocclusion: save anchor position for all channels
          for (auto channel = 0u; channel < _engine.getNumChannels ();
               ++channel)
            {
              auto const posChannel = _engine.getChannelPosition (channel);
              _uiStates[channel]->posAnchor
                  = normalizedToLocal2DPosition (posChannel);
            }
        }
    }
}

void
MotionComponent::mouseUp (const juce::MouseEvent &event)
{
  juce::ignoreUnused (event);
  for (auto channel = 0u; channel < _engine.getNumChannels (); ++channel)
    {
      _uiStates[channel]->grabbed = false;
    }
  _grabbedIndex = {};

  _engine.releaseRecordingPosition ();
}

void
MotionComponent::mouseDrag (const juce::MouseEvent &event)
{
  auto const posPixel = event.getPosition ().toFloat ();

  if (_engine.isRecording ())
    {
      auto const posHOA = localToNormalized2DPosition (posPixel);
      _engine.setRecording2DPosition (posHOA);
    }
  else if (_grabbedIndex.has_value ())
    {
      // Direct lookup via cached index — no loop over all channels
      auto const channel = _grabbedIndex.value ();
      auto const posPixelOffsetted
          = posPixel + _uiStates[channel]->grabOffset;
      auto const posHOA
          = localToNormalized2DPosition (posPixelOffsetted);
      _engine.setChannel2DPosition (channel, posHOA);
    }
}

void
MotionComponent::updateChannelBlobHighlight (juce::Point<float> posMousePixel)
{
  jassert (_boundsCenterRegion.getWidth ()
           == _boundsCenterRegion.getHeight ());

  for (auto channel = 0u; channel < _engine.getNumChannels (); ++channel)
    _uiStates[channel]->highlighted = false;

  auto closestIndex = getClosestBlobIndexWithinRadius (
      posMousePixel, getActiveDistanceInPixel ());
  if (closestIndex.has_value ())
    _uiStates[closestIndex.value ()]->highlighted = true;
}

std::optional<index_t>
MotionComponent::getClosestBlobIndexWithinRadius (juce::Point<float> posPixel,
                                                  float radiusPixel) const
{
  auto minDistance = std::numeric_limits<float>::infinity ();
  auto minIndex = 0u;
  for (auto channel = 0u; channel < _engine.getNumChannels (); ++channel)
    {
      auto const blobPos = _engine.getChannelPosition (channel);
      if (!blobPos.isValid ())
        continue;

      auto const blobPosPixel = normalizedToLocal2DPosition (blobPos);
      auto const distance = blobPosPixel.getDistanceFrom (posPixel);

      if (distance < radiusPixel && distance < minDistance)
        {
          minDistance = distance;
          minIndex = channel;
        }
    }

  if (std::isfinite (minDistance))
    return { minIndex };

  return {};
}

float
MotionComponent::getActiveDistanceInPixel () const
{
  return _boundsCenterRegion.getWidth () * reduceFactorBlobs
         * activeAreaAroundBlobFactor / 2.f;
}

void
MotionComponent::newOpenGLContextCreated ()
{
  using namespace juce::gl;
  // glDebugMessageControl is GL 4.3 / GL_KHR_debug — not available on
  // RPi4 V3D (GL 2.1).  The JUCE-loaded function pointer may be null.
  if (glDebugMessageControl != nullptr)
    glDebugMessageControl (GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_OTHER,
                           GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);

  // Initialise the 3D sphere shader
  if (!_sphereShader.initialise (_glContext))
    {
      DBG ("WARNING: SphereShader failed to initialise – falling back to 2D");
    }

  // Initialise blit shader for FBO compositing
  _blit.create ();

  _configWatcher = ConfigFileWatcher{ visualConfigFile () };
  applyVisualConfig (userConfig);
}


void
MotionComponent::applyVisualConfig (juce::var const &config)
{
  // Load glow / spotlight config from config
  {
    auto cfgF = [] (const juce::var &obj, const char *key,
                    float def) -> float {
      return obj.hasProperty (key) ? static_cast<float> (obj[key]) : def;
    };

    SphereShader::GlowConfig gc;
    auto const &sg = config["sphereGlow"];
    gc.r = cfgF (sg, "r", 230.f) / 255.f;
    gc.g = cfgF (sg, "g", 26.f) / 255.f;
    gc.b = cfgF (sg, "b", 13.f) / 255.f;
    gc.alphaMax = cfgF (sg, "alphaMax", 0.6f);
    gc.vuMax = cfgF (sg, "vuMax", 0.2f);
    gc.curve = cfgF (sg, "curve", 0.4f);
    _sphereShader.setGlowConfig (gc);

    SphereShader::BackgroundGlowConfig bgc;
    auto const &bg = config["backgroundGlow"];
    bgc.r         = cfgF (bg, "r", 230.f) / 255.f;
    bgc.g         = cfgF (bg, "g", 26.f) / 255.f;
    bgc.b         = cfgF (bg, "b", 13.f) / 255.f;
    bgc.falloff   = cfgF (bg, "falloff", 1.5f);
    bgc.intensity = cfgF (bg, "intensity", 0.8f);
    _sphereShader.setBackgroundGlowConfig (bgc);

    SphereShader::SpotlightConfig sc;
    auto const &sl = config["speakerLight"];
    sc.r = cfgF (sl, "r", 255.f) / 255.f;
    sc.g = cfgF (sl, "g", 64.f) / 255.f;
    sc.b = cfgF (sl, "b", 166.f) / 255.f;
    sc.alphaMax = cfgF (sl, "alphaMax", 0.35f);
    sc.vuMax = cfgF (sl, "vuMax", 0.2f);
    sc.curve = cfgF (sl, "curve", 0.4f);
    sc.speakerRadius = cfgF (sl, "speakerRadius", 1.55f);
    sc.beamConeExp = cfgF (sl, "beamConeExp", 6.f);
    sc.beamFalloff = cfgF (sl, "beamFalloff", 0.6f);
    sc.beamIntensity = cfgF (sl, "beamIntensity", 0.8f);
    sc.widthStart = cfgF (sl, "widthStart", 0.1f);
    sc.widthEnd = cfgF (sl, "widthEnd", 0.2929f);
    _sphereShader.setSpotlightConfig (sc);
  }

  // Cache corona config (avoids JSON lookups every frame per blob).
  // Used directly by drawChannelBlobs() (2D overlay).
  _coronaCfg = loadCoronaConfig (config);
}

// Visual tuning is judged by eye, so the values get changed a lot. Picking up
// config.json while running turns a rebuild-and-restart cycle into a file save.
// Runs on the GL thread, throttled to roughly once a second, so no handoff from
// the message thread is needed.
void
MotionComponent::reloadVisualConfigIfChanged ()
{
  if (!_configWatcher.hasChanged ())
    return;

  juce::var parsed;
  if (juce::JSON::parse (visualConfigFile ().loadFileAsString (), parsed)
          .failed ())
    return; // half-written save — the next check picks up the finished file

  applyVisualConfig (parsed);
  juce::Logger::writeToLog ("reloaded " + visualConfigFile ().getFullPathName ());
}

void
MotionComponent::renderOpenGL ()
{
  using namespace juce::gl;
  using juce::OpenGLHelpers;

  jassert (OpenGLHelpers::isContextActive ());
  _glContext.setSwapInterval (1);  // vsync @ 60 Hz — frees CPU for timer thread

  updateBoundsAndTransform ();

  if (_frameCount % 60 == 0)
    reloadVisualConfigIfChanged ();

  // Clear background first
  OpenGLHelpers::clear (Colours::background);

  // ── Smooth VU values (exponential moving average per frame) ───
  // Attack fast, release slower → no flicker, responsive feel.
  // Corona uses configurable attack/decay; others use fixed values.
  {
    constexpr float dt = 1.f / 60.f;  // frame time at 60fps

    // Fixed smoothing for glow and speakers
    auto smoothFixed = [] (float &current, float target) {
      float alpha = (target > current) ? 0.5f : 0.15f;
      current += alpha * (target - current);
    };
    smoothFixed (_smoothGlowPeak, _vuSphereGlowPeak.load ());
    smoothFixed (_smoothGlowRms,  _vuSphereGlowRms.load ());
    for (int i = 0; i < 4; ++i)
      {
        smoothFixed (_smoothSpotPeak[i], _vuSpeakerPeak[i].load ());
        smoothFixed (_smoothSpotRms[i],  _vuSpeakerRms[i].load ());
      }

    // Configurable attack/decay for blob coronas
    // alpha = 1 - exp(-dt/tau) approximated for small dt/tau
    float attackAlpha = 1.f - std::exp (-dt / std::max (0.001f, _coronaCfg.attack));
    float decayAlpha  = 1.f - std::exp (-dt / std::max (0.001f, _coronaCfg.decay));
    auto smoothBlob = [attackAlpha, decayAlpha] (float &current, float target) {
      float alpha = (target > current) ? attackAlpha : decayAlpha;
      current += alpha * (target - current);
    };
    auto numCh = static_cast<int> (_engine.getNumChannels ());
    for (int ch = 0; ch < numCh && ch < 4; ++ch)
      {
        smoothBlob (_smoothBlobPeak[ch], _uiStates[ch]->vuPeak.load ());
        smoothBlob (_smoothBlobRms[ch],  _uiStates[ch]->vuLevel.load ());
      }
  }

  // ── 3D scene via shader ───────────────────────────────────────
  {
    // Forward smoothed VU data to shader
    _sphereShader.setSphereGlow (_smoothGlowPeak, _smoothGlowRms);
    for (int i = 0; i < 4; ++i)
      _sphereShader.setSpeakerLight (i, _smoothSpotPeak[i],
                                     _smoothSpotRms[i]);

    // Forward blob data to shader
    auto numChannels = static_cast<int> (_engine.getNumChannels ());
    _sphereShader.setNumBlobs (numChannels);
    for (int ch = 0; ch < numChannels && ch < SphereShader::kMaxBlobs; ++ch)
      {
        SphereShader::BlobData bd;
        auto const position = _engine.getChannelPosition (ch);
        if (position.isValid ())
          {
            auto posJuce = cartesian2DHOA2JUCE (position);
            // JUCE 2D: Y down. Shader: Y up. Flip Y.
            bd.x = posJuce.getX ();
            bd.y = -posJuce.getY ();
            bd.visible = true;
          }

        auto blobSize = reduceFactorBlobs;
        if (position.isValid ())
          blobSize *= (1.f + std::clamp (position.z (), 0.f, 1.f) * 0.7f);
        if (_uiStates[ch]->grabbed)
          blobSize *= activeAreaAroundBlobFactor;
        else if (_uiStates[ch]->highlighted)
          blobSize *= blobHighlightFactor;
        bd.size = blobSize;

        auto col = _uiStates[ch]->colour;
        bd.r = col.getFloatRed ();
        bd.g = col.getFloatGreen ();
        bd.b = col.getFloatBlue ();
        bd.vuPeak = _smoothBlobPeak[ch];
        bd.vuRms = _smoothBlobRms[ch];
        bd.grabbed = _uiStates[ch]->grabbed;
        bd.highlighted = _uiStates[ch]->highlighted;

        _sphereShader.setBlob (ch, bd);
      }

    // Compute sphere position and radius in pixels
    auto const vpW = _boundsRender.getWidth ();
    auto const vpH = _boundsRender.getHeight ();
    auto const scale = static_cast<float> (_glContext.getRenderingScale ());

    auto const centreX
        = static_cast<float> (_boundsCenterRegion.getCentreX ()) * scale;
    // GL has Y flipped compared to JUCE
    auto const centreY
        = static_cast<float> (vpH) * scale
          - static_cast<float> (_boundsCenterRegion.getCentreY ()) * scale;
    auto const radius
        = static_cast<float> (_boundsCenterRegion.getWidth ()) / 2.f * scale;

    glViewport (0, 0, static_cast<int> (vpW * scale),
                static_cast<int> (vpH * scale));

    _sphereShader.draw (static_cast<int> (vpW * scale),
                        static_cast<int> (vpH * scale), radius, centreX,
                        centreY);
  }

  // ── 2D overlay (blobs, corona, speakers, pattern preview) ──────
  // Drawn into _imageBlend FBO with transparent background, then composited
  // over the shader output. This prevents the JUCE software rasterizer from
  // clobbering the shader-rendered background glow and speaker beams.
  {
    _mutexPreview.lock ();
    auto const patternsPreview{ _patternsPreview };
    _mutexPreview.unlock ();

    _mutexDisplayData.lock ();
    auto const patternsDisplayData{ _patternsDisplayData };
    _mutexDisplayData.unlock ();

    ++_frameCount;

    if (_imageBlend)
      {
        _imageBlend->clear (_imageBlend->getBounds ());

        {
          juce::Graphics gFBO{ *_imageBlend };
          gFBO.addTransform (_transformNormalizedToLocal);

          // Speaker SVGs
          if (_drawableSpeaker != nullptr)
            drawCircle (gFBO);

          // Channel blobs + corona
          drawChannelBlobs (gFBO);

          // Pattern preview paths
          for (auto &[pattern, displayData] : patternsPreview)
            drawPatternPreview (*pattern, displayData, gFBO);

          // Faint trajectory lines for all currently playing patterns
          // (skip those already drawn as explicit previews)
          for (auto &[pattern, displayData] : patternsDisplayData)
            {
              if (patternsPreview.count (pattern) > 0)
                continue; // already drawn as preview
              if (pattern->getStatus () == Pattern::Status::Playing
                  || pattern->getStatus () == Pattern::Status::Recording)
                drawPlayingTrajectory (*pattern, displayData, gFBO);
            }
        }

        // Composite the FBO over the shader output using native GL blitting.
        // The old GLContextGraphics approach called createOpenGLGraphicsContext
        // which reset GL state and clobbered the shader-rendered pixels.
        auto *fb = juce::OpenGLImageType::getFrameBufferFrom (*_imageBlend);
        if (fb && fb->getTextureID ())
          {
            // Ensure we're drawing to the default framebuffer (screen)
            juce::gl::glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, 0);

            auto const scale
                = static_cast<float> (_glContext.getRenderingScale ());
            _blit.blit (
                fb->getTextureID (),
                static_cast<int> (_boundsRender.getWidth () * scale),
                static_cast<int> (_boundsRender.getHeight () * scale));
          }
      }
  }
}

void
MotionComponent::updateBoundsAndTransform ()
{
  {
    auto lock = std::lock_guard<std::mutex> (_mutexBounds);
    if (_bounds != _boundsRender)
      {
        _boundsRender = _bounds;
        renderBoundsChanged ();
      }
  }

  auto shorterSideLength
      = juce::jmin (_boundsRender.getWidth (), _boundsRender.getHeight ());
  _boundsCenterRegion = _boundsRender.withSizeKeepingCentre (
      shorterSideLength * reduceFactorCircle,
      shorterSideLength * reduceFactorCircle);

  _transformNormalizedToLocal = juce::AffineTransform ( //
      _boundsCenterRegion.getWidth () / 2.f, 0.f,
      _boundsCenterRegion.getCentreX (),           //
      0.f, _boundsCenterRegion.getHeight () / 2.f, //
      _boundsCenterRegion.getCentreY ());
}

void
MotionComponent::renderBoundsChanged ()
{
  // juce::Logger::writeToLog (juce::String ("bounds changed: ")
  //                           + juce::String (_boundsRender.getWidth ()) + "
  //                           x
  //                           "
  //                           + juce::String (_boundsRender.getHeight ()));

  _imageBlend = std::make_unique<juce::Image> (
      juce::Image::PixelFormat::ARGB,                        //
      _boundsRender.getWidth (), _boundsRender.getHeight (), //
      false, juce::OpenGLImageType ());
}

void
MotionComponent::drawCircle (juce::Graphics &g)
{
  jassert (_boundsCenterRegion.getWidth ()
           == _boundsCenterRegion.getHeight ());

  // The 3D sphere (body, rim, specular, wireframe, head silhouette,
  // VU glow and spotlights) is now rendered by the SphereShader in
  // renderOpenGL() before this 2D overlay pass.
  //
  // What remains here: speaker icons drawn as SVG overlays.

  // --- 4 speakers outside the sphere, like spotlights ---
  if (_drawableSpeaker != nullptr)
    {
      auto constexpr opacitySpeaker = 0.35f;
      auto constexpr speakerSize = 0.28f;
      // Use cached speaker radius from spotlight config
      float speakerRadius = _sphereShader.getSpeakerRadius ();

      for (int i = 0; i < 4; ++i)
        {
          float angleDeg = 45.f + i * 90.f;
          float angleRad = angleDeg * juce::MathConstants<float>::pi / 180.f;

          float sx = speakerRadius * std::cos (angleRad);
          float sy = speakerRadius * std::sin (angleRad);

          auto speakerBounds = juce::Rectangle<float> ().withSizeKeepingCentre (
              speakerSize, speakerSize);

          g.saveState ();
          g.addTransform (juce::AffineTransform::rotation (
              angleRad + juce::MathConstants<float>::pi, sx, sy));
          g.setOpacity (opacitySpeaker);
          _drawableSpeaker->drawWithin (
              g, speakerBounds.withCentre ({ sx, sy }),
              juce::RectanglePlacement::centred, opacitySpeaker);
          g.restoreState ();
        }
    }

  g.setOpacity (1.f);
}

void
MotionComponent::drawChannelBlobs (juce::Graphics &g)
{
  // Draw blobs + corona directly — no FBO compositing (_imageBlend) for
  // maximum performance on RPi4.

  for (auto channel = 0u; channel < _engine.getNumChannels (); ++channel)
  {
      auto const position = _engine.getChannelPosition (channel);
      if (!position.isValid ())
        continue;

      auto blobSize = 2 * reduceFactorBlobs;
      blobSize *= (1.f + std::clamp (position.z (), 0.f, 1.f) * 0.7f);

      // Blobs on the back of the sphere (z < 0): draw smaller and dimmer
      // to give a sense of depth through the semi-transparent sphere.
      float backFade = 1.0f;
      if (position.z () < 0.f)
        {
          backFade = 0.3f + 0.7f * std::clamp (position.z () + 1.f, 0.f, 1.f);
          blobSize *= (0.5f + 0.5f * backFade);
        }

      auto posNormalized = cartesian2DHOA2JUCE (position);

      auto colour = _uiStates[channel]->colour;
      if (backFade < 1.0f)
        colour = colour.withMultipliedAlpha (backFade);

      // Draw VU corona (glow effect based on audio level)
      float vuRms = (channel < 4) ? _smoothBlobRms[channel] : 0.f;
      float vuPeak = (channel < 4) ? _smoothBlobPeak[channel] : 0.f;
      bool isGrabbed = _uiStates[channel]->grabbed;
      bool isHighlighted = _uiStates[channel]->highlighted;

      if (vuRms > 0.0001f || vuPeak > 0.0001f || isGrabbed || isHighlighted)
        {
          float peakScaled = coronaPeakLevel (vuPeak, _coronaCfg.vuMax);
          float vuScaled = coronaVuLevel (vuPeak, vuRms, _coronaCfg.vuMax);
          float coronaScale = coronaScaleFactor (vuScaled, _coronaCfg);

          float baseBlobScale = 1.0f;
          if (isGrabbed)
            {
              baseBlobScale = activeAreaAroundBlobFactor;
              coronaScale *= _coronaCfg.sizeGrabbed;
            }
          else if (isHighlighted)
            {
              baseBlobScale = blobHighlightFactor;
              coronaScale *= 1.2f;
            }

          auto coronaDiam = blobSize * baseBlobScale * coronaScale;
          float coronaAlpha = _coronaCfg.alphaMin
                              + peakScaled * (_coronaCfg.alphaMax - _coronaCfg.alphaMin);

          // Two glow layers (outer → inner) — blend towards white at high VU
          auto whiteBlend = peakScaled * _coronaCfg.whiteBlend;
          auto coronaColour = colour.interpolatedWith (juce::Colours::white, whiteBlend);
          for (int layer = 2; layer >= 1; --layer)
            {
              // Layer 2 is the outer one — keep it tied to the constant the
              // visibility test asserts against.
              float layerScale
                  = 1.0f + (layer - 1) * (coronaOuterLayerScale - 1.0f);
              float layerAlpha = coronaAlpha / (layer * 2.0f);
              auto layerSize = coronaDiam * layerScale;
              auto layerRect = juce::Rectangle<float> (0.f, 0.f, layerSize, layerSize);
              g.setColour (coronaColour.withAlpha (layerAlpha));
              g.fillEllipse (layerRect.withCentre (posNormalized));
            }
        }

      // Grabbed: transparent area
      if (isGrabbed)
        {
          auto grabSize = blobSize * activeAreaAroundBlobFactor;
          auto grabRect = juce::Rectangle<float> (0.f, 0.f, grabSize, grabSize);
          g.setColour (colour.withAlpha (0.4f));
          g.fillEllipse (grabRect.withCentre (posNormalized));
        }

      // Highlighted: brighter outline
      if (isHighlighted)
        {
          auto hlSize = blobSize * blobHighlightFactor;
          auto hlRect = juce::Rectangle<float> (0.f, 0.f, hlSize, hlSize);
          g.setColour (colour.withLightness (colour.getLightness () + 0.2f));
          g.fillEllipse (hlRect.withCentre (posNormalized));
        }

      // Solid blob disc
      auto blob = juce::Rectangle<float> (0.f, 0.f, blobSize, blobSize);
      g.setColour (colour);
      g.fillEllipse (blob.withCentre (posNormalized));
    }
}

// ── Draw a juce::Path (from SVG displayPath) projected onto the sphere ──
// Flattens the Bézier path into line segments, projects each point
// through mapTo3D, and draws with depth-band batching.
// Between consecutive flattened points that are far apart in 2D,
// intermediate sub-samples are inserted so the line hugs the sphere.
static void
drawPathOnSphere (juce::Path const &displayPath,
                  float lineThickness,
                  float alpha,
                  juce::Colour colour,
                  bool fadeByDepth,
                  ElevationParams const &elevationParams,
                  HeightMap const &heightMap,
                  juce::Graphics &g)
{
  if (displayPath.isEmpty ())
    return;

  // Depth-band helpers
  auto depthBand = [] (float z) -> int {
    if (z < -0.5f) return 0;
    if (z < 0.f)   return 1;
    if (z < 0.5f)  return 2;
    return 3;
  };

  // z >= 0 (elevation >= 50%, i.e. at/above the horizon) is always fully
  // visible; below that it fades toward the far/south pole so it reads as
  // "behind" the sphere. fadeByDepth == false skips this entirely — used
  // for whichever trajectory is currently being edited, which must stay
  // fully legible no matter where it sits.
  auto fadeForZ = [] (float z) -> float {
    return (z < 0.f)
        ? 0.3f + 0.7f * std::clamp (z + 1.f, 0.f, 1.f)
        : 1.0f;
  };

  auto flushPath = [&] (juce::Path &path, int band) {
    float fade = fadeByDepth
        ? fadeForZ (band <= 1 ? (band == 0 ? -0.75f : -0.25f)
                              : (band == 2 ?  0.25f :  0.75f))
        : 1.0f;
    float thickness = lineThickness * (0.5f + 0.5f * fade);
    auto stroke = juce::PathStrokeType (
        thickness, juce::PathStrokeType::JointStyle::curved,
        juce::PathStrokeType::EndCapStyle::rounded);
    g.setColour (colour.withAlpha (alpha * fade));
    g.strokePath (path, stroke);
  };

  // Project a 2D HOA point onto the sphere and return screen pos + z.
  auto projectPoint = [&] (float x, float y)
      -> std::pair<juce::Point<float>, float> {
    auto pos3D = heightMap.mapTo3D (Pos::fromCartesian (x, y, 0.f),
                                    elevationParams);
    return { cartesian2DHOA2JUCE (pos3D), pos3D.z () };
  };

  // Maximum 2D step size before we insert intermediate samples.
  // Smaller = more sub-samples = smoother on the sphere. Flat mode has no
  // sphere curvature at all, so coarse sampling is fine there.
  float const maxStep = elevationParams.flat ? 0.06f : 0.03f;

  // Collect all projected points (with sub-sampling for long segments).
  std::vector<std::pair<juce::Point<float>, float>> projected;
  projected.reserve (512);

  juce::PathFlatteningIterator iter (displayPath, {}, 0.005f);

  bool firstPoint = true;
  float prevX = 0.f, prevY = 0.f;

  while (iter.next ())
    {
      if (firstPoint)
        {
          // The very first point of a sub-path
          projected.push_back (projectPoint (iter.x1, iter.y1));
          prevX = iter.x1;
          prevY = iter.y1;
          firstPoint = false;
        }

      float dx = iter.x2 - prevX;
      float dy = iter.y2 - prevY;
      float dist = std::sqrt (dx * dx + dy * dy);

      if (dist > maxStep)
        {
          // Insert intermediate sub-samples along the 2D line
          int nSub = static_cast<int> (std::ceil (dist / maxStep));
          for (int s = 1; s < nSub; ++s)
            {
              float t = static_cast<float> (s)
                        / static_cast<float> (nSub);
              float mx = prevX + dx * t;
              float my = prevY + dy * t;
              projected.push_back (projectPoint (mx, my));
            }
        }

      projected.push_back (projectPoint (iter.x2, iter.y2));
      prevX = iter.x2;
      prevY = iter.y2;
    }

  if (projected.size () < 2)
    return;

  // Draw with depth-band batching
  juce::Path currentPath;
  int currentBand = depthBand (projected[0].second);
  currentPath.startNewSubPath (projected[0].first);

  for (std::size_t i = 1; i < projected.size (); ++i)
    {
      int band = depthBand (projected[i].second);
      if (band != currentBand)
        {
          flushPath (currentPath, currentBand);
          currentPath.clear ();
          currentPath.startNewSubPath (projected[i - 1].first);
          currentBand = band;
        }
      currentPath.lineTo (projected[i].first);
    }
  flushPath (currentPath, currentBand);
}

void
MotionComponent::drawPatternPreview (Pattern const &pattern,
                                    PatternDisplayData const &displayData,
                                    juce::Graphics &g)
{
  auto constexpr lineThickness = 0.04f;

  auto const ch = pattern.getChannel ();
  auto colour = _uiStates[ch]->colour;
  auto const params = pattern.getElevationParams ();
  auto const &heightMap = _engine.getHeightMap ();

  // ── Handle jump-dot patterns ──
  // The pattern currently being edited must always stay fully legible, no
  // matter which hemisphere it sits in — no depth fade.
  if (!displayData.jumpDots.empty ())
    {
      auto constexpr dotSize = lineThickness * 3.f;
      for (auto const &dot : displayData.jumpDots)
        {
          auto pos3D = heightMap.mapTo3D (
              Pos::fromCartesian (dot.first, dot.second, 0.f), params);
          auto posJuce = cartesian2DHOA2JUCE (pos3D);
          g.setColour (colour);
          g.fillEllipse (juce::Rectangle<float> (dotSize, dotSize)
                             .withCentre (posJuce));
        }
      return;
    }

  // ── Draw from SVG displayPath projected onto sphere ──
  drawPathOnSphere (displayData.displayPath, lineThickness, 1.0f, colour,
                    false, params, heightMap, g);
}

void
MotionComponent::drawPlayingTrajectory (Pattern const &pattern,
                                        PatternDisplayData const &displayData,
                                        juce::Graphics &g)
{
  // Thinner line is what distinguishes a merely-playing trajectory from
  // the one currently being edited — depth fade still applies here (full
  // at/above the horizon, receding toward the far pole below it), unlike
  // drawPatternPreview()'s always-full override above.
  auto constexpr lineThickness = 0.025f;

  auto const ch = pattern.getChannel ();
  auto colour = _uiStates[ch]->colour;
  auto const params = pattern.getElevationParams ();
  auto const &heightMap = _engine.getHeightMap ();

  // ── Handle jump-dot patterns ──
  if (!displayData.jumpDots.empty ())
    {
      auto constexpr dotSize = lineThickness * 3.f;
      for (auto const &dot : displayData.jumpDots)
        {
          auto pos3D = heightMap.mapTo3D (
              Pos::fromCartesian (dot.first, dot.second, 0.f), params);
          auto posJuce = cartesian2DHOA2JUCE (pos3D);
          float fade = (pos3D.z () < 0.f)
              ? 0.3f + 0.7f * std::clamp (pos3D.z () + 1.f, 0.f, 1.f)
              : 1.0f;
          float ds = dotSize * (0.5f + 0.5f * fade);
          g.setColour (colour.withAlpha (fade));
          g.fillEllipse (juce::Rectangle<float> (ds, ds)
                             .withCentre (posJuce));
        }
      return;
    }

  // ── Draw from SVG displayPath projected onto sphere ──
  drawPathOnSphere (displayData.displayPath, lineThickness, 1.0f, colour,
                    true, params, heightMap, g);
}

juce::Point<float>
MotionComponent::normalizedToLocal2DPosition (Pos const &posNorm) const
{
  return cartesian2DHOA2JUCE (posNorm).transformedBy (
      _transformNormalizedToLocal);
}

Pos
MotionComponent::localToNormalized2DPosition (
    juce::Point<float> const &posLocal) const
{
  return cartesian2DJUCE2HOA (
      posLocal.transformedBy (_transformNormalizedToLocal.inverted ()));
}

void
MotionComponent::openGLContextClosing ()
{
  DBG ("openGLContextClosing");
  _blit.destroy ();
  _sphereShader.shutdown ();
}

}
