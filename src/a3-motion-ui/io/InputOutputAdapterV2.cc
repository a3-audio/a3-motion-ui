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

namespace a3
{

InputOutputAdapterV2::InputOutputAdapterV2 () : InputOutputAdapter ()
{
  serialInit ();
}

InputOutputAdapterV2::~InputOutputAdapterV2 ()
{
  _serialPort.Close ();
}

void
InputOutputAdapterV2::serialInit ()
{
  using namespace LibSerial;

  auto constexpr serialDevice = "/dev/ttyACM0";
  _serialPort.Open (serialDevice);
  _serialPort.SetBaudRate (BaudRate::BAUD_115200);
  _serialPort.SetCharacterSize (CharacterSize::CHAR_SIZE_8);
  _serialPort.SetFlowControl (FlowControl::FLOW_CONTROL_NONE);
  _serialPort.SetParity (Parity::PARITY_NONE);
  _serialPort.SetStopBits (StopBits::STOP_BITS_1);

  juce::Logger::writeToLog ("initialized libserial");
}

void
InputOutputAdapterV2::processInput ()
{
  char c;
  auto const bytesReady = _serialPort.GetNumberOfBytesAvailable ();
  for (auto readCount = 0; readCount < bytesReady; ++readCount)
    {
      _serialPort.ReadByte (c);
      if (c == '\n')
        {
          auto line = juce::String (_serialBuffer.data (), _nextWriteOffset);
          serialParseLine (line);
          _nextWriteOffset = 0;
          continue;
        }
      _serialBuffer[_nextWriteOffset++] = c;
    }
}

void
InputOutputAdapterV2::serialParseLine (juce::String line)
{
  // juce::Logger::writeToLog (juce::String ("**** parsing line: ") + line);

  auto const prefixButton = juce::String ("B");
  auto const prefixEncoderPress = juce::String ("EB");
  auto const prefixEncoderIncrement = juce::String ("Enc");
  auto const prefixPot = juce::String ("P");
  auto const delimiter = juce::String (":");

  auto const value = line.fromFirstOccurrenceOf (delimiter, false, false)
                         .upToFirstOccurrenceOf (delimiter, false, false)
                         .getIntValue ();

  if (line.startsWith (prefixButton))
    {
      auto const indexSigned
          = line.substring (prefixButton.length ())
                .upToFirstOccurrenceOf (delimiter, false, false)
                .getIntValue ();
      jassert (indexSigned >= 0);
      auto const index = static_cast<index_t> (indexSigned);

      if (index < 16)
        {
          auto const channel = index % numChannels;
          auto const pad = index / numChannels;
          inputPadValue ({ channel, pad }, value);
        }
      else
        {
          switch (index)
            {
            case 16:
              inputButtonValue (Button::Shift, value);
              break;
            case 17:
              inputButtonValue (Button::Record, value);
              break;
            case 18:
              inputButtonValue (Button::Tap, value);
              if (value)
                {
                  auto const timeMicros
                      = line.fromLastOccurrenceOf (delimiter, false, false)
                            .getLargeIntValue ();
                  inputTapTime (timeMicros);
                }
              break;
            }
        }
    }
  else if (line.startsWith (prefixEncoderPress))
    {
      auto const indexSigned
          = line.substring (prefixEncoderPress.length ())
                .upToFirstOccurrenceOf (delimiter, false, false)
                .getIntValue ();
      jassert (indexSigned >= 0);
      auto const index = static_cast<index_t> (indexSigned);

      if (value == 1)
        inputEncoderEvent (index, InputMessageEncoder::Event::Press);
      else if (value == 0)
        inputEncoderEvent (index, InputMessageEncoder::Event::Release);
    }
  else if (line.startsWith (prefixEncoderIncrement))
    {
      auto const indexSigned
          = line.substring (prefixEncoderIncrement.length ())
                .upToFirstOccurrenceOf (delimiter, false, false)
                .getIntValue ();
      jassert (indexSigned >= 0);
      auto const index = static_cast<index_t> (indexSigned);

      if (value == 1)
        inputEncoderEvent (index, InputMessageEncoder::Event::Increment);
      else if (value == -1)
        inputEncoderEvent (index, InputMessageEncoder::Event::Decrement);
    }
  else if (line.startsWith (prefixPot))
    {
      auto const indexSigned
          = line.substring (prefixPot.length ())
                .upToFirstOccurrenceOf (delimiter, false, false)
                .getIntValue ();
      jassert (indexSigned >= 0);
      auto const index = static_cast<index_t> (indexSigned);

      auto const channel = index % numChannels;
      auto const pad = index / numChannels;
      auto constexpr potMaxValue = 1023.f;
      inputPotValue (channel, pad, value / potMaxValue);
    }
}

void
InputOutputAdapterV2::outputButtonLED (Button button, bool value)
{
  juce::String line = "BL,";
  line += juce::String (static_cast<int> (button));
  line += ",";
  line += juce::String (static_cast<int> (value));
  line += "\n";

  // juce::Logger::writeToLog ("++++ sending: " + line);
  _serialPort.Write (line.toStdString ());
}

void
InputOutputAdapterV2::outputPadLED (PadIndex padIndex, juce::Colour colour)
{
  juce::String line = "L,";
  line += juce::String (
      static_cast<int> (padIndex.channel + numChannels * padIndex.pad));
  line += ",";
  line += juce::String (colour.getRed ());
  line += ",";
  line += juce::String (colour.getGreen ());
  line += ",";
  line += juce::String (colour.getBlue ());
  line += "\n";

  // juce::Logger::writeToLog ("++++ sending: " + line);
  _serialPort.Write (line.toStdString ());
}

}
