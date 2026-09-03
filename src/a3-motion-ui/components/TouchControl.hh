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

#include <a3-motion-ui/components/DragAccumulator.hh>

namespace a3
{

/**
 * TouchControl
 *
 * An invisible hit area with a gesture. It draws nothing and holds no
 * value — that stays with the parent, which positions it and paints in
 * its place. What it contributes is exactly what JUCE will not give you
 * without a component: bounds-based hit testing, event routing, and a
 * drag whose origin is the touch that started it.
 *
 * `primary`/`secondary` are the identity the parent hands out (section
 * and sub-element, say) and gets back unchanged in the callbacks, so the
 * parent never has to compare pointers.
 *
 * Vertical and relative: up is more. A drag that emitted no step is a
 * tap; one that emitted steps is not, or a toggle would step once more
 * when the finger comes off.
 *
 * Several of these can be dragged at once — JUCE delivers each finger's
 * events to the component it went down on, and each instance keeps its
 * own drag.
 */
class TouchControl : public juce::Component
{
public:
  TouchControl ();

  void setIdentity (int primary, int secondary = -1);

  /** The finger going down, before anything is known about what it will
   *  do. Where selection belongs: doing it per increment instead made two
   *  fingers on two controls fight over the one selection and flicker. */
  std::function<void (int primary, int secondary)> onPress;
  /** A touch that emitted no step. */
  std::function<void (int primary, int secondary)> onTap;
  /** Once per threshold crossed, with +1 or -1. */
  std::function<void (int primary, int secondary, int increment)>
      onDragIncrement;
  /** Coming off after a drag that emitted steps — where there is something
   *  to confirm. */
  /** The drag ended. Only after one: a press that never moved reports as
   *  onTap instead. */
  std::function<void (int primary, int secondary)> onDragEnd;
  /** The finger came up, whatever it did in between.
   *
   *  Separate from onDragEnd because a *held* control — the controller page's
   *  modifiers, a pad running a preview for as long as it is down — has to
   *  hear the release even though nothing was dragged, and onDragEnd is
   *  silent in exactly that case. */
  std::function<void (int primary, int secondary)> onRelease;

  void mouseDown (juce::MouseEvent const &event) override;
  void mouseDrag (juce::MouseEvent const &event) override;
  void mouseUp (juce::MouseEvent const &event) override;

private:
  int _primary = 0;
  int _secondary = -1;
  DragAccumulator _drag{ 12 };
};

}
