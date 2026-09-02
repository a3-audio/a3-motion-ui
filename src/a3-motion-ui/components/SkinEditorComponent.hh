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

#include <JuceHeader.h>

#include <a3-motion-ui/theme/TextInput.hh>
#include <a3-motion-ui/theme/SkinParameters.hh>
#include <a3-motion-ui/components/TouchControl.hh>

#include <memory>

#include <functional>
#include <vector>

namespace a3
{

/** How tall the typing field is. Sized from the text it holds, not from a
 *  count of list rows: a fixed two rows left a large field around a small
 *  line of text, with the caret stranded well below it. */
int typingFieldHeight (float textFontSize, int rowHeight);

/** Where the caret's underline sits — just below the text, which is drawn
 *  centred in the field. Pinned to the field's bottom edge instead, it drifted
 *  further from the text the taller the field became. */
float typingCaretY (juce::Rectangle<int> field, juce::Font const &font);

/**
 * SkinEditorComponent
 *
 * Every number in the active skin, in one scrolling list, editable with the
 * same encoder that drives the settings menu: turning browses the list,
 * pressing arms the row, turning then changes its value, pressing again
 * lets go. It is a page of the Global Settings menu, not a window of its
 * own — and like that menu it is a child of MotionComponent, so the sphere
 * it is changing stays visible behind it.
 *
 * The list is derived from the file (see skinParameters()), not from a
 * hand-written catalogue: a key added to a skin shows up here without
 * anyone remembering to register it, and one removed stops being offered.
 * That also means the list is long — a couple of dozen colour channels and
 * roughly eighty tuning numbers — so only a window of rows around the
 * selected one is drawn.
 *
 * The component holds the edited skin and hands it back; who applies it and
 * who writes it to disk is the caller's business.
 */
class SkinEditorComponent : public juce::Component
{
public:
  SkinEditorComponent ();

  /** The skin to edit, and the name to show above it. Skins get the three
   *  actions on top; see setDocument for anything else. */
  void setSkin (juce::var skin, juce::String const &name);

  /** What pressing the encoder on a number row does.
   *
   *  A skin value is `Turned`: it is dialled while the sphere it changes is in
   *  view, which is the reason to edit a skin on the device at all. A config
   *  value is `Typed`: a port is a number somebody knows, and turning an
   *  encoder across ten thousand of them is not a way to reach it. Text and
   *  colour rows ignore this — they were always typed and picked. */
  enum class Numbers
  {
    Turned,
    Typed,
  };

  /** Any other document — a slice of config.json, say. Same list, same two
   *  encoder levels, no skin actions. */
  void setDocument (juce::var document, juce::String const &title,
                    bool withSkinActions, Numbers numbers);
  juce::var const &getSkin () const { return _skin; }
  juce::String const &getSkinName () const { return _name; }

  /** What the browsed row does. The three actions sit above the parameters,
   *  where they are reached first and cannot be turned past by accident. */
  enum class Row
  {
    Save,
    SaveAsNew,
    Rename,
    Delete,
    /** Put every value back to what the shipped default holds, keeping the
     *  skin's own name. The way out of a skin dialled into a corner. */
    Reset,
    Parameter,
  };

  /** Turn the encoder: browse the list, or change what the armed row holds. */
  void navigate (int delta);

  /** Name a row outright, which is what a tap does — the encoder has to turn
   *  past everything in between. Lets an armed row go, like browsing with the
   *  encoder does. An index outside the list is ignored. */
  void browseRow (int index);
  int browsedRowIndex () const { return _index; }

  /** The window of rows currently drawn. The list is far longer than the
   *  panel, so only these carry hit areas. */
  int firstVisibleRow () const;
  int visibleRowCount () const { return visibleRows (); }

  /** Press the encoder: arm the browsed row, or let it go again. On an
   *  action row this is what asks for it — the caller decides whether it
   *  happens, and tells this component the outcome via setSkin(). */
  void toggleEditing ();
  bool isEditing () const { return _editing; }

  Row browsedRow () const;

  /** Asked when an action row is pressed. Delete asks twice: the row says so
   *  in between. */
  std::function<void ()> onSave;
  std::function<void ()> onSaveAsNew;
  std::function<void ()> onDelete;
  std::function<void ()> onReset;

  /** Asked when a rename is finished, with the typed name. */
  std::function<void (juce::String const &)> onRename;

  /** Asked when a colour row is pressed, with its path — the caller owns
   *  the picker page. */
  std::function<void (juce::String const &)> onColourPicked;

  /** Asked when a name opens or closes, so the keyboard can come up by
   *  itself — nobody starts typing a name and then goes looking for it. */
  std::function<void (bool)> onNamingChanged;

  /** True while a name is being typed; the caller's Menu button finishes it
   *  rather than leaving the editor. */
  bool isNaming () const { return _naming; }
  void finishNaming ();

  /** The touchscreen's way into whatever is being typed. */
  void typeIntoName (juce::juce_wchar character);
  void backspaceName ();

  /** Start typing the browsed row — a number as much as a name. Returns
   *  false when the row is not something that can be typed, which is what
   *  greys the keyboard icon out. */
  bool beginTypingBrowsedRow ();
  bool canTypeBrowsedRow () const;

  /** The path of the browsed parameter row, empty on an action row. */
  juce::String browsedPath () const;

  /** Whether the browsed row is one a drag can turn — a plain number in a
   *  document whose numbers are turned. An action row acts, a colour row
   *  opens a picker and a text row opens the keyboard; none of those may be
   *  set off by a finger that is only dragging past them. */
  bool canTurnBrowsedRow () const;

  /** What is currently in the typing field. */
  juce::String typedText () const { return _nameEntry.buffer (); }

  /** Called whenever a value changed, so the caller can put the edited skin
   *  in force straight away — seeing the change is the whole point of
   *  editing on the device. */
  std::function<void ()> onValueChanged;

  /** Real key events, from the system's on-screen keyboard or a plugged-in
   *  one — the app draws no keyboard of its own any more. */
  bool keyPressed (juce::KeyPress const &key) override;

  void paint (juce::Graphics &g) override;
  void resized () override;

private:
  /** The panel, and a row inside it by its slot in the drawn window. One
   *  calculation for the picture and for the hit areas. */
  juce::Rectangle<int> listPanelBounds () const;
  juce::Rectangle<int> listContentBounds () const;
  juce::Rectangle<int> visibleRowBounds (int slot) const;
  /** A row's two halves. The split follows the value's length, so it needs
   *  the row's index and cannot be a free function. */
  juce::Rectangle<int> rowValueArea (juce::Rectangle<int> row,
                                     int absoluteIndex) const;
  juce::Rectangle<int> rowNameArea (juce::Rectangle<int> row,
                                    int absoluteIndex) const;

  /** One pair of hit areas per drawn row, plus one behind the whole list
   *  that a drag scrolls with. Created once; resized() re-places them and
   *  re-labels them with the absolute row they currently show. */
  struct RowTouch
  {
    std::unique_ptr<TouchControl> name;
    std::unique_ptr<TouchControl> value;
  };
  std::vector<RowTouch> _rowTouch;

  /** The absolute row a drag started on, or -1.
   *
   *  A drag has to stay on the row it began on. browseRow() moves the window
   *  of drawn rows, resized() then re-labels the hit areas with their new
   *  absolute rows — and the area under the finger comes to stand for a
   *  different row mid-drag. Following it turned "drag the value up" into
   *  "arm whatever scrolled under your finger", which on a colour row opened
   *  the picker. */
  int _dragRow = -1;
  std::unique_ptr<TouchControl> _listScroll;

  void createTouchControls ();

  /** How many rows fit, given the height this page was handed. */
  int visibleRows () const;

  /** How many rows the actions take before the parameters begin — three for
   *  a skin, none for anything else. */
  int _actionRows = 3;

  int totalRows () const;
  juce::String rowLabel (int index) const;
  juce::String rowValue (int index) const;

  juce::var _skin;
  juce::String _name;
  std::vector<SkinParameter> _parameters;
  int _index = 0;
  bool _editing = false;
  bool _naming = false;
  bool _deleteAsked = false;
  /** Shown briefly on the Save row, so a press that writes a file says so. */
  bool _saved = false;
  TextInput _nameEntry;
  /** Set while a text parameter, rather than a skin name, is being typed. */
  juce::String _textPath;
  bool _typingNumber = false;
  Numbers _numbers = Numbers::Turned;
};

}
