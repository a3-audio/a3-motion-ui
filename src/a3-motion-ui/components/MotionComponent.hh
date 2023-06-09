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

#include <a3-motion-engine/util/Geometry.hh>

namespace a3
{

class Channel;
class ChannelViewState;

class MotionComponent : public juce::Component,
                        public juce::OpenGLRenderer,
                        public juce::Timer
{
public:
  MotionComponent (std::vector<std::unique_ptr<Channel> > const &,
                   std::vector<std::unique_ptr<ChannelViewState> > &);
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

private:
  void printFrameTime ();
  void updateBounds ();
  void renderBoundsChanged ();

  void updateChannelBlobHighlight (juce::Point<float> posMousePixel);

  void drawCircle (juce::Graphics &g);
  void drawChannelBlobs (juce::Graphics &g);

  float getBlobSizeInPixel () const;
  float getActiveDistanceInPixel () const;
  juce::Point<float> normalizedToLocalPosition (Pos const &posNorm) const;
  Pos localToNormalizedPosition (juce::Point<float> const &posLocal) const;

  std::optional<size_t>
  getClosestBlobIndexWithinRadius (juce::Point<float> posPixel,
                                   float radiusPixel) const;

  void disoccludeBlobs ();

  std::vector<std::unique_ptr<Channel> > const &_channels;
  std::vector<std::unique_ptr<ChannelViewState> > &_viewStates;
  std::optional<size_t> _grabbedIndex;

  juce::OpenGLContext _glContext;

  // component bounds updated by the UI thread
  juce::Rectangle<int> _bounds;

  // copies of the component bounds for asynchronous use in the GL
  // renderer thread
  std::mutex _mutexBounds;
  juce::Rectangle<int> _boundsRender;

  // derived bounds for drawing convenience
  juce::Rectangle<int> _boundsCenterRegion;

  std::unique_ptr<juce::Image> _imageBlend;
  juce::Image _imageIsoSphere;
  std::unique_ptr<juce::Drawable> _drawableHead;
};
}
