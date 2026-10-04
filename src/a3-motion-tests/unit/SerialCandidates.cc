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

#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <a3-motion-ui/io/SerialCandidates.hh>

using namespace a3;

// ── Which ports to ask, and in which order ───────────────────────────────

// The panel used to be looked for on /dev/ttyACM0..2 and ttyUSB0..2 only.
// The tty number is whatever the kernel handed out, so a panel enumerated
// after three other USB-serial devices was never found. Its USB ID does not
// change between machines: the CH343 bridge, 1a86:55d3, the same one
// PlatformIO, host.py and the installer look for.

namespace
{

UsbId const panelBridge{ "1a86", "55d3" };
UsbId const pico{ "2e8a", "000a" };

std::vector<juce::String>
paths (std::initializer_list<char const *> names)
{
  std::vector<juce::String> result;
  for (auto const *name : names)
    result.push_back (juce::String ("/dev/") + name);
  return result;
}

}

TEST (SerialCandidates, ThePanelsBridgeIsAskedFirst)
{
  std::vector<SerialTty> const ttys = {
    { "ttyACM0", pico },
    { "ttyACM1", panelBridge },
    { "ttyUSB0", std::nullopt },
  };

  EXPECT_EQ (paths ({ "ttyACM1", "ttyACM0", "ttyUSB0" }),
             orderSerialCandidates (ttys, panelBridge));
}

// The ID only decides who goes first. A different bridge on a future board
// is still asked -- the PING handshake has the final word, not the ID.
TEST (SerialCandidates, EveryOtherSerialPortIsStillAsked)
{
  std::vector<SerialTty> const ttys = {
    { "ttyUSB1", std::nullopt },
    { "ttyACM2", pico },
  };

  EXPECT_EQ (paths ({ "ttyACM2", "ttyUSB1" }),
             orderSerialCandidates (ttys, panelBridge));
}

TEST (SerialCandidates, AnyTtyNumberIsFound)
{
  std::vector<SerialTty> const ttys = {
    { "ttyACM0", pico },
    { "ttyACM7", panelBridge },
  };

  EXPECT_EQ (paths ({ "ttyACM7", "ttyACM0" }),
             orderSerialCandidates (ttys, panelBridge));
}

// Natural order, so ttyACM10 comes after ttyACM2 and not between 1 and 2.
TEST (SerialCandidates, EachGroupIsInNameOrder)
{
  std::vector<SerialTty> const ttys = {
    { "ttyUSB0", std::nullopt },  { "ttyACM10", std::nullopt },
    { "ttyACM2", std::nullopt },  { "ttyACM11", panelBridge },
    { "ttyACM3", panelBridge },
  };

  EXPECT_EQ (paths ({ "ttyACM3", "ttyACM11", "ttyACM2", "ttyACM10",
                      "ttyUSB0" }),
             orderSerialCandidates (ttys, panelBridge));
}

// The sysfs files are lower case; the board file writes 0x1A86. Both are the
// same bridge.
TEST (SerialCandidates, TheIdIsComparedWithoutCase)
{
  std::vector<SerialTty> const ttys = {
    { "ttyACM0", pico },
    { "ttyACM1", UsbId{ "1A86", "55D3" } },
  };

  EXPECT_EQ (paths ({ "ttyACM1", "ttyACM0" }),
             orderSerialCandidates (ttys, panelBridge));
}

TEST (SerialCandidates, ConsolesAndPseudoTerminalsAreNotSerialPorts)
{
  std::vector<SerialTty> const ttys = {
    { "tty0", std::nullopt },
    { "ttyS0", std::nullopt },
    { "console", std::nullopt },
    { "ptmx", std::nullopt },
    { "ttyACM0", std::nullopt },
  };

  EXPECT_EQ (paths ({ "ttyACM0" }), orderSerialCandidates (ttys, panelBridge));
}

// ── Reading the USB ID out of sysfs ──────────────────────────────────────

namespace
{

/** /sys/class/tty and the device tree behind it, as the kernel lays it out:
 *  an ACM tty sits directly under its USB interface, a usb-serial one a level
 *  deeper, and the USB device with idVendor/idProduct is the interface's
 *  parent. The same shape as the installer's fake_serial_devices(). */
class FakeSysfs
{
public:
  FakeSysfs ()
      : root (juce::File::getSpecialLocation (juce::File::tempDirectory)
                  .getChildFile ("a3-serial-candidates-"
                                 + juce::String (juce::Random ().nextInt64 ())))
  {
    classTty ().createDirectory ();
  }

  ~FakeSysfs () { root.deleteRecursively (false); }

  juce::File
  classTty () const
  {
    return root.getChildFile ("class/tty");
  }

  enum class Driver
  {
    acm,
    usbSerial
  };

  /** The tty's class entry is a plain directory with an absolute device
   *  link, as in the installer's fake tree. */
  void
  addUsbTty (juce::String const &tty, UsbId const &id, Driver driver)
  {
    auto const usb = root.getChildFile ("devices/usb-" + tty + "/3-1");
    auto const interface = usb.getChildFile ("3-1:1.0");
    auto const device
        = driver == Driver::usbSerial ? interface.getChildFile (tty) : interface;
    device.createDirectory ();
    usb.getChildFile ("idVendor").replaceWithText (id.vendor + "\n");
    usb.getChildFile ("idProduct").replaceWithText (id.product + "\n");

    auto const entry = classTty ().getChildFile (tty);
    entry.createDirectory ();
    juce::File::createSymbolicLink (entry.getChildFile ("device"),
                                    device.getFullPathName (), true);
  }

  /** The way the real kernel does it: the class entry is itself a relative
   *  link into the device tree, and the device link is relative to where
   *  that one lands -- not to /sys/class/tty. */
  void
  addKernelStyleAcmTty (juce::String const &tty, UsbId const &id)
  {
    auto const usb = root.getChildFile ("devices/pci0000:00/usb3/3-1");
    auto const interface = usb.getChildFile ("3-1:1.0");
    auto const ttyNode = interface.getChildFile ("tty/" + tty);
    ttyNode.createDirectory ();
    usb.getChildFile ("idVendor").replaceWithText (id.vendor + "\n");
    usb.getChildFile ("idProduct").replaceWithText (id.product + "\n");

    juce::File::createSymbolicLink (ttyNode.getChildFile ("device"),
                                    "../../../3-1:1.0", true);
    juce::File::createSymbolicLink (
        classTty ().getChildFile (tty),
        "../../devices/pci0000:00/usb3/3-1/3-1:1.0/tty/" + tty, true);
  }

  /** A tty with no USB device above it, like a console or a built-in UART. */
  void
  addPlainTty (juce::String const &tty)
  {
    auto const device = root.getChildFile ("devices/platform/serial8250");
    device.createDirectory ();
    auto const entry = classTty ().getChildFile (tty);
    entry.createDirectory ();
    juce::File::createSymbolicLink (entry.getChildFile ("device"),
                                    device.getFullPathName (), true);
  }

  void
  addVirtualTty (juce::String const &tty)
  {
    classTty ().getChildFile (tty).createDirectory ();
  }

private:
  juce::File root;
};

}

TEST (SerialCandidates, TheIdIsFoundAboveAnAcmInterface)
{
  FakeSysfs sysfs;
  sysfs.addUsbTty ("ttyACM0", panelBridge, FakeSysfs::Driver::acm);

  auto const id = readUsbId (sysfs.classTty (), "ttyACM0");

  ASSERT_TRUE (id.has_value ());
  EXPECT_EQ (juce::String ("1a86"), id->vendor);
  EXPECT_EQ (juce::String ("55d3"), id->product);
}

TEST (SerialCandidates, TheIdIsFoundAboveAUsbSerialTty)
{
  FakeSysfs sysfs;
  sysfs.addUsbTty ("ttyUSB0", panelBridge, FakeSysfs::Driver::usbSerial);

  auto const id = readUsbId (sysfs.classTty (), "ttyUSB0");

  ASSERT_TRUE (id.has_value ());
  EXPECT_EQ (juce::String ("1a86"), id->vendor);
  EXPECT_EQ (juce::String ("55d3"), id->product);
}

TEST (SerialCandidates, TheIdIsFoundThroughTheKernelsRelativeLinks)
{
  FakeSysfs sysfs;
  sysfs.addKernelStyleAcmTty ("ttyACM3", panelBridge);

  auto const id = readUsbId (sysfs.classTty (), "ttyACM3");

  ASSERT_TRUE (id.has_value ());
  EXPECT_EQ (juce::String ("1a86"), id->vendor);
  EXPECT_EQ (juce::String ("55d3"), id->product);
}

TEST (SerialCandidates, ATtyWithoutAUsbDeviceHasNoId)
{
  FakeSysfs sysfs;
  sysfs.addPlainTty ("ttyS0");
  sysfs.addVirtualTty ("tty0");

  EXPECT_FALSE (readUsbId (sysfs.classTty (), "ttyS0").has_value ());
  EXPECT_FALSE (readUsbId (sysfs.classTty (), "tty0").has_value ());
  EXPECT_FALSE (readUsbId (sysfs.classTty (), "ttyACM9").has_value ());
}

TEST (SerialCandidates, TheWholeTreeGivesThePanelFirstAndNothingElse)
{
  FakeSysfs sysfs;
  sysfs.addUsbTty ("ttyACM0", pico, FakeSysfs::Driver::acm);
  sysfs.addUsbTty ("ttyACM7", panelBridge, FakeSysfs::Driver::acm);
  sysfs.addUsbTty ("ttyUSB0", UsbId{ "0403", "6001" },
                   FakeSysfs::Driver::usbSerial);
  sysfs.addPlainTty ("ttyS0");
  sysfs.addVirtualTty ("tty0");

  EXPECT_EQ (paths ({ "ttyACM7", "ttyACM0", "ttyUSB0" }),
             panelSerialCandidates (sysfs.classTty ()));
}

TEST (SerialCandidates, NoSysfsMeansNoCandidates)
{
  auto const missing
      = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("a3-serial-candidates-does-not-exist");

  EXPECT_TRUE (panelSerialCandidates (missing).empty ());
}
