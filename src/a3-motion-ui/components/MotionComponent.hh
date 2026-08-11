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

#include <array>
#include <map>

#include <a3-motion-engine/Measure.hh>
#include <a3-motion-engine/util/Types.hh>

#include <a3-motion-ui/Helpers.hh>
#include <a3-motion-ui/ConfigFileWatcher.hh>
#include <a3-motion-ui/components/CoronaScaling.hh>
#include <a3-motion-ui/components/EnergyMap.hh>
#include <a3-motion-ui/components/SphereShader.hh>
#include <a3-motion-ui/osc/OscMessageHandler.hh>

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

  void setPreviewPattern (std::shared_ptr<Pattern> pattern,
                         juce::Path displayPath = {},
                         std::vector<std::pair<float,float>> jumpDots = {});
  void unsetPreviewPattern (std::shared_ptr<Pattern> pattern);

  /** Register display data for a pattern so its trajectory can be drawn
   *  as a faint line whenever it is playing. */
  void setPatternDisplayData (std::shared_ptr<Pattern> pattern,
                              juce::Path displayPath = {},
                              std::vector<std::pair<float,float>> jumpDots = {});
  void removePatternDisplayData (std::shared_ptr<Pattern> pattern);

  void setBackgroundColour (juce::Colour const &colour);

  // Temporarily pause/resume GL redraws (keeps component alive and interactive).
  void setRenderingPaused (bool paused);

  // VU-driven lighting: sphere glow and speaker spotlights
  void setSphereGlow (float peak, float rms);
  void setSpeakerLight (int speakerIndex, float peak, float rms);

  /** One value per grid point of the IEM EnergyVisualizer, in its own order.
   *  Safe to call from the OSC thread. */
  void setEnergyGrid (float const *values, int count);

private:
  void updateBoundsAndTransform ();
  void renderBoundsChanged ();

  void updateChannelBlobHighlight (juce::Point<float> posMousePixel);

  void drawCircle (juce::Graphics &g);
  void drawChannelBlobs (juce::Graphics &g);

  struct PatternDisplayData
  {
    juce::Path displayPath;
    std::vector<std::pair<float,float>> jumpDots;
  };

  void drawPatternPreview (Pattern const &pattern,
                          PatternDisplayData const &displayData,
                          juce::Graphics &g);

  /** Draw a faint trajectory line for a playing pattern. */
  void drawPlayingTrajectory (Pattern const &pattern,
                              PatternDisplayData const &displayData,
                              juce::Graphics &g);

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

  std::map<std::shared_ptr<Pattern>, PatternDisplayData> _patternsPreview;
  std::mutex _mutexPreview;

  // Display data for all loaded patterns — used to draw faint trajectory
  // lines for currently playing patterns (separate from explicit previews).
  std::map<std::shared_ptr<Pattern>, PatternDisplayData> _patternsDisplayData;
  std::mutex _mutexDisplayData;

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

  // Blit resources for compositing 2D overlay onto 3D shader output
  struct BlitResources
  {
    unsigned int program = 0;
    unsigned int vbo = 0;
    int  aPos = -1;
    int  uTex = -1;
    bool valid = false;

    void create ();
    void destroy ();
    void blit (unsigned int textureID, int vpW, int vpH) const;
  };
  BlitResources _blit;

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
  CoronaConfig _coronaCfg;

  // Envelope time constants for the speaker beams, in seconds
  float _spotAttack = 0.08f, _spotDecay = 0.4f;

  // Envelope for the subwoofer glow, in seconds
  float _glowAttack = 0.05f, _glowDecay = 1.2f;

  // Energy over the sphere, from the IEM EnergyVisualizer. Arrives on the OSC
  // thread at 9 Hz, is folded into an equirectangular map on the GL thread and
  // uploaded as a texture — 426 values cannot be uniforms in GLSL 1.20.
  juce::SpinLock _energyLock;
  std::array<float, energyGridPointCount> _energyIncoming{};
  bool _energyPending = false;
  std::unique_ptr<EnergyMapProjection> _energyProjection;
  std::vector<float> _energyTarget, _energySmoothed;
  std::vector<unsigned char> _energyTexels;
  unsigned int _energyTexture = 0;
  float _energyVuMax = 0.05f, _energyCurve = 0.8f;
  float _energyAttack = 0.05f, _energyDecay = 0.25f;

  juce::uint32 _startMillis = 0;

  void uploadEnergyMap ();

  void applyVisualConfig (juce::var const &config);
  void reloadVisualConfigIfChanged ();

  ConfigFileWatcher _configWatcher{
    juce::File::getCurrentWorkingDirectory ().getChildFile (
        "config/config.json")
  };

  // Frame counter for throttling expensive 2D overlay (speaker SVGs)
  unsigned _frameCount = 0;
};

}
