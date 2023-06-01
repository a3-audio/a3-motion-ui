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

#include <a3-motion-ui/LookAndFeel.hh>

namespace
{

/* Convenience RAII object to make sure we always free the
 * OpenGLGraphicsContext after it was used in the rendering callback.
 */
class GLContextGraphics
{
public:
  GLContextGraphics (juce::OpenGLContext &glContext, int width, int height)
      : _glRenderer (
          juce::createOpenGLGraphicsContext (glContext, width, height)),
        _graphics (*_glRenderer)
  {
    _graphics.addTransform (
        juce::AffineTransform::scale (float (glContext.getRenderingScale ())));
  }

  ~GLContextGraphics ()
  {
    _glRenderer = nullptr;
  }

  juce::Graphics &
  get ()
  {
    return _graphics;
  }

private:
  std::unique_ptr<juce::LowLevelGraphicsContext> _glRenderer;
  juce::Graphics _graphics;
};
}

namespace a3
{

MotionComponent::MotionComponent (
    std::vector<std::unique_ptr<Channel> > const &channels)
    : _channels (channels)
{
  _glContext.setRenderer (this);
  _glContext.setContinuousRepainting (true);
  _glContext.setComponentPaintingEnabled (false);
  _glContext.attachTo (*this);
}

MotionComponent::~MotionComponent ()
{
  _glContext.detach ();
}

void
MotionComponent::paint (juce::Graphics &g)
{
  juce::ignoreUnused (g);
}

void
MotionComponent::resized ()
{
  auto lock = std::lock_guard<std::mutex> (_mutexBounds);
  _bounds = getLocalBounds ();
}

void
MotionComponent::newOpenGLContextCreated ()
{
  DBG ("newOpenGLContextCreated");

  using namespace juce::gl;
  glDebugMessageControl (GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_OTHER,
                         GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
}

void
MotionComponent::renderOpenGL ()
{
  using namespace juce::gl;
  using juce::OpenGLHelpers;

  jassert (OpenGLHelpers::isContextActive ());

  // printFrameTime ();

  updateBounds ();
  GLContextGraphics graphics (_glContext, //
                              _boundsRender.getWidth (),
                              _boundsRender.getHeight ());

  OpenGLHelpers::clear (Colours::background);
  draw2D (graphics.get ());
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
MotionComponent::updateBounds ()
{
  auto lock = std::lock_guard<std::mutex> (_mutexBounds);
  if (_bounds != _boundsRender)
    _boundsRender = _bounds;
}

void
MotionComponent::draw2D (juce::Graphics &g)
{
  g.setColour (juce::Colours::aquamarine);
  g.drawEllipse (_boundsRender.toFloat (), 10.f);
}

void
MotionComponent::openGLContextClosing ()
{
  DBG ("openGLContextClosing");
}
}
