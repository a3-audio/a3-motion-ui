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

  refreshIdleButtonLeds ();

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

namespace
{
// Record/Tap/Menu/Shift are each wired to a mirrored pair of physical buttons
// (left + right hand side); both share one logical Button and light together.
// ClockMode was a V2-only physical button with no LED on V3 hardware, so it
// has no entry — and therefore stays dark, which is what a key with no
// function should do.
/** The two firmware indices of each function-key row, left column then
 *  right, top row first. Read off the panel's RC labels: col0 is
 *  "00","10","20","30","40","50" and col9 is "09","19","29","39","49","59".
 *  What each row *does* is not here — it is functionKeyOrder. */
constexpr int functionRowHwIndices[numFunctionKeys][2] = {
  { 40, 42 }, // row 0
  { 41, 43 }, // row 1
  {  2, 36 }, // row 2
  {  1, 38 }, // row 3
  {  0, 39 }, // row 4
  {  3, 37 }, // row 5
};
}

void
InputOutputAdapterV3::refreshIdleButtonLeds ()
{
  // Written when it changes, not every poll: the serial link is shared with
  // the input frames, and repainting four LEDs at poll rate would crowd it.
  auto const idle = buttonLedIdleColour (userConfig["buttonLeds"]);
  if (idle == _idleLedWritten)
    return;

  _idleLedWritten = idle;

  auto const colour = toColour (idle);
  for (auto const &pair : functionRowHwIndices)
    for (auto const idx : pair)
      writeSetLed (hwIndexToLedId[idx], colour);
}

bool
InputOutputAdapterV3::isRightHandColumn (int hwIndex)
{
  // col9's function keys, by firmware index: "09", "19", "29", "39", "49",
  // "59". Everything else in the end columns is col0.
  switch (hwIndex)
    {
    case 42:
    case 43:
    case 36:
    case 38:
    case 39:
    case 37:
      return true;
    default:
      return false;
    }
}

void
InputOutputAdapterV3::dispatchButtonEvent (int idx, bool pressed)
{
  const auto &m = buttonMap[idx];

  switch (m.role)
    {
    case ButtonRole::Function:
      {
        // Which row this is comes from the panel; what the row *does* comes
        // from functionKeyOrder — the same list the global strip is laid out
        // from, so the hand learns one arrangement and not two.
        auto const row = static_cast<std::size_t> (m.functionRow);
        if (row >= functionKeyOrder.size ())
          break;

        auto const key = functionKeyOrder[row];

        // The two end columns are two places to press one key. Tracked apart
        // so that holding one side and pressing the other is not a release —
        // which is the whole point of their being mirrored.
        auto const side = isRightHandColumn (idx) ? 1u : 0u;
        auto &state = _functionKeyState[row];

        bool const wasDown = state[0] || state[1];
        state[side] = pressed;
        bool const isDown = state[0] || state[1];

        if (isDown == wasDown)
          break;

        inputButtonValue (key, isDown);

        if (key == FunctionKey::Tap && isDown)
          {
            auto ticks = juce::Time::getHighResolutionTicks ();
            auto freq = juce::Time::getHighResolutionTicksPerSecond ();
            auto timeMicros = static_cast<juce::int64> (
                static_cast<double> (ticks) / static_cast<double> (freq)
                * 1'000'000.0);
            inputTapTime (timeMicros);
          }
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
InputOutputAdapterV3::outputButtonLED (Button button, juce::Colour colour)
{
  // What a key looks like is decided once, in theme/FunctionKeyColours.hh,
  // and arrives here already decided — this used to look the colour up by the
  // key's *name* out of the user config, which meant the panel and the screen
  // could disagree about what a key was doing and nothing would say so.
  //
  // A transparent colour is a key with nothing to report: it takes the resting
  // light, which is not darkness — a key that has a function should say so
  // while nobody is touching it.
  auto const lit = ledColour (
      colour.isTransparent ()
          ? toColour (buttonLedIdleColour (userConfig["buttonLeds"]))
          : colour);

  // Both sides of the key light: they are one key with two places to press
  // it, and a lit left with a dark right would say they were two.
  auto const row = functionKeyPosition (button);
  if (row < 0)
    return;

  for (auto const idx : functionRowHwIndices[row])
    writeSetLed (hwIndexToLedId[idx], lit);
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
