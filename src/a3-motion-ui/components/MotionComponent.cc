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
  {
    _glRenderer = juce::createOpenGLGraphicsContext (glContext, width, height);

    _graphics = std::make_unique<juce::Graphics> (*_glRenderer);
    _graphics->addTransform (
        juce::AffineTransform::scale (float (glContext.getRenderingScale ())));
  }

  ~GLContextGraphics ()
  {
    _graphics = nullptr;
    _glRenderer = nullptr;
  }

  juce::Graphics &
  get ()
  {
    return *_graphics;
  }

private:
  std::unique_ptr<juce::LowLevelGraphicsContext> _glRenderer;
  std::unique_ptr<juce::Graphics> _graphics;
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

// relative to the (square) component extents
auto constexpr reduceFactorCircle = .9f;
auto constexpr reduceFactorHead = .35f;

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

  // @TODO: compile as binary resources into executable
  _imageIsoSphere = juce::ImageFileFormat::loadFrom (
      juce::File::getCurrentWorkingDirectory ().getChildFile (
          "resources/iso-sphere-wireframe.png"));
  _drawableHead = juce::Drawable::createFromSVGFile (
      juce::File::getCurrentWorkingDirectory ().getChildFile (
          "resources/head.svg"));
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

  enum HighlightStrategy
  {
    Closest,
    AllWithinActiveArea
  };

  auto const strategy = HighlightStrategy::Closest;

  auto const activeDistanceInPixel
      = _boundsCenterRegion.getWidth () * activeAreaAroundBlobFactor;

  switch (strategy)
    {
    case HighlightStrategy::Closest:
      {
        auto minDistance = std::numeric_limits<float>::infinity ();
        auto minIndex = 0u;
        for (auto channelIndex = 0u; channelIndex < _channels.size ();
             ++channelIndex)
          {
            auto const blobPosInPixel = normalizedToLocalPosition (
                _channels[channelIndex]->getPosition ());

            _viewStates[channelIndex]->highlighted = false;

            auto const distance
                = blobPosInPixel.getDistanceFrom (mousePosition);
            if (distance < minDistance)
              {
                minDistance = distance;
                minIndex = channelIndex;
              }
          }

        if (minDistance < activeDistanceInPixel)
          _viewStates[minIndex]->highlighted = true;

        break;
      }
    case HighlightStrategy::AllWithinActiveArea:
      {
        for (auto channelIndex = 0u; channelIndex < _channels.size ();
             ++channelIndex)
          {
            auto const blobPosInPixel = normalizedToLocalPosition (
                _channels[channelIndex]->getPosition ());
            _viewStates[channelIndex]->highlighted
                = blobPosInPixel.getDistanceFrom (mousePosition)
                  < activeDistanceInPixel;
          }
        break;
      }
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

  {
    GLContextGraphics graphics (_glContext, //
                                _boundsRender.getWidth (),
                                _boundsRender.getHeight ());

    OpenGLHelpers::clear (Colours::background);

    drawCircle (graphics.get ());
    drawChannelBlobs (graphics.get ());
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
MotionComponent::drawCircle (juce::Graphics &g)
{
  jassert (_boundsCenterRegion.getWidth ()
           == _boundsCenterRegion.getHeight ());

  // draw head
  auto constexpr opacityHead = 0.4f;
  auto const diameterHead = _boundsCenterRegion.getWidth () * reduceFactorHead;
  auto const boundsHead
      = _boundsCenterRegion.toFloat ().withSizeKeepingCentre (diameterHead,
                                                              diameterHead);
  _drawableHead->drawWithin (g, boundsHead, juce::RectanglePlacement::centred,
                             opacityHead);

  // draw iso sphere
  if (!_shaderTextured)
    {
      auto fragmentFile = juce::File (
          juce::File::getCurrentWorkingDirectory ().getChildFile (
              "resources/shaders/textured.fsh"));

      auto fragmentCode = fragmentFile.loadFileAsString ();
      juce::Logger::writeToLog (fragmentCode);

      _shaderTextured
          = std::make_unique<juce::OpenGLGraphicsContextCustomShader> (
              fragmentCode);

      auto result
          = _shaderTextured->checkCompilation (g.getInternalContext ());
      if (result.failed ())
        {
          juce::Logger::writeToLog (result.getErrorMessage ());
          exit (0);
        }
      else
        {
          juce::Logger::writeToLog (
              "SUCCESSSUCCESSSUCCESSSUCCESSSUCCESSSUCCESS");
        }
    }

  auto const diameterCircle
      = _boundsCenterRegion.getWidth () * reduceFactorCircle;
  auto const boundsCircle
      = _boundsCenterRegion.toFloat ().withSizeKeepingCentre (diameterCircle,
                                                              diameterCircle);

  // _shaderTextured->fillRect (g.getInternalContext (),
  //                            boundsCircle.toNearestInt ());

  // auto constexpr opacityIsoSphere = 0.6f;
  // g.setOpacity (opacityIsoSphere);
  // g.drawImage (_imageIsoSphere, boundsCircle);

  g.setOpacity (1.f);
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

  g.setOpacity (0.8f);
  g.drawImage (*_imageBlend, _boundsRender.toFloat ());
  g.setOpacity (1.f);
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
