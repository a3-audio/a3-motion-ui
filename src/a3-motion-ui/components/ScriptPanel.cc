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

  // The editor's frame: it cuts off the empty room JUCE leaves left of the
  // line numbers, and passes every touch through to the editor in it.
  _frame = std::make_unique<juce::Component> ();
  _frame->setInterceptsMouseClicks (false, true);
  addAndMakeVisible (*_frame);
  _document.addListener (this);

  buildEditor ();

}

ScriptPanel::~ScriptPanel () { _document.removeListener (this); }

void
ScriptPanel::buildEditor ()
{
  if (_editor)
    _frame->removeChildComponent (_editor.get ());

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
  _frame->addAndMakeVisible (*_editor);
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
  notifyKeys ();
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
  _layout = layOutScriptPanel (getLocalBounds (), _errors.size (),
                               scriptLineHeight ());
  _frame->setBounds (_layout.textArea.reduced (textInset ()));
  auto const trim = gutterTrim ();
  _editor->setBounds (-trim, 0, _frame->getWidth () + trim,
                      _frame->getHeight ());
}

int
ScriptPanel::gutterTrim () const
{
  // JUCE draws the numbers right-aligned in its gutter less two pixels, at
  // most 13 px tall (CodeEditorComponent::GutterComponent::paint). Three
  // digits and a little air are what they need; the rest is cut.
  constexpr float juceGutterText = 33.f;
  constexpr float juceNumberCap = 13.f;
  auto const numbers = scriptFont ().withHeight (
      juce::jmin (juceNumberCap, scriptFont ().getHeight () * 0.8f));
  auto const needed = juce::GlyphArrangement::getStringWidth (numbers, "999");
  auto const air = numbers.getHeight () / 2.f;
  return juce::jmax (0, static_cast<int> (juceGutterText - needed - air));
}

int
ScriptPanel::widthFor (int characters) const
{
  return widthAt (scriptFont ().getHeight (), characters);
}

int
ScriptPanel::usualWidthFor (int characters) const
{
  return widthAt (usualFontSize (), characters);
}

int
ScriptPanel::widthAt (float fontSize, int characters) const
{
  // Measured the way JUCE's editor measures itself, or the promise is a
  // guess: a character is the width of "0" (CodeEditorComponent keeps
  // charWidth from TextLayout::getStringWidth), the line numbers take a fixed
  // gutter (getGutterSize), and the scroll bar is as thick as dressEditor()
  // makes it.
  constexpr int juceGutter = 35;
  juce::Font const font (juce::FontOptions (
      juce::Font::getDefaultMonospacedFontName (), fontSize,
      juce::Font::plain));
  auto const charW = juce::TextLayout::getStringWidth (font, "0");
  // One character to spare, so the last one is never under the bar's edge.
  auto const text = charW * static_cast<float> (characters + 1);
  auto const bar = static_cast<float> (
      juce::jmax (1, juce::roundToInt (font.getHeight () / 2.f)));
  // The frame cuts off the gutter's empty room (gutterTrim), measured at the
  // same font.
  return static_cast<int> (std::ceil (juceGutter + text + bar))
         - gutterTrim () + 2 * textInset ()
         + 2 * juce::roundToInt (theme ().strokeThick);
}

int
ScriptPanel::textInset () const
{
  // A fixed pad, not a share of the height: over the sphere a fortieth of it
  // came to fifteen pixels a side, which is five characters of a line.
  return juce::roundToInt (theme ().paddingSmall);
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

void
ScriptPanel::applyEdit (juce::String const &text)
{
  // ACTION wrote into the script this shows (2026-09-29). In even while it
  // is being typed into -- the edit is to one line, and what was typed stays
  // because the host applied the same line change to this very text.
  if (text == _document.getAllContent ())
    return;
  auto const wasSaved = !hasUnsavedChanges ();
  auto const caret = _editor != nullptr
                         ? _editor->getCaretPos ().getLineNumber ()
                         : 0;
  _document.replaceAllContent (text);
  if (_editor != nullptr)
    _editor->moveCaretTo (juce::CodeDocument::Position (_document, caret, 0), false);
  if (wasSaved)
    _document.setSavePoint ();
  repaint ();
  notifyKeys ();
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
  notifyKeys ();
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
  notifyKeys ();
}

void
ScriptPanel::setHasFile (bool hasFile)
{
  if (_hasFile == hasFile)
    return;
  _hasFile = hasFile;
  repaint ();
  notifyKeys ();
}

void
ScriptPanel::setSlotHolds (bool holds)
{
  if (_slotHolds == holds)
    return;
  _slotHolds = holds;
  repaint ();
  notifyKeys ();
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
  notifyKeys ();
  juce::Component::SafePointer<ScriptPanel> self (this);
  juce::Timer::callAfterDelay (flashMs, [self] {
    if (self == nullptr)
      return;
    self->_flashing = false;
    self->notifyKeys ();
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

// Each key asks the rule it is lit by (scriptKeysFor), so a dark key is also
// a dead one: a save that depends on a key having been dark happens the first
// time something else lights it.
void
ScriptPanel::pressFromClip ()
{
  if (keys ().fromClip && onFromClip)
    onFromClip ();
}

void
ScriptPanel::pressCancel ()
{
  stopEditing ();
  if (onCancel)
    onCancel ();
  repaint ();
}

void
ScriptPanel::pressSave ()
{
  if (!keys ().save)
    return;
  stopEditing ();
  if (onSave)
    onSave ();
  repaint ();
}

void
ScriptPanel::pressSaveAs ()
{
  if (!keys ().saveAs)
    return;
  stopEditing ();
  if (onSaveAs)
    onSaveAs ();
  repaint ();
}

void
ScriptPanel::notifyKeys ()
{
  if (onKeysChanged)
    onKeysChanged ();
}

void
ScriptPanel::codeDocumentTextInserted (juce::String const &, int)
{
  notifyKeys ();
  repaint ();
}

void
ScriptPanel::codeDocumentTextDeleted (int, int)
{
  notifyKeys ();
  repaint ();
}

float
ScriptPanel::usualFontSize () const
{
  // The list's size beside it (maintainer, 2026-09-27: "die schriftgröße vom
  // editor soll gleich sein"); a line longer than the column scrolls.
  return theme ().fontSize (FontRole::Body);
}

juce::Font
ScriptPanel::scriptFont () const
{
  // Monospaced, because a script is read by column as much as by line: what
  // lines up under what is half of how you find your way in one.
  auto const size = usualFontSize ();

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
ScriptPanel::paint (juce::Graphics &g)
{
  paintField (g);
  paintErrors (g);
}

}
