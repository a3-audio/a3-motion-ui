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

#include <a3-motion-ui/components/LineMapStrokes.hh>

#include <array>
#include <memory>
#include <vector>

namespace a3
{

/** Paints a channel's line or strand map on the GPU (a3-motion-ui#34).
 *
 *  These maps were stroked with juce::Graphics every frame and uploaded;
 *  with four clips playing that was ninety per cent of the renderer and 8.5
 *  frames a second (measured 2026-09-26; 38.5 with this). This paints the
 *  stroke list -- lineMapStrokes(), strandMapStrokes() -- into a framebuffer: one capsule
 *  per segment (capsuleVertices()), in the list's order, each fragment
 *  keeping what lies within half the stroke's width and fading the last
 *  texel, composited premultiplied "over" the way JUCE's own renderer does.
 *  The sphere shader samples the result exactly as it sampled the upload.
 *
 *  GL thread only.
 */
class LineMapRenderer
{
public:
  /** How many texels across the map is: lineMapSize or strandMapSize. */
  explicit LineMapRenderer (int mapSize) : _size (mapSize) {}

  /** False, with a line in the log, if the program does not build here --
   *  the trajectories then have no glow. */
  bool initialise (juce::OpenGLContext &context);
  void shutdown ();
  bool isReady () const { return _program != nullptr; }

  /** Clears this channel's map, paints `strokes` into it and returns the
   *  texture to sample -- 0 if there is none to give. */
  GLuint paint (int channel, std::vector<MapStroke> const &strokes);

  static constexpr int channels = 4;

private:
  int const _size;
  juce::OpenGLContext *_context = nullptr;
  std::unique_ptr<juce::OpenGLShaderProgram> _program;
  GLuint _vbo = 0;
  GLint _aPosition = -1;
  GLint _aSegment = -1;
  GLint _aHalfWidth = -1;
  GLint _aColour = -1;
  GLint _uMapSize = -1;
  std::array<juce::OpenGLFrameBuffer, channels> _maps;
};

}
