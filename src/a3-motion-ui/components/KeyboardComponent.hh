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

#include <functional>

namespace a3
{

/**
 * KeyboardComponent
 *
 * An on-screen keyboard for the touchscreen, for the one thing on this
 * device that is text: a skin's name. It offers exactly the characters a
 * name may hold — lowercase letters, digits and a dash — so nothing typed
 * on it can produce a name the file system or config.json would choke on.
 *
 * Laid out QWERTZ rather than alphabetically: this is a German rig and the
 * hands already know where the letters are.
 *
 * Like the menu it belongs to, it is a child of MotionComponent, whose
 * OpenGL context draws its children over the rendered image — the only
 * place anything can be seen on top of the sphere.
 */
class KeyboardComponent : public juce::Component
{
public:
  KeyboardComponent ();

  std::function<void (juce::juce_wchar)> onCharacter;
  std::function<void ()> onBackspace;
  std::function<void ()> onDone;

  void paint (juce::Graphics &g) override;
  void mouseDown (juce::MouseEvent const &event) override;
  void mouseUp (juce::MouseEvent const &event) override;

private:
  /** One key's face and where it sits. */
  struct Key
  {
    juce::String label;
    juce::juce_wchar character = 0; //< 0 for the two command keys
    bool isBackspace = false;
    bool isDone = false;
    juce::Rectangle<int> bounds;
  };

  void layOutKeys ();
  void resized () override;

  std::vector<Key> _keys;
  int _pressed = -1; //< which key is under the finger, for the lit face
};

}
