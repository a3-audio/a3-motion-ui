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

#include <memory>

#include <a3-motion-ui/Helpers.hh>

namespace a3
{

InputOutputAdapter::InputOutputAdapter () : juce::Thread ("InputOutputAdapter")
{
  for (auto &buttonLED : _valueButtonLEDs)
    buttonLED.addListener (this);
  for (auto &channelLEDs : _valuePadLEDs)
    for (auto &padLED : channelLEDs)
      padLED.addListener (this);

  startTimer (1);
}

InputOutputAdapter::~InputOutputAdapter ()
{
  for (auto &channelLEDs : _valuePadLEDs)
    for (auto &padLED : channelLEDs)
      padLED.removeListener (this);
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
InputOutputAdapter::getPadLED (int channel, int pad)
{
  jassert (channel >= 0 && channel < numChannels);
  jassert (pad >= 0 && pad < numPadsPerChannel);
  return _valuePadLEDs[static_cast<size_t> (channel)]
                      [static_cast<size_t> (pad)];
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

int
InputOutputAdapter::getNumChannels ()
{
  return numChannels;
}

int
InputOutputAdapter::getNumPadsPerChannel ()
{
  return numPadsPerChannel;
}

int
InputOutputAdapter::getNumPotsPerChannel ()
{
  return numPotsPerChannel;
}

int
InputOutputAdapter::getNumButtons ()
{
  return numButtons;
}

void
InputOutputAdapter::run ()
{
  while (true)
    {
      if (threadShouldExit ())
        break;

      processInput ();
      processOutput ();

      auto constexpr sleepMs = 1;
      juce::Thread::sleep (sleepMs);
    }
}

void
InputOutputAdapter::inputPadValue (PadIndex const &padIndex, bool value)
{
  jassert (padIndex.channel >= 0 && padIndex.channel < numChannels);
  jassert (padIndex.pad >= 0 && padIndex.pad < numPadsPerChannel);

  if (value != _lastPadValues[padIndex])
    {
      auto message = std::make_unique<InputMessagePad> ();
      message->event = value ? InputMessagePad::Event::Press
                             : InputMessagePad::Event::Release;
      message->padIndex = padIndex;
      submitInputMessage (std::move (message));

      _lastPadValues[padIndex] = value;
    }
}

void
InputOutputAdapter::inputButtonValue (Button button, bool value)
{
  if (value != _lastButtonValues[button])
    {
      auto message = std::make_unique<InputMessageButton> ();
      message->button = button;
      message->event = value ? InputMessageButton::Event::Press
                             : InputMessageButton::Event::Release;
      submitInputMessage (std::move (message));

      _lastButtonValues[button] = value;
    }
}

void
InputOutputAdapter::inputEncoderEvent (int channel,
                                       InputMessageEncoder::Event event)
{
  jassert (channel >= 0 && channel < numChannels);

  auto message = std::make_unique<InputMessageEncoder> ();
  message->event = event;
  message->channel = channel;
  submitInputMessage (std::move (message));
}

void
InputOutputAdapter::inputPotValue (int channel, int pot, float value)
{
  jassert (channel >= 0 && channel < numChannels);
  jassert (pot >= 0 && pot < numPotsPerChannel);
  jassert (value >= 0 && value <= 1.f);

  auto message = std::make_unique<InputMessagePot> ();
  message->channel = channel;
  message->pot = pot;
  message->value = value;
  submitInputMessage (std::move (message));
}

void
InputOutputAdapter::inputTapTime (juce::int64 timeMicros)
{
  auto message = std::make_unique<InputMessageTap> ();
  message->timeMicros = timeMicros;
  submitInputMessage (std::move (message));
}

void
InputOutputAdapter::submitInputMessage (std::unique_ptr<InputMessage> message)
{
  jassert (_fifoAbstractInput.getFreeSpace () > 0);

  const auto scope = _fifoAbstractInput.write (1);
  jassert (scope.blockSize1 == 1);
  jassert (scope.blockSize2 == 0);
  jassert (scope.startIndex1 >= 0);

  auto startIndex = static_cast<std::size_t> (scope.startIndex1);
  _fifoInput[startIndex] = std::move (message);
}

void
InputOutputAdapter::timerCallback ()
{
  auto const ready = _fifoAbstractInput.getNumReady ();
  const auto scope = _fifoAbstractInput.read (ready);

  jassert (scope.blockSize1 + scope.blockSize2 == ready);

  if (scope.blockSize1 > 0)
    {
      for (int idx = scope.startIndex1;
           idx < scope.startIndex1 + scope.blockSize1; ++idx)
        {
          jassert (idx >= 0);
          handleInputMessage (
              std::move (_fifoInput[static_cast<std::size_t> (idx)]));
        }
    }

  if (scope.blockSize2 > 0)
    {
      for (int idx = scope.startIndex2;
           idx < scope.startIndex2 + scope.blockSize2; ++idx)
        {
          jassert (idx >= 0);
          handleInputMessage (
              std::move (_fifoInput[static_cast<std::size_t> (idx)]));
        }
    }
}

void
InputOutputAdapter::handleInputMessage (std::unique_ptr<InputMessage> message)
{
  switch (message->type)
    {
    case InputMessage::Type::Pad:
      {
        auto messagePad = dynamic_cast<InputMessagePad *> (message.get ());
        jassert (messagePad != nullptr);
        handlePad (*messagePad);
        break;
      }
    case InputMessage::Type::Button:
      {
        auto messageButton
            = dynamic_cast<InputMessageButton *> (message.get ());
        jassert (messageButton != nullptr);
        handleButton (*messageButton);
        break;
      }
    case InputMessage::Type::Encoder:
      {
        auto messageEncoder
            = dynamic_cast<InputMessageEncoder *> (message.get ());
        jassert (messageEncoder != nullptr);
        handleEncoder (*messageEncoder);
        break;
      }
    case InputMessage::Type::Pot:
      {
        auto messagePot = dynamic_cast<InputMessagePot *> (message.get ());
        jassert (messagePot != nullptr);
        handlePot (*messagePot);
        break;
      }
    case InputMessage::Type::Tap:
      {
        auto messageTap = dynamic_cast<InputMessageTap *> (message.get ());
        jassert (messageTap != nullptr);
        handleTap (*messageTap);
        break;
      }
    }

  // freeing should happen automatically by leaving the scope
  message = nullptr;
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
  jassert (juce::MessageManager::getInstance ()->isThisTheMessageThread ());

  auto index = static_cast<size_t> (message.button);
  _valueButtons[index]
      = message.event == InputMessageButton::Event::Press ? true : false;
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
  for (auto index = 0u; index < numButtons; ++index)
    {
      if (value.refersToSameSourceAs (_valueButtonLEDs[index]))
        {
          auto message = std::make_unique<OutputMessageButtonLED> ();
          message->button = static_cast<Button> (index);
          message->value = value.getValue ();
          submitOutputMessage (std::move (message));
          return;
        }
    }
  for (auto channel = 0u; channel < numChannels; ++channel)
    {
      for (auto pad = 0u; pad < numPadsPerChannel; ++pad)
        {
          if (value.refersToSameSourceAs (_valuePadLEDs[channel][pad]))
            {
              auto message = std::make_unique<OutputMessagePadLED> ();
              message->padIndex.channel = static_cast<int> (channel);
              message->padIndex.pad = static_cast<int> (pad);
              message->colour = juce::VariantConverter<juce::Colour>::fromVar (
                  value.getValue ());
              submitOutputMessage (std::move (message));
              return;
            }
        }
    }
}

void
InputOutputAdapter::submitOutputMessage (
    std::unique_ptr<OutputMessage> message)
{
  jassert (_fifoAbstractOutput.getFreeSpace () > 0);

  const auto scope = _fifoAbstractOutput.write (1);
  jassert (scope.blockSize1 == 1);
  jassert (scope.blockSize2 == 0);
  jassert (scope.startIndex1 >= 0);

  auto startIndex = static_cast<std::size_t> (scope.startIndex1);
  _fifoOutput[startIndex] = std::move (message);
}

void
InputOutputAdapter::processOutput ()
{
  auto const ready = _fifoAbstractOutput.getNumReady ();
  const auto scope = _fifoAbstractOutput.read (ready);

  jassert (scope.blockSize1 + scope.blockSize2 == ready);

  if (scope.blockSize1 > 0)
    {
      for (int idx = scope.startIndex1;
           idx < scope.startIndex1 + scope.blockSize1; ++idx)
        {
          jassert (idx >= 0);
          handleOutputMessage (
              std::move (_fifoOutput[static_cast<std::size_t> (idx)]));
        }
    }

  if (scope.blockSize2 > 0)
    {
      for (int idx = scope.startIndex2;
           idx < scope.startIndex2 + scope.blockSize2; ++idx)
        {
          jassert (idx >= 0);
          handleOutputMessage (
              std::move (_fifoOutput[static_cast<std::size_t> (idx)]));
        }
    }
}

void
InputOutputAdapter::handleOutputMessage (
    std::unique_ptr<OutputMessage> message)
{
  switch (message->type)
    {
    case OutputMessage::Type::ButtonLED:
      {
        auto messageButtonLED
            = dynamic_cast<OutputMessageButtonLED *> (message.get ());
        jassert (messageButtonLED != nullptr);
        outputButtonLED (messageButtonLED->button, messageButtonLED->value);
        break;
      }
    case OutputMessage::Type::PadLED:
      {
        auto messagePadLED
            = dynamic_cast<OutputMessagePadLED *> (message.get ());
        jassert (messagePadLED != nullptr);
        outputPadLED (messagePadLED->padIndex, messagePadLED->colour);
        break;
      }
    }

  message = nullptr;
}
}
