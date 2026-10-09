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
#include <vector>

#include <a3-motion-ui/io/FunctionKeys.hh>
#include <a3-motion-ui/io/PadFunctions.hh>

namespace a3
{

/** What a press on an end key turned out to mean. */
struct EndKeyMeaning
{
  enum class Kind
  {
    /** SHIFT on a row with no shifted function, or a key whose shifted
     *  function SHIFT's release has already ended. */
    Nothing,
    Function,
    /** The same pad on every channel -- the scene path. */
    ScenePad,
  };

  Kind kind = Kind::Nothing;
  FunctionKey function = FunctionKey::Tap;
  index_t pad = 0;
  /** Reached through SHIFT, and so over when SHIFT is let go. */
  bool shifted = false;

  static EndKeyMeaning
  plain (FunctionKey key)
  {
    return { Kind::Function, key, 0, false };
  }

  static EndKeyMeaning
  viaShift (FunctionKey key)
  {
    return { Kind::Function, key, 0, true };
  }

  static EndKeyMeaning
  scenePad (index_t pad)
  {
    return { Kind::ScenePad, FunctionKey::Tap, pad, false };
  }

  /** The shifted flag is the layer's bookkeeping, not part of what the key
   *  does, so two meanings compare by what they do. */
  bool
  operator== (EndKeyMeaning const &other) const
  {
    if (kind != other.kind)
      return false;
    switch (kind)
      {
      case Kind::Nothing:  return true;
      case Kind::Function: return function == other.function;
      case Kind::ScenePad: return pad == other.pad;
      }
    return false;
  }
};

struct EndKeyEvent
{
  EndKeyMeaning meaning;
  bool down = false;

  bool
  operator== (EndKeyEvent const &other) const
  {
    return meaning == other.meaning && down == other.down;
  }
};

/** The pad an end key fires on every channel: PLAY all the Play|Pause pad,
 *  an action key its action's pad. Nothing for TAP and SHIFT. */
inline std::optional<index_t>
scenePadOf (EndKey key)
{
  if (key == EndKey::PlayAll)
    return padIndexFor (PadFunction::PlayPause);
  auto const button = endKeyActionButton (key);
  if (button < 0)
    return std::nullopt;
  return padIndexForAction (button);
}

/** What the end keys mean, SHIFT layer included.
 *
 *  Fed each key's combined state -- both ends of the panel and the PADS page
 *  already joined (EndKeyHold) -- and answers with what that does, in order.
 *  A key is decided when it goes down and keeps that meaning until it comes
 *  up, so a release always reaches what its press started. The one exception
 *  is SHIFT's release: a combination lasts while both its keys are held, so
 *  letting go of SHIFT ends every shifted function still held -- REC above
 *  all, which is held as a modifier -- and the other key's later release
 *  then means nothing.
 *
 *  Only SHIFT first and the key second is a combination. Pressed the other
 *  way round the key has already done its plain thing by the time SHIFT
 *  arrives, and that cannot be taken back.
 */
class EndKeyLayer
{
public:
  std::vector<EndKeyEvent>
  set (EndKey key, bool down)
  {
    return down ? press (key) : release (key);
  }

  /** Whether a function is held through the end keys right now. */
  bool
  isDown (FunctionKey function) const
  {
    for (auto const &held : _held)
      if (held && held->kind == EndKeyMeaning::Kind::Function
          && held->function == function)
        return true;
    return false;
  }

private:
  std::vector<EndKeyEvent>
  press (EndKey key)
  {
    auto const meaning = resolve (key);
    at (key) = meaning;
    if (meaning.kind == EndKeyMeaning::Kind::Nothing)
      return {};
    return { { meaning, true } };
  }

  std::vector<EndKeyEvent>
  release (EndKey key)
  {
    std::vector<EndKeyEvent> events;
    if (key == EndKey::Shift)
      for (auto &held : _held)
        if (held && held->shifted
            && held->kind == EndKeyMeaning::Kind::Function)
          {
            events.push_back ({ *held, false });
            held = EndKeyMeaning{};
          }

    auto &held = at (key);
    if (held && held->kind != EndKeyMeaning::Kind::Nothing)
      events.push_back ({ *held, false });
    held.reset ();
    return events;
  }

  EndKeyMeaning
  resolve (EndKey key) const
  {
    if (key == EndKey::Shift)
      return EndKeyMeaning::plain (FunctionKey::Shift);

    if (isDown (FunctionKey::Shift))
      {
        auto const shifted = shiftedFunction (key);
        return shifted ? EndKeyMeaning::viaShift (*shifted)
                       : EndKeyMeaning{};
      }

    if (auto const plain = plainFunction (key))
      return EndKeyMeaning::plain (*plain);
    if (auto const pad = scenePadOf (key))
      return EndKeyMeaning::scenePad (*pad);
    return {};
  }

  std::optional<EndKeyMeaning> &
  at (EndKey key)
  {
    return _held[static_cast<std::size_t> (key)];
  }

  /** What each key down was pressed as; empty for a key that is up. */
  std::array<std::optional<EndKeyMeaning>, numEndKeys> _held{};
};

}
