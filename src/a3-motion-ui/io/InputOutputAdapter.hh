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

#include <a3-motion-ui/MotionController.hh>

namespace a3
{

class InputOutputAdapter : public juce::Thread,
                           public juce::MessageListener,
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
  juce::Value &getPad (int channel, int pad);
  juce::Value &getEncoderPress (int channel);
  juce::Value &getEncoderIncrement (int channel);
  juce::Value &getPot (int channel, int pot);

  void handleMessage (juce::Message const &) override;
  void valueChanged (juce::Value &) override;

  void run () override;

protected:
  static auto constexpr numChannels = 4;
  static auto constexpr numPadsPerChannel = 4;
  static auto constexpr numPotsPerChannel = 2;
  static auto constexpr numButtonTypes = 3;

  struct InputMessage : public juce::Message
  {
    enum class Type
    {
      Pad,
      Button,
      Encoder,
      Pot,
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

    enum class Id
    {
      Record,
      Tap,
      Shift,
    } id;
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

    struct PadIndex
    {
      int channel;
      int pad;

      friend bool
      operator< (const PadIndex &lhs, const PadIndex &rhs)
      {
        return std::tie (lhs.channel, lhs.pad)
               < std::tie (rhs.channel, rhs.pad);
      }
    } padIndex;
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

    int index;
  };

  struct InputMessagePot : public InputMessage
  {
    InputMessagePot ()
    {
      type = Type::Pot;
    }

    int channel;
    int pot;
    float value;
  };

  // called by the derived classes to read the specific hardware
  // interface and call the input* methods accordingly.
  virtual void processInput () = 0;

  // input functions are called by the derived classes in the
  // InputOutputAdapter thread (this).
  void inputPadValue (InputMessagePad::PadIndex const &padIndex, bool value);
  void inputButtonValue (InputMessageButton::Id id, bool value);
  void inputEncoderEvent (int index, InputMessageEncoder::Event event);
  void inputPotValue (int channel, int pot, float value);

  virtual void outputButtonLED (Button button, bool value) = 0;

private:
  void handlePad (InputMessagePad const &message);
  void handleButton (InputMessageButton const &message);
  void handleEncoder (InputMessageEncoder const &message);
  void handlePot (InputMessagePot const &message);

  std::map<InputMessagePad::PadIndex, bool> _lastPadValues;
  std::array<std::array<juce::Value, numPadsPerChannel>, numChannels>
      _valuePads;

  std::map<InputMessageButton::Id, bool> _lastButtonValues;
  std::array<juce::Value, numButtonTypes> _valueButtons;
  std::array<juce::Value, numButtonTypes> _valueButtonLEDs;

  std::map<int, bool> _lastEncoderPressValues;
  std::array<juce::Value, numChannels> _valueEncoderPresses;
  std::array<juce::Value, numChannels> _valueEncoderIncrements;

  std::array<std::array<juce::Value, numPotsPerChannel>, numChannels>
      _valuePots;
};

}
