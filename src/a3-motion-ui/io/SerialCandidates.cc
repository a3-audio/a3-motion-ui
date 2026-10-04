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

#include "SerialCandidates.hh"

#include <algorithm>
#include <iterator>

namespace a3
{

namespace
{

bool
isSerialPortName (juce::String const &name)
{
  return name.startsWith ("ttyACM") || name.startsWith ("ttyUSB");
}

bool
sameUsbId (UsbId const &a, UsbId const &b)
{
  return a.vendor.equalsIgnoreCase (b.vendor)
         && a.product.equalsIgnoreCase (b.product);
}

/** Follows a chain of symbolic links to the directory they end in. One step
 *  at a time, because a relative link means relative to where the previous
 *  one landed: /sys/class/tty/ttyACM0 points into the device tree, and its
 *  "device" link is relative to that, not to /sys/class/tty. A bound keeps a
 *  link loop from hanging the poll thread. */
juce::File
followLinks (juce::File file)
{
  constexpr int maxLinks = 16;

  for (int i = 0; i < maxLinks && file.isSymbolicLink (); ++i)
    file = file.getLinkedTarget ();

  return file;
}

}

UsbId
panelUsbId ()
{
  return { "1a86", "55d3" };
}

std::vector<juce::String>
orderSerialCandidates (std::vector<SerialTty> const &ttys,
                       UsbId const &preferred)
{
  std::vector<SerialTty> serial;
  std::copy_if (ttys.begin (), ttys.end (), std::back_inserter (serial),
                [] (SerialTty const &tty) { return isSerialPortName (tty.name); });

  auto const isPreferred = [&preferred] (SerialTty const &tty) {
    return tty.usbId.has_value () && sameUsbId (*tty.usbId, preferred);
  };

  std::sort (serial.begin (), serial.end (),
             [&isPreferred] (SerialTty const &a, SerialTty const &b) {
               if (isPreferred (a) != isPreferred (b))
                 return isPreferred (a);
               return a.name.compareNatural (b.name) < 0;
             });

  std::vector<juce::String> paths;
  for (auto const &tty : serial)
    paths.push_back ("/dev/" + tty.name);
  return paths;
}

std::optional<UsbId>
readUsbId (juce::File const &classTty, juce::String const &ttyName)
{
  auto const ttyDir = followLinks (classTty.getChildFile (ttyName));
  auto const deviceLink = ttyDir.getChildFile ("device");
  if (!deviceLink.exists ())
    return std::nullopt;

  // An ACM tty's device is the USB interface itself, a usb-serial one's sits
  // a level below it; either way the USB device is the first parent that
  // carries both files. Walking until the root rather than a fixed depth is
  // what makes both drivers -- and whatever a hub adds -- the same case.
  constexpr int maxDepth = 32;

  auto dir = followLinks (deviceLink);
  for (int depth = 0; depth < maxDepth; ++depth)
    {
      auto const vendor = dir.getChildFile ("idVendor");
      auto const product = dir.getChildFile ("idProduct");
      if (vendor.existsAsFile () && product.existsAsFile ())
        return UsbId{ vendor.loadFileAsString ().trim (),
                      product.loadFileAsString ().trim () };

      auto const parent = dir.getParentDirectory ();
      if (parent == dir)
        break;
      dir = parent;
    }

  return std::nullopt;
}

std::vector<SerialTty>
readSerialTtys (juce::File const &classTty)
{
  std::vector<SerialTty> ttys;

  // Entries in /sys/class/tty are symbolic links into the device tree, so
  // they are looked for as files and directories both. Only the serial ones
  // have their ID read: there are dozens of virtual consoles and pseudo
  // terminals beside them, and this runs every two seconds while no panel is
  // found.
  for (auto const &entry : juce::RangedDirectoryIterator (
           classTty, false, "*",
           juce::File::findFilesAndDirectories
               | juce::File::ignoreHiddenFiles))
    {
      auto const name = entry.getFile ().getFileName ();
      if (isSerialPortName (name))
        ttys.push_back ({ name, readUsbId (classTty, name) });
    }

  return ttys;
}

std::vector<juce::String>
panelSerialCandidates (juce::File const &classTty)
{
  return orderSerialCandidates (readSerialTtys (classTty), panelUsbId ());
}

}
