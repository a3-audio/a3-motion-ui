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
#include <memory>

#include <a3-motion-ui/components/TouchControl.hh>

namespace a3
{

/**
 * OverlayButtons
 *
 * Back and close, top right, over whatever overlay is open — the menu, the
 * skin editor, the colour picker. One component rather than three: they are
 * the same two questions wherever you are, and a corner that means different
 * things in different pages is a corner you have to think about.
 *
 * The Menu key has always stepped back one level per press. It still does;
 * this only gives that a target you can see, and adds the one thing a key
 * cannot offer — leaving all of it at once, however deep you are.
 */
class OverlayButtons : public juce::Component
{
public:
  OverlayButtons ();

  /** One level up: the picker, then the name being typed, then the editor,
   *  then the menu. What the Menu key does. */
  std::function<void ()> onBack;
  /** Out of all of it, from wherever. */
  std::function<void ()> onClose;

  /** How tall the pair wants to be, from the header font. */
  static int preferredHeight ();

  void paint (juce::Graphics &g) override;
  void resized () override;

private:
  std::unique_ptr<TouchControl> _backTouch;
  std::unique_ptr<TouchControl> _closeTouch;
  juce::Rectangle<int> _backArea;
  juce::Rectangle<int> _closeArea;

  void paintGlyph (juce::Graphics &g, juce::Rectangle<int> area, bool isClose);
};

}
