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

#include <a3-motion-engine/util/Types.hh>

namespace a3
{

class InputOutputAdapter : public juce::Thread,
                           public juce::Timer,
                           public juce::Value::Listener
{
public:
  enum class Button
  {
    Record,
    Tap,
    Shift,
  };

  InputOutputAdapter ();
  virtual ~InputOutputAdapter ();

  juce::Value &getButton (Button button);
  juce::Value &getButtonLED (Button button);
  juce::Value &getPad (index_t channel, index_t pad);
  juce::Value &getPadLED (index_t channel, index_t pad);
  juce::Value &getEncoderPress (index_t channel);
  juce::Value &getEncoderIncrement (index_t channel);
  juce::Value &getPot (index_t channel, index_t pot);
  juce::Value &getTapTimeMicros ();

  void valueChanged (juce::Value &) override;
  void run () override;
  void timerCallback () override;

  // these might become virtual and be implemented by the specific
  // hardware interface later on.
  index_t getNumChannels ();
  index_t getNumPadsPerChannel ();
  index_t getNumPotsPerChannel ();
  index_t getNumButtons ();

protected:
  static auto constexpr numChannels = 4u;
  static auto constexpr numPadsPerChannel = 4u;
  static auto constexpr numPotsPerChannel = 2u;
  static auto constexpr numButtons = 3u;

  struct PadIndex
  {
    index_t channel;
    index_t pad;

    friend bool
    operator< (const PadIndex &lhs, const PadIndex &rhs)
    {
      return std::tie (lhs.channel, lhs.pad) < std::tie (rhs.channel, rhs.pad);
    }
  };

  struct InputMessage
  {
    virtual ~InputMessage (){};

    enum class Type
    {
      Pad,
      Button,
      Encoder,
      Pot,
      Tap,
    };

    Type type;
  };

  struct InputMessageButton : public InputMessage
  {
    InputMessageButton ()
    {
      type = Type::Button;
    }

    enum class Event
    {
      Press,
      Release,
    } event;

    Button button;
  };

  struct InputMessagePad : public InputMessage
  {
    InputMessagePad ()
    {
      type = Type::Pad;
    }

    enum class Event
    {
      Press,
      Release,
    } event;

    PadIndex padIndex;
  };

  struct InputMessageEncoder : public InputMessage
  {
    InputMessageEncoder ()
    {
      type = Type::Encoder;
    }

    enum class Event
    {
      Press,
      Release,
      Increment,
      Decrement,
    } event;

    index_t channel;
  };

  struct InputMessagePot : public InputMessage
  {
    InputMessagePot ()
    {
      type = Type::Pot;
    }

    index_t channel;
    index_t pot;
    float value;
  };

  struct InputMessageTap : public InputMessage
  {
    InputMessageTap ()
    {
      type = Type::Tap;
    }

    juce::int64 timeMicros;
  };

  struct OutputMessage
  {
    virtual ~OutputMessage (){};

    enum class Type
    {
      ButtonLED,
      PadLED
    };

    Type type;
  };

  struct OutputMessageButtonLED : public OutputMessage
  {
    OutputMessageButtonLED ()
    {
      type = Type::ButtonLED;
    }

    Button button;
    bool value;
  };

  struct OutputMessagePadLED : public OutputMessage
  {
    OutputMessagePadLED ()
    {
      type = Type::PadLED;
    }

    PadIndex padIndex;
    juce::Colour colour;
  };

  // called by the derived classes to read the specific hardware
  // interface and call the input* methods accordingly.
  virtual void processInput () = 0;

  // input functions are called by the derived classes in the
  // InputOutputAdapter thread (this).
  void inputPadValue (PadIndex const &padIndex, bool value);
  void inputButtonValue (Button button, bool value);
  void inputEncoderEvent (index_t channel, InputMessageEncoder::Event event);
  void inputPotValue (index_t channel, index_t pot, float value);
  void inputTapTime (juce::int64 timeMicros);

  virtual void outputButtonLED (Button button, bool value) = 0;
  virtual void outputPadLED (PadIndex, juce::Colour colour) = 0;

private:
  void submitInputMessage (std::unique_ptr<InputMessage> message);
  void handleInputMessage (std::unique_ptr<InputMessage> message);
  void handlePad (InputMessagePad const &message);
  void handleButton (InputMessageButton const &message);
  void handleEncoder (InputMessageEncoder const &message);
  void handlePot (InputMessagePot const &message);
  void handleTap (InputMessageTap const &message);

  void submitOutputMessage (std::unique_ptr<OutputMessage> message);
  void processOutput ();
  void handleOutputMessage (std::unique_ptr<OutputMessage> message);

  std::map<PadIndex, bool> _lastPadValues;
  std::array<std::array<juce::Value, numPadsPerChannel>, numChannels>
      _valuePads;
  std::array<std::array<juce::Value, numPadsPerChannel>, numChannels>
      _valuePadLEDs;

  std::map<Button, bool> _lastButtonValues;
  std::array<juce::Value, numButtons> _valueButtons;
  std::array<juce::Value, numButtons> _valueButtonLEDs;

  std::map<int, bool> _lastEncoderPressValues;
  std::array<juce::Value, numChannels> _valueEncoderPresses;
  std::array<juce::Value, numChannels> _valueEncoderIncrements;

  std::array<std::array<juce::Value, numPotsPerChannel>, numChannels>
      _valuePots;

  juce::Value _valueTapTimeMicros;

  static constexpr int fifoSize = 32;
  juce::AbstractFifo _fifoAbstractInput{ fifoSize };
  std::array<std::unique_ptr<InputMessage>, fifoSize> _fifoInput;
  juce::AbstractFifo _fifoAbstractOutput{ fifoSize };
  std::array<std::unique_ptr<OutputMessage>, fifoSize> _fifoOutput;
};

}
