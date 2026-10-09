/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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

// The sphere shader, compiled and run: FPV's ships and groups drawn into an
// offscreen picture and read back. Everything else about the shader is held
// by tests that read its source; this one looks at what it paints.
//
// It needs an OpenGL implementation reachable through EGL without a window
// (Mesa's surfaceless platform does it, llvmpipe included). Where there is
// none the tests are skipped and say so.

#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <a3-motion-engine/elevation/HeightMapSphere.hh>
#include <a3-motion-engine/flight/BaseOrbit.hh>
#include <a3-motion-ui/components/LineMapGeometry.hh>
#include <a3-motion-ui/components/LineMapRenderer.hh>
#include <a3-motion-ui/components/LineMapStrokes.hh>
#include <a3-motion-ui/components/SphereShader.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/components/SpeakerLightScaling.hh>
#include <a3-motion-ui/components/fpv/FlightScene.hh>
#include <a3-motion-ui/components/fpv/ShipPath.hh>

#include <cmath>
#include <vector>

using namespace a3;
using namespace juce::gl;

namespace
{
constexpr int side = 420;
constexpr float sphereRadius = 90.f;

/** A GL context with nothing on screen: Mesa's surfaceless platform, or the
 *  default display with a pbuffer where that is missing. */
class OffscreenGl
{
public:
  OffscreenGl ()
  {
    auto const getPlatformDisplay
        = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC> (
            eglGetProcAddress ("eglGetPlatformDisplayEXT"));
    if (getPlatformDisplay != nullptr)
      _display = getPlatformDisplay (EGL_PLATFORM_SURFACELESS_MESA,
                                     EGL_DEFAULT_DISPLAY, nullptr);
    if (_display == EGL_NO_DISPLAY)
      _display = eglGetDisplay (EGL_DEFAULT_DISPLAY);
    if (_display == EGL_NO_DISPLAY || !eglInitialize (_display, nullptr, nullptr))
      return;
    if (!eglBindAPI (EGL_OPENGL_API))
      return;

    // A pbuffer config, not the default window one: there is no window, and
    // Mesa's surfaceless platform offers no other.
    EGLint const configAttributes[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
                                        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                                        EGL_NONE };
    EGLConfig config = nullptr;
    EGLint count = 0;
    if (!eglChooseConfig (_display, configAttributes, &config, 1, &count)
        || count < 1)
      return;

    _context = eglCreateContext (_display, config, EGL_NO_CONTEXT, nullptr);
    if (_context == EGL_NO_CONTEXT)
      return;
    if (!eglMakeCurrent (_display, EGL_NO_SURFACE, EGL_NO_SURFACE, _context))
      return;

    loadFunctions ();
    _ready = glGetString (GL_VERSION) != nullptr;
  }

  ~OffscreenGl ()
  {
    if (_display == EGL_NO_DISPLAY)
      return;
    eglMakeCurrent (_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (_context != EGL_NO_CONTEXT)
      eglDestroyContext (_display, _context);
    eglTerminate (_display);
  }

  bool ready () const { return _ready; }

private:
  EGLDisplay _display = EGL_NO_DISPLAY;
  EGLContext _context = EGL_NO_CONTEXT;
  bool _ready = false;
};

/** RGBA, bottom row first, as glReadPixels hands it over. */
struct Picture
{
  std::vector<unsigned char> rgba;

  /** The shader's own screen units (the ball's radius 1, y up) to a pixel. */
  float
  brightness (std::array<float, 2> uv) const
  {
    auto const x = juce::roundToInt (side / 2.f + uv[0] * sphereRadius);
    auto const y = juce::roundToInt (side / 2.f + uv[1] * sphereRadius);
    auto const at = static_cast<size_t> (4 * (y * side + x));
    return (rgba[at] + rgba[at + 1] + rgba[at + 2]) / 3.f;
  }

  float
  red (std::array<float, 2> uv) const
  {
    auto const x = juce::roundToInt (side / 2.f + uv[0] * sphereRadius);
    auto const y = juce::roundToInt (side / 2.f + uv[1] * sphereRadius);
    return rgba[static_cast<size_t> (4 * (y * side + x))];
  }

  /** The largest change from `other` anywhere within `radius` of `uv`. */
  float
  changeFrom (Picture const &other, std::array<float, 2> uv, float radius) const
  {
    auto most = 0.f;
    auto const cx = side / 2.f + uv[0] * sphereRadius;
    auto const cy = side / 2.f + uv[1] * sphereRadius;
    auto const r = radius * sphereRadius;
    for (auto y = juce::roundToInt (cy - r); y <= juce::roundToInt (cy + r); ++y)
      for (auto x = juce::roundToInt (cx - r); x <= juce::roundToInt (cx + r); ++x)
        {
          if (x < 0 || y < 0 || x >= side || y >= side)
            continue;
          auto const at = static_cast<size_t> (4 * (y * side + x));
          for (auto c = 0; c < 3; ++c)
            most = std::max (
                most, std::abs (static_cast<float> (rgba[at + c])
                                - static_cast<float> (other.rgba[at + c])));
        }
    return most;
  }

  int
  pixelsDifferentFrom (Picture const &other) const
  {
    auto count = 0;
    for (size_t i = 0; i < rgba.size (); i += 4)
      count += (rgba[i] != other.rgba[i] || rgba[i + 1] != other.rgba[i + 1]
                || rgba[i + 2] != other.rgba[i + 2])
                   ? 1
                   : 0;
    return count;
  }
};

/** Draws the shader into a framebuffer of its own and reads it back. */
class Renderer
{
public:
  Renderer ()
  {
    if (!_gl.ready ())
      return;
    _ready = _shader.initialise (_juceContext);
    if (!_ready)
      return;

    glGenTextures (1, &_texture);
    glBindTexture (GL_TEXTURE_2D, _texture);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, side, side, 0, GL_RGBA,
                  GL_UNSIGNED_BYTE, nullptr);
    glGenFramebuffers (1, &_frameBuffer);
    glBindFramebuffer (GL_FRAMEBUFFER, _frameBuffer);
    glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            _texture, 0);
    _ready = glCheckFramebufferStatus (GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
  }

  ~Renderer ()
  {
    if (!_gl.ready ())
      return;
    _shader.shutdown ();
    glBindFramebuffer (GL_FRAMEBUFFER, 0);
    if (_frameBuffer != 0)
      glDeleteFramebuffers (1, &_frameBuffer);
    if (_texture != 0)
      glDeleteTextures (1, &_texture);
    if (_lineTexture != 0)
      glDeleteTextures (1, &_lineTexture);
  }

  bool glThere () const { return _gl.ready (); }
  SphereShader &shader () { return _shader; }
  juce::OpenGLContext &juceContext () { return _juceContext; }
  bool ready () const { return _ready; }

  /** A line map lying everywhere on the picture for `channel`: the line
   *  is near every pixel, so how brightly it is drawn is all that differs. */
  void
  lineEverywhere (int channel, float level)
  {
    if (_lineTexture == 0)
      {
        std::vector<unsigned char> texels (static_cast<size_t> (4 * 16 * 16), 0);
        for (size_t i = 0; i < texels.size (); i += 4)
          {
            texels[i] = 200;     // nearness
            texels[i + 1] = 128; // along the figure
            texels[i + 2] = 255; // no depth fade
            texels[i + 3] = 255;
          }
        glGenTextures (1, &_lineTexture);
        glBindTexture (GL_TEXTURE_2D, _lineTexture);
        glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA,
                      GL_UNSIGNED_BYTE, texels.data ());
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      }
    _shader.setNumBlobs (4);
    _shader.setLineTexture (channel, _lineTexture);
    _shader.setLineLevel (channel, level);
  }

  void
  noLines ()
  {
    for (auto ch = 0; ch < SphereShader::kMaxBlobs; ++ch)
      {
        _shader.setLineTexture (ch, 0);
        _shader.setLineLevel (ch, 1.f);
      }
  }

  Picture
  draw (FlightSceneUniforms const &scene, SphereCamera camera = {})
  {
    _shader.setCamera (camera);
    _shader.setTime (0.f);
    _shader.setFlightScene (scene);

    glBindFramebuffer (GL_FRAMEBUFFER, _frameBuffer);
    glViewport (0, 0, side, side);
    glClearColor (0.f, 0.f, 0.f, 1.f);
    glClear (GL_COLOR_BUFFER_BIT);
    _shader.draw (side, side, sphereRadius, side / 2.f, side / 2.f);

    Picture picture;
    picture.rgba.resize (static_cast<size_t> (4 * side * side));
    glReadPixels (0, 0, side, side, GL_RGBA, GL_UNSIGNED_BYTE,
                  picture.rgba.data ());
    return picture;
  }

private:
  OffscreenGl _gl;
  juce::OpenGLContext _juceContext;
  SphereShader _shader;
  GLuint _texture = 0;
  GLuint _lineTexture = 0;
  GLuint _frameBuffer = 0;
  bool _ready = false;
};

/** With A3_SNAPSHOT_DIR set, the picture as a PNG there, to be looked at. */
void
writeSnapshot (Picture const &picture, juce::String const &name)
{
  auto const dir
      = juce::SystemStats::getEnvironmentVariable ("A3_SNAPSHOT_DIR", {});
  if (dir.isEmpty ())
    return;
  juce::Image image (juce::Image::RGB, side, side, false);
  for (auto y = 0; y < side; ++y)
    for (auto x = 0; x < side; ++x)
      {
        auto const at = static_cast<size_t> (4 * ((side - 1 - y) * side + x));
        image.setPixelAt (x, y,
                          juce::Colour (picture.rgba[at], picture.rgba[at + 1],
                                        picture.rgba[at + 2]));
      }
  auto file = juce::File (dir).getChildFile (name);
  file.deleteFile ();
  juce::FileOutputStream out (file);
  juce::PNGImageFormat ().writeImageToStream (image, out);
}

#define NEEDS_GL(renderer)                                                    \
  if (!(renderer).glThere ())                                                 \
    GTEST_SKIP () << "no OpenGL through EGL here; the shader is not run";     \
  ASSERT_TRUE ((renderer).ready ()) << "the sphere shader did not build"

/** The room's horizontal direction towards the eye. */
Vec3
towardsTheEye (SphereCamera const &camera)
{
  auto const eye = toVec3 (
      asSeenFromInverse (Pos::fromCartesian (0.f, 0.f, 1.f), camera));
  return normalised ({ eye.x, eye.y, 0.f }, { 1.f, 0.f, 0.f });
}

ShipInScene
shipAt (Pos const &direction, SphereCamera const &camera)
{
  auto ship = shipInScene (direction, Vec3{ 0.f, 1.f, 0.f }, 0.25f, camera);
  ship.r = 0.2f;
  ship.g = 0.9f;
  ship.b = 0.3f;
  return ship;
}

FlightSceneUniforms
oneShip (ShipInScene const &ship)
{
  std::array<ShipInScene, maxSceneShips> ships{};
  ships[0] = ship;
  return packFlightScene (ships, 1, {}, 0);
}

FlightSceneUniforms
oneMark (FloorMark const &mark)
{
  std::array<FloorMark, maxSceneMarks> marks{};
  marks[0] = mark;
  auto packed = packFlightScene ({}, 0, marks, 1);
  packed.markStroke = 2.f / sphereRadius;
  return packed;
}

FloorMark
markAt (float x, float y, SphereCamera const &camera,
        BodyRole role = BodyRole::Attract, float radius = 0.1f)
{
  return floorMarkInScene (Pos::fromCartesian (x, y, speakerFloorZ), radius,
                           role, camera);
}
}

TEST (FlightSceneRender, TheShaderBuilds)
{
  Renderer renderer;
  NEEDS_GL (renderer);
}

TEST (FlightSceneRender, AShipInFrontIsDrawn)
{
  Renderer renderer;
  NEEDS_GL (renderer);

  SphereCamera const overhead;
  auto const ship = shipAt (Pos::fromCartesian (0.3f, 0.2f, 0.93f), overhead);
  auto const empty = renderer.draw ({});
  auto const drawn = renderer.draw (oneShip (ship));

  EXPECT_GT (drawn.changeFrom (empty, onShaderScreen (ship.centre), 0.02f), 60.f)
      << "the craft covers its own place";
  EXPECT_LT (drawn.changeFrom (empty, { -0.6f, -0.6f }, 0.05f), 1.f)
      << "and nothing far from it";
}

TEST (FlightSceneRender, AShipBehindTheBallIsOnlyItsGhost)
{
  Renderer renderer;
  NEEDS_GL (renderer);

  // Seen from the horizon, a ship on the far side lies right behind the ball.
  SphereCamera horizon;
  horizon.pitch = 1.5f;
  auto const h = towardsTheEye (horizon);
  auto const front
      = shipAt (Pos::fromCartesian (0.95f * h.x, 0.95f * h.y, 0.3f), horizon);
  auto const back
      = shipAt (Pos::fromCartesian (-0.95f * h.x, -0.95f * h.y, 0.3f), horizon);
  ASSERT_FALSE (hiddenByTheBall (front.centre));
  ASSERT_TRUE (hiddenByTheBall (back.centre));

  auto const empty = renderer.draw ({}, horizon);
  auto const shown = renderer.draw (oneShip (front), horizon)
                         .changeFrom (empty, onShaderScreen (front.centre), 0.01f);
  auto const ghost = renderer.draw (oneShip (back), horizon)
                         .changeFrom (empty, onShaderScreen (back.centre), 0.01f);

  EXPECT_GT (ghost, 0.f) << "its place is not lost";
  EXPECT_LT (ghost, shown * 0.4f) << "but it is only a ghost";
}

TEST (FlightSceneRender, AMarkIsPaintedOnTheFloor)
{
  Renderer renderer;
  NEEDS_GL (renderer);

  auto const camera = defaultCamera ();
  auto const mark = markAt (0.3f, -0.3f, camera);
  auto const empty = renderer.draw ({}, camera);
  auto const drawn = renderer.draw (oneMark (mark), camera);
  EXPECT_GT (drawn.changeFrom (empty, onShaderScreen (mark.centre), 0.01f), 20.f)
      << "the fill lies where the group stands";
  EXPECT_LT (drawn.changeFrom (empty, onShaderScreen (mark.centre), 0.01f), 150.f)
      << "a soft fill, not a solid body";
  EXPECT_LT (drawn.changeFrom (empty, { -0.6f, 0.6f }, 0.05f), 1.f)
      << "and nothing far from it";
}

TEST (FlightSceneRender, ALeanedMarkLiesFlat)
{
  Renderer renderer;
  NEEDS_GL (renderer);

  // Leaned over, the disc on the floor is an ellipse: it reaches its whole
  // radius across the view and less up it.
  SphereCamera camera;
  camera.pitch = 1.0f;
  auto const mark = markAt (0.f, 0.f, camera, BodyRole::Attract, 0.3f);
  auto const empty = renderer.draw ({}, camera);
  auto const drawn = renderer.draw (oneMark (mark), camera);
  auto const centre = onShaderScreen (mark.centre);
  auto const across = drawn.changeFrom (empty, { centre[0] + 0.25f, centre[1] }, 0.f);
  auto const up = drawn.changeFrom (empty, { centre[0], centre[1] + 0.25f }, 0.f);
  EXPECT_GT (across, 10.f) << "inside, across";
  EXPECT_LT (up, 1.f) << "outside, up the view: foreshortened";
}

TEST (FlightSceneRender, ADeadZoneIsHatchedRed)
{
  Renderer renderer;
  NEEDS_GL (renderer);

  SphereCamera const overhead;
  auto const mark = markAt (0.3f, 0.2f, overhead, BodyRole::Repel, 0.3f);
  auto const drawn = renderer.draw (oneMark (mark), overhead);
  auto const empty = renderer.draw ({}, overhead);
  auto const centre = onShaderScreen (mark.centre);

  // Across the mark the red comes and goes: stripes, not a fill.
  auto changes = 0;
  auto wasRed = false;
  for (auto dx = -0.2f; dx < 0.2f; dx += 0.5f / sphereRadius)
    {
      auto const at = std::array<float, 2>{ centre[0] + dx, centre[1] };
      auto const red = drawn.red (at) > empty.red (at) + 25.f;
      changes += red != wasRed ? 1 : 0;
      wasRed = red;
    }
  EXPECT_GE (changes, 4);
}

TEST (FlightSceneRender, AMarkSwellsOnTheOne)
{
  Renderer renderer;
  NEEDS_GL (renderer);

  SphereCamera const overhead;
  auto const empty = renderer.draw ({}, overhead);
  auto const rest = markAt (0.2f, 0.1f, overhead);
  auto swelled = rest;
  swelled.radius = swollenMarkRadius (rest.radius, 1.6f);
  EXPECT_GT (renderer.draw (oneMark (swelled), overhead).pixelsDifferentFrom (empty),
             renderer.draw (oneMark (rest), overhead).pixelsDifferentFrom (empty));
}

TEST (FlightSceneRender, AGroupsRingLiesOnTheFloorWithIt)
{
  Renderer renderer;
  NEEDS_GL (renderer);

  // Leaned over, the ring is an ellipse like its mark: it reaches its whole
  // radius across the view and less up it.
  SphereCamera camera;
  camera.pitch = 1.0f;
  auto mark = markAt (0.f, 0.f, camera, BodyRole::Attract, 0.08f);
  mark.ring = 0.4f;
  auto const empty = renderer.draw ({}, camera);
  auto const drawn = renderer.draw (oneMark (mark), camera);
  writeSnapshot (drawn, "fpv-mark-ring-leaned.png");
  auto const centre = onShaderScreen (mark.centre);
  auto const near = [&] (float dx, float dy) {
    return drawn.changeFrom (empty, { centre[0] + dx, centre[1] + dy }, 1.5f / sphereRadius);
  };
  auto const foreshortened = 0.4f * std::cos (camera.pitch);
  EXPECT_GT (near (0.4f, 0.f), 4.f) << "on the ring, across";
  EXPECT_LT (near (0.25f, 0.f), 1.f) << "between mark and ring, nothing";
  EXPECT_GT (near (0.f, foreshortened), 4.f) << "on the ring, up the view";
  EXPECT_LT (near (0.f, 0.4f), 1.f) << "not a circle on the glass";
}

TEST (FlightSceneRender, TheHoldRingFillsClockwiseFromTheTop)
{
  Renderer renderer;
  NEEDS_GL (renderer);

  SphereCamera const overhead;
  auto mark = markAt (0.2f, 0.f, overhead, BodyRole::Attract, 0.2f);
  auto const empty = renderer.draw ({}, overhead);
  auto const centre = onShaderScreen (mark.centre);
  auto const reach = 0.2f * markHoldRingOfRadius;
  auto const redAt = [&] (Picture const &picture, float dx, float dy) {
    std::array<float, 2> const at{ centre[0] + dx, centre[1] + dy };
    return picture.red (at) - empty.red (at);
  };

  auto const none = renderer.draw (oneMark (mark), overhead);
  mark.hold = 0.5f;
  auto const half = renderer.draw (oneMark (mark), overhead);

  EXPECT_LT (redAt (none, reach, 0.f), 30.f) << "no hold, no ring";
  EXPECT_GT (redAt (half, reach, 0.f), 60.f) << "a quarter round: on the right";
  EXPECT_LT (redAt (half, -reach, 0.f), 30.f) << "three quarters: not yet";
}

TEST (FlightSceneRender, ATowerHidesAMarkBehindIt)
{
  Renderer renderer;
  NEEDS_GL (renderer);

  // Looking in low over the floor, from behind one of the towers: a mark a
  // little further in lies behind it.
  SphereCamera camera;
  camera.pitch = 0.9f;
  auto const towards = towardsTheEye (camera);

  // The tower on the eye's side (SphereShader's bearings, at its default
  // radius).
  auto const k = 0.70710678f;
  Vec3 tower{};
  auto best = -2.f;
  for (auto const b : { Vec3{ k, k, 0.f }, Vec3{ k, -k, 0.f }, Vec3{ -k, -k, 0.f },
                        Vec3{ -k, k, 0.f } })
    if (dot (b, towards) > best)
      {
        best = dot (b, towards);
        tower = b;
      }
  auto const radius = SphereShader::SpotlightConfig{}.speakerRadius;
  auto const sideways = Vec3{ -towards.y, towards.x, 0.f };
  auto const at = [&] (float aside) {
    return markAt (tower.x * radius - towards.x * 0.35f + sideways.x * aside,
                   tower.y * radius - towards.y * 0.35f + sideways.y * aside,
                   camera, BodyRole::Attract, 0.08f);
  };
  auto const hidden = at (0.f);
  auto const open = at (0.5f);

  auto const empty = renderer.draw ({}, camera);
  auto const openPicture = renderer.draw (oneMark (open), camera);
  auto const hiddenPicture = renderer.draw (oneMark (hidden), camera);
  writeSnapshot (hiddenPicture, "fpv-mark-behind-a-tower.png");
  auto const shown
      = openPicture.changeFrom (empty, onShaderScreen (open.centre), 0.005f);
  auto const behind
      = hiddenPicture.changeFrom (empty, onShaderScreen (hidden.centre), 0.005f);
  EXPECT_GT (shown, 15.f);
  EXPECT_LT (behind, shown * 0.5f) << "the tower is solid";
}

TEST (FlightSceneRender, AMarkBehindTheBallIsOnlyItsGhost)
{
  Renderer renderer;
  NEEDS_GL (renderer);

  SphereCamera camera;
  camera.pitch = 1.2f;
  auto const h = towardsTheEye (camera);
  auto const near = markAt (1.2f * h.x, 1.2f * h.y, camera, BodyRole::Attract, 0.15f);
  auto const far = markAt (-1.2f * h.x, -1.2f * h.y, camera, BodyRole::Attract, 0.15f);
  ASSERT_FALSE (hiddenByTheBall (near.centre));
  ASSERT_TRUE (hiddenByTheBall (far.centre));

  auto const empty = renderer.draw ({}, camera);
  auto const shown = renderer.draw (oneMark (near), camera)
                         .changeFrom (empty, onShaderScreen (near.centre), 0.005f);
  auto const ghost = renderer.draw (oneMark (far), camera)
                         .changeFrom (empty, onShaderScreen (far.centre), 0.005f);
  EXPECT_GT (ghost, 0.f) << "its place is not lost";
  EXPECT_LT (ghost, shown * 0.6f) << "but it is only a ghost";
}

TEST (FlightSceneRender, AShipOnTheBackSideIsDarker)
{
  Renderer renderer;
  NEEDS_GL (renderer);

  // Beside the ball rather than behind it, so only the cue differs: the
  // far one is seen past the limb.
  SphereCamera const overhead;
  auto front = shipAt (Pos::fromCartesian (0.98f, 0.f, 0.2f), overhead);
  auto back = shipAt (Pos::fromCartesian (0.98f, 0.f, -0.2f), overhead);
  ASSERT_LT (back.shade, front.shade);
  // The same place on the screen, the same course; only the cue differs.
  back.centre = front.centre;
  back.nose = front.nose;
  back.up = front.up;

  auto const empty = renderer.draw ({});
  auto const lit = renderer.draw (oneShip (front))
                       .changeFrom (empty, onShaderScreen (front.centre), 0.01f);
  auto const dark = renderer.draw (oneShip (back))
                        .changeFrom (empty, onShaderScreen (back.centre), 0.01f);
  EXPECT_LT (dark, lit);
}

TEST (FlightSceneRender, NothingInTheSceneChangesNoPixel)
{
  Renderer renderer;
  NEEDS_GL (renderer);

  // FULL's picture: an empty scene leaves every pixel as the shader drew it
  // before ships and groups were added -- the same picture as with a scene
  // whose things are all off the picture, which takes the whole of the new
  // path except what draws a thing.
  SphereCamera const overhead;
  auto ship = shipAt (Pos::fromCartesian (0.f, 0.f, 1.f), overhead);
  ship.centre = { 0.f, 0.f, 1.f };
  ship.length = 0.0001f;
  auto const empty = renderer.draw ({});
  auto const tiny = renderer.draw (oneShip (ship));
  EXPECT_LE (tiny.pixelsDifferentFrom (empty), 4)
      << "outside what it covers, a ship changes nothing";
}

TEST (FlightSceneRender, AWholeSceneIsDrawn)
{
  Renderer renderer;
  NEEDS_GL (renderer);

  FlightTuning const tuning;
  for (auto const &[name, camera] :
       { std::pair<char const *, SphereCamera>{ "default", defaultCamera () },
         std::pair<char const *, SphereCamera>{ "leaned", { 0.9f, 0.6f } } })
    {
      std::array<ShipInScene, maxSceneShips> ships{};
      Pos const at[] = { Pos::fromCartesian (0.5f, 0.3f, 0.81f),
                         Pos::fromCartesian (-0.4f, 0.6f, 0.69f),
                         Pos::fromCartesian (0.1f, -0.7f, 0.7f),
                         Pos::fromCartesian (-0.9f, -0.2f, -0.39f) };
      float const colours[][3] = { { 1.f, 0.3f, 0.6f }, { 0.3f, 0.8f, 1.f },
                                   { 1.f, 0.8f, 0.2f }, { 0.5f, 1.f, 0.4f } };
      for (auto i = 0; i < maxSceneShips; ++i)
        {
          auto &ship = ships[static_cast<size_t> (i)];
          ship = shipInScene (at[i], Vec3{ 0.f, 1.f, 0.f }, 0.16f, camera);
          ship.r = colours[i][0];
          ship.g = colours[i][1];
          ship.b = colours[i][2];
        }
      std::array<FloorMark, maxSceneMarks> marks{};
      auto const size = [&] (float mass) {
        return discMarkRadius (mass, 0.1f, tuning);
      };
      marks[0] = markAt (0.4f, 0.2f, camera, BodyRole::Attract, size (tuning.groupMass));
      marks[1] = markAt (-0.3f, 0.5f, camera, BodyRole::Attract, size (tuning.crowdMass));
      marks[2] = markAt (1.1f, -0.6f, camera, BodyRole::Attract, size (tuning.hotspotMass));
      marks[3] = markAt (-0.5f, -0.5f, camera, BodyRole::Repel, size (tuning.deadZoneMass));
      auto const empty = renderer.draw ({}, camera);
      auto packed = packFlightScene (ships, 4, marks, 4);
      packed.markStroke = 2.f / sphereRadius;
      auto const scene = renderer.draw (packed, camera);
      EXPECT_GT (scene.pixelsDifferentFrom (empty), 200) << name;
      writeSnapshot (empty, juce::String ("fpv-scene-empty-") + name + ".png");
      writeSnapshot (scene, juce::String ("fpv-scene-") + name + ".png");
    }
}

// An orbit is drawn through the same line maps as a trajectory, so that it
// and the trajectory it replaces change over on one frame; only its level
// makes it quieter.
TEST (FlightSceneRender, AnOrbitsLineIsAQuieterTrajectory)
{
  Renderer renderer;
  NEEDS_GL (renderer);

  SphereCamera const overhead;
  renderer.noLines ();
  auto const empty = renderer.draw ({}, overhead);
  renderer.lineEverywhere (0, 1.f);
  auto const trajectory = renderer.draw ({}, overhead);
  renderer.lineEverywhere (0, lineLevelOf (ShipPathShown::Orbit));
  auto const orbit = renderer.draw ({}, overhead);
  renderer.lineEverywhere (0, 0.f);
  auto const off = renderer.draw ({}, overhead);

  std::array<float, 2> const at{ 0.3f, -0.2f };
  auto const full = trajectory.changeFrom (empty, at, 0.02f);
  auto const quiet = orbit.changeFrom (empty, at, 0.02f);
  EXPECT_GT (full, 10.f) << "the line is drawn";
  EXPECT_GT (quiet, 2.f) << "the orbit is drawn";
  EXPECT_LT (quiet, full * 0.8f) << "and quieter";
  EXPECT_EQ (off.pixelsDifferentFrom (empty), 0) << "a level of nothing is no line";
}

namespace
{
juce::Path
pathThrough (std::vector<Vec2> const &points)
{
  juce::Path path;
  path.startNewSubPath (points.front ().x, points.front ().y);
  for (size_t i = 1; i < points.size (); ++i)
    path.lineTo (points[i].x, points[i].y);
  return path;
}

std::vector<Vec2>
circleOn (Vec2 centre, float radius)
{
  return escortPathPoints (centre, radius, 64);
}
}

// What MotionComponent does for every playing channel, through the same
// maps: a CLIP ship's line is its clip, an ORBIT ship's is the orbit, quieter,
// and its clip is not drawn at all.
TEST (FlightSceneRender, AnOrbitShipsLineReplacesItsTrajectory)
{
  Renderer renderer;
  NEEDS_GL (renderer);
  LineMapRenderer lines{ lineMapSize };
  LineMapRenderer strands{ strandMapSize };
  ASSERT_TRUE (lines.initialise (renderer.juceContext ()));
  ASSERT_TRUE (strands.initialise (renderer.juceContext ()));

  HeightMapSphere heightMap;
  SphereCamera const camera = defaultCamera ();
  FlightTuning const tuning;
  auto const guide = orbitGuidePoints (0.0, 4, 96, tuning);
  // The clips each channel has loaded: a small ring in its own corner.
  std::array<std::vector<Vec2>, 4> const clips{
    circleOn ({ 0.45f, 0.45f }, 0.15f), circleOn ({ -0.45f, 0.45f }, 0.15f),
    circleOn ({ -0.45f, -0.45f }, 0.15f), circleOn ({ 0.45f, -0.45f }, 0.15f)
  };
  std::array<bool, 4> const orbit{ true, false, true, false };
  float const colours[][3] = { { 1.f, 0.3f, 0.6f }, { 0.3f, 0.8f, 1.f },
                               { 1.f, 0.8f, 0.2f }, { 0.5f, 1.f, 0.4f } };
  SheathRing const braid{ theme ().braidRadius, theme ().braidTurns,
                          theme ().braidSpin,
                          juce::roundToInt (theme ().braidStrands) };

  auto &shader = renderer.shader ();
  shader.setLineExtent (lineMapExtent);
  shader.setNumBlobs (4);
  auto const drawLines = [&] (bool fpv) {
    for (auto ch = 0; ch < 4; ++ch)
      {
        SphereShader::BlobData blob;
        blob.r = colours[ch][0];
        blob.g = colours[ch][1];
        blob.b = colours[ch][2];
        shader.setBlob (ch, blob);

        ShipPathFacts facts;
        facts.fpv = fpv;
        facts.orbit = orbit[static_cast<size_t> (ch)];
        auto const shown = shipPathShown (facts);
        auto const points
            = drawsTrajectory (shown)
                  ? clips[static_cast<size_t> (ch)]
                  : shipPathPoints (shown, guide, {}, noBodyId, tuning);
        auto const line = projectLine (pathThrough (points), ElevationParams{},
                                       heightMap, PlaneShaping{}, camera,
                                       SpaceTurn{});
        shader.setLineTexture (ch, lines.paint (ch, lineMapStrokes (line)));
        shader.setStrandTexture (
            ch, strands.paint (ch, strandMapStrokes (braidCord (line, braid, 0.f))));
        shader.setLineLevel (ch, lineLevelOf (shown));
      }
  };

  renderer.noLines ();
  auto const empty = renderer.draw ({}, camera);
  drawLines (false);
  auto const full = renderer.draw ({}, camera);
  drawLines (true);
  auto const fpv = renderer.draw ({}, camera);
  writeSnapshot (full, "fpv-lines-full.png");
  writeSnapshot (fpv, "fpv-lines-orbit-and-clip.png");

  auto const onScreen = [&] (Vec2 floor) {
    auto const room = heightMap.mapTo3D (Pos::fromCartesian (floor.x, floor.y, 0.f),
                                         ElevationParams{});
    return onShaderScreen (toVec3 (asSeenFrom (room, camera)));
  };
  auto const changeAt = [&] (Picture const &picture, Vec2 floor) {
    return picture.changeFrom (empty, onScreen (floor), 1.5f / sphereRadius);
  };

  for (auto ch = 0; ch < 4; ++ch)
    {
      auto const onClip = clips[static_cast<size_t> (ch)][8];
      EXPECT_GT (changeAt (full, onClip), 8.f) << "FULL draws every clip, " << ch;
      if (orbit[static_cast<size_t> (ch)])
        EXPECT_LT (changeAt (fpv, onClip), changeAt (full, onClip) * 0.3f)
            << "an ORBIT ship's clip is gone, " << ch;
      else
        EXPECT_GT (changeAt (fpv, onClip), 8.f) << "a CLIP ship keeps it, " << ch;
    }
  EXPECT_GT (changeAt (fpv, guide[10]), 4.f) << "the orbit is drawn";
  EXPECT_LT (changeAt (full, guide[10]), changeAt (fpv, guide[10]))
      << "and only where a ship flies it";

  lines.shutdown ();
  strands.shutdown ();
}
