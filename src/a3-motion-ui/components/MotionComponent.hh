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

#include <a3-motion-engine/Measure.hh>
#include <a3-motion-engine/util/Types.hh>

#include <a3-motion-ui/Helpers.hh>
#include <a3-motion-ui/components/SphereShader.hh>

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
  void unsetPreviewPattern (std::shared_ptr<Pattern> pattern);

  void setBackgroundColour (juce::Colour const &colour);

  // VU-driven lighting: sphere glow and speaker spotlights
  void setSphereGlow (float peak, float rms);
  void setSpeakerLight (int speakerIndex, float peak, float rms);

private:
  void printFrameTime ();
  void updateBoundsAndTransform ();
  void renderBoundsChanged ();

  void updateChannelBlobHighlight (juce::Point<float> posMousePixel);

  void drawCircle (juce::Graphics &g);
  void drawChannelBlobs (juce::Graphics &g);
  void drawPatternPreview (Pattern const &pattern, juce::Graphics &g);

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

  std::set<std::shared_ptr<Pattern> > _patternsPreview;
  std::mutex _mutexPreview;

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
  std::unique_ptr<juce::Drawable> _drawableSpeaker;

  // 3D raytraced sphere shader
  SphereShader _sphereShader;

  // VU-driven sphere glow (/vu/4 = subwoofer)
  std::atomic<float> _vuSphereGlowPeak{ 0.f };
  std::atomic<float> _vuSphereGlowRms{ 0.f };

  // VU-driven speaker spotlights (/vu/5-8 → speakers at 45°,135°,225°,315°)
  std::atomic<float> _vuSpeakerPeak[4]{ {0.f}, {0.f}, {0.f}, {0.f} };
  std::atomic<float> _vuSpeakerRms[4]{ {0.f}, {0.f}, {0.f}, {0.f} };

  // Smoothed VU values (updated per render frame, exponential decay)
  float _smoothGlowPeak = 0.f, _smoothGlowRms = 0.f;
  float _smoothSpotPeak[4]{}, _smoothSpotRms[4]{};
  float _smoothBlobPeak[4]{}, _smoothBlobRms[4]{};

  // Background colour packed as ARGB — lock-free atomic access
  std::atomic<juce::uint32> _backgroundColourPacked{ 0 };

  // Cached corona config (loaded once in newOpenGLContextCreated, avoids JSON lookup per frame)
  struct CoronaConfig
  {
    float vuMax = 0.4f;
    float sizeMin = 1.2f;
    float sizeMax = 2.0f;
    float sizeGrabbed = 1.5f;
    float alphaMin = 0.15f;
    float alphaMax = 0.75f;
    float whiteBlend = 0.5f;
  };
  CoronaConfig _coronaCfg;

  // Frame counter for throttling expensive 2D overlay (speaker SVGs)
  unsigned _frameCount = 0;
};

}
