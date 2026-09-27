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

#include "ScriptPanel.hh"

#include <a3-motion-ui/components/ControllerLayout.hh>
#include <a3-motion-ui/components/FittedFont.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>
#include <a3-motion-ui/theme/TransportLook.hh>

namespace a3
{

ScriptPanel::ScriptPanel ()
{
  // The keys have to land here rather than in the void: Onboard types into
  // whatever has the focus, and a panel that never asked for it gets nothing.
  setWantsKeyboardFocus (true);

  // A fresh document counts as changed until it has a save point; nothing has
  // been typed into this one yet.
  _document.setSavePoint ();

  buildEditor ();

  // Each key asks the rule it is lit by (scriptKeysFor), so a dark key is
  // also a dead one: a save that depends on a key having been dark happens
  // the first time something else lights it.
  _fromClipTouch = std::make_unique<TouchControl> ();
  _fromClipTouch->onTap = [this] (int, int) {
    if (!keys ().fromClip)
      return;
    if (onFromClip)
      onFromClip ();
  };
  addAndMakeVisible (*_fromClipTouch);

  _cancelTouch = std::make_unique<TouchControl> ();
  _cancelTouch->onTap = [this] (int, int) {
    stopEditing ();
    if (onCancel)
      onCancel ();
    repaint ();
  };
  addAndMakeVisible (*_cancelTouch);

  _saveTouch = std::make_unique<TouchControl> ();
  _saveTouch->onTap = [this] (int, int) {
    if (!keys ().save)
      return;
    stopEditing ();
    if (onSave)
      onSave ();
    repaint ();
  };
  addAndMakeVisible (*_saveTouch);

  _saveAsTouch = std::make_unique<TouchControl> ();
  _saveAsTouch->onTap = [this] (int, int) {
    if (!keys ().saveAs)
      return;
    stopEditing ();
    if (onSaveAs)
      onSaveAs ();
    repaint ();
  };
  addAndMakeVisible (*_saveAsTouch);
}

ScriptPanel::~ScriptPanel () = default;

void
ScriptPanel::buildEditor ()
{
  if (_editor)
    removeChildComponent (_editor.get ());

  // The script is JUCE's editor, read-only until it is touched -- see
  // ScriptEditor for the three things a finger needs on top of it.
  _editor = std::make_unique<ScriptEditor> (
      _document, _language == ScriptLanguage::Xml
                     ? static_cast<juce::CodeTokeniser *> (&_xmlTokeniser)
                     : static_cast<juce::CodeTokeniser *> (&_tokeniser));
  // JUCE's editor calls itself opaque, but it is drawn transparent over the
  // darker field. Believed, a scroll repaints only the editor and not the
  // ground behind it -- and over the sphere that ground is the OpenGL
  // picture, so the trajectory showed through the script.
  _editor->setOpaque (false);
  _editor->onStartEditing = [this] {
    if (!_editing)
      {
        _editing = true;
        _editor->setReadOnly (false);
        _editor->grabKeyboardFocus ();
        if (onEditingChanged)
          onEditingChanged (true);
      }
    repaint ();
  };
  _editor->onEscape = [this] { stopEditing (); };
  addAndMakeVisible (*_editor);
  dressEditor ();
}

void
ScriptPanel::setLanguage (ScriptLanguage language)
{
  if (language == _language)
    return;

  stopEditing ();
  _language = language;
  buildEditor ();
  resized ();
}

void
ScriptPanel::setFromLabel (juce::String const &label)
{
  if (label == _fromLabel)
    return;
  _fromLabel = label;
  repaint ();
}


void
ScriptPanel::applyTheme ()
{
  dressEditor ();
  resized ();
  repaint ();
}

void
ScriptPanel::dressEditor ()
{
  if (!_editor)
    return;

  // The skin's colours, through the editor's own ids -- the same way a slider
  // gets its channel colour. The field behind it is already drawn darker than
  // the page, so the editor itself stays transparent to it.
  _editor->setColour (juce::CodeEditorComponent::backgroundColourId,
                      juce::Colours::transparentBlack);
  _editor->setColour (juce::CodeEditorComponent::defaultTextColourId,
                      toColour (theme ().textPrimary,
                                theme ().alphaTextStrong));
  _editor->setColour (juce::CodeEditorComponent::lineNumberBackgroundId,
                      juce::Colours::transparentBlack);
  _editor->setColour (juce::CodeEditorComponent::lineNumberTextId,
                      toColour (theme ().textMuted, theme ().alphaMuted));
  _editor->setColour (juce::CodeEditorComponent::highlightColourId,
                      _channelColour.withAlpha (theme ().alphaFillEmphasis));

  // What the tokeniser names, in the skin's words: a comment is what tells a
  // written-out script from one somebody explained, which is why it was the
  // one thing the hand-drawn editor coloured at all.
  juce::CodeEditorComponent::ColourScheme scheme;
  scheme.set ("Comment", toColour (theme ().textMuted, theme ().alphaInactive));
  scheme.set ("String", toColour (theme ().accent));
  scheme.set ("Integer", toColour (theme ().accent));
  scheme.set ("Float", toColour (theme ().accent));
  scheme.set ("Keyword", toColour (theme ().highlight));
  scheme.set ("Operator", toColour (theme ().textPrimary, theme ().alphaSecondary));
  scheme.set ("Bracket", toColour (theme ().textPrimary, theme ().alphaSecondary));
  scheme.set ("Punctuation", toColour (theme ().textPrimary, theme ().alphaSecondary));
  scheme.set ("Identifier", toColour (theme ().textPrimary, theme ().alphaTextStrong));
  scheme.set ("Error", toColour (theme ().danger));
  _editor->setColourScheme (scheme);

  _editor->setFont (scriptFont ());

  // The bars that come with it: the skin's grey, and as wide as a line is
  // tall rather than JUCE's sixteen pixels -- everything here is measured in
  // what it stands next to.
  _editor->setColour (juce::ScrollBar::backgroundColourId,
                      juce::Colours::transparentBlack);
  _editor->setColour (juce::ScrollBar::thumbColourId,
                      toColour (theme ().textMuted, theme ().alphaGuide));
  _editor->setColour (juce::ScrollBar::trackColourId,
                      juce::Colours::transparentBlack);
  _editor->setScrollbarThickness (
      juce::jmax (1, juce::roundToInt (scriptFont ().getHeight () / 2.f)));
}

void
ScriptPanel::resized ()
{
  _layout = layOutScriptPanel (getLocalBounds (), buttonHeight (),
                               _errors.size (), scriptLineHeight ());
  _editor->setBounds (_layout.textArea.reduced (textInset ()));
  _fromClipTouch->setBounds (_layout.fromClipButton);
  _cancelTouch->setBounds (_layout.cancelButton);
  _saveTouch->setBounds (_layout.saveButton);
  _saveAsTouch->setBounds (_layout.saveAsButton);
}

// The key height the ACTION page used: a fingertip, or twice the header
// size, whichever is more.
int
ScriptPanel::buttonHeight () const
{
  return juce::jmax (fingertipSize,
                     juce::roundToInt (theme ().fontSize (FontRole::Header)
                                       * 2.f));
}

int
ScriptPanel::textInset () const
{
  return juce::jmax (juce::roundToInt (theme ().paddingSmall),
                     _layout.textArea.getHeight () / 40);
}

void
ScriptPanel::setScript (juce::String const &script)
{
  // Never while it is being typed into, and otherwise only when it is
  // actually different: the host refreshes on a timer, and either would
  // throw away what is being written and put the caret back at the top.
  if (_editing || script == _document.getAllContent ())
    return;

  _document.replaceAllContent (script);
  _document.clearUndoHistory ();
  _document.setSavePoint ();
  repaint ();
}

void
ScriptPanel::offerScript (juce::String const &script)
{
  stopEditing ();
  _document.replaceAllContent (script);
  // No save point: this text is on no disk yet, and Save or Save as decides
  // where it goes.
  repaint ();
}

bool
ScriptPanel::hasUnsavedChanges () const
{
  return _document.hasChangedSinceSavePoint ();
}

void
ScriptPanel::markSaved ()
{
  _document.setSavePoint ();
  repaint ();
}

// Errors change the layout (they take room off the text), so a change lays
// out again rather than only repainting.
void
ScriptPanel::setErrors (juce::StringArray const &errors)
{
  if (errors == _errors)
    return;
  _errors = errors;
  resized ();
  repaint ();
}

void
ScriptPanel::setProtected (bool locked)
{
  if (_protected == locked)
    return;
  _protected = locked;
  repaint ();
}

void
ScriptPanel::setHasFile (bool hasFile)
{
  if (_hasFile == hasFile)
    return;
  _hasFile = hasFile;
  repaint ();
}

void
ScriptPanel::setSlotHolds (bool holds)
{
  if (_slotHolds == holds)
    return;
  _slotHolds = holds;
  repaint ();
}

void
ScriptPanel::setChannelColour (juce::Colour colour)
{
  if (_channelColour == colour)
    return;
  _channelColour = colour;
  dressEditor ();
  repaint ();
}

void
ScriptPanel::stopEditing ()
{
  if (!_editing)
    return;

  _editing = false;
  _editor->setReadOnly (true);
  if (onEditingChanged)
    onEditingChanged (false);

  repaint ();
}

void
ScriptPanel::flashKeys ()
{
  // Long enough to be seen, short enough not to be mistaken for a state.
  constexpr int flashMs = 350;
  _flashing = true;
  repaint ();
  juce::Component::SafePointer<ScriptPanel> self (this);
  juce::Timer::callAfterDelay (flashMs, [self] {
    if (self == nullptr)
      return;
    self->_flashing = false;
    self->repaint ();
  });
}

void
ScriptPanel::focusLost (FocusChangeType)
{
  // The keyboard follows the focus, so losing it is the end of the edit
  // whatever took it away.
  stopEditing ();
}

ScriptKeyStates
ScriptPanel::keys () const
{
  return scriptKeysFor (hasUnsavedChanges (), _protected, _hasFile,
                        _slotHolds);
}

juce::Font
ScriptPanel::scriptFont () const
{
  // Monospaced, because a script is read by column as much as by line: what
  // lines up under what is half of how you find your way in one.
  auto const size = juce::jlimit (
      9.f, 15.f, theme ().fontSize (FontRole::Body) * 0.8f);

  return juce::Font (juce::FontOptions (
      juce::Font::getDefaultMonospacedFontName (), size, juce::Font::plain));
}

int
ScriptPanel::scriptLineHeight () const
{
  return juce::jmax (1, juce::roundToInt (scriptFont ().getHeight () * 1.25f));
}

void
ScriptPanel::paintField (juce::Graphics &g)
{
  auto const bounds = _layout.textArea;
  if (bounds.isEmpty ())
    return;

  // Darker than the page and squared off, because this is a terminal and
  // reads as one: a script is text you scan line by line, not a control.
  g.setColour (toColour (theme ().background).darker (0.4f));
  g.fillRect (bounds);

  // The edge says whether it is being typed into and whether what is in it
  // has been written -- three states, one line, no words spent on any of it.
  auto const edited = hasUnsavedChanges ();
  g.setColour (edited     ? toColour (theme ().warning)
               : _editing ? _channelColour
                          : toColour (theme ().textPrimary,
                                      theme ().alphaOutline));
  g.drawRect (bounds, juce::roundToInt (_editing || edited
                                            ? theme ().strokeThick
                                            : theme ().strokeThin));

  // The text itself is the editor's (ScriptEditor), which stands inside this
  // frame and draws its own lines, numbers and caret.
  if (_document.getNumCharacters () == 0 && !_editing)
    {
      g.setColour (toColour (theme ().textMuted, theme ().alphaMuted));
      g.setFont (scriptFont ());
      g.drawText ("-- no script --", bounds.reduced (textInset ()),
                  juce::Justification::topLeft);
    }
}

void
ScriptPanel::paintErrors (juce::Graphics &g)
{
  if (_errors.isEmpty () || _layout.errorArea.isEmpty ())
    return;

  auto const lineH = scriptLineHeight ();
  auto at = _layout.errorArea;

  g.setFont (scriptFont ());
  g.setColour (toColour (theme ().danger));

  for (auto const &error : _errors)
    {
      g.drawText (error, at.removeFromTop (lineH),
                  juce::Justification::centredLeft);
      if (at.isEmpty ())
        break;
    }
}

void
ScriptPanel::paintKeys (juce::Graphics &g)
{
  auto const key = [&g] (juce::Rectangle<int> at, char const *word,
                         juce::Colour ink) {
    if (at.isEmpty ())
      return;
    g.setColour (toColour (theme ().textPrimary, theme ().alphaFill));
    g.fillRoundedRectangle (at.toFloat (), theme ().radiusControl);
    g.setColour (toColour (theme ().textPrimary, theme ().alphaOutline));
    g.drawRoundedRectangle (at.toFloat (), theme ().radiusControl,
                            theme ().strokeThin);

    g.setColour (ink);
    // What "save"/"cancel" may cost.
    constexpr float scriptKeyCap = 16.f;
    g.setFont (juce::Font (juce::FontOptions (
        fittedFontHeight (at.getHeight () * 0.4f, scriptKeyCap))));
    g.drawText (word, at, juce::Justification::centred);
  };

  auto const lit = readableInk (_channelColour, toColour (theme ().background),
                                toColour (theme ().textPrimary));
  auto const dark = toColour (theme ().textMuted, theme ().alphaDisabled);

  // Lit only while there is something to keep, to lose or to take: a key
  // offering to save nothing is a key you have to stop and think about.
  // Save stays dark on a protected script however much has been typed --
  // Save as is the way out, which is why it is lit in exactly that case.
  auto const k = keys ();
  // Flashing: the two ways out of an unsaved edit, in the warning colour the
  // field's edge already wears for it.
  auto const flash = toColour (theme ().warning);
  key (_layout.fromClipButton, _fromLabel.toRawUTF8 (),
       k.fromClip ? lit : dark);
  key (_layout.cancelButton, "cancel",
       _flashing  ? flash
       : k.cancel ? toColour (theme ().textPrimary, theme ().alphaTextStrong)
                  : dark);
  key (_layout.saveButton, "save", _flashing && k.save ? flash
                                   : k.save            ? lit
                                                       : dark);
  key (_layout.saveAsButton, "save as", k.saveAs ? lit : dark);
}

void
ScriptPanel::paint (juce::Graphics &g)
{
  paintField (g);
  paintErrors (g);
  paintKeys (g);
}

}
