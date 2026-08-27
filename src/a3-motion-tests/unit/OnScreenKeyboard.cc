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

#include <a3-motion-ui/io/OnScreenKeyboard.hh>

using namespace a3;

// Onboard has a second owner of its visibility — its own "Hide Onboard" key —
// so the app cannot keep the answer in a flag of its own. It asks, and these
// are the replies `dbus-send --print-reply` gives back.

TEST (OnScreenKeyboardReply, AVisibleKeyboardReadsAsShown)
{
  EXPECT_TRUE (onScreenKeyboard::visibleFromReply (
      "method return time=1787828617.132311 sender=:1.42 -> destination=:1.77 "
      "serial=7 reply_serial=2\n   variant       boolean true\n"));
}

TEST (OnScreenKeyboardReply, AHiddenKeyboardReadsAsHidden)
{
  EXPECT_FALSE (onScreenKeyboard::visibleFromReply (
      "method return time=1787828617.132311 sender=:1.42 -> destination=:1.77 "
      "serial=7 reply_serial=2\n   variant       boolean false\n"));
}

TEST (OnScreenKeyboardReply, NoReplyIsNotAKeyboardOnScreen)
{
  EXPECT_FALSE (onScreenKeyboard::visibleFromReply (""));
}

TEST (OnScreenKeyboardReply, AnErrorIsNotAKeyboardOnScreen)
{
  EXPECT_FALSE (onScreenKeyboard::visibleFromReply (
      "Error org.freedesktop.DBus.Error.ServiceUnknown: The name "
      "org.onboard.Onboard was not provided by any .service files\n"));
}

// A reply that carries no boolean at all is not an answer, and the caller has
// to be able to tell that apart from a "no" — otherwise a broken query reads
// as "hidden" forever and the icon only ever shows, never hides.
TEST (OnScreenKeyboardReply, AReplyWithoutABooleanIsNotAnAnswer)
{
  EXPECT_FALSE (onScreenKeyboard::replyIsAnAnswer (""));
  EXPECT_FALSE (onScreenKeyboard::replyIsAnAnswer (
      "Error org.freedesktop.DBus.Error.ServiceUnknown: nope\n"));
  EXPECT_TRUE (
      onScreenKeyboard::replyIsAnAnswer ("   variant       boolean false\n"));
}
