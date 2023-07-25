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

#include <vector>

#include <a3-motion-engine/MotionEngine.hh>

#include <a3-motion-ui/components/ChannelFooter.hh>
#include <a3-motion-ui/components/ChannelHeader.hh>
#include <a3-motion-ui/components/ChannelViewState.hh>
#include <a3-motion-ui/components/LookAndFeel.hh>

namespace a3
{

class MotionComponent;
class InputOutputAdapter;

class MotionController : public juce::Component, public juce::MessageListener

{
public:
  struct InputMessage : public juce::Message
  {
  public:
    enum class Type
    {
      Button,
      Encoder,
      Poti,
    };

    Type type;
  };

  struct InputMessageButton : public InputMessage
  {
    enum class Event
    {
      Press,
      Release,
    };

    enum class ButtonId
    {
      Pad,
      Record,
      Tap,
      Shift,
    };

    Event event;
    ButtonId id;
    std::array<int, 2> padIndex;

    Type type = Type::Button;
  };

  struct InputMessageEncoder : public InputMessage
  {
    enum class Event
    {
      Press,
      Release,
      Increment,
      Decrement,
    };

    Event event;
    int index;

    Type type = Type::Encoder;
  };

  struct InputMessagePoti : public InputMessage
  {
    int index;
    float valueNormalized;

    Type type = Type::Poti;
  };

  MotionController (unsigned int const numChannels);
  ~MotionController ();

  void paint (juce::Graphics &g) override;
  void resized () override;

  void handleMessage (juce::Message const &) override;

  float getMinimumWidth () const;
  float getMinimumHeight () const;

private:
  void createChannelsUI ();

  void handleButton (InputMessageButton const &message);
  void handleEncoder (InputMessageEncoder const &message);
  void handlePoti (InputMessagePoti const &message);

  MotionEngine _engine;

  LookAndFeel_A3 _lookAndFeel;
  std::vector<std::unique_ptr<ChannelViewState> > _viewStates;
  std::vector<std::unique_ptr<ChannelHeader> > _headers;
  std::vector<std::unique_ptr<ChannelFooter> > _footers;
  std::unique_ptr<MotionComponent> _motionComponent;

  std::unique_ptr<InputOutputAdapter> _ioAdapter;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MotionController)
};

}
