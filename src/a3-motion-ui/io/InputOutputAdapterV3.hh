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

#include <array>
#include <cstdint>

#include <libserial/SerialPort.h>

#include <JuceHeader.h>

#include <a3-motion-ui/io/ButtonLedColours.hh>
#include <a3-motion-ui/io/InputOutputAdapter.hh>

namespace a3
{

/**
 * InputOutputAdapterV3
 *
 * For standalone diagnostics against the same firmware protocol outside this
 * adapter, use the canonical `host.py` at the repo root (not a copy in here).
 *
 * Hardware: ESP32-S3-DevKitC-1 with
 *   44 buttons (2-bit state per button, packed)
 *    8 encoders with push-button (i16 delta + u8 switch state)
 *    4 potentiometers (u16 raw ADC, 0-4095)
 *
 * Physical layout (Label format RC = Row, Col):
 *   Col 0 & Col 9   – function buttons (6 per side, mirrored left/right-hand)
 *                     Row 0 = Shift, Row 1 = Record, Row 2 = Tap, Rows 3-4 = spare,
 *                     Row 5 = Menu toggle
 *   Cols 1-2 / 3-4 / 5-6 / 7-8  – channel pairs 0-3
 *                     Each channel: 2 cols × 4 rows (Rows 2-5) = 8 sample pads
 *
 * Encoder mapping (firmware index 0-7):
 *   Even  (0,2,4,6) → Pot-Encoder  (encoder A) per channel 0-3:
 *                     rotate adjusts selected pot value [0,1],
 *                     push toggles between pot 0 and pot 1
 *   Odd   (1,3,5,7) → Motion-Encoder (encoder B) per channel 0-3:
 *                     mapped to base-class encoderIndex 0
 *
 * Hardware pots (4 total, read every potDivider cycles):
 *   Exposed via getGlobalPot(0-3), independent of the per-channel pot system.
 */
class InputOutputAdapterV3 : public InputOutputAdapter
{
public:
  InputOutputAdapterV3 ();
  ~InputOutputAdapterV3 ();

  void processInput () override;
  void outputButtonLED (Button button, bool value) override;
  void outputPadLED (PadIndex padIndex, juce::Colour colour) override;

  // The four physical potentiometers.
  juce::Value &getGlobalPot (index_t potIndex) override;
  index_t getNumGlobalPots () const override { return numGlobalPots; }

private:
  // ── Serial ────────────────────────────────────────────────────────────────
  void serialInit ();
  bool readExact (uint8_t *buf, std::size_t n);
  bool resolveFrameOffsets (const uint8_t *raw, bool withPots,
                            int &buttonOffset, int &encoderOffset,
                            int &potOffset);

  LibSerial::SerialPort _serialPort;
  bool _hardwareAvailable = false;

  /** Light every function button in the resting colour, and keep it in step
   *  with the config. A key that does something says so without being
   *  touched; one with no function stays dark, which is how the panel says
   *  there is nothing there. */
  void refreshIdleButtonLeds ();
  /** What was last written, so the serial link only carries a change. */
  LedColour _idleLedWritten{ -1, -1, -1 };

  // ── Protocol constants ────────────────────────────────────────────────────
  static constexpr uint8_t cmdPing       = 0x01;
  static constexpr uint8_t cmdGetPots    = 0x02;
  static constexpr uint8_t cmdGetEncoders = 0x03;
  static constexpr uint8_t cmdGetButtons = 0x04;
  static constexpr uint8_t cmdSetLed      = 0x05;
  static constexpr uint8_t cmdSetAllLeds  = 0x06;

  // Response sizes (including the leading command echo byte)
  static constexpr std::size_t btnFrameSize = 12;  // 1 + 11 packed bytes
  static constexpr std::size_t encFrameSize = 25;  // 1 + 8×3 bytes
  static constexpr std::size_t potFrameSize =  9;  // 1 + 4×u16

  static constexpr int potDivider      = 5;   // read pots every Nth cycle
  static constexpr unsigned int serialTimeoutMs = 250;

  // ── Button mapping ────────────────────────────────────────────────────────
  enum class ButtonRole : uint8_t
  {
    Spare,
    Function,
    Pad,
  };

  struct ButtonMapping
  {
    ButtonRole role;
    uint8_t    channel;
    uint8_t    pad;
    /** For Function: which row of the panel's end columns this key is, 0 at
     *  the top. What that row *does* comes from functionKeyOrder — the panel
     *  and the screen read the same list. */
    uint8_t    functionRow;
  };

  // Indexed by firmware button index [0..43].
  // Labels are "RC" (Row, Col); see Python script BUTTON_LABELS[] for order.
  //
  // The two end columns, col0 and col9, are the function keys: six rows each,
  // mirrored so either hand reaches them. Which row does what is not written
  // here — it comes from functionKeyOrder (io/FunctionKeys.hh), the same list
  // the global strip is laid out from.
  static constexpr ButtonMapping buttonMap[44] = {
    { ButtonRole::Function, 0, 0, 4 },  //  0: "40" row4 col0 (left)
    { ButtonRole::Function, 0, 0, 3 },  //  1: "30" row3 col0 (left)
    { ButtonRole::Function, 0, 0, 2 },  //  2: "20" row2 col0 (left)
    { ButtonRole::Function, 0, 0, 5 },  //  3: "50" row5 col0 (left)
    { ButtonRole::Pad, 0, 0, 0 },  //  4: "21" ch0 left-col  row2 → pad0
    { ButtonRole::Pad, 0, 3, 0 },  //  5: "51" ch0 left-col  row5 → pad3
    { ButtonRole::Pad, 0, 1, 0 },  //  6: "31" ch0 left-col  row3 → pad1
    { ButtonRole::Pad, 0, 2, 0 },  //  7: "41" ch0 left-col  row4 → pad2
    { ButtonRole::Pad, 0, 6, 0 },  //  8: "42" ch0 right-col row4 → pad6
    { ButtonRole::Pad, 0, 5, 0 },  //  9: "32" ch0 right-col row3 → pad5
    { ButtonRole::Pad, 0, 4, 0 },  // 10: "22" ch0 right-col row2 → pad4
    { ButtonRole::Pad, 0, 7, 0 },  // 11: "52" ch0 right-col row5 → pad7
    { ButtonRole::Pad, 1, 0, 0 },  // 12: "23" ch1 left-col  row2 → pad0
    { ButtonRole::Pad, 1, 3, 0 },  // 13: "53" ch1 left-col  row5 → pad3
    { ButtonRole::Pad, 1, 1, 0 },  // 14: "33" ch1 left-col  row3 → pad1
    { ButtonRole::Pad, 1, 2, 0 },  // 15: "43" ch1 left-col  row4 → pad2
    { ButtonRole::Pad, 1, 6, 0 },  // 16: "44" ch1 right-col row4 → pad6
    { ButtonRole::Pad, 1, 5, 0 },  // 17: "34" ch1 right-col row3 → pad5
    { ButtonRole::Pad, 1, 4, 0 },  // 18: "24" ch1 right-col row2 → pad4
    { ButtonRole::Pad, 1, 7, 0 },  // 19: "54" ch1 right-col row5 → pad7
    { ButtonRole::Pad, 2, 0, 0 },  // 20: "25" ch2 left-col  row2 → pad0
    { ButtonRole::Pad, 2, 3, 0 },  // 21: "55" ch2 left-col  row5 → pad3
    { ButtonRole::Pad, 2, 1, 0 },  // 22: "35" ch2 left-col  row3 → pad1
    { ButtonRole::Pad, 2, 2, 0 },  // 23: "45" ch2 left-col  row4 → pad2
    { ButtonRole::Pad, 2, 6, 0 },  // 24: "46" ch2 right-col row4 → pad6
    { ButtonRole::Pad, 2, 5, 0 },  // 25: "36" ch2 right-col row3 → pad5
    { ButtonRole::Pad, 2, 4, 0 },  // 26: "26" ch2 right-col row2 → pad4
    { ButtonRole::Pad, 2, 7, 0 },  // 27: "56" ch2 right-col row5 → pad7
    { ButtonRole::Pad, 3, 0, 0 },  // 28: "27" ch3 left-col  row2 → pad0
    { ButtonRole::Pad, 3, 3, 0 },  // 29: "57" ch3 left-col  row5 → pad3
    { ButtonRole::Pad, 3, 1, 0 },  // 30: "37" ch3 left-col  row3 → pad1
    { ButtonRole::Pad, 3, 2, 0 },  // 31: "47" ch3 left-col  row4 → pad2
    { ButtonRole::Pad, 3, 6, 0 },  // 32: "48" ch3 right-col row4 → pad6
    { ButtonRole::Pad, 3, 5, 0 },  // 33: "38" ch3 right-col row3 → pad5
    { ButtonRole::Pad, 3, 4, 0 },  // 34: "28" ch3 right-col row2 → pad4
    { ButtonRole::Pad, 3, 7, 0 },  // 35: "58" ch3 right-col row5 → pad7
    { ButtonRole::Function, 0, 0, 2 },  // 36: "29" row2 col9 (right)
    { ButtonRole::Function, 0, 0, 5 },  // 37: "59" row5 col9 (right)
    { ButtonRole::Function, 0, 0, 3 },  // 38: "39" row3 col9 (right)
    { ButtonRole::Function, 0, 0, 4 },  // 39: "49" row4 col9 (right)
    { ButtonRole::Function, 0, 0, 0 },  // 40: "00" row0 col0 (left)
    { ButtonRole::Function, 0, 0, 1 },  // 41: "10" row1 col0 (left)
    { ButtonRole::Function, 0, 0, 0 },  // 42: "09" row0 col9 (right)
    { ButtonRole::Function, 0, 0, 1 },  // 43: "19" row1 col9 (right)
  };

  static constexpr int numHwButtons = 44;

  // Firmware button index [0..43] → NeoPixel chain index (SET_LED's led_id).
  // The LED daisy chain is wired in a different physical order than the
  // button-read order above; this table is the permutation between
  // multiplexer_map.h's MATRIX_BUTTONS[] (buttonMap order) and LED_MAP[]/
  // config.h's LED_PHYSICAL_ORDER[] (this array's order), matched by the "RC"
  // labels in the buttonMap comments above.
  static constexpr uint8_t hwIndexToLedId[44] = {
    39, 40, 41, 38, 34, 37, 35, 36, 31, 32, 33, 30, 26, 29, 27, 28,
    23, 24, 25, 22, 18, 21, 19, 20, 15, 16, 17, 14, 10, 13, 11, 12,
     7,  8,  9,  6,  2,  5,  3,  4, 43, 42,  0,  1
  };

  void writeSetLed (uint8_t ledId, juce::Colour colour);

  // ── Button state tracking ─────────────────────────────────────────────────
  std::array<bool, numHwButtons> _buttonPressed{};
  /** Each function key twice — the left column and the right — because they
   *  are one key with two places to press it. A key is down while either side
   *  is down, so holding one and pressing the other does not read as a
   *  release. Menu alone used to be tracked this way; all six are now, which
   *  is what the mirrored columns are for. */
  std::array<std::array<bool, 2>, numFunctionKeys> _functionKeyState{};

  /** Whether a firmware index sits in the panel's right-hand end column. */
  static bool isRightHandColumn (int hwIndex);

  void parseButtons (const uint8_t *raw, int offset);
  void dispatchButtonEvent (int idx, bool pressed);

  // ── Encoder state ─────────────────────────────────────────────────────────
  // Encoder A (even firmware index) = pot encoder per channel, encoderIndex 1
  // Encoder B (odd  firmware index) = motion encoder per channel, encoderIndex 0
  // Both emit generic increment/decrement + press/release events; the
  // meaning of each is decided by the UI layer (A3MotionUIComponent).

  // Press state tracking for encoder switches
  std::array<bool, numChannels> _encAPressActive{};  // pot encoder push
  std::array<bool, numChannels> _encBPressActive{};  // motion encoder push

  void parseEncoders (const uint8_t *raw, int offset);
  void processEncSwitch (index_t ch, bool isPotEncoder, uint8_t sw);

  // ── Hardware pot state ────────────────────────────────────────────────────
  static constexpr int numGlobalPots = 4;
  static constexpr int potDeadband   = 8;  // raw ADC counts

  std::array<int, numGlobalPots> _prevPotRaw{};
  std::array<juce::Value, numGlobalPots> _globalPotValues;

  void parsePots (const uint8_t *raw, int offset);

  // ── Cycle counter ─────────────────────────────────────────────────────────
  int _cycle = 0;
};

}
