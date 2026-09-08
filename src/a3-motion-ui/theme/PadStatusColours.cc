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

#include "PadStatusColours.hh"

#include <a3-motion-ui/theme/Theme.hh>

namespace a3
{

namespace
{
/** The blink shows the channel's colour on even ticks and the dark shade on
 *  odd ones, so a waiting pad reads as waiting rather than as a fourth state
 *  with a colour of its own. */
bool
blinkIsLit (int step)
{
  return step % 2 == 0;
}
}

juce::Colour
padStatusColour (juce::Colour base, Pattern::Status status,
                 Pattern::Status statusLast, int step, bool oneShotRecording)
{
  auto const shaded = [base] (float amount) { return base.darker (amount); };

  switch (status)
    {
    case Pattern::Status::Empty:
      return shaded (theme ().padShadeEmpty);

    case Pattern::Status::Idle:
      return shaded (theme ().padShadeIdle);

    case Pattern::Status::Recording:
    case Pattern::Status::Playing:
      return base;

    // Both promise the same thing about the next beat, so both say it the
    // same way.
    case Pattern::Status::ScheduledForRecording:
    case Pattern::Status::ScheduledForPlaying:
      return blinkIsLit (step) ? base : shaded (theme ().padShadeBlink);

    case Pattern::Status::ScheduledForIdle:
      jassert (statusLast != Pattern::Status::ScheduledForRecording
               && statusLast != Pattern::Status::Idle);

      // A one-shot take stops at the end of its pass whatever happens next.
      // Blinking would promise a beat that is not coming.
      if (oneShotRecording && statusLast == Pattern::Status::Recording)
        return base;

      // On its way out rather than in, so it alternates between the two
      // shades below it instead of reaching the channel's colour.
      return blinkIsLit (step) ? shaded (theme ().padShadeEmpty)
                               : shaded (theme ().padShadeBlink);
    }

  return shaded (theme ().padShadeEmpty);
}

}
