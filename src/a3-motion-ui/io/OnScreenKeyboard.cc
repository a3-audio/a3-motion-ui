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

#include "OnScreenKeyboard.hh"

namespace a3
{
namespace onScreenKeyboard
{

namespace
{
bool shown = false;

void
call (juce::String const &method)
{
  // dbus-send rather than a D-Bus library: one call on a button press, and a
  // library would be a dependency for it. The service file starts Onboard on
  // the first call, so this both launches and drives it.
  juce::StringArray command{ "dbus-send",
                             "--session",
                             "--type=method_call",
                             "--dest=org.onboard.Onboard",
                             "/org/onboard/Onboard/Keyboard",
                             "org.onboard.Onboard.Keyboard." + method };

  juce::ChildProcess process;
  if (!process.start (command))
    juce::Logger::writeToLog ("on-screen keyboard: could not call " + method);
}
}

void
show ()
{
  call ("Show");
  shown = true;
}

void
hide ()
{
  call ("Hide");
  shown = false;
}

bool
isShown ()
{
  return shown;
}

void
toggle ()
{
  shown ? hide () : show ();
}

}
}
