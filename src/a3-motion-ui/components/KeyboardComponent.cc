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

#include "KeyboardComponent.hh"

#include <a3-motion-ui/theme/ThemeColours.hh>

namespace a3
{

namespace
{
// A German keyboard, in the order the hands know it: digits on top, then
// QWERTZ, ASDF, YXCV. No umlauts — a skin name, a host and a path are all
// ascii, and a key that does nothing when pressed is worse than a missing
// one.
//
// The last row carries what a path needs and the two commands. The
// characters here are a superset of every alphabet a TextInput accepts;
// the field being typed into is what refuses one it may not hold, so one
// keyboard serves all of them.
struct Row
{
  char const *keys;
  float indent; //< in key widths, the stagger that makes it read as a keyboard
};

constexpr Row rows[] = {
  { "1234567890", 0.0f },
  { "qwertzuiop", 0.25f },
  { "asdfghjkl", 0.75f },
  { "yxcvbnm", 1.25f },
};

constexpr int numRows = 4;
constexpr char const *symbolRow = "-._/\\";

constexpr int keyGap = 5;
constexpr float overlayOpacity = 0.72f;
constexpr float keyWash = 0.14f;
constexpr float keyTopWash = 0.22f;
constexpr float pressedWash = 0.34f;
}

KeyboardComponent::KeyboardComponent ()
{
  setInterceptsMouseClicks (true, false);
}

void
KeyboardComponent::resized ()
{
  layOutKeys ();
}

void
KeyboardComponent::layOutKeys ()
{
  _keys.clear ();

  auto area = getLocalBounds ().reduced (keyGap * 2);
  auto const rowH = area.getHeight () / (numRows + 1);

  // Every row is set out on the same grid as the widest one, so the columns
  // line up and the stagger reads as a keyboard rather than as four
  // differently sized rows.
  auto const columns = 10;
  auto const keyW = area.getWidth () / columns;

  for (int row = 0; row < numRows; ++row)
    {
      auto rowArea = area.removeFromTop (rowH);
      auto const letters = juce::String (rows[row].keys);

      auto x = rowArea.getX ()
               + static_cast<int> (rows[row].indent * keyW);

      for (int i = 0; i < letters.length (); ++i)
        {
          auto const bounds
              = juce::Rectangle<int> (x, rowArea.getY (), keyW,
                                      rowArea.getHeight ())
                    .reduced (keyGap / 2);
          _keys.push_back ({ juce::String::charToString (letters[i]),
                             letters[i], false, false, bounds });
          x += keyW;
        }
    }

  // The bottom row: what a path needs, then the two commands, which take
  // the width that is left.
  auto commandRow = area.removeFromTop (rowH);
  auto const symbols = juce::String (symbolRow);

  for (int i = 0; i < symbols.length (); ++i)
    _keys.push_back ({ juce::String::charToString (symbols[i]), symbols[i],
                       false, false,
                       commandRow.removeFromLeft (keyW).reduced (keyGap / 2) });

  auto const half = commandRow.getWidth () / 2;
  _keys.push_back ({ juce::String::fromUTF8 ("\xe2\x8c\xab"), 0, true, false,
                     commandRow.removeFromLeft (half).reduced (keyGap / 2) });
  _keys.push_back ({ "done", 0, false, true, commandRow.reduced (keyGap / 2) });
}

void
KeyboardComponent::paint (juce::Graphics &g)
{
  g.fillAll (toColour (theme ().surface, overlayOpacity));

  for (int i = 0; i < (int)_keys.size (); ++i)
    {
      auto const &key = _keys[(size_t)i];
      auto const face = key.bounds.toFloat ();
      bool const pressed = i == _pressed;

      // A key face with a lighter top: enough to read as a key rather than
      // as a rectangle with a letter in it, without turning into chrome.
      g.setGradientFill (juce::ColourGradient (
          toColour (theme ().textPrimary,
                    pressed ? pressedWash : keyTopWash),
          face.getX (), face.getY (),
          toColour (theme ().textPrimary, pressed ? pressedWash : keyWash),
          face.getX (), face.getBottom (), false));
      g.fillRoundedRectangle (face, 6.f);

      g.setColour (toColour (theme ().textPrimary, keyWash));
      g.drawRoundedRectangle (face, 6.f, 1.f);

      g.setFont (juce::Font (juce::FontOptions (
                                 theme ().fontSize (FontRole::Body) * 1.1f))
                     .withStyle (key.isDone ? juce::Font::bold
                                            : juce::Font::plain));
      g.setColour (key.isDone ? toColour (theme ().accent)
                              : toColour (theme ().textPrimary));
      g.drawText (key.label, key.bounds, juce::Justification::centred, false);
    }
}

void
KeyboardComponent::mouseDown (juce::MouseEvent const &event)
{
  for (int i = 0; i < (int)_keys.size (); ++i)
    if (_keys[(size_t)i].bounds.contains (event.getPosition ()))
      {
        _pressed = i;
        repaint ();
        return;
      }

  _pressed = -1;
}

void
KeyboardComponent::mouseUp (juce::MouseEvent const &event)
{
  juce::ignoreUnused (event);

  if (_pressed < 0)
    return;

  // Fired on release, and only if the finger is still on the key it went
  // down on: a slip off the edge is a change of mind, not a keystroke.
  auto const &key = _keys[(size_t)_pressed];
  auto const stillOnIt = key.bounds.contains (event.getPosition ());

  _pressed = -1;
  repaint ();

  if (!stillOnIt)
    return;

  if (key.isBackspace)
    {
      if (onBackspace)
        onBackspace ();
    }
  else if (key.isDone)
    {
      if (onDone)
        onDone ();
    }
  else if (onCharacter)
    onCharacter (key.character);
}

}
