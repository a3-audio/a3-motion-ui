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

#include "InputOutputAdapterV3.hh"

#include <algorithm>
#include <cstring>

#include <libserial/SerialPort.h>

namespace a3
{

// ── constexpr out-of-line definition (C++14 compatibility) ───────────────────
constexpr InputOutputAdapterV3::ButtonMapping InputOutputAdapterV3::buttonMap[44];

// ─────────────────────────────────────────────────────────────────────────────

InputOutputAdapterV3::InputOutputAdapterV3 () : InputOutputAdapter ()
{
  for (auto &vals : _encoderPotValues)
    vals.fill (0.5f);
  _prevPotRaw.fill (0);

  serialInit ();
}

InputOutputAdapterV3::~InputOutputAdapterV3 ()
{
  if (_hardwareAvailable)
    _serialPort.Close ();
}

void
InputOutputAdapterV3::serialInit ()
{
  using namespace LibSerial;

  auto constexpr serialDevice = "/dev/ttyACM0";
  try
    {
      _serialPort.Open (serialDevice);
      _serialPort.SetBaudRate (BaudRate::BAUD_115200);
      _serialPort.SetCharacterSize (CharacterSize::CHAR_SIZE_8);
      _serialPort.SetFlowControl (FlowControl::FLOW_CONTROL_NONE);
      _serialPort.SetParity (Parity::PARITY_NONE);
      _serialPort.SetStopBits (StopBits::STOP_BITS_1);

      // Give the device a moment to settle
      juce::Thread::sleep (100);
      _hardwareAvailable = true;
      juce::Logger::writeToLog ("InputOutputAdapterV3: serial port opened");
    }
  catch (const std::exception &e)
    {
      juce::Logger::writeToLog (
          juce::String ("InputOutputAdapterV3: cannot open serial port: ")
          + e.what ());
      _hardwareAvailable = false;
    }
}

bool
InputOutputAdapterV3::readExact (uint8_t *buf, std::size_t n)
{
  for (std::size_t i = 0; i < n; ++i)
    {
      try
        {
          char c;
          _serialPort.ReadByte (c, serialTimeoutMs);
          buf[i] = static_cast<uint8_t> (c);
        }
      catch (const LibSerial::ReadTimeout &)
        {
          juce::Logger::writeToLog (
              "InputOutputAdapterV3: read timeout after " + juce::String (i)
              + " of " + juce::String (n) + " bytes");
          return false;
        }
    }
  return true;
}

// ── Main poll loop ────────────────────────────────────────────────────────────

void
InputOutputAdapterV3::processInput ()
{
  if (!_hardwareAvailable)
    return;

  bool includePots = (_cycle % potDivider == 0);

  if (includePots)
    {
      const char cmd[] = { static_cast<char> (cmdGetButtons),
                           static_cast<char> (cmdGetEncoders),
                           static_cast<char> (cmdGetPots) };
      _serialPort.Write (std::string (cmd, 3));

      uint8_t raw[btnFrameSize + encFrameSize + potFrameSize];
      if (!readExact (raw, sizeof (raw)))
        {
          _cycle = 0;
          return;
        }
      parseButtons (raw, 0);
      parseEncoders (raw, static_cast<int> (btnFrameSize));
      parsePots (raw, static_cast<int> (btnFrameSize + encFrameSize));
    }
  else
    {
      const char cmd[] = { static_cast<char> (cmdGetButtons),
                           static_cast<char> (cmdGetEncoders) };
      _serialPort.Write (std::string (cmd, 2));

      uint8_t raw[btnFrameSize + encFrameSize];
      if (!readExact (raw, sizeof (raw)))
        {
          _cycle = 0;
          return;
        }
      parseButtons (raw, 0);
      parseEncoders (raw, static_cast<int> (btnFrameSize));
    }

  ++_cycle;
}

// ── Button parsing ────────────────────────────────────────────────────────────

void
InputOutputAdapterV3::parseButtons (const uint8_t *raw, int offset)
{
  if (raw[offset] != cmdGetButtons)
    return;

  const uint8_t *packed = raw + offset + 1;

  for (int i = 0; i < numHwButtons; ++i)
    {
      // Each button occupies 2 bits:  bits [(i%4)*2 + 1 : (i%4)*2]
      // within byte [i/4] of the packed data.
      uint8_t state = (packed[i >> 2] >> ((i & 3) << 1)) & 0x03;

      // state: 0=HOLD (pressed), 1=idle, 2=BOUNCE (ignore), 3=CLICK
      bool prevPressed = _buttonPressed[i];

      if (state == 3)
        {
          // Full press-release cycle in one poll period
          if (!prevPressed)
            {
              dispatchButtonEvent (i, true);
              dispatchButtonEvent (i, false);
            }
          else
            {
              dispatchButtonEvent (i, false);
              _buttonPressed[i] = false;
            }
        }
      else if (state == 0 && !prevPressed)
        {
          // Button just pressed (HOLD, first time seen)
          dispatchButtonEvent (i, true);
          _buttonPressed[i] = true;
        }
      else if (state == 1 && prevPressed)
        {
          // Button released
          dispatchButtonEvent (i, false);
          _buttonPressed[i] = false;
        }
      // state == 2 (BOUNCE): no action
    }
}

void
InputOutputAdapterV3::dispatchButtonEvent (int idx, bool pressed)
{
  const auto &m = buttonMap[idx];

  switch (m.role)
    {
    case ButtonRole::MenuToggle:
      {
        // Track button 00 (idx 40) and button 09 (idx 42) independently.
        // Fire Button::Menu when both become simultaneously pressed.
        int slot = (idx == 3) ? 0 : 1;
        bool wasBothPressed = _menuButtonState[0] && _menuButtonState[1];
        _menuButtonState[slot] = pressed;
        bool isBothPressed = _menuButtonState[0] && _menuButtonState[1];
        if (isBothPressed && !wasBothPressed)
          inputButtonValue (Button::Menu, true);
        else if (!isBothPressed && wasBothPressed)
          inputButtonValue (Button::Menu, false);
      }
      break;

    case ButtonRole::Record:
      inputButtonValue (Button::Record, pressed);
      break;

    case ButtonRole::Tap:
      inputButtonValue (Button::Tap, pressed);
      if (pressed)
        {
          auto ticks = juce::Time::getHighResolutionTicks ();
          auto freq  = juce::Time::getHighResolutionTicksPerSecond ();
          auto timeMicros = static_cast<juce::int64> (
              static_cast<double> (ticks) / static_cast<double> (freq)
              * 1'000'000.0);
          inputTapTime (timeMicros);
        }
      break;

    case ButtonRole::Pad:
      inputPadValue ({ static_cast<index_t> (m.channel),
                       static_cast<index_t> (m.pad) },
                     pressed);
      break;

    case ButtonRole::Spare:
      break;
    }
}

// ── Encoder parsing ───────────────────────────────────────────────────────────

void
InputOutputAdapterV3::parseEncoders (const uint8_t *raw, int offset)
{
  if (raw[offset] != cmdGetEncoders)
    return;

  const uint8_t *data = raw + offset + 1;

  for (int i = 0; i < 8; ++i)
    {
      const uint8_t *p = data + i * 3;

      // i16 little-endian delta
      int16_t delta = static_cast<int16_t> (static_cast<uint16_t> (p[0])
                                            | (static_cast<uint16_t> (p[1]) << 8));
      uint8_t sw = p[2];

      index_t ch          = static_cast<index_t> (i % 4);
      bool    isPotEncoder = (i < 4);

      // Handle switch state (same 2-bit encoding as buttons)
      processEncSwitch (ch, isPotEncoder, sw);

      if (delta == 0)
        continue;

      if (isPotEncoder)
        {
          // Encoder A: accumulate pot value for the selected pot
          index_t selPot = _selectedPot[ch];
          float   step   = static_cast<float> (delta) * potEncoderSensitivity;
          float   newVal = std::clamp (_encoderPotValues[ch][selPot] + step,
                                       0.0f, 1.0f);
          if (newVal != _encoderPotValues[ch][selPot])
            {
              _encoderPotValues[ch][selPot] = newVal;
              inputPotValue (ch, selPot, newVal);
            }
        }
      else
        {
          // Encoder B: motion encoder – emit individual increment/decrement events
          int    absSteps = std::abs (delta);
          auto   event    = delta > 0 ? InputMessageEncoder::Event::Increment
                                      : InputMessageEncoder::Event::Decrement;
          for (int s = 0; s < absSteps; ++s)
            inputEncoderEvent (ch, 0, event);
        }
    }
}

void
InputOutputAdapterV3::processEncSwitch (index_t ch, bool isPotEncoder,
                                        uint8_t sw)
{
  bool &pressActive = isPotEncoder ? _encAPressActive[ch] : _encBPressActive[ch];

  if (sw == 3)
    {
      // CLICK: full press-release in one cycle
      if (isPotEncoder)
        {
          _selectedPot[ch] = (_selectedPot[ch] + 1) % numPotsPerChannel;
          inputEncoderEvent (ch, 1, InputMessageEncoder::Event::Press);
          inputEncoderEvent (ch, 1, InputMessageEncoder::Event::Release);
        }
      else
        {
          inputEncoderEvent (ch, 0, InputMessageEncoder::Event::Press);
          inputEncoderEvent (ch, 0, InputMessageEncoder::Event::Release);
        }
      pressActive = false;
    }
  else if (sw == 0 && !pressActive)
    {
      // Button just pressed (HOLD)
      pressActive = true;
      if (isPotEncoder)
        {
          _selectedPot[ch] = (_selectedPot[ch] + 1) % numPotsPerChannel;
          inputEncoderEvent (ch, 1, InputMessageEncoder::Event::Press);
        }
      else
        {
          inputEncoderEvent (ch, 0, InputMessageEncoder::Event::Press);
        }
    }
  else if (sw == 1 && pressActive)
    {
      // Released
      pressActive = false;
      if (isPotEncoder)
        inputEncoderEvent (ch, 1, InputMessageEncoder::Event::Release);
      else
        inputEncoderEvent (ch, 0, InputMessageEncoder::Event::Release);
    }
  // sw == 2 (BOUNCE): no action
}

// ── Hardware pot parsing ──────────────────────────────────────────────────────

void
InputOutputAdapterV3::parsePots (const uint8_t *raw, int offset)
{
  if (raw[offset] != cmdGetPots)
    return;

  const uint8_t *data = raw + offset + 1;

  for (int i = 0; i < numGlobalPots; ++i)
    {
      uint16_t rawVal = static_cast<uint16_t> (data[i * 2])
                        | (static_cast<uint16_t> (data[i * 2 + 1]) << 8);

      if (std::abs (static_cast<int> (rawVal) - _prevPotRaw[i]) > potDeadband)
        {
          _prevPotRaw[i]    = static_cast<int> (rawVal);
          _globalPotValues[i] = rawVal / 4095.0f;
        }
    }
}

// ── Output (LED) – stubs for V3 (protocol TBD) ───────────────────────────────

void
InputOutputAdapterV3::outputButtonLED (Button, bool)
{
  // TODO: implement V3 LED output protocol
}

void
InputOutputAdapterV3::outputPadLED (PadIndex, juce::Colour)
{
  // TODO: implement V3 LED output protocol
}

// ── Global pot accessor ───────────────────────────────────────────────────────

juce::Value &
InputOutputAdapterV3::getGlobalPot (index_t potIndex)
{
  jassert (potIndex < static_cast<index_t> (numGlobalPots));
  return _globalPotValues[potIndex];
}

}
