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
#include <optional>

#include <juce_graphics/juce_graphics.h>

#include <a3-motion-ui/io/FunctionKeys.hh>

namespace a3
{

/** What each function key was asked to show, and what it is actually showing.
 *
 *  The serial link is shared with the input frames, so a colour that changes
 *  nothing must not be written again. The trap in that is that the record then
 *  speaks for the hardware -- and it is wrong whenever something was written
 *  past it, or written while nobody was listening. Then the key sits at the
 *  wrong colour until its own colour happens to change, which for a key with a
 *  constant colour is never. That is how MENU came to be the one dark key on a
 *  panel where everything else was right.
 *
 *  So two things are remembered, not one. **Wanted** is the colour the key was
 *  last asked for; **shown** is what was last written for it. They differ
 *  because what is written is the *resolved* colour -- a key with nothing to
 *  report takes the resting light, and the resting light is a config value
 *  that can change under a wanted colour that did not.
 *
 *  `forgetWhatIsShown()` is how a disagreement is ended: after it, the next
 *  write for every key goes through whatever it is. What to write then comes
 *  from `wantedFor()`, so the keys go back to what they were meant to show
 *  rather than to a guess.
 */
class LedCache
{
public:
  /** Remember what this key was asked for. The resolved colour is not this;
   *  see shouldWrite(). */
  void remember (FunctionKey key, juce::Colour wanted);

  /** What the key was last asked for, or nothing if it never was.
   *  Survives forgetWhatIsShown() -- that is the point of it. */
  std::optional<juce::Colour> wantedFor (FunctionKey key) const;

  /** True when `shown` differs from what this key is showing, and then takes
   *  it as shown. False -- do not write -- when it is already showing it. */
  bool shouldWrite (FunctionKey key, juce::Colour shown);

  /** Forget what every key is showing, keeping what each was asked for. */
  void forgetWhatIsShown ();

private:
  std::array<std::optional<juce::Colour>, numFunctionKeys> _wanted;
  std::array<std::optional<juce::Colour>, numFunctionKeys> _shown;
};

}
