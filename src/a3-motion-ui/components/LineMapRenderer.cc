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

#include <a3-motion-ui/components/LineMapRenderer.hh>

#include <a3-motion-ui/components/LineMapCapsules.hh>

#include <cstddef>

namespace a3
{

namespace
{

// Map texels in, image orientation (y down). The sphere shader was written
// against maps uploaded with OpenGLTexture::loadImage, which turns an image
// upside down on the way to the GPU and so puts its top row at the top of
// the texture; drawing y down into clip space here puts this map's rows in
// the same place.
char const *const vertexShader = R"(
attribute vec2 aPosition;
attribute vec4 aSegment;
attribute float aHalfWidth;
attribute vec3 aColour;
uniform float uMapSize;
varying vec2 vTexel;
varying vec4 vSegment;
varying float vHalfWidth;
varying vec3 vColour;

void main ()
{
    vTexel = aPosition;
    vSegment = aSegment;
    vHalfWidth = aHalfWidth;
    vColour = aColour;
    gl_Position = vec4 (aPosition.x / uMapSize * 2.0 - 1.0,
                        1.0 - aPosition.y / uMapSize * 2.0, 0.0, 1.0);
}
)";

// A capsule: everything within half the width of the segment, the last
// texel faded for the edge. Written premultiplied, because that is what a
// juce::Image holds and what the sphere shader has always sampled.
char const *const fragmentShader = R"(
varying vec2 vTexel;
varying vec4 vSegment;
varying float vHalfWidth;
varying vec3 vColour;

void main ()
{
    vec2 a = vSegment.xy;
    vec2 ab = vSegment.zw - a;
    float length2 = dot (ab, ab);
    float t = length2 > 0.0 ? clamp (dot (vTexel - a, ab) / length2, 0.0, 1.0)
                            : 0.0;
    float distance = length (vTexel - (a + t * ab));
    float coverage = clamp (vHalfWidth + 0.5 - distance, 0.0, 1.0);
    if (coverage <= 0.0)
        discard;
    gl_FragColor = vec4 (vColour * coverage, coverage);
}
)";

static_assert (sizeof (CapsuleVertex) == 10 * sizeof (float),
               "the attribute offsets below assume ten packed floats");

}

bool
LineMapRenderer::initialise (juce::OpenGLContext &context)
{
  using namespace juce::gl;

  _context = &context;
  auto program = std::make_unique<juce::OpenGLShaderProgram> (context);
  if (!program->addVertexShader (vertexShader)
      || !program->addFragmentShader (fragmentShader) || !program->link ())
    {
      juce::Logger::writeToLog ("LineMapRenderer: "
                                + program->getLastError ()
                                + " -- the trajectories have no glow");
      return false;
    }

  auto const id = program->getProgramID ();
  _aPosition = glGetAttribLocation (id, "aPosition");
  _aSegment = glGetAttribLocation (id, "aSegment");
  _aHalfWidth = glGetAttribLocation (id, "aHalfWidth");
  _aColour = glGetAttribLocation (id, "aColour");
  _uMapSize = glGetUniformLocation (id, "uMapSize");
  if (_aPosition < 0 || _aSegment < 0 || _aHalfWidth < 0 || _aColour < 0)
    {
      juce::Logger::writeToLog (
          "LineMapRenderer: an attribute is missing -- the trajectories "
          "have no glow");
      return false;
    }

  glGenBuffers (1, &_vbo);
  _program = std::move (program);
  return true;
}

void
LineMapRenderer::shutdown ()
{
  using namespace juce::gl;

  for (auto &map : _maps)
    map.release ();
  if (_vbo != 0)
    glDeleteBuffers (1, &_vbo);
  _vbo = 0;
  _program.reset ();
  _context = nullptr;
}

GLuint
LineMapRenderer::paint (int channel, std::vector<MapStroke> const &strokes)
{
  using namespace juce::gl;

  if (!isReady () || channel < 0 || channel >= channels)
    return 0;

  auto &map = _maps[static_cast<std::size_t> (channel)];
  if (!map.isValid ())
    map.initialise (*_context, _size, _size);
  if (!map.isValid ())
    return 0;

  auto const vertices = capsuleVertices (strokes);

  // Leave everything as it was found: this runs at the start of a frame,
  // ahead of the sphere pass and JUCE's own drawing.
  GLint framebufferWas = 0, arrayBufferWas = 0, programWas = 0;
  GLint viewportWas[4] = { 0, 0, 0, 0 };
  glGetIntegerv (GL_FRAMEBUFFER_BINDING, &framebufferWas);
  glGetIntegerv (GL_ARRAY_BUFFER_BINDING, &arrayBufferWas);
  glGetIntegerv (GL_CURRENT_PROGRAM, &programWas);
  glGetIntegerv (GL_VIEWPORT, viewportWas);
  auto const blendWasOn = glIsEnabled (GL_BLEND);
  GLint srcRGB = 0, dstRGB = 0, srcAlpha = 0, dstAlpha = 0;
  glGetIntegerv (GL_BLEND_SRC_RGB, &srcRGB);
  glGetIntegerv (GL_BLEND_DST_RGB, &dstRGB);
  glGetIntegerv (GL_BLEND_SRC_ALPHA, &srcAlpha);
  glGetIntegerv (GL_BLEND_DST_ALPHA, &dstAlpha);

  map.makeCurrentAndClear ();
  // makeCurrentAndClear() binds and clears but leaves the viewport alone, so
  // without this the map was painted at the *screen's* size: every stroke
  // magnified from the bottom left and cut off at the map's edge, which on
  // the rig was a hard line through the sphere 1.3 radii right of centre.
  glViewport (0, 0, _size, _size);

  if (!vertices.empty ())
    {
      _program->use ();
      glUniform1f (_uMapSize, static_cast<GLfloat> (_size));

      glEnable (GL_BLEND);
      glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

      glBindBuffer (GL_ARRAY_BUFFER, _vbo);
      glBufferData (GL_ARRAY_BUFFER,
                    static_cast<GLsizeiptr> (vertices.size ()
                                             * sizeof (CapsuleVertex)),
                    vertices.data (), GL_STREAM_DRAW);

      auto const stride = static_cast<GLsizei> (sizeof (CapsuleVertex));
      auto const attribute = [stride] (GLint location, GLint floats,
                                       std::size_t offset) {
        glEnableVertexAttribArray (static_cast<GLuint> (location));
        glVertexAttribPointer (static_cast<GLuint> (location), floats,
                               GL_FLOAT, GL_FALSE, stride,
                               reinterpret_cast<void const *> (offset));
      };
      attribute (_aPosition, 2, offsetof (CapsuleVertex, x));
      attribute (_aSegment, 4, offsetof (CapsuleVertex, ax));
      attribute (_aHalfWidth, 1, offsetof (CapsuleVertex, halfWidth));
      attribute (_aColour, 3, offsetof (CapsuleVertex, r));

      glDrawArrays (GL_TRIANGLES, 0, static_cast<GLsizei> (vertices.size ()));

      for (auto const location : { _aPosition, _aSegment, _aHalfWidth, _aColour })
        glDisableVertexAttribArray (static_cast<GLuint> (location));
    }

  map.releaseAsRenderingTarget ();
  glBindFramebuffer (GL_FRAMEBUFFER, static_cast<GLuint> (framebufferWas));
  glBindBuffer (GL_ARRAY_BUFFER, static_cast<GLuint> (arrayBufferWas));
  glUseProgram (static_cast<GLuint> (programWas));
  glViewport (viewportWas[0], viewportWas[1], viewportWas[2], viewportWas[3]);

  glBlendFuncSeparate (static_cast<GLenum> (srcRGB),
                       static_cast<GLenum> (dstRGB),
                       static_cast<GLenum> (srcAlpha),
                       static_cast<GLenum> (dstAlpha));
  if (blendWasOn)
    glEnable (GL_BLEND);
  else
    glDisable (GL_BLEND);

  return map.getTextureID ();
}

}
