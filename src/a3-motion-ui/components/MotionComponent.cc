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

/* One-time blit resources (created in newOpenGLContextCreated) */
struct BlitResources
{
  GLuint program = 0;
  GLuint vbo = 0;
  GLint  aPos = -1;
  GLint  uTex = -1;
  bool   valid = false;

  void create ()
  {
    using namespace juce::gl;
    // Compile vertex shader
    GLuint vs = glCreateShader (GL_VERTEX_SHADER);
    glShaderSource (vs, 1, &blitVertSrc, nullptr);
    glCompileShader (vs);
    // Compile fragment shader
    GLuint fs = glCreateShader (GL_FRAGMENT_SHADER);
    glShaderSource (fs, 1, &blitFragSrc, nullptr);
    glCompileShader (fs);
    // Link
    program = glCreateProgram ();
    glAttachShader (program, vs);
    glAttachShader (program, fs);
    glLinkProgram (program);
    glDeleteShader (vs);
    glDeleteShader (fs);

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

  void destroy ()
  {
    using namespace juce::gl;
    if (vbo)     { glDeleteBuffers (1, &vbo); vbo = 0; }
    if (program) { glDeleteProgram (program);  program = 0; }
    valid = false;
  }

  /** Draw textureID as fullscreen quad with alpha blending. */
  void blit (GLuint textureID, int vpW, int vpH) const
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
};

static BlitResources s_blit;

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

// relative to the (square) component extents
auto constexpr reduceFactorCircle = .8f;
auto constexpr reduceFactorHead = .35f;
auto constexpr reduceFactorBlobs = 0.05f;

auto constexpr activeAreaAroundBlobFactor = 3.f;
auto constexpr blobHighlightFactor = 1.1f;

}

namespace a3
{

MotionComponent::MotionComponent (
    MotionEngine &engine,
    std::vector<std::unique_ptr<ChannelUIState> > &uiStates)
    : _engine (engine), _uiStates (uiStates)
{
  _glContext.setOpenGLVersionRequired (
      juce::OpenGLContext::OpenGLVersion::defaultGLVersion);
  _glContext.setRenderer (this);
  _glContext.setContinuousRepainting (true);
  _glContext.setComponentPaintingEnabled (false);
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
MotionComponent::setPreviewPattern (std::shared_ptr<Pattern> pattern)
{
  jassert (pattern != nullptr);
  std::lock_guard<std::mutex> guard (_mutexPreview);
  _patternsPreview.insert (pattern);
}

void
MotionComponent::unsetPreviewPattern (std::shared_ptr<Pattern> pattern)
{
  jassert (pattern != nullptr);
  std::lock_guard<std::mutex> guard (_mutexPreview);
  _patternsPreview.erase (pattern);
}

void
MotionComponent::setBackgroundColour (juce::Colour const &colour)
{
  _backgroundColourPacked.store (colour.getARGB (), std::memory_order_relaxed);
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
          _uiStates[index]->grabOffset
              = normalizedToLocal2DPosition (
                    _engine.getChannelPosition (index))
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
  glDebugMessageControl (GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_OTHER,
                         GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);

  // Initialise the 3D sphere shader
  if (!_sphereShader.initialise (_glContext))
    {
      DBG ("WARNING: SphereShader failed to initialise – falling back to 2D");
    }

  // Initialise blit shader for FBO compositing
  s_blit.create ();

  // Load glow / spotlight config from userConfig
  {
    auto cfgF = [] (const juce::var &obj, const char *key,
                    float def) -> float {
      return obj.hasProperty (key) ? static_cast<float> (obj[key]) : def;
    };

    SphereShader::GlowConfig gc;
    auto const &sg = userConfig["sphereGlow"];
    gc.r = cfgF (sg, "r", 230.f) / 255.f;
    gc.g = cfgF (sg, "g", 26.f) / 255.f;
    gc.b = cfgF (sg, "b", 13.f) / 255.f;
    gc.alphaMax = cfgF (sg, "alphaMax", 0.6f);
    gc.vuMax = cfgF (sg, "vuMax", 0.2f);
    gc.curve = cfgF (sg, "curve", 0.4f);
    _sphereShader.setGlowConfig (gc);

    SphereShader::BackgroundGlowConfig bgc;
    auto const &bg = userConfig["backgroundGlow"];
    bgc.r         = cfgF (bg, "r", 230.f) / 255.f;
    bgc.g         = cfgF (bg, "g", 26.f) / 255.f;
    bgc.b         = cfgF (bg, "b", 13.f) / 255.f;
    bgc.falloff   = cfgF (bg, "falloff", 1.5f);
    bgc.intensity = cfgF (bg, "intensity", 0.8f);
    _sphereShader.setBackgroundGlowConfig (bgc);

    SphereShader::SpotlightConfig sc;
    auto const &sl = userConfig["speakerLight"];
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
    _sphereShader.setSpotlightConfig (sc);
  }

  // Cache corona config (avoids JSON lookups every frame per blob)
  {
    auto cfgF = [] (const juce::var &obj, const char *key,
                    float def) -> float {
      return obj.hasProperty (key) ? static_cast<float> (obj[key]) : def;
    };
    auto const &corona = userConfig["corona"];
    _coronaCfg.vuMax       = cfgF (corona, "vuMax", 0.4f);
    _coronaCfg.sizeMin     = cfgF (corona, "sizeMin", 1.2f);
    _coronaCfg.sizeMax     = cfgF (corona, "sizeMax", 2.0f);
    _coronaCfg.sizeGrabbed = cfgF (corona, "sizeGrabbed", 1.5f);
    _coronaCfg.alphaMin    = cfgF (corona, "alphaMin", 0.15f);
    _coronaCfg.alphaMax    = cfgF (corona, "alphaMax", 0.75f);
    _coronaCfg.whiteBlend  = cfgF (corona, "whiteBlend", 0.5f);
    // Corona config is used directly by drawChannelBlobs() (2D overlay)
  }
}

void
MotionComponent::renderOpenGL ()
{
  using namespace juce::gl;
  using juce::OpenGLHelpers;

  jassert (OpenGLHelpers::isContextActive ());
  _glContext.setSwapInterval (1);  // vsync @ 60 Hz — frees CPU for timer thread

  // printFrameTime ();

  updateBoundsAndTransform ();

  // Clear background first
  OpenGLHelpers::clear (Colours::background);

  // ── Smooth VU values (exponential moving average per frame) ───
  // Attack fast (~0.5), release slower (~0.15) → no flicker, responsive feel
  {
    auto smooth = [] (float &current, float target) {
      float alpha = (target > current) ? 0.5f : 0.15f;
      current += alpha * (target - current);
    };
    smooth (_smoothGlowPeak, _vuSphereGlowPeak.load ());
    smooth (_smoothGlowRms,  _vuSphereGlowRms.load ());
    for (int i = 0; i < 4; ++i)
      {
        smooth (_smoothSpotPeak[i], _vuSpeakerPeak[i].load ());
        smooth (_smoothSpotRms[i],  _vuSpeakerRms[i].load ());
      }
    auto numCh = static_cast<int> (_engine.getNumChannels ());
    for (int ch = 0; ch < numCh && ch < 4; ++ch)
      {
        smooth (_smoothBlobPeak[ch], _uiStates[ch]->vuPeak.load ());
        smooth (_smoothBlobRms[ch],  _uiStates[ch]->vuLevel.load ());
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
          for (auto &pattern : patternsPreview)
            drawPatternPreview (*pattern, gFBO);
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
            s_blit.blit (
                fb->getTextureID (),
                static_cast<int> (_boundsRender.getWidth () * scale),
                static_cast<int> (_boundsRender.getHeight () * scale));
          }
      }
  }
}

void
MotionComponent::printFrameTime ()
{
  static auto lastT = std::chrono::high_resolution_clock::now ();

  auto now = std::chrono::high_resolution_clock::now ();
  auto deltaT
      = std::chrono::duration_cast<std::chrono::microseconds> (now - lastT)
            .count ()
        / 1000.f;
  lastT = now;

  juce::Logger::writeToLog ("frametime: " + juce::String (deltaT));
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

      auto posScreenNormalized = cartesian2DHOA2JUCE (position);
      auto colour = _uiStates[channel]->colour;

      // Draw VU corona (glow effect based on audio level)
      float vuRms = (channel < 4) ? _smoothBlobRms[channel] : 0.f;
      float vuPeak = (channel < 4) ? _smoothBlobPeak[channel] : 0.f;
      bool isGrabbed = _uiStates[channel]->grabbed;
      bool isHighlighted = _uiStates[channel]->highlighted;

      if (vuRms > 0.0001f || vuPeak > 0.0001f || isGrabbed || isHighlighted)
        {
          float vuMax = _coronaCfg.vuMax;
          float rmsNorm = std::clamp (vuRms / vuMax, 0.0f, 1.0f);
          float peakNorm = std::clamp (vuPeak / vuMax, 0.0f, 1.0f);

          // Perceptual curve: x^0.6 ≈ sqrt(x)*x^0.1 — use sqrt approx
          float rmsScaled = std::sqrt (rmsNorm) * (0.4f + 0.6f * rmsNorm);
          float peakScaled = std::sqrt (peakNorm) * (0.4f + 0.6f * peakNorm);

          float vuScaled = std::max (rmsScaled, peakScaled * 0.8f);
          float coronaScale = _coronaCfg.sizeMin
                              + vuScaled * (_coronaCfg.sizeMax - _coronaCfg.sizeMin);

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

          // Two glow layers (outer → inner) — same colour as ball
          for (int layer = 2; layer >= 1; --layer)
            {
              float layerScale = 1.0f + (layer - 1) * 0.15f;
              float layerAlpha = coronaAlpha / (layer * 2.0f);
              auto layerSize = coronaDiam * layerScale;
              auto layerRect = juce::Rectangle<float> (0.f, 0.f, layerSize, layerSize);
              g.setColour (colour.withAlpha (layerAlpha));
              g.fillEllipse (layerRect.withCentre (posScreenNormalized));
            }
        }

      // Grabbed: transparent area
      if (isGrabbed)
        {
          auto grabSize = blobSize * activeAreaAroundBlobFactor;
          auto grabRect = juce::Rectangle<float> (0.f, 0.f, grabSize, grabSize);
          g.setColour (colour.withAlpha (0.4f));
          g.fillEllipse (grabRect.withCentre (posScreenNormalized));
        }

      // Highlighted: brighter outline
      if (isHighlighted)
        {
          auto hlSize = blobSize * blobHighlightFactor;
          auto hlRect = juce::Rectangle<float> (0.f, 0.f, hlSize, hlSize);
          g.setColour (colour.withLightness (colour.getLightness () + 0.2f));
          g.fillEllipse (hlRect.withCentre (posScreenNormalized));
        }

      // Solid blob disc
      auto blob = juce::Rectangle<float> (0.f, 0.f, blobSize, blobSize);
      g.setColour (colour);
      g.fillEllipse (blob.withCentre (posScreenNormalized));
    }
}

void
MotionComponent::drawPatternPreview (Pattern const &pattern, juce::Graphics &g)
{
  auto ticks = pattern.getTicks ();

  auto constexpr lineThickness = 0.04f;

  auto colour = _uiStates[pattern.getChannel ()]->colour;
  g.setColour (colour.withAlpha (0.6f));
  auto const strokeStyle = juce::PathStrokeType (
      lineThickness, juce::PathStrokeType::JointStyle::curved,
      juce::PathStrokeType::EndCapStyle::rounded);
  auto path = juce::Path ();

  jassert (ticks.positions.size () <= std::numeric_limits<int>::max ());
  path.preallocateSpace (static_cast<int> (ticks.positions.size ()));

  auto hasStarted = false;
  for (auto offset = 0u; offset < ticks.positions.size (); ++offset)
    {
      auto const indexWrapped
          = (ticks.lastUpdatedTick + 1 + offset) % ticks.positions.size ();
      auto const tick = ticks.positions[indexWrapped];

      if (tick.isValid ())
        {
          auto posNormalized = cartesian2DHOA2JUCE (tick);
          if (!hasStarted)
            {
              path.startNewSubPath (posNormalized);
              hasStarted = true;
            }
          path.lineTo (posNormalized);
        }
      else
        {
          if (hasStarted)
            {
              if (path.getLength () > lineThickness)
                {
                  g.strokePath (path, strokeStyle);
                }
              else
                {
                  auto ellipse = juce::Rectangle<float> ();
                  auto constexpr sizeEllipse = lineThickness * 2.f;
                  ellipse.setSize (sizeEllipse, sizeEllipse);
                  g.fillEllipse (
                      ellipse.withCentre (path.getCurrentPosition ()));
                }
              path.clear ();
              hasStarted = false;
            }
        }
    }
  if (hasStarted)
    {
      g.strokePath (path, strokeStyle);
    }
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
  s_blit.destroy ();
  _sphereShader.shutdown ();
}

}
