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

InputOutputAdapter::InputOutputAdapter () : juce::Thread ("InputOutputAdapter")
{
  for (auto &valueLED : _valueButtonLEDs)
    valueLED.addListener (this);
}

InputOutputAdapter::~InputOutputAdapter ()
{
  for (auto &valueLED : _valueButtonLEDs)
    valueLED.removeListener (this);
}

juce::Value &
InputOutputAdapter::getButton (Button button)
{
  return _valueButtons[static_cast<size_t> (button)];
}

juce::Value &
InputOutputAdapter::getButtonLED (Button button)
{
  return _valueButtonLEDs[static_cast<size_t> (button)];
}

juce::Value &
InputOutputAdapter::getPad (int channel, int pad)
{
  jassert (channel >= 0 && channel < numChannels);
  jassert (pad >= 0 && pad < numPadsPerChannel);
  return _valuePads[static_cast<size_t> (channel)][static_cast<size_t> (pad)];
}

juce::Value &
InputOutputAdapter::getEncoderPress (int channel)
{
  jassert (channel >= 0 && channel < numChannels);
  return _valueEncoderPresses[static_cast<size_t> (channel)];
}

juce::Value &
InputOutputAdapter::getEncoderIncrement (int channel)
{
  jassert (channel >= 0 && channel < numChannels);
  return _valueEncoderIncrements[static_cast<size_t> (channel)];
}

juce::Value &
InputOutputAdapter::getPot (int channel, int pot)
{
  jassert (channel >= 0 && channel < numChannels);
  jassert (pot >= 0 && pot < numPotsPerChannel);
  return _valuePots[static_cast<size_t> (channel)][static_cast<size_t> (pot)];
}

juce::Value &
InputOutputAdapter::getTapTimeMicros ()
{
  return _valueTapTimeMicros;
}

void
InputOutputAdapter::run ()
{
  while (true)
    {
      if (threadShouldExit ())
        break;

      processInput ();
      // processOutput ();

      auto constexpr sleepMs = 1;
      juce::Thread::sleep (sleepMs);
    }
}

void
InputOutputAdapter::inputPadValue (InputMessagePad::PadIndex const &padIndex,
                                   bool value)
{
  jassert (padIndex.channel >= 0 && padIndex.channel < numChannels);
  jassert (padIndex.pad >= 0 && padIndex.pad < numPadsPerChannel);

  if (value != _lastPadValues[padIndex])
    {
      auto message = new InputMessagePad ();

      message->event = value ? InputMessagePad::Event::Press
                             : InputMessagePad::Event::Release;
      message->padIndex = padIndex;
      postMessage (message);

      _lastPadValues[padIndex] = value;
    }
}

void
InputOutputAdapter::inputButtonValue (InputMessageButton::Id id, bool value)
{
  if (value != _lastButtonValues[id])
    {
      auto message = new InputMessageButton ();
      message->id = id;
      message->event = value ? InputMessageButton::Event::Press
                             : InputMessageButton::Event::Release;
      postMessage (message);

      _lastButtonValues[id] = value;
    }
}

void
InputOutputAdapter::inputEncoderEvent (int channel,
                                       InputMessageEncoder::Event event)
{
  jassert (channel >= 0 && channel < numChannels);

  auto message = new InputMessageEncoder ();
  message->event = event;
  message->channel = channel;
  postMessage (message);
}

void
InputOutputAdapter::inputPotValue (int channel, int pot, float value)
{
  jassert (channel >= 0 && channel < numChannels);
  jassert (pot >= 0 && pot < numPotsPerChannel);
  jassert (value >= 0 && value <= 1.f);

  auto message = new InputMessagePot ();
  message->channel = channel;
  message->pot = pot;
  message->value = value;
  postMessage (message);
}

void
InputOutputAdapter::inputTapTime (juce::int64 timeMicros)
{
  auto message = new InputMessageTap ();
  message->timeMicros = timeMicros;
  postMessage (message);
}

void
InputOutputAdapter::handleMessage (juce::Message const &message)
{
  auto inputMessage = reinterpret_cast<InputMessage const *> (&message);
  switch (inputMessage->type)
    {
    case InputMessage::Type::Pad:
      handlePad (*reinterpret_cast<InputMessagePad const *> (&message));
      break;
    case InputMessage::Type::Button:
      handleButton (*reinterpret_cast<InputMessageButton const *> (&message));
      break;
    case InputMessage::Type::Encoder:
      handleEncoder (
          *reinterpret_cast<InputMessageEncoder const *> (&message));
      break;
    case InputMessage::Type::Pot:
      handlePot (*reinterpret_cast<InputMessagePot const *> (&message));
      break;
    case InputMessage::Type::Tap:
      handleTap (*reinterpret_cast<InputMessageTap const *> (&message));
      break;
    }
}

void
InputOutputAdapter::handlePad (InputMessagePad const &message)
{
  _valuePads[static_cast<size_t> (message.padIndex.channel)]
            [static_cast<size_t> (message.padIndex.pad)]
      = message.event == InputMessagePad::Event::Press ? true : false;
}

void
InputOutputAdapter::handleButton (InputMessageButton const &message)
{
  switch (message.id)
    {
    case InputMessageButton::Id::Shift:
      {
        _valueButtons[static_cast<size_t> (Button::Shift)]
            = message.event == InputMessageButton::Event::Press ? true : false;
        break;
      }
    case InputMessageButton::Id::Tap:
      {
        _valueButtons[static_cast<size_t> (Button::Tap)]
            = message.event == InputMessageButton::Event::Press ? true : false;
        break;
      }
    case InputMessageButton::Id::Record:
      {
        _valueButtons[static_cast<size_t> (Button::Record)]
            = message.event == InputMessageButton::Event::Press ? true : false;
        break;
      }
    default:
      {
        throw std::runtime_error (
            "InputOutputAdapter::handleButton: unknown button type");
      }
    }
}

void
InputOutputAdapter::handleEncoder (InputMessageEncoder const &message)
{
  if (message.event == InputMessageEncoder::Event::Increment
      || message.event == InputMessageEncoder::Event::Decrement)
    {
      auto offset
          = message.event == InputMessageEncoder::Event::Increment ? 1 : -1;

      auto value
          = _valueEncoderIncrements[static_cast<size_t> (message.channel)];

      // The following is required to send the same increment
      // repeatedly. juce::Value notifies observers only when the
      // actual value changes.
      if (value.getValue ().isInt ()
          && static_cast<int> (value.getValue ()) == offset)
        {
          // last value was 'offset' -> notify observers manually
          value.getValueSource ().sendChangeMessage (true);
        }
      else
        {
          // if value was not 'offset' -> assignment triggers notification
          value = offset;
        }
    }
  else if (message.event == InputMessageEncoder::Event::Press
           || message.event == InputMessageEncoder::Event::Release)
    {
      auto value = _valueEncoderPresses[static_cast<size_t> (message.channel)];
      value = message.event == InputMessageEncoder::Event::Press ? 1 : 0;
    }
}

void
InputOutputAdapter::handlePot (InputMessagePot const &message)
{
  _valuePots[static_cast<size_t> (message.channel)]
            [static_cast<size_t> (message.pot)]
      = message.value;
}

void
InputOutputAdapter::handleTap (InputMessageTap const &message)
{
  auto &value = _valueTapTimeMicros;
  if (value.getValue ().isInt64 ()
      && static_cast<juce::int64> (value.getValue ()) == message.timeMicros)
    {
      value.getValueSource ().sendChangeMessage (true);
    }
  else
    {
      value = message.timeMicros;
    }
}

void
InputOutputAdapter::valueChanged (juce::Value &value)
{
  for (auto index = 0u; index < numButtonTypes; ++index)
    {
      if (value.refersToSameSourceAs (_valueButtonLEDs[index]))
        outputButtonLED (static_cast<Button> (index), value.getValue ());
    }
}

}
