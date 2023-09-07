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

#include <a3-motion-ui/components/ChannelUIState.hh>
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
MotionComponent::setPreviewPattern (std::shared_ptr<Pattern> pattern)
{
  _patternPreview = pattern;
}

void
MotionComponent::unsetPreviewPattern ()
{
  _patternPreview = nullptr;
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
      _engine.setRecord2DPosition (posHOA);
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

  _engine.releaseRecordPosition ();
}

void
MotionComponent::mouseDrag (const juce::MouseEvent &event)
{
  auto const posPixel = event.getPosition ().toFloat ();

  if (_engine.isRecording ())
    {
      auto const posHOA = localToNormalized2DPosition (posPixel);
      _engine.setRecord2DPosition (posHOA);
    }
  else
    {
      for (auto channel = 0u; channel < _engine.getNumChannels (); ++channel)
        {
          if (_uiStates[channel]->grabbed)
            {
              auto const posPixelOffsetted
                  = posPixel + _uiStates[channel]->grabOffset;
              auto const posHOA
                  = localToNormalized2DPosition (posPixelOffsetted);
              _engine.setChannel2DPosition (channel, posHOA);
            }
        }
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

std::optional<size_t>
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

    if (_patternPreview)
      {
        drawPatternPreview (graphics.get ());
      }

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

    for (auto channel = 0u; channel < _engine.getNumChannels (); ++channel)
      {
        auto const position = _engine.getChannelPosition (channel);
        if (!position.isValid ())
          continue;

        auto blobSize = 2 * reduceFactorBlobs;
        blobSize *= (1.f + std::clamp (position.z (), 0.f, 1.f) * 0.7f);

        auto const blob
            = juce::Rectangle<float> (0.f, 0.f, blobSize, blobSize);
        auto const blobGrabbed
            = juce::Rectangle<float> (0.f, 0.f, //
                                      blobSize * activeAreaAroundBlobFactor,
                                      blobSize * activeAreaAroundBlobFactor);
        auto const blobHighlight = juce::Rectangle<float> (
            0.f, 0.f, //
            blobSize * blobHighlightFactor, blobSize * blobHighlightFactor);

        auto posScreenNormalized = cartesian2DHOA2JUCE (position);

        auto colour = _uiStates[channel]->colour;

        // DBG (juce::String ("drawing at position: ")
        //      + juce::String (pos.getX ()) + " " + juce::String (pos.getY
        //      ()));

        if (_uiStates[channel]->grabbed)
          {
            gFBO.setColour (colour.withAlpha (0.4f));
            gFBO.fillEllipse (blobGrabbed.withCentre (posScreenNormalized));
          }

        if (_uiStates[channel]->highlighted)
          {
            gFBO.setColour (
                colour.withLightness (colour.getLightness () + 0.2f));
            gFBO.fillEllipse (blobHighlight.withCentre (posScreenNormalized));
          }

        // debug: draw anchor
        // gFBO.setColour (colour.withAlpha (0.4f));
        // gFBO.fillEllipse (
        //     blob.withCentre (_uiStates[channelIndex]->posAnchor));
        gFBO.setColour (colour);
        gFBO.fillEllipse (blob.withCentre (posScreenNormalized));
      }
  }

  g.setOpacity (0.8f);
  g.drawImage (*_imageBlend, _boundsRender.toFloat ().transformedBy (
                                 _transformNormalizedToLocal.inverted ()));
  g.setOpacity (1.f);
}

void
MotionComponent::drawPatternPreview (juce::Graphics &g)
{
  auto ticks = _patternPreview->getTicks ();

  auto constexpr lineThickness = 0.04f;

  auto colour = _uiStates[_patternPreview->getChannel ()]->colour;
  g.setColour (colour.withAlpha (0.6f));
  auto const strokeStyle = juce::PathStrokeType (
      lineThickness, juce::PathStrokeType::JointStyle::curved,
      juce::PathStrokeType::EndCapStyle::rounded);
  auto path = juce::Path ();

  jassert (ticks.size () <= std::numeric_limits<int>::max ());
  path.preallocateSpace (static_cast<int> (ticks.size ()));

  auto hasStarted = false;
  for (auto &tick : ticks)
    {
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
              if (path.getLength () > 0.f)
                {
                  g.strokePath (path, strokeStyle);
                }
              else
                {
                  auto ellipse = juce::Rectangle<float> ();
                  ellipse.setSize (lineThickness * 3.f, lineThickness * 3.f);
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
}

}
