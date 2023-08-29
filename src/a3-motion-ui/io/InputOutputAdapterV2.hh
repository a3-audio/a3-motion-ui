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

#include <libserial/SerialPort.h>

#include <a3-motion-ui/io/InputOutputAdapter.hh>

namespace a3
{

class InputOutputAdapterV2 : public InputOutputAdapter
{
public:
  InputOutputAdapterV2 ();
  ~InputOutputAdapterV2 ();

  void processInput () override;
  void outputButtonLED (Button button, bool value) override;
  void outputPadLED (PadIndex padIndex, juce::Colour colour) override;

private:
  void serialInit ();
  void serialParseLine (juce::String line);

  LibSerial::SerialPort _serialPort;
  static int constexpr serialBufferSize = 64;
  std::array<char, serialBufferSize> _serialBuffer;
  size_t _nextWriteOffset = 0;
};

}
