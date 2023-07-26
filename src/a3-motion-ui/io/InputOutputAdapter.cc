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

#include "InputOutputAdapter.hh"

namespace a3
{

InputOutputAdapter::InputOutputAdapter (MotionController &motionController)
    : juce::Thread ("InputOutputAdapter"), _motionController (motionController)
{
}

InputOutputAdapter::~InputOutputAdapter () {}

void
InputOutputAdapter::run ()
{
  while (true)
    {
      if (threadShouldExit ())
        break;

      processInput ();
      processLEDOutput ();

      auto constexpr sleepMs = 1;
      juce::Thread::sleep (sleepMs);
    }
}

void
InputOutputAdapter::setPadValue (std::array<int, 2> const &padIndex,
                                 bool value)
{
  if (value != _lastPadValues[padIndex])
    {
      auto message = new MotionController::InputMessageButton ();

      message->id = MotionController::InputMessageButton::ButtonId::Pad;
      message->event
          = value ? MotionController::InputMessageButton::Event::Press
                  : MotionController::InputMessageButton::Event::Release;
      message->padIndex = padIndex;

      _motionController.postMessage (message);
    }
}

void
InputOutputAdapter::setButtonValue (ButtonId id, bool value)
{
  if (value != _lastButtonValues[id])
    {
      auto message = new MotionController::InputMessageButton ();

      message->id = id;
      message->event
          = value ? MotionController::InputMessageButton::Event::Press
                  : MotionController::InputMessageButton::Event::Release;

      _motionController.postMessage (message);
    }
}

void
InputOutputAdapter::processLEDOutput ()
{
}

}
