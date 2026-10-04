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

#include <optional>
#include <vector>

namespace a3
{

/** Which serial ports serialInit() asks for the panel, and in which order.
 *
 *  It used to ask /dev/ttyACM0..2 and ttyUSB0..2 and nothing else. The tty
 *  number is whatever the kernel handed out at enumeration, so a panel that
 *  came up after three other USB-serial devices sat on ttyACM3 and was never
 *  looked at. The panel's USB ID does not move between machines: the CH343
 *  bridge on the board, 1a86:55d3 -- the same ID PlatformIO (the board file's
 *  build.hwids), a3-motion's host.py and the a3-system installer look for.
 *
 *  The ID decides who is asked first, not who is asked at all. Every
 *  ttyACM* and ttyUSB* is a candidate, so a different bridge on a future board
 *  still gets through, and the PING handshake stays the final word: opening
 *  is not finding.
 */

/** A USB vendor/product pair as sysfs writes it: four hex digits each. */
struct UsbId
{
  juce::String vendor;
  juce::String product;
};

/** One entry of /sys/class/tty, with the USB ID above it if there is one. */
struct SerialTty
{
  juce::String name;
  std::optional<UsbId> usbId;
};

/** The panel's bridge, from a3-motion's
 *  firmware/boards/esp32-s3-devkitc-1-n16r8.json. */
UsbId panelUsbId ();

/** The device paths to try, in order: every ttyACM* and ttyUSB* whose ID is
 *  `preferred` first, then the rest, each group in natural name order (so
 *  ttyACM10 follows ttyACM2). Any other tty is not a serial port to ask. */
std::vector<juce::String>
orderSerialCandidates (std::vector<SerialTty> const &ttys,
                       UsbId const &preferred);

/** The USB ID of `ttyName`, read the way the kernel lays it out:
 *  <classTty>/<tty>/device leads to the USB interface (ACM) or one level
 *  below it (usb-serial), and the USB device is the first parent holding
 *  idVendor and idProduct. A parameter rather than /sys/class/tty so a test
 *  can hand it a fake tree. */
std::optional<UsbId> readUsbId (juce::File const &classTty,
                                juce::String const &ttyName);

/** The ttyACM and ttyUSB entries of `classTty` with their USB IDs, in
 *  directory order. */
std::vector<SerialTty> readSerialTtys (juce::File const &classTty);

/** readSerialTtys() and orderSerialCandidates() for the panel's ID. */
std::vector<juce::String> panelSerialCandidates (
    juce::File const &classTty = juce::File ("/sys/class/tty"));

}
