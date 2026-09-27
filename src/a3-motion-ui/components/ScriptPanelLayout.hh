/*

  A3 Motion UI
  Copyright (C) 2026 Patric Schmitz

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

/** Where the script panel puts things: the text, the errors under it, and
 *  four keys at its foot. Reads no theme, so it can be checked at any size. */
struct ScriptPanelLayout
{
  juce::Rectangle<int> textArea;
  /** Empty when there is nothing wrong -- the text gets the room back. */
  juce::Rectangle<int> errorArea;
  juce::Rectangle<int> fromClipButton;
  juce::Rectangle<int> cancelButton;
  juce::Rectangle<int> saveButton;
  juce::Rectangle<int> saveAsButton;
};

ScriptPanelLayout layOutScriptPanel (juce::Rectangle<int> bounds,
                                     int buttonHeight, int errorLines,
                                     int lineHeight);

/** Which of the panel's keys would do something. Dark otherwise: a key
 *  offering to save nothing is a key you have to stop and think about. */
struct ScriptKeyStates
{
  bool fromClip;
  bool cancel;
  bool save;
  bool saveAs;
};

/** @param unsaved   the text differs from what was last saved or loaded
 *  @param locked    one of the instrument's own (shippedFileMayBeOverwritten)
 *  @param hasFile   a file stands behind the text to write back to
 *  @param slotHolds the shown slot has a clip to take settings from */
ScriptKeyStates scriptKeysFor (bool unsaved, bool locked, bool hasFile,
                               bool slotHolds);

}
