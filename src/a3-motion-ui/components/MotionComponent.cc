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

#include <a3-motion-ui/AppPaths.hh>
#include <a3-motion-ui/components/BlobPush.hh>
#include <a3-motion-ui/components/ControllerLayout.hh>
#include <a3-motion-ui/components/fpv/BodyLook.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>
#include <a3-motion-ui/components/SourceKeys.hh>

#include <a3-motion-engine/ClipSettings.hh>
#include <a3-motion-engine/PatternRunning.hh>

#include <a3-motion-engine/TempoLfo.hh>
#include <a3-motion-engine/TrajectoryShaping.hh>

#include <a3-motion-engine/TrajectoryShape.hh>

#include <a3-motion-engine/MotionEngine.hh>
#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/UserConfig.hh>
#include <a3-motion-engine/elevation/HeightMap.hh>

#include <a3-motion-ui/components/ChannelUIState.hh>
#include <a3-motion-ui/components/PlasmaSheath.hh>
#include <a3-motion-ui/components/LookAndFeel.hh>
#include <a3-motion-ui/theme/ThemedComponent.hh>
#include <a3-motion-ui/components/SphereShader.hh>
#include <a3-motion-ui/components/SpeakerLightScaling.hh>
#include <a3-motion-ui/components/Listener.hh>
#include <a3-motion-ui/components/SphereProjection.hh>
#include <a3-motion-ui/components/LineMapGeometry.hh>
#include <a3-motion-ui/components/LineMapStrokes.hh>
#include <a3-motion-ui/components/SphereMarks.hh>
#include <a3-motion-ui/components/SkinPanelLayout.hh>
#include <a3-motion-engine/SpaceTurn.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/SkinSections.hh>

#include <mutex>

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

// Sphere and blob size come from config.json now (ui.sphereScale,
// ui.blobScale); these are the fallbacks. The sphere's scale is not free: the
// speaker icons are drawn at speakerRadius plus their own half-diagonal in the
// same normalised space, so past a point they run off the shorter edge. See
// speakerIconsFitOnScreen() and SphereScale.IconsFitAtTheShippedScale.
auto constexpr reduceFactorCircleDefault = .62f;
auto constexpr reduceFactorBlobsDefault = 0.05f;

// Where the bearing ring sits, in sphere radii.
//
// The numbers dock to the equator and the ticks stand outside them — the other
// way round from a ship's compass card, and on purpose: the equator is the
// line the numbers name, and a bearing read off a ring floating clear of it is
// one you have to carry across a gap.
auto constexpr bearingLabelRadius = 1.055f;
auto constexpr bearingTickInner = 1.105f;
auto constexpr reduceFactorHead = .35f;

auto constexpr activeAreaAroundBlobFactor = 3.f;

// How hard each link of the blob's wake chases the one in front of it, per
// rendered frame. Eight links at this rate settle roughly a second and a half
// behind the blob -- the maintainer's call, after the first version came out
// short: "der schweif vom blob soll laenger".
auto constexpr blobTrailLag = 0.09f;


// How much finer than the screen the sphere pass is rendered before being
// drawn back down onto it. See _superBuffer.
//
// Two, and measured rather than picked: on the device's 768x1024 panel that
// is a 1536x2048 buffer -- already finer than Full HD in this orientation --
// and it costs about a millisecond and a half a frame, 75 fps down to 68 on
// the same picture. Three costs fourteen and halves the rate, 36 fps, which
// is the point where the GPU becomes the thing in the way rather than the
// drawing. An odd factor would also give up what makes two exact: sampled at
// the centre of a screen pixel, a texture twice as fine lands precisely
// between four texels, so one bilinear tap *is* the average of the four.
auto constexpr sphereSupersample = 2;


// What counts as a jump rather than a movement, in the sphere's normalised
// units: a clip looping back to its start, or a finger dropping the blob
// somewhere else. The wake is cut there instead of being dragged across a
// path nothing travelled.
auto constexpr blobTrailCutDistance = 0.35f;

}

namespace a3
{

namespace
{
// Motion's own pictures: beside a dev build, in /usr/share for the package.
juce::File
resource (char const *name)
{
  static auto const directory = resourceDirectory (
      juce::File::getSpecialLocation (juce::File::currentExecutableFile));
  // Once, so a broken package layout shows in the log instead of as blank art.
  static auto const reported = !directory.isDirectory ();
  static std::once_flag once;
  if (reported)
    std::call_once (once, [] {
      juce::Logger::writeToLog ("Resource directory does not exist: "
                                + directory.getFullPathName ());
    });
  return directory.getChildFile (name);
}

juce::File
configFile ()
{
  return juce::File::getCurrentWorkingDirectory ().getChildFile (
      "config/config.json");
}

// The tuned visual values live in the active skin now, not in config.json.
juce::File
visualConfigFile ()
{
  return skinFile (configFile ().getParentDirectory (),
                   userConfig["ui"]["skin"].toString ());
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
      resource ("iso-sphere-wireframe.png"));
  _drawableHead = juce::Drawable::createFromSVGFile (resource ("head.svg"));

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
  if (_grabs.empty ())
    updateChannelBlobHighlight (event.getPosition ().toFloat ());
}

void
MotionComponent::timerCallback ()
{
  // Also with nothing held: a pushed blob eases back to its channel's place.
  disoccludeBlobs ();

  // The long press on a body: the gesture only knows the time it is told.
  auto const now = floorClockMs ();
  auto const holding = _floorFinger.has_value () && !_floorFingerIsPage;
  if (holding)
    carryOutFloorAction (_floorGesture.held (now), {});
  auto const body = holding ? _floorGesture.body () : std::nullopt;
  _holdBody = body.value_or (noBodyId);
  _holdProgress = body ? _floorGesture.holdProgress (now) : 0.f;
}

double
MotionComponent::floorClockMs ()
{
  return juce::Time::getMillisecondCounterHiRes ();
}

bool
MotionComponent::floorFingerDown (SourceKey key, juce::Point<float> at)
{
  auto const screenRadius = [&] {
    auto const flat = localToNormalized2DPosition (at);
    return std::hypot (flat.x (), flat.y ());
  }();
  auto const route = fpvFingerDown (
      true, onTheFloor (screenRadius, floorAt (at)), _pageHeld);

  switch (route)
    {
    case FpvFingerDown::Camera:
      return false;
    case FpvFingerDown::PageTap:
      _floorFinger = key;
      _floorFingerIsPage = true;
      if (onPageTap)
        onPageTap (bodyAt (at));
      return true;
    case FpvFingerDown::Floor:
      _floorFinger = key;
      _floorFingerIsPage = false;
      _floorGesture.setBlobDiameter (blobDiameterInPixels ());
      _floorGesture.down (at, floorClockMs (), bodyAt (at));
      // Should it turn out to be the camera, it turns from here.
      _cameraGrabbedAt = at;
      _cameraAtGrab = getCamera ();
      return true;
    }
  return false;
}

void
MotionComponent::cancelFloorFinger ()
{
  _floorGesture.cancel ();
  _floorFinger.reset ();
  _floorFingerIsPage = false;
}

void
MotionComponent::carryOutFloorAction (FloorAction action,
                                      juce::Point<float> at)
{
  auto const body = _floorGesture.body ();
  switch (action)
    {
    case FloorAction::Place:
      if (onFloorPlaced)
        onFloorPlaced (floorAt (at));
      return;
    case FloorAction::CycleWeight:
      if (body && onBodyCycled)
        onBodyCycled (*body);
      return;
    case FloorAction::Drag:
      if (body && onBodyMoved)
        onBodyMoved (*body, floorAt (at));
      return;
    case FloorAction::Remove:
      if (body && onBodyRemoved)
        onBodyRemoved (*body);
      return;
    case FloorAction::None:
    case FloorAction::Camera:
      return;
    }
}

Vec2
MotionComponent::floorAt (juce::Point<float> posPixel) const
{
  auto const onFloor = _engine.getHeightMap ().mapTo2D (
      pixelToDirection (posPixel), ElevationParams{});
  return { onFloor.x (), onFloor.y () };
}

std::optional<juce::Point<float> >
MotionComponent::floorToPixel (Vec2 at) const
{
  auto const direction = _engine.getHeightMap ().mapTo3D (
      Pos::fromCartesian (at.x, at.y, 0.f), ElevationParams{});
  if (!direction.isValid ())
    return std::nullopt;
  auto const screen = projectToScreen (direction);
  if (!std::isfinite (screen.x) || !std::isfinite (screen.y))
    return std::nullopt;
  return screen.transformedBy (_transformNormalizedToLocal);
}

float
MotionComponent::floorLengthInPixels (Vec2 at, float length) const
{
  auto const centre = floorToPixel (at);
  auto const alongX = floorToPixel (at + Vec2{ length, 0.f });
  auto const alongY = floorToPixel (at + Vec2{ 0.f, length });
  if (!centre || !alongX || !alongY)
    return 0.f;
  return (centre->getDistanceFrom (*alongX)
          + centre->getDistanceFrom (*alongY))
         / 2.f;
}

float
MotionComponent::blobDiameterInPixels () const
{
  // The pass's unit is the sphere's radius; a blob is 2 * _blobScale of it.
  return _blobScale * static_cast<float> (_boundsCenterRegion.getWidth ());
}

std::optional<int>
MotionComponent::bodyAt (juce::Point<float> posPixel) const
{
  std::array<BodyOnScreen, maxFlightBodies> onScreen{};
  auto count = 0;
  for (auto i = 0; i < _flightBodiesShown.count; ++i)
    {
      auto const &body = _flightBodiesShown.body[static_cast<size_t> (i)];
      auto const centre = floorToPixel (body.at);
      if (!centre)
        continue;
      onScreen[static_cast<size_t> (count++)]
          = { body.id, *centre,
              bodyHitRadius (body.mass, blobDiameterInPixels (),
                             static_cast<float> (displayFingertip ()),
                             _engine.getFlightTuning ()) };
    }
  return bodyUnderFinger (onScreen, count, posPixel);
}

void
MotionComponent::setFlightDisplay (FlightDisplay display)
{
  _flightBodiesShown = display.bodies;
  std::lock_guard<std::mutex> guard (_mutexDisplayData);
  _flightDisplay = std::move (display);
}

void
MotionComponent::setPageHeld (bool held)
{
  _pageHeld = held;
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
MotionComponent::setRecordingUnderlay (std::shared_ptr<Pattern> pattern)
{
  std::lock_guard<std::mutex> guard (_mutexUnderlay);
  _recordingUnderlay = std::move (pattern);
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
MotionComponent::setSelectedPattern (std::shared_ptr<Pattern> pattern)
{
  std::lock_guard<std::mutex> guard (_mutexDisplayData);
  _selectedPattern = std::move (pattern);
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
MotionComponent::setEnergyGrid (float const *values, int count)
{
  if (count != energyGridPointCount)
    return;

  juce::SpinLock::ScopedLockType lock{ _energyLock };
  std::copy (values, values + count, _energyIncoming.begin ());
  _energyPending = true;
}

std::vector<MapStroke> *
MotionComponent::lineStrokesFor (int channel)
{
  if (channel < 0 || channel >= 4)
    return nullptr;
  return &_lineStrokes[static_cast<std::size_t> (channel)];
}

std::vector<MapStroke> *
MotionComponent::strandStrokesFor (int channel)
{
  if (channel < 0 || channel >= 4)
    return nullptr;
  return &_strandStrokes[static_cast<std::size_t> (channel)];
}

void
MotionComponent::resetLineMaps ()
{
  for (auto &strokes : _lineStrokes)
    strokes.clear ();
  for (auto &strokes : _strandStrokes)
    strokes.clear ();
}

void
MotionComponent::uploadLineMaps ()
{
  for (auto channel = 0; channel < 4; ++channel)
    {
      auto const index = static_cast<std::size_t> (channel);
      if (_lineStrokes[index].empty ())
        {
          _sphereShader.setLineTexture (channel, 0);
          _sphereShader.setStrandTexture (channel, 0);
          continue;
        }

      _sphereShader.setLineTexture (
          channel, _lineMapRenderer.paint (channel, _lineStrokes[index]));
      _sphereShader.setStrandTexture (
          channel, _strandMapRenderer.paint (channel, _strandStrokes[index]));
    }
}

// Runs on the GL thread. The plugin only sends 9 times a second, so the map is
// eased towards each new frame rather than stepped to it.
void
MotionComponent::uploadEnergyMap ()
{
  using namespace juce::gl;

  if (_energyProjection == nullptr || _energyTexture == 0)
    return;

  {
    juce::SpinLock::ScopedTryLockType lock{ _energyLock };
    if (lock.isLocked () && _energyPending)
      {
        _energyProjection->project (_energyIncoming.data (),
                                    _energyTarget.data ());
        _energyPending = false;
      }
  }

  constexpr float dt = 1.f / 60.f;
  for (int i = 0; i < energyMapTexelCount; ++i)
    {
      _energySmoothed[static_cast<size_t> (i)] = speakerLightEnvelope (
          _energySmoothed[static_cast<size_t> (i)],
          _energyTarget[static_cast<size_t> (i)], _energyAttack, _energyDecay,
          dt);

      // The perceptual mapping happens here rather than in the shader so the
      // texture can stay 8-bit, which every GL 2.1 driver handles.
      auto const level = speakerLightLevel (
          _energySmoothed[static_cast<size_t> (i)], _energyVuMax, _energyCurve);
      _energyTexels[static_cast<size_t> (i)]
          = static_cast<unsigned char> (std::clamp (level, 0.f, 1.f) * 255.f);
    }

  glBindTexture (GL_TEXTURE_2D, _energyTexture);
  glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, energyMapWidth, energyMapHeight,
                   GL_LUMINANCE, GL_UNSIGNED_BYTE, _energyTexels.data ());
  glBindTexture (GL_TEXTURE_2D, 0);
}

void
MotionComponent::disoccludeBlobs ()
{
  // Everything in here works in the height map's own 2D space, because that is
  // what the channel's place is read in. Projecting by dropping z instead —
  // which is what the drawing does — is a different space, shorter by
  // sqrt(2), and reading in one while writing in the other once shrank every
  // untouched blob's radius by that factor per frame until it sat on the
  // centre. See HeightMapSphere.DropZRoundTripShrinksTowardsTheCentre.
  //
  // Only the drawing is pushed (#56, maintainer 2026-09-29): an untouched
  // blob gives way on the screen so the held one stays reachable, and its
  // channel stays where it is in the room. The engine is not written here.
  std::vector<juce::Point<float> > held;
  for (auto const channel : _grabs.heldChannels ())
    {
      auto const position = _engine.getChannelPosition (channel);
      if (position.isValid ())
        held.push_back (normalizedToLocal2DPosition (directionToDisc (position)));
    }

  for (auto channel = 0u; channel < _engine.getNumChannels (); ++channel)
    {
      auto &state = *_uiStates[channel];
      if (state.grabbed)
        {
          state.pushOffset = {};
          continue;
        }

      auto const position = _engine.getChannelPosition (channel);
      if (!position.isValid ())
        continue;

      state.pushOffset
          = held.empty ()
                ? easedPushOffset (state.pushOffset)
                : nextPushOffset (
                      normalizedToLocal2DPosition (directionToDisc (position)),
                      state.pushOffset, held, getActiveDistanceInPixel ());
    }
}

Pos
MotionComponent::drawnChannelPosition (index_t channel) const
{
  auto const position = _engine.getChannelPosition (channel);
  auto const &offset = _uiStates[channel]->pushOffset;
  if (!position.isValid () || offset == juce::Point<float>{})
    return position;
  return pixelToDirection (
      normalizedToLocal2DPosition (directionToDisc (position)) + offset);
}

void
MotionComponent::mouseDown (const juce::MouseEvent &event)
{
  // No blanket reset any more: a finger going down must not let go of what
  // another finger is already holding.
  auto const source = grabKey (event.source);

  // In camera mode the whole sphere turns the room: selected by the elevation
  // picture in the bar, which is in plain view while it is on. Two taps put
  // the view back where it starts, the one you want back in a hurry.
  //
  // A second finger turns the gesture into a pinch: the turning stops, and
  // the distance between the two zooms the sphere in and out.
  if (_cameraMode)
    {
      auto const at = event.getPosition ().toFloat ();
      auto const key = sourceKeyOf (event.source);

      _cameraFingers.forgetIfNotDown (isSourceDown);
      if (_cameraFingers.count () == 0)
        _cameraGrab.reset ();

      // One finger arrives twice on the device, as a touch and as X's
      // emulated mouse; only the kind that touched first counts, or the
      // second copy turned every turn into a pinch.
      auto const alone = _cameraFingers.count () == 0;
      if (!_cameraFingers.press (key, at))
        return;

      if (_cameraFingers.count () == 2)
        {
          // A pinch: whatever the first finger was deciding on the floor
          // decides nothing more.
          cancelFloorFinger ();
          _cameraGrab.reset ();
          _pinchDistanceAtStart = _cameraFingers.pinchDistance ();
          _zoomAtPinch = _cameraZoom;
          return;
        }

      if (!alone)
        return;

      // In FPV the floor is the groups': only off it (the rim, the
      // background) does a finger start the camera and count towards the
      // double tap that resets the view.
      if (_fpv && floorFingerDown (key, at))
        return;

      // Only a finger alone on the sphere counts towards two taps: the
      // second finger of a pinch lands just as quickly.
      auto const now = juce::Time::currentTimeMillis ();
      constexpr int doubleTapMs = 400;

      if (_cameraTapMs != 0 && now - _cameraTapMs < doubleTapMs)
        {
          _cameraTapMs = 0;
          _cameraZoom = 1.f;
          setCamera (defaultCamera ());
          if (onCameraChanged)
            onCameraChanged ();
          return;
        }

      _cameraTapMs = now;
      _cameraGrab = key;
      _cameraGrabbedAt = at;
      _cameraAtGrab = getCamera ();
      return;
    }

  if (_engine.isRecordingOrScheduled ())
    {
      // A recording follows one finger and has to keep following the same
      // one. Every finger writing the position would make the trajectory
      // jump between them — impossible with a single pointer, easy with ten.
      auto const wasEmpty = _grabs.empty ();
      _grabs.down (source, {});

      if (wasEmpty)
        {
          // The same projection the drag path uses. Handing the finger's
          // disc position over as a pattern coordinate read its radius in the
          // wrong space and put the blob short of the finger by 1/sqrt(2) —
          // the same mistake disoccludeBlobs once made.
          auto const posPixel = event.getPosition ().toFloat ();
          _engine.setRecording3DPosition (
              pixelToDirection (posPixel));
        }
    }
  else
    {
      // The nearest blob that is *free*. Taking one already under somebody
      // else's finger is how two fingers would end up fighting over one blob.
      auto closestIndex = getClosestFreeBlobIndexWithinRadius (
          event.getPosition ().toFloat (), getActiveDistanceInPixel ());
      _grabs.down (source, closestIndex);
      if (closestIndex.has_value ())
        {
          auto const index = closestIndex.value ();
          _uiStates[index]->grabbed = true;

          // Playback writes this channel's position on every tick, and so
          // does the drag. Holding it means the clip carries on running and
          // stops fighting the finger for where the blob is.
          _engine.setChannelPositionHeld (index, true);

          // Recover the raw 2D position via the exact inverse mapping
          // (unambiguous between front/back hemisphere) rather than
          // inverting the on-screen (orthographic) position, which would
          // be ambiguous whenever the blob is currently on the back of
          // the sphere and could snap it to the front on grab. No Pattern
          // is in scope here (this is a live/manual grab, not a clip) — use
          // whatever clip is currently playing on the channel, if any.
          // The blob jumps under the finger and stays there. Keeping the
          // offset it was grabbed at is the mouse convention, and on a panel
          // with a fat finger and a small blob it reads as the blob lagging
          // beside the finger rather than being held by it.
          _uiStates[index]->grabOffset = {};

          // Put it there now rather than on the first movement: "jumps under
          // the finger when grabbed" means when grabbed, and a touch that
          // presses without moving would otherwise leave it where it was.
          _engine.setChannel3DPosition (
              index, pixelToDirection ((
                         event.getPosition ().toFloat ())));

        }
    }
}

void
MotionComponent::mouseUp (const juce::MouseEvent &event)
{
  // Exactly the channel this finger held, and no other. Clearing them all was
  // right while there could only be one grab; with several it handed every
  // other blob back to playback mid-drag.
  if (_cameraMode)
    {
      // A pinch that loses a finger does not turn back into a turn: the
      // finger left is still wherever the pinch put it, and a view that
      // jumped from there would be a surprise.
      auto const key = sourceKeyOf (event.source);
      if (!_cameraFingers.release (key))
        return;
      if (_floorFinger == std::optional<SourceKey>{ key })
        {
          // A tap, a drag or a hold: none of it moved the view.
          auto const wasPage = _floorFingerIsPage;
          _floorFinger.reset ();
          if (!wasPage)
            carryOutFloorAction (
                _floorGesture.up (event.getPosition ().toFloat (),
                                  floorClockMs ()),
                event.getPosition ().toFloat ());
          return;
        }
      if (_cameraGrab == std::optional<SourceKey>{ key })
        _cameraGrab.reset ();
      if (onCameraChanged)
        onCameraChanged ();
      return;
    }

  auto const released = _grabs.up (grabKey (event.source));
  if (released.has_value ())
    {
      _uiStates[released.value ()]->grabbed = false;
      _engine.setChannelPositionHeld (released.value (), false);
    }

  // The recording follows whichever finger is on the screen, so it is only
  // over when the last one leaves.
  if (_grabs.empty ())
    _engine.releaseRecordingPosition ();
}

void
MotionComponent::mouseDrag (const juce::MouseEvent &event)
{
  auto const posPixel = event.getPosition ().toFloat ();

  if (_cameraMode)
    {
      auto const key = sourceKeyOf (event.source);
      if (!_cameraFingers.move (key, posPixel))
        return;

      if (_cameraFingers.count () == 2)
        {
          _cameraZoom = zoomFromPinch (_zoomAtPinch, _pinchDistanceAtStart,
                                       _cameraFingers.pinchDistance ());
          repaint ();
          return;
        }

      if (_floorFinger == std::optional<SourceKey>{ key })
        {
          if (_floorFingerIsPage)
            return;
          auto const action = _floorGesture.move (posPixel, floorClockMs ());
          if (action != FloorAction::Camera)
            {
              carryOutFloorAction (action, posPixel);
              return;
            }
          // Off the bodies and past the slop: the camera, from where the
          // finger went down (floorFingerDown kept the view then).
          _floorFinger.reset ();
          _cameraGrab = key;
        }

      // A finger that is not turning the view -- the one left over from a
      // pinch -- does nothing. In camera mode no finger takes a blob.
      if (_cameraGrab != std::optional<SourceKey>{ key })
        return;

      // Up and down leans the eye over the room, left and right walks it
      // round -- see cameraFromBallDrag(), which is where the feel of it is
      // decided and where it can be tested. The sphere's own size is the
      // scale: a sweep across it is a whole turn, its height a right angle.
      setCamera (cameraSettled (cameraFromBallDrag (
          _cameraAtGrab, posPixel - _cameraGrabbedAt, getLocalBounds ())));
      return;
    }

  auto const source = grabKey (event.source);

  if (_engine.isRecordingOrScheduled ())
    {
      // Only the finger that started it; the others are along for the ride.
      if (_grabs.firstSource () == std::optional<int>{ source })
        _engine.setRecording3DPosition (
            pixelToDirection (posPixel));
    }
  else if (auto const grabbed = _grabs.channelFor (source))
    {
      // Only this finger's channel. Another finger's blob is another finger's
      // business.
      auto const channel = grabbed.value ();
      auto const posPixelOffsetted
          = posPixel + _uiStates[channel]->grabOffset;

      // Straight into the projection the blob is drawn in, so it lands under
      // the finger. Going through the height map's 2D space instead read the
      // finger's radius as a pattern radius, where 1.0 is 45 degrees off the
      // zenith rather than the horizon — the blob came up short by 1/sqrt(2).
      auto const direction
          = pixelToDirection (posPixelOffsetted);
      _engine.setChannel3DPosition (channel, direction);
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
      // Where it is drawn, pushed aside or not: a finger aims at the blob it
      // sees.
      auto const blobPos = drawnChannelPosition (channel);
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

std::optional<index_t>
MotionComponent::getClosestFreeBlobIndexWithinRadius (
    juce::Point<float> posPixel, float radiusPixel) const
{
  // Same search, skipping what another finger already holds. Without this a
  // second finger landing near an occupied blob takes it away from the first,
  // which is the single-pointer behaviour wearing a multitouch coat.
  auto minDistance = std::numeric_limits<float>::infinity ();
  std::optional<index_t> minIndex;

  for (auto channel = 0u; channel < _engine.getNumChannels (); ++channel)
    {
      if (_grabs.isHeld (channel))
        continue;

      auto const blobPos = drawnChannelPosition (channel);
      if (!blobPos.isValid ())
        continue;

      auto const distance
          = normalizedToLocal2DPosition (blobPos).getDistanceFrom (posPixel);

      if (distance < radiusPixel && distance < minDistance)
        {
          minDistance = distance;
          minIndex = channel;
        }
    }

  return minIndex;
}

float
MotionComponent::getActiveDistanceInPixel () const
{
  return _boundsCenterRegion.getWidth () * _blobScale
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

  // The line and strand maps are painted on the GPU (a3-motion-ui#34). A GPU
  // that cannot build the program says so in the log, and the trajectories
  // then have no glow -- there is no software path any more.
  _lineMapRenderer.initialise (_glContext);
  _strandMapRenderer.initialise (_glContext);

  // Energy map from the IEM EnergyVisualizer. Folding 426 directions into the
  // map is a fixed geometry problem, so the weights are resolved once here.
  {
    auto const grid = loadEnergyGrid (resource ("EnergyVisualizerGrid.json"));

    if (grid.size () == energyGridPointCount)
      {
        _energyProjection
            = std::make_unique<EnergyMapProjection> (grid, energyMapSpreadDegrees);
        _energyTarget.assign (energyMapTexelCount, 0.f);
        _energySmoothed.assign (energyMapTexelCount, 0.f);
        _energyTexels.assign (energyMapTexelCount, 0);

        glGenTextures (1, &_energyTexture);
        glBindTexture (GL_TEXTURE_2D, _energyTexture);
        glTexImage2D (GL_TEXTURE_2D, 0, GL_LUMINANCE, energyMapWidth,
                      energyMapHeight, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                      _energyTexels.data ());
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // Azimuth wraps, elevation does not.
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture (GL_TEXTURE_2D, 0);
      }
    else
      {
        juce::Logger::writeToLog (
            "energy grid missing or wrong size — sphere stays unlit by it");
      }
  }

  _startMillis = juce::Time::getMillisecondCounter ();

  _activeSkinFile = visualConfigFile ();
  _appConfigWatcher = ConfigFileWatcher{ configFile () };
  _configWatcher = ConfigFileWatcher{ _activeSkinFile };

  auto const skin = loadActiveSkinVar (configFile (), userConfig);
  applyVisualConfig (skin);
  applyTheme (skin);
}


void
MotionComponent::applyVisualConfig (juce::var const &skin)
{
  // What the file says, with every effect its switches turn off written over
  // -- the renderers below read values that already mean "none" at zero.
  auto const config = withSkinSwitchesApplied (skin);

  // Load glow / spotlight config from config
  {
    auto cfgF = [] (const juce::var &obj, const char *key,
                    float def) -> float {
      return obj.hasProperty (key) ? static_cast<float> (obj[key]) : def;
    };

    SphereShader::GlowConfig gc;
    auto const &sg = config["backgroundGlow"];
    gc.r = cfgF (sg, "r", 230.f) / 255.f;
    gc.g = cfgF (sg, "g", 26.f) / 255.f;
    gc.b = cfgF (sg, "b", 13.f) / 255.f;
    gc.alphaMax = cfgF (sg, "alphaMax", 0.6f);
    gc.vuMax = cfgF (sg, "vuMax", 0.2f);
    gc.curve = cfgF (sg, "curve", 0.4f);
    gc.intensity = cfgF (sg, "intensity", 0.8f);
    gc.netFlow = cfgF (sg, "netFlow", -0.18f);
    gc.netReach = cfgF (sg, "reach", 2.6f);
    gc.netRise = cfgF (sg, "rise", 0.25f);
    gc.netTwist = cfgF (sg, "netTwist", 9.f);
    gc.netScale = cfgF (sg, "netScale", 7.f);
    gc.netSharpness = cfgF (sg, "netSharpness", 6.f);
    gc.netOctaves = cfgF (sg, "netOctaves", 3.f);
    gc.netLacunarity = cfgF (sg, "netLacunarity", 2.f);
    gc.netGain = cfgF (sg, "netGain", 0.5f);
    gc.attack = cfgF (sg, "attack", 0.05f);
    gc.decay = cfgF (sg, "decay", 1.2f);
    _glowAttack = gc.attack;
    _glowDecay = gc.decay;
    _sphereShader.setGlowConfig (gc);

    SphereShader::SpotlightConfig sc;
    auto const &sl = config["speakerLight"];
    sc.r = cfgF (sl, "r", 255.f) / 255.f;
    sc.g = cfgF (sl, "g", 64.f) / 255.f;
    sc.b = cfgF (sl, "b", 166.f) / 255.f;
    sc.alphaMax = cfgF (sl, "alphaMax", 0.35f);
    sc.vuMax = cfgF (sl, "vuMax", 0.2f);
    sc.curve = cfgF (sl, "curve", 0.4f);
    sc.speakerRadius = cfgF (sl, "speakerRadius", 1.55f);
    _speakerRadius = sc.speakerRadius;
    sc.edgeSoftness = cfgF (sl, "edgeSoftness", 0.7f);
    sc.beamIntensity = cfgF (sl, "beamIntensity", 0.8f);
    sc.apertureAngle = cfgF (sl, "apertureAngle", 6.f);
    sc.wrapAngle = cfgF (sl, "wrapAngle", 45.f);
    sc.wander = cfgF (sl, "wander", 14.f);
    sc.wanderTwist = cfgF (sl, "wanderTwist", 5.f);
    sc.wanderScale = cfgF (sl, "wanderScale", 4.f);
    sc.wanderFlow = cfgF (sl, "wanderFlow", 0.08f);
    sc.root = cfgF (sl, "root", 0.35f);
    sc.levelFloor = cfgF (sl, "levelFloor", 0.25f);
    sc.bleed = cfgF (sl, "bleed", 0.22f);
    sc.fray = cfgF (sl, "fray", 0.8f);
    sc.cover = cfgF (sl, "cover", 3.f);
    sc.boltWidth = cfgF (sl, "boltWidth", 0.9f);
    sc.boltThin = cfgF (sl, "boltThin", 0.3f);
    sc.beamGate = cfgF (sl, "beamGate", 0.004f);
    sc.boltWander = cfgF (sl, "boltWander", 0.55f);
    sc.boltScale = cfgF (sl, "boltScale", 6.f);
    sc.boltFlow = cfgF (sl, "boltFlow", 0.5f);
    sc.boltRate = cfgF (sl, "boltRate", 1.4f);
    sc.boltDuty = cfgF (sl, "boltDuty", 0.55f);
    sc.boltCoreExp = cfgF (sl, "boltCoreExp", 5.f);
    sc.boltCore = cfgF (sl, "boltCore", 0.9f);
    sc.boltCount = cfgF (sl, "boltCount", 6.f);
    sc.boltFewest = cfgF (sl, "boltFewest", 2.f);
    sc.boltDim = cfgF (sl, "boltDim", 0.45f);
    sc.floorLevel = cfgF (sl, "floorLevel", 1.f);
    sc.floorThrough = cfgF (sl, "floorThrough", 0.32f);
    sc.floorDark = cfgF (sl, "floorDark", 0.16f);
    sc.floorBeams = cfgF (sl, "floorBeams", 1.4f);
    sc.floorBeamInner = cfgF (sl, "floorBeamInner", 0.02f);
    sc.boxOcclude = cfgF (sl, "boxOcclude", 0.85f);
    // Off the root rather than out of the speaker-light block: it is the
    // ball's own edge, and it is grouped under Sphere in the editor.
    sc.sphereLimb = cfgF (config, "sphereLimb", 0.18f);
    sc.floorGrain = cfgF (sl, "floorGrain", 14.f);
    sc.ballLevel = cfgF (sl, "ballLevel", 1.f);
    sc.topGlow = cfgF (sl, "topGlow", 1.7f);
    sc.subGlow = cfgF (sl, "subGlow", 0.5f);
    sc.boxGlow = cfgF (sl, "boxGlow", 0.f);
    sc.ballCount = cfgF (sl, "ballCount", 3.f);
    sc.ballRate = cfgF (sl, "ballRate", 0.32f);
    sc.ballReach = cfgF (sl, "ballReach", 0.72f);
    sc.ballSize = cfgF (sl, "ballSize", 0.048f);
    sc.ballWander = cfgF (sl, "ballWander", 0.10f);
    sc.ballHeight = cfgF (sl, "ballHeight", 0.05f);
    // The panel's Bolt length when the skin has it; the shader's own value
    // otherwise, so a skin written before it draws as it did.
    sc.boltInner = sl.hasProperty ("boltLength")
                       ? speakerBoltInner (cfgF (sl, "boltLength", 0.55f))
                       : cfgF (sl, "boltInner", 0.45f);
    sc.boltEscape = cfgF (sl, "boltEscape", 0.55f);
    sc.boltBranches = cfgF (sl, "boltBranches", 2.f);
    sc.boltBranch = cfgF (sl, "boltBranch", 1.6f);
    _sphereShader.setSpotlightConfig (sc);

    auto const &energy = config["energy"];
    _energyVuMax = cfgF (energy, "vuMax", 0.05f);
    _energyCurve = cfgF (energy, "curve", 0.8f);
    _energyAttack = cfgF (energy, "attack", 0.05f);
    _energyDecay = cfgF (energy, "decay", 0.25f);

    SphereShader::EnergyConfig ec;
    ec.r = cfgF (energy, "r", 255.f) / 255.f;
    ec.g = cfgF (energy, "g", 255.f) / 255.f;
    ec.b = cfgF (energy, "b", 255.f) / 255.f;
    ec.intensity = cfgF (energy, "intensity", 1.0f);
    ec.netIntensity = cfgF (energy, "netIntensity", 0.8f);
    ec.netScale = cfgF (energy, "netScale", 6.f);
    ec.netSharpness = cfgF (energy, "netSharpness", 8.f);
    ec.netFlow = cfgF (energy, "netFlow", 0.15f);
    ec.netBeamIntensity = cfgF (energy, "netBeamIntensity", 1.5f);
    ec.netTwist = cfgF (energy, "netTwist", 9.f);
    ec.netOctaves = cfgF (energy, "netOctaves", 3.f);
    ec.netLacunarity = cfgF (energy, "netLacunarity", 2.f);
    ec.netGain = cfgF (energy, "netGain", 0.5f);
    _sphereShader.setEnergyConfig (ec);
    _sphereShader.setEnergyTexture (_energyTexture);

    // Top level of the skin, which is where they are written — they used to
    // be read from config["ui"], a place the skin has nothing in, so both
    // silently kept their built-in defaults. Nobody noticed because the
    // shipped skin says exactly what those defaults are.
    _sphereScale = cfgF (config, "sphereScale", reduceFactorCircleDefault);
    _blobScale = cfgF (config["blob"], "scale", reduceFactorBlobsDefault);

    auto const underlay = config["recordingUnderlay"];
    _underlayOpacity = cfgF (underlay, "opacity", 0.28f);
    _underlayBlobScale = cfgF (underlay, "blobScale", 0.55f);
    _underlayLineThickness = cfgF (underlay, "lineThickness", 0.02f);

    _spotAttack = cfgF (sl, "attack", 0.08f);
    _spotDecay = cfgF (sl, "decay", 0.4f);
  }

  // Cache corona config (avoids JSON lookups every frame per blob).
  _coronaCfg = loadCoronaConfig (config);
}

// Visual tuning is judged by eye, so the values get changed a lot. Picking up
// config.json while running turns a rebuild-and-restart cycle into a file save.
// Runs on the GL thread, throttled to roughly once a second, so no handoff from
// the message thread is needed.
// Loading the theme is the message thread's job — the 2D components read it
// while painting — so the repaint goes through the message manager rather than
// being called from here on the GL thread.
void
MotionComponent::applyTheme (juce::var const &skin)
{
  auto const loaded = loadTheme (skin);

  juce::Component::SafePointer<MotionComponent> safeThis{ this };
  juce::MessageManager::callAsync ([safeThis, loaded] {
    if (safeThis != nullptr)
      applyThemeEverywhere (loaded, *safeThis);
  });
}

void
MotionComponent::reloadVisualConfigIfChanged ()
{
  auto skinChanged = false;

  // config.json first: it may have named a different skin, and then the second
  // watcher has to follow before it is asked anything.
  if (_appConfigWatcher.hasChanged ())
    {
      juce::var config;
      if (juce::JSON::parse (configFile ().loadFileAsString (), config)
              .wasOk ())
        {
          // The whole config, not just the skin's name. userConfig was
          // parsed once at startup and never again, so everything read from
          // it at runtime — the function keys' LED colours above all — kept
          // the values the app had booted with, and editing them in the menu
          // changed the file and nothing else.
          //
          // Handed over on the message thread, which is where it is read.
          juce::Component::SafePointer<MotionComponent> safeThis{ this };
          juce::MessageManager::callAsync ([safeThis, config] {
            userConfig = config;

            // Everything that was read out of the file at startup has to be
            // told, not only the visuals this component owns.
            if (safeThis != nullptr && safeThis->onAppConfigReloaded)
              safeThis->onAppConfigReloaded (config);
          });

          auto const named = skinFile (configFile ().getParentDirectory (),
                                       config["ui"]["skin"].toString ());
          if (named != _activeSkinFile)
            {
              _activeSkinFile = named;
              _configWatcher = ConfigFileWatcher{ named };
              skinChanged = true;
              juce::Logger::writeToLog ("skin is now "
                                        + named.getFileName ());
            }
        }
    }

  if (!_configWatcher.hasChanged () && !skinChanged)
    return;

  juce::var parsed;
  if (juce::JSON::parse (_activeSkinFile.loadFileAsString (), parsed).failed ())
    return; // half-written save — the next check picks up the finished file

  // Through the rename like every other read. Skipped here, a file still
  // carrying the old spellings loses its groups entirely and the sphere falls
  // back to built-in defaults — which looks like a working skin, only wrong.
  auto const skin = migrateSkinNames (parsed);

  applyVisualConfig (skin);
  applyTheme (skin);
  juce::Logger::writeToLog ("reloaded " + _activeSkinFile.getFileName ());
}

void
MotionComponent::renderOpenGL ()
{
  using namespace juce::gl;
  using juce::OpenGLHelpers;

  jassert (OpenGLHelpers::isContextActive ());

  if (_tracesFrames)
    {
      auto const line = _frameRate.tick (
          juce::Time::getMillisecondCounterHiRes () * 0.001);
      if (line.isNotEmpty ())
        juce::Logger::writeToLog (line);
    }
  _glContext.setSwapInterval (1);  // vsync @ 60 Hz — frees CPU for timer thread

  updateBoundsAndTransform ();

  if (_frameCount % 60 == 0)
    reloadVisualConfigIfChanged ();

  uploadEnergyMap ();
  _sphereShader.setEnergyTexture (_energyTexture);
  // Last frame's maps: the 2D pass that fills them runs after this one.
  uploadLineMaps ();
  _sphereShader.setLineExtent (lineMapExtent);
  // Seconds since this context came up, not since the machine booted: the
  // uniform is a float, and on an installation left running for a week the
  // per-frame increment falls below what it can still represent, freezing the
  // net in place.
  _sphereShader.setTime (
      static_cast<float> (juce::Time::getMillisecondCounter () - _startMillis)
      * 0.001f);

  // Clear background first
  OpenGLHelpers::clear (Colours::background ());

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

    // The glow is the room rather than an event, so it gets its own envelope
    // and outlasts the bolts striking in front of it.
    _smoothGlowRms = speakerLightEnvelope (_smoothGlowRms,
                                           _vuSphereGlowRms.load (),
                                           _glowAttack, _glowDecay, dt);
    for (int i = 0; i < 4; ++i)
      {
        smoothFixed (_smoothSpotPeak[i], _vuSpeakerPeak[i].load ());

        // The beams get their own configurable envelope: smoothFixed rises in
        // two frames and falls in seven, which turns even an rms input back
        // into a peak follower.
        _smoothSpotRms[i] = speakerLightEnvelope (
            _smoothSpotRms[i], _vuSpeakerRms[i].load (), _spotAttack,
            _spotDecay, dt);
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
        // From the raw peak: a hit is a change, and smoothing it first would
        // take the change out.
        _blobPunch[static_cast<size_t> (ch)].update (_uiStates[ch]->vuPeak.load (), dt);
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
        // Drawn where it is pushed to (#56); the channel itself is not moved.
        auto const position = drawnChannelPosition (static_cast<index_t> (ch));
        if (position.isValid ())
          {
            auto posJuce = projectToScreen (position);
            // JUCE 2D: Y down. Shader: Y up. Flip Y.
            bd.x = posJuce.getX ();
            bd.y = -posJuce.getY ();
            // FPV draws ships in the 2D pass instead; an invisible blob
            // lets its trail go, as for a channel without a position.
            bd.visible = !_fpv;
          }

        // The wake follows the blob in the space it is drawn in, not the
        // room's: the camera can be walked round the sphere, and a trail
        // advanced in room coordinates would swing as the view turned.
        if (bd.visible)
          advanceBlobTrail (_blobTrails[ch], bd.x, bd.y, blobTrailLag,
                            blobTrailCutDistance);
        else
          releaseBlobTrail (_blobTrails[ch]);

        for (int k = 0; k < BlobTrail::numLinks; ++k)
          {
            bd.trailX[k] = _blobTrails[ch].x[k];
            bd.trailY[k] = _blobTrails[ch].y[k];
          }

        // Two sizes and no third, and the held one is the drawn mark rather
        // than the hit radius. It used to be activeAreaAroundBlobFactor, so a
        // blob under a finger was drawn at the full size of the area that
        // catches it -- three times itself. What a finger has to hit is
        // unchanged; see getActiveDistanceInPixel().
        auto blobSize = _blobScale * blobDrawScale (_uiStates[ch]->grabbed,
                                                    _coronaCfg);

        // Elevation is perspective, not a third size: a blob overhead is
        // nearer the eye than one at the horizon.
        if (position.isValid ())
          blobSize *= (1.f + std::clamp (position.z (), 0.f, 1.f) * 0.7f);
        bd.size = blobSize;

        auto col = _uiStates[ch]->colour;
        bd.r = col.getFloatRed ();
        bd.g = col.getFloatGreen ();
        bd.b = col.getFloatBlue ();
        // On a scale, not raw. The blob was a 2D disc once and took its
        // level through coronaVuLevel(); moving it into the shader left the
        // scaling behind, so what reached it was whatever the meter read —
        // and a meter reads small. Real material measured at the rig peaks
        // around 0.3, which is barely over the threshold the blob's own bolt
        // needs, so most of what it can do never came out. vuMax is what says
        // how loud "loud" is on this rig.
        auto const blobLevel = coronaVuLevel (
            _smoothBlobPeak[ch], _smoothBlobRms[ch], _coronaCfg.vuMax);
        bd.vuPeak = blobLevel;

        // How far the corona reaches, from the skin's own sizeMin..sizeMax
        // rather than a ramp written into the shader. This is the part of the
        // blob that says "level" at a glance -- the sparks and the bolt are
        // detail you have to be looking at it to catch.
        bd.corona = coronaScaleFactor (blobLevel, _coronaCfg);
        bd.vuRms = _smoothBlobRms[ch];
        bd.punch = _blobPunch[static_cast<size_t> (ch)].value ();
        bd.grabbed = _uiStates[ch]->grabbed;

        // What the blob wears while an action runs. From the engine rather
        // than from whether a finger is down: the accent outlives the hand by
        // its decay, and so does the action's hold on the clip's settings.
        bd.action = _engine.isChannelAccentActive (ch) ? 1.f : 0.f;

        // Depth. The sphere is semi-transparent, so a blob behind it is dimmed
        // rather than hidden. The same rule the trajectory goes behind the
        // ball by, written once: this carried its own copy of the arithmetic,
        // and two copies of a fade are two things to forget to change.
        bd.depthFade
            = position.isValid () ? lineDepthFade (position.z ()) : 1.f;

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

    auto const screenW = static_cast<int> (vpW * scale);
    auto const screenH = static_cast<int> (vpH * scale);
    auto const superW = screenW * sphereSupersample;
    auto const superH = screenH * sphereSupersample;

    if (_superBuffer.getWidth () != superW || _superBuffer.getHeight () != superH)
      {
        _superBuffer.release ();
        _superBuffer.initialise (_glContext, superW, superH);
      }

    if (_superBuffer.isValid ())
      {
        // Cleared to the background rather than to nothing: the sphere pass
        // blends itself over whatever is behind it, and the buffer is drawn
        // back down opaque.
        _superBuffer.makeCurrentAndClear ();
        OpenGLHelpers::clear (Colours::background ());

        glViewport (0, 0, superW, superH);
        auto const ss = static_cast<float> (sphereSupersample);
        _sphereShader.draw (superW, superH, radius * ss, centreX * ss,
                            centreY * ss);

        _superBuffer.releaseAsRenderingTarget ();
        glBindFramebuffer (GL_FRAMEBUFFER, 0);

        // The filter is the whole point: sampled at the centre of a screen
        // pixel, a texture twice as fine lands exactly between four texels,
        // so the bilinear tap *is* the box average of the four samples.
        glBindTexture (GL_TEXTURE_2D, _superBuffer.getTextureID ());
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture (GL_TEXTURE_2D, 0);

        _blit.blit (_superBuffer.getTextureID (), screenW, screenH);
      }
    else
      {
        glViewport (0, 0, screenW, screenH);
        _sphereShader.draw (screenW, screenH, radius, centreX, centreY);
      }
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
    auto const selected = _selectedPattern;
    // Copy-assigned into a member so the vectors keep their capacity: no
    // allocation per frame on this thread.
    if (_fpv)
      _flightDisplayDrawn = _flightDisplay;
    else
      {
        _flightDisplayDrawn.guide.clear ();
        _flightDisplayDrawn.bodies.count = 0;
      }
    auto const &flightDisplay = _flightDisplayDrawn;
    _mutexDisplayData.unlock ();

    ++_frameCount;

    if (_imageBlend)
      {
        _imageBlend->clear (_imageBlend->getBounds ());

        // Under the ships, and in pixels (see drawFlight).
        {
          juce::Graphics gPixels{ *_imageBlend };
          drawFlight (gPixels, flightDisplay);
        }

        {
          juce::Graphics gFBO{ *_imageBlend };
          gFBO.addTransform (_transformNormalizedToLocal);

          drawCircle (gFBO);
          drawShips (gFBO);

          drawBearings (gFBO);
          drawListener (gFBO);

          // The blobs are the shader's now -- blobLight(), drawn additively
          // over the finished scene: a hot core, a corona that follows the
          // level, sparks, a bolt on transients and the action's neon ring.
          //
          // This drew a flat 2D disc with two corona rings. It was taken out
          // once before, on the assumption that the shader already drew blobs
          // because its header said so; it did not, and the blobs vanished.
          // The order that works is the other one: make the shader draw first,
          // look at it, then take this away.

          // Before anything writes into them: the trail being played in goes
          // into the same per-channel maps the playing lines use, so the
          // clearing has to happen ahead of it rather than between the two.
          resetLineMaps ();

          // The take as it stands, while it is being played in. A fresh
          // recording has no display path — those come from the library — so
          // it is drawn from its own ticks.
          std::shared_ptr<Pattern> underlay;
          {
            std::lock_guard<std::mutex> guard (_mutexUnderlay);
            underlay = _recordingUnderlay;
          }
          if (showsRecordingUnderlay (_engine.getRecMode (),
                                      _engine.isRecording (),
                                      underlay != nullptr))
            drawRecordingUnderlay (*underlay, gFBO);

          if (auto const recording = _engine.getRecordingPattern ())
            if (_engine.isRecording ())
              drawRecordingTrail (*recording, gFBO);

          // Pattern preview paths
          for (auto &[pattern, displayData] : patternsPreview)
            drawPatternPreview (*pattern, displayData, gFBO);

          // The selected clip's own preview while it is not playing -- see
          // clipsToDraw(). Playing, it is drawn below with the others.
          if (selected && patternsPreview.count (selected) == 0
              && !patternIsRunning (selected->getStatus ()))
            if (auto const found = patternsDisplayData.find (selected);
                found != patternsDisplayData.end ())
              drawPatternPreview (*selected, found->second, gFBO);

          // Faint trajectory lines for all currently playing patterns
          // (skip those already drawn as explicit previews)
          for (auto &[pattern, displayData] : patternsDisplayData)
            {
              if (patternsPreview.count (pattern) > 0)
                continue; // already drawn as preview
              // Counted by hand here until 2026-09-24, and it counted one
              // status short: a clip asked to stop goes on playing until the
              // downbeat it was asked to stop on, and its line vanished the
              // instant the key went down. The blob kept travelling along a
              // line that was no longer drawn.
              if (patternIsRunning (pattern->getStatus ()))
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
MotionComponent::setSphereScalePreview (float scale)
{
  _sphereScale = scale;
}

void
MotionComponent::setSphereLeftInset (int pixels)
{
  _sphereLeftInset = juce::jmax (0, pixels);
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
  // The camera's zoom on top of the skin's sphere size: camera mode's wheel
  // and pinch make the sphere bigger or smaller, and everything drawn on it
  // follows because everything is placed through this region.
  auto const scale = _sphereScale * _cameraZoom;
  _boundsCenterRegion = sphereRegion (
      _boundsRender, static_cast<int> (shorterSideLength * scale),
      _sphereLeftInset.load (), speakerSceneReach (_speakerRadius.load ()));

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
MotionComponent::setCameraMode (bool on)
{
  _cameraMode = on;
  _cameraTapMs = 0;
  _cameraFingers.clear ();
  cancelFloorFinger ();
  if (!on)
    _cameraGrab.reset ();
  else
    releaseBlobGrabs ();
}

void
MotionComponent::releaseBlobGrabs ()
{
  // A finger lifted in camera mode never reaches the blob path of mouseUp,
  // so whatever it held would stay held -- and a take would keep its last
  // position -- until grabbed again in FULL. Let go of all of it here.
  // Only when a finger was down: with none, the recording position is not
  // the sphere's to release.
  if (_grabs.empty ())
    return;
  for (auto const channel : _grabs.releaseAll ())
    {
      _uiStates[channel]->grabbed = false;
      _engine.setChannelPositionHeld (channel, false);
    }
  _engine.releaseRecordingPosition ();
}

void
MotionComponent::setFpv (bool on)
{
  if (on == _fpv)
    return;
  // FPV turns the camera and never takes a blob; FULL gets back whatever
  // the elevation picture had set.
  if (on)
    {
      _cameraModeBeforeFpv = _cameraMode;
      setCameraMode (true);
    }
  else
    setCameraMode (_cameraModeBeforeFpv);
  // The headings belong to the GL thread; it forgets them on its next frame.
  _resetShipHeadings = true;
  _fpv = on;
}

float
MotionComponent::getCameraZoom () const
{
  return _cameraZoom;
}

void
MotionComponent::setCameraZoom (float zoom)
{
  _cameraZoom = std::clamp (zoom, minCameraZoom, maxCameraZoom);
  repaint ();
}

void
MotionComponent::mouseWheelMove (juce::MouseEvent const &,
                                 juce::MouseWheelDetails const &wheel)
{
  if (!_cameraMode)
    return;

  _cameraZoom = zoomFromWheel (_cameraZoom, wheel.deltaY);
  repaint ();
  if (onCameraChanged)
    onCameraChanged ();
}

void
MotionComponent::mouseMagnify (juce::MouseEvent const &, float scaleFactor)
{
  if (!_cameraMode)
    return;

  _cameraZoom = zoomFromPinch (_cameraZoom, 1.f, scaleFactor);
  repaint ();
  if (onCameraChanged)
    onCameraChanged ();
}

SphereCamera
MotionComponent::getCamera () const
{
  return _sphereShader.getCamera ();
}

void
MotionComponent::setCamera (SphereCamera const &camera)
{
  // The shader holds it, because the ball and its graticule have to turn with
  // the trajectories drawn over them and the shader is what draws the ball.
  // One copy, one truth.
  _sphereShader.setCamera (camera);
  repaint ();
}

/** And back: where a pixel of the view points, in the room's own terms.
 *
 *  The disc gives the direction *as seen*; the room is what the engine is
 *  told about, so it has to be handed back. Without this a blob dragged on a
 *  tilted view goes where the finger is on the screen and not where it is in
 *  the room -- which is the one thing a drag has to get right. */
Pos
MotionComponent::pixelToDirection (juce::Point<float> const &posPixel) const
{
  return asSeenFromInverse (
      discToDirection (localToNormalized2DPosition (posPixel)),
      _sphereShader.getCamera ());
}

/** Where a direction in the room lands on the screen, from where the room is
 *  being looked at.
 *
 *  Every projection in this file goes through here. Seven of them applied
 *  cartesian2DHOA2JUCE by hand, and a camera applied at six of the seven is a
 *  picture whose halves disagree about the view -- which is worse than one
 *  that cannot turn at all. */
juce::Point<float>
MotionComponent::projectToScreen (Pos const &direction) const
{
  return cartesian2DHOA2JUCE (
      asSeenFrom (direction, _sphereShader.getCamera ()));
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

  // The speakers used to be drawn here: a flat SVG arrow at a fixed screen
  // angle, four of them at 45 degrees apart on the *display*. They are
  // raytraced in the shader now (speakerBoxes) as cabinets standing in the
  // room, so they turn and lean with it, face the listener, and occlude the
  // ball and are occluded by it.
  //
  // That was the whole of the maintainer's "die speaker müssen mitdrehen wenn
  // die sphäre dreht": nailed to the glass, they stayed in the corners of the
  // screen while everything else turned. The four beam directions were written
  // in as the screen's own diagonals for the same reason and have been given
  // the room's bearings too -- though the bands themselves are still a flat
  // annulus and only follow a walk, not a lean.

  g.setOpacity (1.f);
}

/** FPV's ships, one per channel, where the shader draws blobs in FULL.
 *
 *  Drawn in the 2D pass's own units -- the sphere's radius is 1, the same
 *  space the underlay blob is sized in -- so a ship is shipLengthOfBlob blob
 *  diameters long and zooms with the sphere. It points along its last
 *  movement on the screen, which is where it flies as seen from here. */
void
MotionComponent::drawShips (juce::Graphics &g)
{
  if (_resetShipHeadings.exchange (false))
    for (auto &heading : _shipHeadings)
      heading.lose ();

  if (!_fpv || _boundsCenterRegion.getWidth () <= 0)
    return;

  auto const length = 2.f * _blobScale * shipLengthOfBlob;
  // The theme's stroke is in pixels; the pass is scaled by the sphere's
  // radius in pixels.
  auto const outline = theme ().strokeThin * 2.f
                       / static_cast<float> (_boundsCenterRegion.getWidth ());

  for (index_t ch = 0;
       ch < _engine.getNumChannels () && ch < _shipHeadings.size (); ++ch)
    {
      auto const position = drawnChannelPosition (ch);
      auto const at = position.isValid () ? projectToScreen (position)
                                           : juce::Point<float>{};
      if (!position.isValid () || !std::isfinite (at.x)
          || !std::isfinite (at.y))
        {
          _shipHeadings[ch].lose ();
          continue;
        }

      _shipHeadings[ch].update (at, length * shipStepOfLength);
      auto const ship = shipPath (at, _shipHeadings[ch].radians (), length);
      g.setColour (_uiStates[ch]->colour);
      g.fillPath (ship);
      g.setColour (toColour (theme ().textPrimary, theme ().alphaOutline));
      g.strokePath (ship, juce::PathStrokeType (outline));
    }
}

void
MotionComponent::drawFlight (juce::Graphics &g, FlightDisplay const &display)
{
  if (!_fpv || _boundsCenterRegion.getWidth () <= 0)
    return;

  auto const &tuning = _engine.getFlightTuning ();
  auto const stroke = theme ().strokeThin;

  // The big path: a faint closed line.
  {
    juce::Path guide;
    for (auto const &point : display.guide)
      if (auto const at = floorToPixel (point))
        {
          if (guide.isEmpty ())
            guide.startNewSubPath (*at);
          else
            guide.lineTo (*at);
        }
    if (!guide.isEmpty ())
      {
        guide.closeSubPath ();
        g.setColour (toColour (theme ().textPrimary, theme ().alphaGuide));
        g.strokePath (guide, juce::PathStrokeType (stroke));
      }
  }

  auto const bodyWithId = [&display] (int id) -> FlightBody const * {
    for (auto i = 0; i < display.bodies.count; ++i)
      if (display.bodies.body[static_cast<size_t> (i)].id == id)
        return &display.bodies.body[static_cast<size_t> (i)];
    return nullptr;
  };

  // A thin line from each escorting ship to its group, in the ship's colour.
  for (index_t ch = 0; ch < display.escort.size () && ch < _engine.getNumChannels ();
       ++ch)
    {
      auto const *body = bodyWithId (display.escort[ch]);
      auto const ship = drawnChannelPosition (ch);
      if (body == nullptr || !ship.isValid ())
        continue;
      auto const from
          = projectToScreen (ship).transformedBy (_transformNormalizedToLocal);
      auto const to = floorToPixel (body->at);
      if (!to || !std::isfinite (from.x) || !std::isfinite (from.y))
        continue;
      g.setColour (_uiStates[ch]->colour.withMultipliedAlpha (
          theme ().alphaGuide));
      g.drawLine ({ from, *to }, stroke);
    }

  auto const blob = blobDiameterInPixels ();
  auto const holdBody = _holdBody.load ();
  for (auto i = 0; i < display.bodies.count; ++i)
    {
      auto const &body = display.bodies.body[static_cast<size_t> (i)];
      auto const centre = floorToPixel (body.at);
      if (!centre)
        continue;

      auto const ring = bodyRole (body.mass) == BodyRole::Repel
                            ? tuning.deadZoneClearance
                            : escortRadius (body.mass, tuning);

      BodyPaint paint;
      paint.centre = *centre;
      paint.radius = bodyRadius (body.mass, blob, tuning);
      paint.mass = body.mass;
      paint.label = bodyLabel (body.id);
      paint.pulse = display.pulse;
      paint.ringRadius = floorLengthInPixels (body.at, ring);
      paint.holdProgress = body.id == holdBody ? _holdProgress.load () : 0.f;
      paint.stroke = stroke;
      paint.fontHeight = theme ().fontSize (FontRole::Body);
      paintBody (g, paint);
    }
}

/** The four bearings, written round the rim.
 *
 *  A room seen from above has no up in it, and once the view can be turned it
 *  has no fixed up either: the only way to know which way you are looking is
 *  to be told. Nought is the front of the room, which is where the OSC sends
 *  a channel at azimuth nought -- the numbers on the ring are the numbers on
 *  the wire.
 */
void
MotionComponent::drawBearings (juce::Graphics &g)
{
  static_assert (bearingLabelRadius < bearingTickInner,
                 "the numbers dock to the equator, the ticks sit outside them");

  // A graduated ring round the outside of the sphere, which is how a chart, a
  // compass and every globe worth reading does it -- rather than four numbers
  // floating on the ball itself, which is what this was and which put a
  // rotated glyph over whatever happened to be under it.
  //
  // The ring is a bezel: it takes the turn and ignores the lean, so it stays a
  // compass however far the room is tipped. Tipping the room does not change
  // which way north is, and a compass that leant over with the view would be
  // one more thing to read rather than the thing you read everything else off.
  auto const camera = _sphereShader.getCamera ();
  auto const turn = camera.turn;

  auto const on = [turn] (float degrees, float radius) {
    auto const a = degrees * pi<float> () / 180.f - turn;
    // The overhead convention: the room's front up the screen, its left to the
    // left. Written out rather than projected, because a bezel is flat.
    return juce::Point<float> (-std::sin (a) * radius, -std::cos (a) * radius);
  };

  // The numbers sit against the equator and the ticks outside them, which is
  // the other way round from a ship's compass card and deliberate: the equator
  // is the line the numbers *name*, and a bearing read off a ring that floats
  // clear of it is a bearing you have to carry across a gap. Docked to the
  // line, the number and the place it marks are the same glance.
  for (int degrees = 0; degrees < 360; degrees += 10)
    {
      auto const major = degrees % 90 == 0;
      auto const medium = degrees % 30 == 0;

      auto const from = on (static_cast<float> (degrees), bearingTickInner);
      auto const to = on (static_cast<float> (degrees),
                          major   ? bearingTickInner + 0.070f
                          : medium ? bearingTickInner + 0.050f
                                   : bearingTickInner + 0.030f);

      g.setColour (toColour (theme ().textPrimary,
                             major ? theme ().alphaMuted
                                   : medium ? theme ().alphaFillEmphasis
                                            : theme ().alphaOutline));
      auto constexpr tickWidthMajor = 0.01f;
      auto constexpr tickWidthMinor = 0.006f;
      g.drawLine (from.x, from.y, to.x, to.y,
                  major ? tickWidthMajor : tickWidthMinor);
    }

  // And the four numbers outside the ticks, upright: a compass card is read
  // at a glance and a glance does not tilt its head. Nought is the front of
  // the room, which is where the OSC sends a channel at azimuth nought -- the
  // numbers on the ring are the numbers on the wire.
  struct
  {
    float degrees;
    char const *label;
  } const marks[]{ { 0.f, "0" }, { 90.f, "90" }, { -90.f, "-90" },
                   { 180.f, "180" } };

  g.setFont (juce::Font (0.072f, juce::Font::plain));

  for (auto const &mark : marks)
    {
      auto const out = on (mark.degrees, bearingLabelRadius);
      auto const box = juce::Rectangle<float> (0.36f, 0.1f).withCentre (out);

      g.setColour (toColour (theme ().textPrimary, theme ().alphaInactive));
      g.drawText (mark.label, box, juce::Justification::centred, false);
    }
}

/** The listener, in the middle of the room they are listening to.
 *
 *  The same figure the little sphere in the corner carries, so the two say the
 *  same thing in the same words: which way the room is turned is which way
 *  they face, and how far it is tipped is how much of them you can see. */
void
MotionComponent::drawListener (juce::Graphics &g)
{
  // Big enough to read as a person from a metre away, small enough that the
  // room is still the subject: the trajectories run round them, not over them.
  auto const &figure
      = _listenerFigure.silhouette (_sphereShader.getCamera (), 0.30f);
  if (figure.isEmpty ())
    return;

  // Lit rather than dark: the room is dark, so a dark figure in the middle of
  // it is a hole. Soft enough that the trajectories running past keep the eye.
  g.setColour (toColour (theme ().textPrimary, theme ().alphaOutline));
  g.fillPath (figure);
  g.setColour (toColour (theme ().textPrimary, theme ().alphaMuted));
  g.strokePath (figure, juce::PathStrokeType (0.005f));
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
                  juce::Graphics &g,
                  PlaneShaping const &shaping,
                  SphereCamera const &camera,
                  /** The clip's lean in the room -- spaceTurnOf(). */
                  SpaceTurn turn,
                  /** Where this line's strokes for the shader's map are
                   *  collected, or nullptr for a line the glow is not asked
                   *  to follow. */
                  std::vector<MapStroke> *lineStrokes = nullptr,
                  /** Where the braid's strokes for the strand map are
                   *  collected, or nullptr for a line whose strands are
                   *  stroked here. */
                  std::vector<MapStroke> *strandStrokes = nullptr)
{
  if (displayPath.isEmpty ())
    return;


  // z >= 0 (elevation >= 50%, i.e. at/above the horizon) is always fully
  // visible; below that it fades toward the far/south pole so it reads as
  // "behind" the sphere. fadeByDepth == false skips this entirely — used
  // for whichever trajectory is currently being edited, which must stay
  // fully legible no matter where it sits.
  auto fadeForZ = [] (float z) -> float { return lineDepthFade (z); };

  // Where the line runs, as the camera sees it: projectLine() is the one
  // place that decides, for this and for the GPU pass (a3-motion-ui#34).
  auto const onSphere = projectLine (displayPath, elevationParams, heightMap,
                                 shaping, camera, turn);

  if (onSphere.points.size () < 2)
    return;

  // Three hairlines braided into a cord.
  //
  // Vectors, on purpose, and this is the third attempt. A hairline *is* a
  // vector: a stroke can be a crisp line thinner than a pixel and a field
  // sampled from a map two and a half screen pixels a texel cannot. The
  // plasma is the other way round, so the two split the work -- the cord is
  // drawn here, the light around it is the shader's, and the same twist runs
  // through both.
  //
  // The first attempt fanned into straight grey spokes at the pole, where a
  // figure's azimuths all meet and an offset copy of a curve folds;
  // foldGuard() is the answer to that. The second looked cheap, and it looked
  // cheap because the commit that built it deleted the line map at the same
  // time -- the cord was drawn with no glow at all around it.

  // The braid is built for every line, and only *where it is drawn* differs.
  // Without a map — the recording trail, the previews — it is stroked here and
  // is the whole line. With one, the strands go into a map of their own and
  // the shader draws them: stroked here as vectors they pasted a raw line over
  // the finished frame, above depth and above the towers, and the blob could
  // not travel inside a braid it was drawn underneath.
  if (lineStrokes == nullptr || strandStrokes != nullptr)
  {
    auto const seconds
        = static_cast<float> (juce::Time::getMillisecondCounter ()) * 0.001f;

    SheathRing const braid{ theme ().braidRadius, theme ().braidTurns,
                            theme ().braidSpin,
                            juce::roundToInt (theme ().braidStrands) };

    // Where the strands run, and which pieces go in which tier, is
    // braidCord()'s: the strand map and the visible braid draw from it.
    auto const cord = braidCord (onSphere, braid, seconds);

    if (lineStrokes != nullptr)
      {
        auto strokes = strandMapStrokes (cord);
        strandStrokes->insert (strandStrokes->end (),
                               std::make_move_iterator (strokes.begin ()),
                               std::make_move_iterator (strokes.end ()));
      }
    else
      {
        // Back to front, so a strand passes behind the cord and comes out
        // the other side. See strandMapStrokes() for the same order.
        for (auto tier = 0; tier < BraidCord::tiers; ++tier)
          {
            auto const front
                = BraidCord::tiers > 1
                      ? static_cast<float> (tier)
                            / static_cast<float> (BraidCord::tiers - 1)
                      : 1.f;
            for (auto band = 0; band < BraidCord::bands; ++band)
              {
                auto const &piece
                    = cord.pieces[static_cast<std::size_t> (band)]
                                 [static_cast<std::size_t> (tier)];
                if (piece.points.size () < 2)
                  continue;

                auto const fade
                    = fadeByDepth
                          ? fadeForZ (band <= 1 ? (band == 0 ? -0.75f : -0.25f)
                                                : (band == 2 ? 0.25f : 0.75f))
                          : 1.0f;

                g.setColour (colour.brighter (0.30f * front * front)
                                 .withAlpha (juce::jlimit (
                                     0.f, 1.f,
                                     alpha * fade * (0.55f + 0.45f * front))));
                // Round caps: the pieces of one tier are a winding apart and
                // share only a stitched point, so the beading rule does not
                // bite here -- and butt caps end square to the last segment
                // rather than to the joint, which on a curve leaves a
                // hairline wedge at every piece.
                g.strokePath (pathOf (piece.points, piece.lifts),
                              juce::PathStrokeType (
                                  lineThickness,
                                  juce::PathStrokeType::JointStyle::curved,
                                  juce::PathStrokeType::EndCapStyle::rounded));
              }
          }
      }
  }

  // ── The map the shader finds this line through ──────────────────
  //
  // Lost once already: the braid was built by replacing the block this sits
  // in, and it went with it -- the trajectory ran for two commits with no glow
  // at all, which is most of what "das sieht jetzt wieder billig aus" was
  // looking at. Kept at the end of the function and said out loud here.
  if (lineStrokes == nullptr || onSphere.points.size () < 2)
    return;

  // What is painted into the map, and in which order, is lineMapStrokes()'s:
  // the GPU pass paints the same list (a3-motion-ui#34).
  auto strokes = lineMapStrokes (onSphere);
  lineStrokes->insert (lineStrokes->end (),
                       std::make_move_iterator (strokes.begin ()),
                       std::make_move_iterator (strokes.end ()));
}

void
MotionComponent::drawRecordingTrail (Pattern const &pattern, juce::Graphics &g)
{
  auto const ticks = pattern.getTicks ();
  if (ticks.positions.empty ())
    return;

  auto const ch = pattern.getChannel ();
  if (ch >= _uiStates.size ())
    return;

  // One subpath per run of ticks that is actually travelled through. What has
  // not been played is simply absent — plainer than a faint line, and it is
  // the thing you are looking for while recording: where the gaps still are.
  // The blob is the write head; it needs no mark of its own.
  //
  // Cut at teleports as well as at gaps. Tapping quickly leaves no gap to cut
  // at: the write head advances a tick or two between two taps, so the last
  // tick of one and the first of the next are neighbours and the run carried
  // straight on through, drawing the jump as a line.
  juce::Path path;

  //
  // Thinned before it is drawn -- see trailPoints(). As many points as a saved
  // take keeps (PatternFile), so the line while recording is the line that
  // will be saved, at a quarter of the cost of drawing every tick.
  constexpr size_t trailMaxPoints = 128;
  for (auto const &segment : trajectorySegments (ticks.positions, BridgePlan{}))
    {
      auto const points = trailPoints (segment, trailMaxPoints);
      if (points.empty ())
        continue;
      path.startNewSubPath (points.front ().x (), points.front ().y ());
      for (size_t i = 1; i < points.size (); ++i)
        path.lineTo (points[i].x (), points[i].y ());
    }

  // The take as it is being played in, at the same width as a played one:
  // this is the same line, and it had its own constant for the same reason
  // drawPlayingTrajectory() did.
  auto const lineThickness = theme ().trajectoryThickness;
  // Unshaped: a take is recorded in the frame it was played in. Turning or
  // squeezing the trail under the finger would draw the take somewhere the
  // finger never was.
  // Through the shader, like a playing line: the same line map and strand map
  // the channel's played trajectory uses. It was the one trajectory still
  // drawn as bare vectors, and against the plasma beside it that reads as
  // exactly what it is -- "das sieht sehr billig aus". The maps are the
  // channel's own and nothing else is writing them: a slot being recorded
  // into is not playing.
  drawPathOnSphere (path, lineThickness, 0.9f, _uiStates[ch]->colour, true,
                    pattern.getElevationParams (), _engine.getHeightMap (), g,
                    PlaneShaping{}, _sphereShader.getCamera (),
                    // Leant, though: the finger was turned back before the
                    // take wrote it, so the trail leans again to lie under it.
                    spaceTurnOf (pattern),
                    lineStrokesFor (static_cast<int> (ch)),
                    strandStrokesFor (static_cast<int> (ch)));
}

void
MotionComponent::drawRecordingUnderlay (Pattern const &pattern,
                                        juce::Graphics &g)
{
  auto const ticks = pattern.getTicks ();
  if (ticks.positions.empty ())
    return;

  auto const ch = pattern.getChannel ();
  if (ch >= _uiStates.size ())
    return;

  auto const params = pattern.getElevationParams ();
  auto const colour = _uiStates[ch]->colour;

  // Well below the take's own trail, which is drawn straight over this: it has
  // to be readable as ground, not competing with what is being played in. How
  // far below is a judgement made by eye, so it lives in the skin.
  auto const underlayOpacity = _underlayOpacity;
  auto const lineThickness = _underlayLineThickness;

  // Drawn once into an image of its own and kept while nothing it depends on
  // changes -- see UnderlayLook. Its braid stands still there, on purpose.
  UnderlayLook const look{ &pattern,
                           ticks.positions.size (),
                           _sphereShader.getCamera (),
                           _imageBlend->getWidth (),
                           _imageBlend->getHeight (),
                           colour,
                           underlayOpacity,
                           lineThickness,
                           spaceTurnOf (pattern) };
  if (look != _underlayLook || !_underlayImage.isValid ())
    {
      _underlayImage
          = juce::Image (juce::Image::ARGB, look.width, look.height, true);
      juce::Graphics picture (_underlayImage);
      picture.addTransform (_transformNormalizedToLocal);

      juce::Path path;
      for (auto const &segment :
           trajectorySegments (ticks.positions, BridgePlan{}))
        {
          path.startNewSubPath (segment.front ().x (), segment.front ().y ());
          for (size_t i = 1; i < segment.size (); ++i)
            path.lineTo (segment[i].x (), segment[i].y ());
        }

      drawPathOnSphere (path, lineThickness, underlayOpacity, colour, true,
                        params, _engine.getHeightMap (), picture,
                        PlaneShaping{}, _sphereShader.getCamera (),
                        spaceTurnOf (pattern));
      _underlayLook = look;
    }

  // Pixel for pixel: g carries the normalised-to-local transform, and the
  // picture is already in local pixels.
  g.drawImageTransformed (_underlayImage,
                          _transformNormalizedToLocal.inverted ());

  // And where it would be right now. The write head's own position is the
  // phase into the loop -- the old pattern is not playing, so there is nothing
  // else to ask.
  auto const progress = _engine.getRecordingProgress ();
  if (progress < 0.f)
    return;

  auto const count = ticks.positions.size ();
  auto const index = std::min (
      count - 1, static_cast<std::size_t> (progress * static_cast<float> (count)));
  auto const position2D = ticks.positions[index];
  if (!position2D.isValid ())
    return;

  auto const position = turnedInSpace (
      _engine.getHeightMap ().mapTo3D (position2D, params),
      spaceTurnOf (pattern));
  if (!position.isValid ())
    return;

  auto const diameter = 2 * _blobScale * _underlayBlobScale;
  auto const centre = projectToScreen (position);
  g.setColour (colour.withAlpha (underlayOpacity));
  g.fillEllipse (juce::Rectangle<float> (0.f, 0.f, diameter, diameter)
                     .withCentre (centre));
}

void
MotionComponent::drawPatternPreview (Pattern const &pattern,
                                    PatternDisplayData const &displayData,
                                    juce::Graphics &g)
{
  auto const lineThickness = theme ().trajectoryThickness;

  auto const ch = pattern.getChannel ();
  auto colour = _uiStates[ch]->colour;
  // The same sweep the engine applies before it projects (performPlayback):
  // the drawn coverage has to be the coverage the blob is running in.
  auto const params = sweptElevation (pattern.getElevationParams (), pattern);
  // The same turn and the same squeeze the engine puts the blob through
  // (performPlayback): the drawn line has to be the line it is running on.
  auto const shaping = shapingOf (pattern);
  auto const &heightMap = _engine.getHeightMap ();

  // ── Handle jump-dot patterns ──
  // The pattern currently being edited must always stay fully legible, no
  // matter which hemisphere it sits in — no depth fade.
  if (!displayData.jumpDots.empty ())
    {
      // Three times the line, and never less than a dot of its own -- see
      // jumpDotDiameter().
      auto const dotSize = jumpDotDiameter (lineThickness);
      for (auto const &dot : displayData.jumpDots)
        {
          auto pos3D = turnedInSpace (
              heightMap.mapTo3D (
                  shapedPosition (
                      Pos::fromCartesian (dot.first, dot.second, 0.f), shaping),
                  params),
              spaceTurnOf (pattern));
          auto posJuce = projectToScreen (pos3D);
          pos3D = asSeenFrom (pos3D, _sphereShader.getCamera ());
          g.setColour (colour);
          g.fillEllipse (juce::Rectangle<float> (dotSize, dotSize)
                             .withCentre (posJuce));
        }
      return;
    }

  // ── Draw from SVG displayPath projected onto sphere ──
  // Into the channel's maps on the GPU, as a playing line is: drawn here in
  // software it was the bare strands at the skin's line width -- 0.0018 of
  // the radius since the line went to the GPU, less than a pixel -- and the
  // preview was not there at all.
  drawPathOnSphere (displayData.displayPath, lineThickness, 1.0f, colour,
                    false, params, heightMap, g, shaping,
                    _sphereShader.getCamera (), spaceTurnOf (pattern),
                    lineStrokesFor (static_cast<int> (ch)),
                    strandStrokesFor (static_cast<int> (ch)));
}

void
MotionComponent::drawPlayingTrajectory (Pattern const &pattern,
                                        PatternDisplayData const &displayData,
                                        juce::Graphics &g)
{
  // Depth fade applies here -- full at or above the horizon, receding towards
  // the far pole below it -- unlike drawPatternPreview()'s always-full
  // override above. That, and not a different width, is what distinguishes a
  // merely-playing trajectory from the one being edited.
  //
  // This carried its own `constexpr lineThickness = 0.025f` until 2026-09-13,
  // which is where every attempt to make the line thinner went to die: the
  // skin value was introduced by moving the *other* constant, in
  // drawPatternPreview(), and this one was left where it was. The maintainer
  // halved the skin value twice, looked at a line fourteen times thicker than
  // it said, and asked whether something was lying on top of it. Nothing was.
  auto const lineThickness = theme ().trajectoryThickness;

  auto const ch = pattern.getChannel ();
  auto colour = _uiStates[ch]->colour;
  // The same sweep the engine applies before it projects (performPlayback):
  // the drawn coverage has to be the coverage the blob is running in.
  auto const params = sweptElevation (pattern.getElevationParams (), pattern);
  // The same turn and the same squeeze the engine puts the blob through
  // (performPlayback): the drawn line has to be the line it is running on.
  auto const shaping = shapingOf (pattern);
  auto const &heightMap = _engine.getHeightMap ();

  // ── Handle jump-dot patterns ──
  if (!displayData.jumpDots.empty ())
    {
      auto const dotSize = jumpDotDiameter (lineThickness);
      for (auto const &dot : displayData.jumpDots)
        {
          auto pos3D = turnedInSpace (
              heightMap.mapTo3D (
                  shapedPosition (
                      Pos::fromCartesian (dot.first, dot.second, 0.f), shaping),
                  params),
              spaceTurnOf (pattern));
          auto posJuce = projectToScreen (pos3D);
          pos3D = asSeenFrom (pos3D, _sphereShader.getCamera ());
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
                    true, params, heightMap, g, shaping,
                    _sphereShader.getCamera (), spaceTurnOf (pattern),
                    lineStrokesFor (static_cast<int> (ch)),
                    strandStrokesFor (static_cast<int> (ch)));
}

juce::Point<float>
MotionComponent::normalizedToLocal2DPosition (Pos const &posNorm) const
{
  return projectToScreen (posNorm).transformedBy (
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
  using namespace juce::gl;

  DBG ("openGLContextClosing");
  if (_energyTexture != 0)
    {
      glDeleteTextures (1, &_energyTexture);
      _energyTexture = 0;
    }
  _blit.destroy ();
  _lineMapRenderer.shutdown ();
  _strandMapRenderer.shutdown ();
  _sphereShader.shutdown ();
}

}
