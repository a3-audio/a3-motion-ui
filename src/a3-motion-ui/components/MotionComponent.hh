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

#pragma once

#include <JuceHeader.h>

#include <a3-motion-engine/tempo/Measure.hh>
#include <a3-motion-engine/util/Types.hh>

#include <a3-motion-ui/Helpers.hh>

namespace a3
{

class MotionEngine;
class Pattern;
class ChannelUIState;

class MotionComponent : public juce::Component,
                        public juce::OpenGLRenderer,
                        public juce::Timer
{
public:
  MotionComponent (MotionEngine &engine,
                   std::vector<std::unique_ptr<ChannelUIState> > &);
  ~MotionComponent ();

  void resized () override;
  // void paint (juce::Graphics &g) override;

  void mouseMove (const juce::MouseEvent &) override;
  void mouseDown (const juce::MouseEvent &) override;
  void mouseUp (const juce::MouseEvent &) override;
  void mouseDrag (const juce::MouseEvent &) override;

  void newOpenGLContextCreated () override;
  void renderOpenGL () override;
  void openGLContextClosing () override;

  void timerCallback () override;

  void setPreviewPattern (std::shared_ptr<Pattern> pattern);
  void unsetPreviewPattern ();
  void setPreviewTick (index_t tick);

private:
  void printFrameTime ();
  void updateBoundsAndTransform ();
  void renderBoundsChanged ();

  void updateChannelBlobHighlight (juce::Point<float> posMousePixel);

  void drawCircle (juce::Graphics &g);
  void drawChannelBlobs (juce::Graphics &g);
  void drawPatternPreview (juce::Graphics &g);

  float getActiveDistanceInPixel () const;

  juce::Point<float> normalizedToLocal2DPosition (Pos const &posNorm) const;
  Pos localToNormalized2DPosition (juce::Point<float> const &posLocal) const;

  std::optional<index_t>
  getClosestBlobIndexWithinRadius (juce::Point<float> posPixel,
                                   float radiusPixel) const;

  void disoccludeBlobs ();

  MotionEngine &_engine;

  std::vector<std::unique_ptr<ChannelUIState> > &_uiStates;
  std::optional<index_t> _grabbedIndex;

  std::shared_ptr<Pattern> _patternPreview;
  index_t _tickPreview;

  juce::OpenGLContext _glContext;

  // component bounds updated by the UI thread
  juce::Rectangle<int> _bounds;

  // copies of the component bounds for asynchronous use in the GL
  // renderer thread
  std::mutex _mutexBounds;
  juce::Rectangle<int> _boundsRender;

  // derived bounds for drawing convenience
  juce::Rectangle<int> _boundsCenterRegion;
  juce::AffineTransform _transformNormalizedToLocal;

  std::unique_ptr<juce::Image> _imageBlend;
  juce::Image _imageIsoSphere;
  std::unique_ptr<juce::Drawable> _drawableHead;
};

}
