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

#include <a3-motion-ui/components/ChannelViewState.hh>
#include <a3-motion-ui/components/LookAndFeel.hh>

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
auto constexpr reduceFactorCircle = .8f;
auto constexpr reduceFactorHead = .35f;
auto constexpr reduceFactorBlobs = 0.05f;

auto constexpr activeAreaAroundBlobFactor = 2.5f;
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
      juce::File ("resources/iso-sphere-wireframe.png"));
  _drawableHead
      = juce::Drawable::createFromSVGFile (juce::File ("resources/head.svg"));

  // start disocclusion / animation timer
  startTimer (1 / 60.f);
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
  updateChannelBlobHighlight (event.getPosition ().toFloat ());
}

void
MotionComponent::timerCallback ()
{
  if (_grabbedIndex.has_value ())
    disoccludeBlobs ();
}

void
MotionComponent::disoccludeBlobs ()
{
  jassert (_grabbedIndex.has_value ());

  auto posGrabbedPixel = normalizedToLocal2DPosition (
      _channels[_grabbedIndex.value ()]->getPosition ());

  for (auto channelIndex = 0u; channelIndex < _channels.size ();
       ++channelIndex)
    {
      if (!_viewStates[channelIndex]->grabbed)
        {
          auto posPixel = normalizedToLocal2DPosition (
              _channels[channelIndex]->getPosition ());

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
          else if (posPixel.getDistanceFrom (
                       _viewStates[channelIndex]->posAnchor)
                   > 1.f)
            { // snap back by projection onto circle
            // borrowing math from:
            //
            https
                : // www.geometrictools.com/Documentation/IntersectionLine2Circle2.pdf
              auto R = getActiveDistanceInPixel ();

              auto C = posGrabbedPixel;
              auto P = posPixel;
              auto Pa = _viewStates[channelIndex]->posAnchor;
              auto D = Pa - posPixel;

              auto Delta = P - C;
              auto D_dot_Delta = D.getDotProduct (Delta);

              auto delta
                  = D_dot_Delta * D_dot_Delta
                    - D.getDistanceSquaredFromOrigin ()
                          * (Delta.getDistanceSquaredFromOrigin () - R * R);

              auto t = .01f; // default: snap back with exponential
                             // smoothing
              std::set<float> ts;
              if (delta > 0.f)
                {
                  auto t0 = -(D_dot_Delta - std::sqrt (delta))
                            / D.getDistanceSquaredFromOrigin ();
                  auto t1 = -(D_dot_Delta + std::sqrt (delta))
                            / D.getDistanceSquaredFromOrigin ();

                  auto constexpr eps = 0.001f;
                  if (t0 >= -eps && t0 <= 1.f + eps)
                    ts.insert (t0);
                  if (t1 >= -eps && t1 <= 1.f + eps)
                    ts.insert (t1);

                  auto tIt = std::min_element (
                      ts.begin (), ts.end (),
                      [&] (auto const &lhs, auto const &rhs) {
                        return P.getDistanceSquaredFrom (P + lhs * D)
                               < P.getDistanceSquaredFrom (P + rhs * D);
                        ;
                      });

                  if (tIt != ts.end ())
                    t = *tIt;
                }

              posPixel = P + t * D;

              if (!ts.empty ())
                {
                  // after projecting shift outwards to induce slipping
                  auto Drot = juce::Point<float> (-D.y, D.x);
                  Drot /= Drot.getDistanceFromOrigin ();
                  if ((P - C).getDotProduct (Drot) < 0.f)
                    Drot *= -1.f;
                  posPixel += .25f * Drot;
                }
            }

          _channels[channelIndex]->setPosition (
              localToNormalized2DPosition (posPixel));
          _channels[channelIndex]->recomputeHeight ();
        }
    }
}

void
MotionComponent::mouseDown (const juce::MouseEvent &event)
{
  for (auto channelIndex = 0u; channelIndex < _channels.size ();
       ++channelIndex)
    _viewStates[channelIndex]->grabbed = false;

  auto closestIndex = getClosestBlobIndexWithinRadius (
      event.getPosition ().toFloat (), getActiveDistanceInPixel ());
  if (closestIndex.has_value ())
    {
      auto const index = closestIndex.value ();
      _viewStates[index]->grabbed = true;
      _viewStates[index]->grabOffset
          = normalizedToLocal2DPosition (_channels[index]->getPosition ())
            - event.getPosition ().toFloat ();
      _grabbedIndex = index;

      // disocclusion: save anchor position for all channels
      for (auto channelIndex = 0u; channelIndex < _channels.size ();
           ++channelIndex)
        {
          _viewStates[channelIndex]->posAnchor = normalizedToLocal2DPosition (
              _channels[channelIndex]->getPosition ());
        }
    }
}

void
MotionComponent::mouseUp (const juce::MouseEvent &event)
{
  for (auto channelIndex = 0u; channelIndex < _channels.size ();
       ++channelIndex)
    {
      _viewStates[channelIndex]->grabbed = false;
    }
  _grabbedIndex = {};
}

void
MotionComponent::mouseDrag (const juce::MouseEvent &event)
{
  for (auto channelIndex = 0u; channelIndex < _channels.size ();
       ++channelIndex)
    {
      if (_viewStates[channelIndex]->grabbed)
        {
          auto const posPixel = event.getPosition ().toFloat ()
                                + _viewStates[channelIndex]->grabOffset;
          auto const posHOA = localToNormalized2DPosition (posPixel);
          _channels[channelIndex]->setPosition (posHOA);
          _channels[channelIndex]->recomputeHeight ();
        }
    }
}

void
MotionComponent::updateChannelBlobHighlight (juce::Point<float> posMousePixel)
{
  jassert (_boundsCenterRegion.getWidth ()
           == _boundsCenterRegion.getHeight ());

  for (auto channelIndex = 0u; channelIndex < _channels.size ();
       ++channelIndex)
    _viewStates[channelIndex]->highlighted = false;

  auto closestIndex = getClosestBlobIndexWithinRadius (
      posMousePixel, getActiveDistanceInPixel ());
  if (closestIndex.has_value ())
    _viewStates[closestIndex.value ()]->highlighted = true;
}

std::optional<size_t>
MotionComponent::getClosestBlobIndexWithinRadius (juce::Point<float> posPixel,
                                                  float radiusPixel) const
{
  auto minDistance = std::numeric_limits<float>::infinity ();
  auto minIndex = 0u;
  for (auto channelIndex = 0u; channelIndex < _channels.size ();
       ++channelIndex)
    {
      auto const blobPosInPixel = normalizedToLocal2DPosition (
          _channels[channelIndex]->getPosition ());

      auto const distance = blobPosInPixel.getDistanceFrom (posPixel);
      if (distance < radiusPixel && distance < minDistance)
        {
          minDistance = distance;
          minIndex = channelIndex;
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

  updateBoundsAndTransform ();

  {
    GLContextGraphics graphics (_glContext, //
                                _boundsRender.getWidth (),
                                _boundsRender.getHeight ());

    graphics.get ().addTransform (_transformNormalizedToLocal);

    OpenGLHelpers::clear (Colours::background);

    drawCircle (graphics.get ());
    drawChannelBlobs (graphics.get ());

    // auto constexpr blobSize = 2;
    // auto blob = juce::Rectangle<float> (blobSize, blobSize);
    // graphics.get ().fillEllipse (
    //     blob.withCentre (juce::Point<float> (0.f, 0.f)));
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

  auto constexpr opacityHead = 0.4f;
  auto const diameterHead = 2.f * reduceFactorHead;
  auto const boundsHead = juce::Rectangle<float> ().withSizeKeepingCentre (
      diameterHead, diameterHead);
  _drawableHead->drawWithin (g, boundsHead, juce::RectanglePlacement::centred,
                             opacityHead);

  auto const diameterCircle = 2.f;
  auto const boundsCircle = juce::Rectangle<float> ().withSizeKeepingCentre (
      diameterCircle, diameterCircle);

  auto constexpr opacityIsoSphere = 0.3f;
  g.setOpacity (opacityIsoSphere);
  g.drawImage (_imageIsoSphere, boundsCircle);

  g.setOpacity (1.f);
}

void
MotionComponent::drawChannelBlobs (juce::Graphics &g)
{
  using namespace juce::gl;

  _imageBlend->clear (_imageBlend->getBounds ());

  {
    juce::Graphics gFBO{ *_imageBlend };
    gFBO.addTransform (_transformNormalizedToLocal);

    for (auto channelIndex = 0u; channelIndex < _channels.size ();
         ++channelIndex)
      {
        auto blobSize = 2 * reduceFactorBlobs;
        blobSize
            *= (1.f
                + std::clamp (_channels[channelIndex]->getPosition ().z (),
                              0.f, 1.f)
                      * 0.7f);

        auto const blob
            = juce::Rectangle<float> (0.f, 0.f, blobSize, blobSize);
        auto const blobGrabbed
            = juce::Rectangle<float> (0.f, 0.f, //
                                      blobSize * activeAreaAroundBlobFactor,
                                      blobSize * activeAreaAroundBlobFactor);
        auto const blobHighlight = juce::Rectangle<float> (
            0.f, 0.f, //
            blobSize * blobHighlightFactor, blobSize * blobHighlightFactor);

        auto pos
            = cartesian2DHOA2JUCE (_channels[channelIndex]->getPosition ());

        auto colour = _viewStates[channelIndex]->colour;

        // DBG (juce::String ("drawing at position: ")
        //      + juce::String (pos.getX ()) + " " + juce::String (pos.getY
        //      ()));

        if (_viewStates[channelIndex]->grabbed)
          {
            gFBO.setColour (juce::Colours::grey.withAlpha (0.2f));
            gFBO.fillEllipse (blobGrabbed.withCentre (pos));
          }

        if (_viewStates[channelIndex]->highlighted)
          {
            gFBO.setColour (
                colour.withLightness (colour.getLightness () + 0.2f));
            gFBO.fillEllipse (blobHighlight.withCentre (pos));
          }

        // debug: draw anchor
        // gFBO.setColour (colour.withAlpha (0.4f));
        // gFBO.fillEllipse (
        //     blob.withCentre (_viewStates[channelIndex]->posAnchor));
        gFBO.setColour (colour);
        gFBO.fillEllipse (blob.withCentre (pos));
      }
  }

  g.setOpacity (0.8f);
  g.drawImage (*_imageBlend, _boundsRender.toFloat ().transformedBy (
                                 _transformNormalizedToLocal.inverted ()));
  g.setOpacity (1.f);
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

// Pos
// MotionComponent::localToNormalizedPosition (
//     juce::Point<float> const &posLocal) const
// {
//   jassert (_boundsCenterRegion.getWidth ()
//            == _boundsCenterRegion.getHeight ());

//   auto const halfSize = _boundsCenterRegion.getWidth () / 2.f;

//   auto posNormalized = (posLocal - _boundsCenterRegion.getCentre
//   ().toFloat
//   ())
//                        / halfSize / reduceFactorCircle;

//   return Pos::fromCartesian ( //
//       posNormalized.getX (),  //
//       posNormalized.getY (),  //
//       0                       // no elevation for now
//   );
// }

void
MotionComponent::openGLContextClosing ()
{
  DBG ("openGLContextClosing");
}
}
