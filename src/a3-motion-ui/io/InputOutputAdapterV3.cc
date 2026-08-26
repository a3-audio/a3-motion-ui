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

#include <a3-motion-engine/UserConfig.hh>
#include <a3-motion-ui/io/ButtonLedColours.hh>

#include <algorithm>
#include <cstring>

#include <libserial/SerialPort.h>

namespace a3
{

// ── constexpr out-of-line definition (C++14 compatibility) ───────────────────
constexpr InputOutputAdapterV3::ButtonMapping InputOutputAdapterV3::buttonMap[44];
constexpr uint8_t InputOutputAdapterV3::hwIndexToLedId[44];

// ─────────────────────────────────────────────────────────────────────────────

InputOutputAdapterV3::InputOutputAdapterV3 () : InputOutputAdapter ()
{
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

  std::array<juce::String, 6> const candidates = {
    "/dev/ttyACM0", "/dev/ttyACM1", "/dev/ttyACM2",
    "/dev/ttyUSB0", "/dev/ttyUSB1", "/dev/ttyUSB2"
  };

  for (auto const &serialDevice : candidates)
    {
      try
        {
          _serialPort.Open (serialDevice.toStdString ());
          _serialPort.SetBaudRate (BaudRate::BAUD_115200);
          _serialPort.SetCharacterSize (CharacterSize::CHAR_SIZE_8);
          _serialPort.SetFlowControl (FlowControl::FLOW_CONTROL_NONE);
          _serialPort.SetParity (Parity::PARITY_NONE);
          _serialPort.SetStopBits (StopBits::STOP_BITS_1);

          // Give the device a moment to settle
          juce::Thread::sleep (100);
          _hardwareAvailable = true;
          juce::Logger::writeToLog (
              "InputOutputAdapterV3: serial port opened: " + serialDevice);
          return;
        }
      catch (const std::exception &e)
        {
          juce::Logger::writeToLog (
              "InputOutputAdapterV3: serial candidate failed: " + serialDevice
              + " (" + e.what () + ")");
        }
    }

  _hardwareAvailable = false;
  juce::Logger::writeToLog (
      "InputOutputAdapterV3: no usable serial port found "
      "(/dev/ttyACM0..2, /dev/ttyUSB0..2)");
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

bool
InputOutputAdapterV3::resolveFrameOffsets (const uint8_t *raw, bool withPots,
                                           int &buttonOffset,
                                           int &encoderOffset,
                                           int &potOffset)
{
  potOffset = withPots ? static_cast<int> (btnFrameSize + encFrameSize) : -1;

  // Layout A: BTN(12) + ENC(25) [+ POT(9)]
  if (raw[0] == cmdGetButtons && raw[btnFrameSize] == cmdGetEncoders)
    {
      buttonOffset = 0;
      encoderOffset = static_cast<int> (btnFrameSize);
    }
  // Layout B: ENC(25) + BTN(12) [+ POT(9)]
  else if (raw[0] == cmdGetEncoders && raw[encFrameSize] == cmdGetButtons)
    {
      encoderOffset = 0;
      buttonOffset = static_cast<int> (encFrameSize);
    }
  else
    {
      juce::Logger::writeToLog (
          "InputOutputAdapterV3: bad frame markers: b0="
          + juce::String (raw[0]) + " b12="
          + juce::String (raw[btnFrameSize]) + " b25="
          + juce::String (raw[encFrameSize]));
      return false;
    }

  if (withPots && raw[potOffset] != cmdGetPots)
    {
      juce::Logger::writeToLog (
          "InputOutputAdapterV3: bad pot frame marker: "
          + juce::String (raw[potOffset]));
      return false;
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

      int buttonOffset = 0;
      int encoderOffset = 0;
      int potOffset = -1;
      if (!resolveFrameOffsets (raw, true, buttonOffset, encoderOffset,
                                potOffset))
        {
          _cycle = 0;
          return;
        }

      parseButtons (raw, buttonOffset);
      parseEncoders (raw, encoderOffset);
      parsePots (raw, potOffset);
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

      int buttonOffset = 0;
      int encoderOffset = 0;
      int potOffset = -1;
      if (!resolveFrameOffsets (raw, false, buttonOffset, encoderOffset,
                                potOffset))
        {
          _cycle = 0;
          return;
        }

      parseButtons (raw, buttonOffset);
      parseEncoders (raw, encoderOffset);
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
        // Two physical buttons (50 at idx 3, 59 at idx 37), either of which
        // is the Menu button. They used to have to be pressed together — a
        // chord to reach the menu, which is a lot of ceremony for the one
        // key somebody presses to get out of somewhere.
        //
        // Tracked separately so that holding one and pressing the other
        // does not read as a release: Menu is down while either is down.
        int const slot = (idx == 3) ? 0 : 1;
        bool const wasAnyPressed
            = _menuButtonState[0] || _menuButtonState[1];
        _menuButtonState[slot] = pressed;
        bool const isAnyPressed = _menuButtonState[0] || _menuButtonState[1];

        if (isAnyPressed != wasAnyPressed)
          inputButtonValue (Button::Menu, isAnyPressed);
      }
      break;

    case ButtonRole::Record:
      inputButtonValue (Button::Record, pressed);
      break;

    case ButtonRole::Shift:
      inputButtonValue (Button::Shift, pressed);
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

      // Both encoders emit individual increment/decrement events per step;
      // encoder A (pot encoder) as encoderIndex 1, encoder B (motion
      // encoder) as encoderIndex 0. The UI layer decides what each does.
      index_t const encoderIndex = isPotEncoder ? 1 : 0;
      int           absSteps     = std::abs (delta);
      auto          event        = delta > 0 ? InputMessageEncoder::Event::Increment
                                             : InputMessageEncoder::Event::Decrement;
      for (int s = 0; s < absSteps; ++s)
        inputEncoderEvent (ch, encoderIndex, event);
    }
}

void
InputOutputAdapterV3::processEncSwitch (index_t ch, bool isPotEncoder,
                                        uint8_t sw)
{
  bool &pressActive = isPotEncoder ? _encAPressActive[ch] : _encBPressActive[ch];
  index_t const encoderIndex = isPotEncoder ? 1 : 0;

  if (sw == 3)
    {
      // CLICK: full press-release in one cycle
      inputEncoderEvent (ch, encoderIndex, InputMessageEncoder::Event::Press);
      inputEncoderEvent (ch, encoderIndex, InputMessageEncoder::Event::Release);
      pressActive = false;
    }
  else if (sw == 0 && !pressActive)
    {
      // Button just pressed (HOLD)
      pressActive = true;
      inputEncoderEvent (ch, encoderIndex, InputMessageEncoder::Event::Press);
    }
  else if (sw == 1 && pressActive)
    {
      // Released
      pressActive = false;
      inputEncoderEvent (ch, encoderIndex, InputMessageEncoder::Event::Release);
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

// ── Output (LED) ──────────────────────────────────────────────────────────────

void
InputOutputAdapterV3::writeSetLed (uint8_t ledId, juce::Colour colour)
{
  if (!_hardwareAvailable)
    return;

  const uint8_t frame[] = { cmdSetLed, ledId, colour.getRed (),
                            colour.getGreen (), colour.getBlue () };
  _serialPort.Write (
      std::string (reinterpret_cast<const char *> (frame), sizeof (frame)));
}

void
InputOutputAdapterV3::outputButtonLED (Button button, bool value)
{
  // Record/Tap/Menu/Shift are each wired to a mirrored pair of physical
  // buttons (left + right hand side); both share one logical Button and
  // light up together. ClockMode was a V2-only physical button with no LED
  // on V3 hardware (kept for backward compat), so there's nothing to send
  // for it.
  static constexpr int recordHwIndices[] = { 41, 43 };
  static constexpr int tapHwIndices[]    = { 2, 36 };
  static constexpr int menuHwIndices[]   = { 3, 37 };
  static constexpr int shiftHwIndices[]  = { 40, 42 };

  // Each function button lights in its own colour, so the panel says which key
  // does what without reading the legend. Unconfigured ones stay white, which
  // is what all of them used to be.
  auto const named = [&] () -> juce::String {
    switch (button)
      {
      case Button::Record: return "record";
      case Button::Tap: return "tap";
      case Button::Menu: return "menu";
      case Button::Shift: return "shift";
      case Button::ClockMode: break;
      }
    return {};
  }();

  auto const led = buttonLedColour (userConfig["buttonLeds"], named);
  // Off is unlit, not black-coloured — transparentBlack is how this codebase
  // says "no colour", and the wire carries the rgb, which is zero either way.
  auto const colour = value ? toColour (led) : juce::Colours::transparentBlack;

  switch (button)
    {
    case Button::Record:
      for (auto idx : recordHwIndices)
        writeSetLed (hwIndexToLedId[idx], colour);
      break;
    case Button::Tap:
      for (auto idx : tapHwIndices)
        writeSetLed (hwIndexToLedId[idx], colour);
      break;
    case Button::Menu:
      for (auto idx : menuHwIndices)
        writeSetLed (hwIndexToLedId[idx], colour);
      break;
    case Button::Shift:
      for (auto idx : shiftHwIndices)
        writeSetLed (hwIndexToLedId[idx], colour);
      break;
    case Button::ClockMode:
      break;
    }
}

void
InputOutputAdapterV3::outputPadLED (PadIndex padIndex, juce::Colour colour)
{
  for (int i = 0; i < numHwButtons; ++i)
    {
      auto const &m = buttonMap[i];
      if (m.role == ButtonRole::Pad && m.channel == padIndex.channel
          && m.pad == padIndex.pad)
        {
          writeSetLed (hwIndexToLedId[i], colour);
          return;
        }
    }
}

// ── Global pot accessor ───────────────────────────────────────────────────────

juce::Value &
InputOutputAdapterV3::getGlobalPot (index_t potIndex)
{
  jassert (potIndex < static_cast<index_t> (numGlobalPots));
  return _globalPotValues[potIndex];
}

}
