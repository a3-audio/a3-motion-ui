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

namespace a3
{

MotionComponent::MotionComponent (
    std::vector<std::unique_ptr<Channel> > const &channels)
    : _channels (channels),
      _slider (juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox)
{
  // _image.setImage (juce::ImageFileFormat::loadFrom (
  //     juce::File ("./resources/a3_logo-dark.png")));

  // addChildComponent (_image);
  // _image.setVisible (true);

  addChildComponent (_slider);
  _slider.setBounds (0, 0, 50, 50);
  _slider.setVisible (true);

  _glContext.setRenderer (this);
  _glContext.setContinuousRepainting (true);
  _glContext.attachTo (*this);
}

MotionComponent::~MotionComponent ()
{
  _glContext.detach ();
}

void
MotionComponent::paint (juce::Graphics &g)
{
  //  juce::ignoreUnused (g);
  g.fillAll (juce::Colours::aquamarine);
}

void
MotionComponent::resized ()
{
  _image.setBounds (getLocalBounds ());
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
  // DBG ("renderOpenGL");

  static auto lastT = std::chrono::high_resolution_clock::now ();

  auto now = std::chrono::high_resolution_clock::now ();
  auto deltaT
      = std::chrono::duration_cast<std::chrono::milliseconds> (now - lastT)
            .count ();
  lastT = now;

  juce::Logger::writeToLog ("frametime: " + juce::String (deltaT));
}

void
MotionComponent::openGLContextClosing ()
{
  DBG ("openGLContextClosing");
}
}
