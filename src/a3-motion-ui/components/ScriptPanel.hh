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

#include <a3-motion-ui/components/ScriptEditor.hh>
#include <a3-motion-ui/components/ScriptPanelLayout.hh>
#include <a3-motion-ui/components/TouchControl.hh>
#include <a3-motion-ui/theme/ThemedComponent.hh>

#include <functional>
#include <memory>

namespace a3
{

/** A script to read and edit: the text, what is wrong with it, and four keys
 *  under it -- FROM CLIP, Cancel, Save, Save as.
 *
 *  Pulled out of the ACTION page on 2026-09-27 so the editor could stand in
 *  FILES beside the list, without a second copy of it. It decides nothing:
 *  what the text is, where Save writes it and what runs it afterwards are the
 *  page's that hosts it, and a key only says it was pressed. It knows no more
 *  than that it holds text, so the other kinds of file can be shown in it
 *  later.
 */
class ScriptPanel : public juce::Component, public ThemedComponent
{
public:
  ScriptPanel ();
  ~ScriptPanel () override;

  void paint (juce::Graphics &g) override;
  void resized () override;
  void applyTheme () override;

  /** Text as it stands in a file: saved, nothing to keep. Never while it is
   *  being typed into, and only when it differs -- the host refreshes on a
   *  timer, and either would throw away what is being written. */
  void setScript (juce::String const &script);
  /** Text that is on no disk yet (FROM CLIP): in, and marked unsaved. */
  void offerScript (juce::String const &script);
  juce::String script () const { return _document.getAllContent (); }
  bool hasUnsavedChanges () const;
  /** What is shown is on disk now. Called by the host once the file is
   *  written, so a failed write leaves the edge marked. */
  void markSaved ();

  void setErrors (juce::StringArray const &errors);
  /** One of the instrument's own: Save stays dark, Save as is the way out. */
  void setProtected (bool locked);
  /** A file stands behind the text to write back to. */
  void setHasFile (bool hasFile);
  /** The shown slot holds a clip, so FROM CLIP has something to take. */
  void setSlotHolds (bool holds);
  void setChannelColour (juce::Colour colour);
  void stopEditing ();
  /** How the text is coloured: C-like for scripts and JSON, XML for SVG.
   *  JUCE's editor takes its tokeniser once, so this builds a new one; the
   *  text, being the document's, stays. */
  void setLanguage (ScriptLanguage language);
  /** The word on the FROM key -- what it takes the current state of. */
  void setFromLabel (juce::String const &label);

  /** How tall the panel's keys are, so the page beside it can stand its own
   *  in the same row. */
  int keyHeight () const;
  /** How wide the panel has to be for `characters` of text in a line, line
   *  numbers, scroll bar and insets included -- so the page can give it just
   *  that. */
  int widthFor (int characters) const;
  /** The same at the usual font size: what the page should offer, so a font
   *  stepped down once is not what decides the next layout. */
  int usualWidthFor (int characters) const;
  /** Keep `characters` of a line in view: where the panel is narrower than
   *  that at the usual size, the font steps down until it fits (never below
   *  a size that is still read). 0 lets the text scroll sideways instead. */
  void setColumnsToFit (int characters);
  /** Save and Cancel light up for a moment: the list beside the panel is
   *  waiting for one of them (2026-09-27). */
  void flashKeys ();

  std::function<void ()> onSave;
  std::function<void ()> onSaveAs;
  std::function<void ()> onCancel;
  std::function<void ()> onFromClip;
  /** The editor took or gave up the caret -- the host shows and hides the
   *  system keyboard on it. */
  std::function<void (bool editing)> onEditingChanged;

private:
  /** Puts the skin on the editor: its colour ids, the tokeniser's scheme and
   *  the script font. Called whenever the skin changes. */
  void dressEditor ();
  void buildEditor ();
  ScriptKeyStates keys () const;

  void paintField (juce::Graphics &g);
  void paintErrors (juce::Graphics &g);
  void paintKeys (juce::Graphics &g);

  void focusLost (FocusChangeType cause) override;

  /** One font for the script, and the measurements everything else reads
   *  off it. */
  juce::Font scriptFont () const;
  float usualFontSize () const;
  int widthAt (float fontSize, int characters) const;
  /** Works out _fittedSize for the width the panel has now. */
  void fitFontToWidth ();
  int scriptLineHeight () const;
  int textInset () const;

  ScriptPanelLayout _layout;
  juce::Colour _channelColour;

  /** The script itself, and the editor over it -- JUCE's, see ScriptEditor.
   *  The C++ tokeniser rather than one of our own: SuperCollider's comments,
   *  strings, numbers and brackets are close enough to read by, and a
   *  tokeniser for the rest is a job of its own. */
  juce::CodeDocument _document;
  juce::CPlusPlusCodeTokeniser _tokeniser;
  juce::XmlTokeniser _xmlTokeniser;
  ScriptLanguage _language = ScriptLanguage::CLike;
  juce::String _fromLabel{ "from clip" };
  int _columnsToFit = 0;
  /** The size the font steps down to so a line fits; 0 is the usual size. */
  float _fittedSize = 0.f;
  std::unique_ptr<ScriptEditor> _editor;
  juce::StringArray _errors;
  bool _editing = false;
  bool _protected = false;
  bool _hasFile = false;
  bool _slotHolds = false;
  bool _flashing = false;

  std::unique_ptr<TouchControl> _fromClipTouch;
  std::unique_ptr<TouchControl> _cancelTouch;
  std::unique_ptr<TouchControl> _saveTouch;
  std::unique_ptr<TouchControl> _saveAsTouch;
};

}
