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

#include "InputOutputAdapterV2.hh"

#include <gpiod.h>
#include <time.h>

#include <a3-motion-ui/MotionController.hh>

namespace
{

auto constexpr nameConsumer = "a3-motion-ui";
auto constexpr gpioChipname = "gpiochip0";

auto constexpr gpioOffsetShiftButton = 17;
auto constexpr gpioOffsetShiftLED = 27;

auto constexpr gpioOffsetTapButton = 23;
auto constexpr gpioOffsetTapLED = 24;

auto constexpr gpioOffsetRecordButton = 12;
auto constexpr gpioOffsetRecordLED = 16;

}

namespace a3
{

class GPIOThread : public juce::Thread
{
public:
  GPIOThread (InputOutputAdapterV2 &ioAdapter)
      : juce::Thread ("InputOutputAdapterV2 GPIO"), _ioAdapter (ioAdapter)
  {
  }

  static int
  eventCallback (int eventType, uint lineOffset, const timespec *,
                 void *userdata)
  {
    auto gpioThread = static_cast<GPIOThread *> (userdata);

    if (gpioThread->threadShouldExit ())
      return GPIOD_CTXLESS_EVENT_CB_RET_STOP;
    if (eventType == GPIOD_CTXLESS_EVENT_CB_TIMEOUT)
      return GPIOD_CTXLESS_EVENT_CB_RET_OK;

    auto value
        = eventType == GPIOD_CTXLESS_EVENT_CB_RISING_EDGE ? true : false;

    switch (lineOffset)
      {
      case gpioOffsetShiftButton:
        gpioThread->_ioAdapter.setButtonValue (
            MotionController::InputMessageButton::ButtonId::Shift, value);
        break;
      case gpioOffsetTapButton:
        gpioThread->_ioAdapter.setButtonValue (
            MotionController::InputMessageButton::ButtonId::Tap, value);
        break;
      case gpioOffsetRecordButton:
        gpioThread->_ioAdapter.setButtonValue (
            MotionController::InputMessageButton::ButtonId::Record, value);
        break;
      default:
        break;
      }

    return GPIOD_CTXLESS_EVENT_CB_RET_OK;
  }

  void
  run () override
  {
    uint constexpr offsets[] = {
      gpioOffsetShiftButton,
      gpioOffsetTapButton,
      gpioOffsetRecordButton,
    };

    auto const timeout = timespec{ 1, 0 };
    gpiod_ctxless_event_monitor_multiple (gpioChipname,                   //
                                          GPIOD_CTXLESS_EVENT_BOTH_EDGES, //
                                          offsets, 3,                     //
                                          false,                          //
                                          nameConsumer,                   //
                                          &timeout,                       //
                                          nullptr,                        //
                                          eventCallback,                  //
                                          this                            //
    );

    juce::Logger::writeToLog ("gpiod_ctxless_event_monitor_multiple returned");
  }

private:
  InputOutputAdapterV2 &_ioAdapter;
};

InputOutputAdapterV2::InputOutputAdapterV2 (MotionController &motionController)
    : InputOutputAdapter (motionController)
{
  serialInit ();

  _gpioThread = std::make_unique<GPIOThread> (*this);
  _gpioThread->startThread ();
}

InputOutputAdapterV2::~InputOutputAdapterV2 ()
{
  _gpioThread->stopThread (-1);
  _serialPort.Close ();
}

void
InputOutputAdapterV2::serialInit ()
{
  using namespace LibSerial;

  auto constexpr serialDevice = "/dev/ttyACM0";
  _serialPort.Open (serialDevice, std::ios::in);
  _serialPort.SetBaudRate (BaudRate::BAUD_115200);
  _serialPort.SetCharacterSize (CharacterSize::CHAR_SIZE_8);
  _serialPort.SetFlowControl (FlowControl::FLOW_CONTROL_NONE);
  _serialPort.SetParity (Parity::PARITY_NONE);
  _serialPort.SetStopBits (StopBits::STOP_BITS_1);

  juce::Logger::writeToLog ("initialized libserial");
}

void
InputOutputAdapterV2::gpioInit ()
{
}

void
InputOutputAdapterV2::processInput ()
{
  if (_serialPort.IsDataAvailable ())
    {
      auto const bytesReady = _serialPort.GetNumberOfBytesAvailable ();
      for (auto readCount = 0; readCount < bytesReady; ++readCount)
        {
          char c;
          _serialPort.ReadByte (c);
          _serialBuffer[_nextWriteOffset++] = c;
          if (c == '\n')
            {
              serialParseLine ();
              _nextWriteOffset = 0;
            }
        }
    }
}

void
InputOutputAdapterV2::serialParseLine ()
{
  juce::Logger::writeToLog (juce::String (&_serialBuffer[0]));
}

}
