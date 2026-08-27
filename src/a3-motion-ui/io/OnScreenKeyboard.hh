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

namespace a3
{

/** The system's on-screen keyboard, Onboard.
 *
 *  A keyboard drawn by this application was a keyboard this application had
 *  to maintain: a layout, key faces, a press model, and every character set
 *  anybody might need. Onboard already is one, it is installed, and it types
 *  into whatever window has the focus — which is the app.
 *
 *  Driven over its D-Bus interface. The service file starts it on the first
 *  call, so there is nothing to launch and nothing to keep running. */
namespace onScreenKeyboard
{

void show ();
void hide ();
/** Whether it is on screen right now — asked of Onboard, not remembered.
 *  Onboard has a second owner of that state, its own "Hide Onboard" key, so
 *  a flag kept here goes stale the first time somebody uses it and stays
 *  inverted from then on. This is what the icon in the status bar draws. */
bool isShown ();
void toggle ();

/** Whether a `dbus-send --print-reply` body says the keyboard is visible.
 *  Separated from the call so the parsing is testable without a bus. */
bool visibleFromReply (juce::String const &reply);

/** Whether that body is an answer at all. A reply that carries no boolean —
 *  no bus, no Onboard, an error — must not read as a plain "hidden", or the
 *  icon would only ever show and never hide again. */
bool replyIsAnAnswer (juce::String const &reply);

}

}
