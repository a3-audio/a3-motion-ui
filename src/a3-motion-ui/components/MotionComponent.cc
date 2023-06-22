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

#include <a3-motion-engine/Channel.hh>

#include <a3-motion-ui/LookAndFeel.hh>
#include <a3-motion-ui/components/ChannelViewState.hh>

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

auto constexpr reduceFactorCircle = 0.9f; // relative to the (square)
                                          // component extents
auto constexpr reduceFactorArrow = 0.8f;  // relative to the already reduced
                                          // circle

// relative to the (square) component extents
auto constexpr activeAreaAroundBlobFactor = 0.05f;
auto constexpr blobHighlightFactor = 1.1f;

}

namespace a3
{

MotionComponent::MotionComponent (
    std::vector<std::unique_ptr<Channel> > const &channels,
    std::vector<std::unique_ptr<ChannelViewState> > &viewStates)
    : _channels (channels), _viewStates (viewStates)
{
  _glContext.setOpenGLVersionRequired (
      juce::OpenGLContext::OpenGLVersion::openGL4_3);
  _glContext.setRenderer (this);
  _glContext.setContinuousRepainting (true);
  _glContext.setComponentPaintingEnabled (false);
  _glContext.attachTo (*this);
}

MotionComponent::~MotionComponent ()
{
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
  updateChannelBlobHighlight (event.getPosition ().toFloat ());
}

void
MotionComponent::mouseDown (const juce::MouseEvent &event)
{
  for (auto channelIndex = 0u; channelIndex < _channels.size ();
       ++channelIndex)
    {
      _viewStates[channelIndex]->grabbed
          = _viewStates[channelIndex]->highlighted;

      if (_viewStates[channelIndex]->grabbed)
        {
          _viewStates[channelIndex]->grabOffset
              = normalizedToLocalPosition (
                    _channels[channelIndex]->getPosition ())
                - event.getPosition ().toFloat ();
        }
    }
}

void
MotionComponent::mouseDrag (const juce::MouseEvent &event)
{
  for (auto channelIndex = 0u; channelIndex < _channels.size ();
       ++channelIndex)
    {
      if (_viewStates[channelIndex]->grabbed)
        {
          _channels[channelIndex]->setPosition (localToNormalizedPosition (
              event.getPosition ().toFloat ()
              + _viewStates[channelIndex]->grabOffset));
        }
    }
}

void
MotionComponent::updateChannelBlobHighlight (juce::Point<float> mousePosition)
{
  jassert (_boundsCenterRegion.getWidth ()
           == _boundsCenterRegion.getHeight ());

  auto activeDistanceInPixel
      = _boundsCenterRegion.getWidth () * activeAreaAroundBlobFactor;

  for (auto channelIndex = 0u; channelIndex < _channels.size ();
       ++channelIndex)
    {
      auto const blobPosInPixel = normalizedToLocalPosition (
          _channels[channelIndex]->getPosition ());

      _viewStates[channelIndex]->highlighted
          = blobPosInPixel.getDistanceFrom (mousePosition)
            < activeDistanceInPixel;
    }
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

  drawArrowCircle (graphics.get ());
  drawChannelBlobs (graphics.get ());
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
      shorterSideLength, shorterSideLength);
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
MotionComponent::drawArrowCircle (juce::Graphics &g)
{
  jassert (_boundsCenterRegion.getWidth ()
           == _boundsCenterRegion.getHeight ());

  auto const diameter = _boundsCenterRegion.getWidth () * reduceFactorCircle;
  auto const boundsCircle
      = _boundsCenterRegion.withSizeKeepingCentre (diameter, diameter);
  auto const thickness = diameter / 50.f;

  g.setColour (Colours::circle);
  g.drawEllipse (boundsCircle.toFloat (), thickness);

  auto const height = diameter * reduceFactorArrow;
  auto const boundsArrow
      = boundsCircle.withSizeKeepingCentre (boundsCircle.getWidth (), height);
  auto const start = juce::Point<float> (boundsCircle.getCentreX (),
                                         boundsArrow.getBottom ());
  auto const end = juce::Point<float> (boundsCircle.getCentreX (), //
                                       boundsArrow.getY ());
  auto headSize = 4.f * thickness;

  auto boundsStartCircle
      = juce::Rectangle<float> (start.getX (), start.getY (), 0, 0);
  boundsStartCircle = boundsStartCircle.withSizeKeepingCentre (20, 20);

  g.drawArrow ({ start, end }, thickness, headSize, headSize);
}

void
MotionComponent::drawChannelBlobs (juce::Graphics &g)
{
  using namespace juce::gl;

  auto const blobSize = getBlobSize ();

  _imageBlend->clear (_imageBlend->getBounds ());

  {
    juce::Graphics gFBO{ *_imageBlend };

    auto const blob = juce::Rectangle<float> (0.f, 0.f, blobSize, blobSize);
    auto const blobHighlight = juce::Rectangle<float> (
        0.f, 0.f, //
        blobSize * blobHighlightFactor, blobSize * blobHighlightFactor);

    for (auto channelIndex = 0u; channelIndex < _channels.size ();
         ++channelIndex)
      {
        auto pos = normalizedToLocalPosition (
            _channels[channelIndex]->getPosition ());

        auto colour = _viewStates[channelIndex]->colour;

        if (_viewStates[channelIndex]->highlighted)
          {
            gFBO.setColour (
                colour.withLightness (colour.getLightness () + 0.2f));
            gFBO.fillEllipse (blobHighlight.withCentre (pos));
          }

        gFBO.setColour (colour);
        gFBO.fillEllipse (blob.withCentre (pos));
      }
  }

  g.drawImage (*_imageBlend, _boundsRender.toFloat ());
}

float
MotionComponent::getBlobSize () const
{
  return _boundsCenterRegion.getWidth () * 0.05f;
}

juce::Point<float>
MotionComponent::normalizedToLocalPosition (Pos const &posNorm) const
{
  jassert (_boundsCenterRegion.getWidth ()
           == _boundsCenterRegion.getHeight ());

  auto const halfSize = _boundsCenterRegion.getWidth () / 2.f;

  return _boundsCenterRegion.getCentre ().toFloat ()
         + juce::Point<float> (posNorm.x () * halfSize * reduceFactorCircle,
                               posNorm.y () * halfSize * reduceFactorCircle);
}

Pos
MotionComponent::localToNormalizedPosition (
    juce::Point<float> const &posLocal) const
{
  jassert (_boundsCenterRegion.getWidth ()
           == _boundsCenterRegion.getHeight ());

  auto const halfSize = _boundsCenterRegion.getWidth () / 2.f;

  auto posNormalized = (posLocal - _boundsCenterRegion.getCentre ().toFloat ())
                       / halfSize / reduceFactorCircle;

  return Pos::fromCartesian ( //
      posNormalized.getX (),  //
      posNormalized.getY (),  //
      0                       // no elevation for now
  );
}

void
MotionComponent::openGLContextClosing ()
{
  DBG ("openGLContextClosing");
}
}
