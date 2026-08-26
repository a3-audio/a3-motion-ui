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
// The rows a name is typed from. Only what isUsableSkinName() allows.
char const *const rows[] = {
  "qwertzuiop",
  "asdfghjkl",
  "yxcvbnm-",
  "0123456789",
};

constexpr int numRows = 4;
constexpr int keyGap = 4;
constexpr float overlayOpacity = 0.72f;
constexpr float keyWash = 0.12f;
constexpr float pressedWash = 0.3f;
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

  for (int row = 0; row < numRows; ++row)
    {
      auto rowArea = area.removeFromTop (rowH).reduced (0, keyGap / 2);
      auto const letters = juce::String (rows[row]);
      auto const keyW = rowArea.getWidth () / juce::jmax (1, letters.length ());

      // Centred, so the shorter rows sit under the middle of the longer ones
      // the way a keyboard looks.
      rowArea = rowArea.withWidth (keyW * letters.length ())
                    .withX (rowArea.getX ()
                            + (rowArea.getWidth ()
                               - keyW * letters.length ())
                                  / 2);

      for (int i = 0; i < letters.length (); ++i)
        _keys.push_back ({ juce::String::charToString (letters[i]), letters[i],
                           false, false,
                           rowArea.removeFromLeft (keyW).reduced (keyGap / 2) });
    }

  // The two command keys share the last row.
  auto commandRow = area.removeFromTop (rowH).reduced (0, keyGap / 2);
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

      g.setColour (toColour (theme ().textPrimary,
                             i == _pressed ? pressedWash : keyWash));
      g.fillRoundedRectangle (key.bounds.toFloat (), 5.f);

      g.setFont (juce::Font (theme ().fontSize (FontRole::Body),
                             key.isDone ? juce::Font::bold
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
