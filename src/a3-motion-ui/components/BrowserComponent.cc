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

#include "BrowserComponent.hh"

#include <a3-motion-engine/ClipFile.hh>

#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

namespace a3
{

namespace
{
/** The library is longer than any list of rows, so the rows are a window onto
 *  it -- as many as fit, hit-sized, rather than all of them squeezed in. */
constexpr int maxVisibleRowTouches = 24;

// Sits between alphaInactive (0.6) and alphaTextStrong (0.85), further from
// either than the 0.05 a snap would tolerate. This is a row's name when it is
// not the chosen one -- muted, but less so than alphaInactive and more than
// alphaTextStrong would read. Listed in
// issues/a3-motion-ui-metric-role-deviations.md (Task 16) pending a decision
// on whether it becomes a rung of its own.
constexpr float unselectedRowNameOpacity = 0.7f;

// A one-pixel inset on the chosen row's fillRoundedRectangle highlight, not a
// stroke -- there is no stroke here to keep inside its bounds. No rung of the
// spacing scale carries a bare 1 (paddingTight is 2), and binding it to
// strokeThin would be wrong in a way that only shows up later: a skin that
// thickens the device's lines would silently grow this highlight's inset too,
// for no reason anyone could name. Left as its own literal pending a
// decision. Listed in issues/a3-motion-ui-metric-role-deviations.md
// (Task 16).
constexpr float selectedRowHighlightInset = 1.f;

// 0.10 from alphaTextStrong (0.85) and 0.15 from alphaInactive (0.6), too far
// from both to snap to either. This is the settings-preset dot on a row that
// is not the chosen one. Listed in
// issues/a3-motion-ui-metric-role-deviations.md (Task 16) pending a decision
// on whether it becomes a rung of its own.
constexpr float unselectedPresetDotOpacity = 0.75f;
}

BrowserComponent::BrowserComponent ()
    : _channelColour (toColour (theme ().accent))
{
  // Built once and given bounds when the layout says how many there are, the
  // same way the dropdown's entries are: a control created per repaint is a
  // control that loses the finger that is already on it.
  for (int i = 0; i < maxVisibleRowTouches; ++i)
    {
      auto row = std::make_unique<TouchControl> ();
      row->setIdentity (i);
      row->onTap = [this] (int index, int) {
        auto const entry = _scrollOffset + index;
        if (entry >= 0 && entry < _names.size () && onEntryChosen)
          onEntryChosen (entry);
      };
      // Dragging a row scrolls the list under it, and TouchControl only calls
      // onTap when nothing was dragged -- so the same finger browses and
      // chooses without a mode. There is no wheel on this device; a list that
      // could only be scrolled with one would be a list of six patterns out of
      // seventy.
      //
      // The list goes with the finger, like a phone and like the main menu:
      // pushing it up brings what is below into view, so a finger travelling
      // up (a positive increment) moves the window further down the library.
      row->onDragIncrement = [this] (int, int, int increment) {
        setScrollOffset (_scrollOffset + increment);
        if (onScrolled)
          onScrolled (increment);
      };
      row->setVisible (false);
      addChildComponent (*row);
      _rowTouch.push_back (std::move (row));
    }

  auto const makeButton
      = [this] (std::unique_ptr<TouchControl> &into,
                std::function<void ()> BrowserComponent::*callback) {
          into = std::make_unique<TouchControl> ();
          into->onTap = [this, callback] (int, int) {
            if (this->*callback)
              (this->*callback) ();
          };
          addAndMakeVisible (*into);
        };

  makeButton (_clipsTabTouch, &BrowserComponent::onClipsChosen);
  makeButton (_shapesTabTouch, &BrowserComponent::onShapesChosen);
  makeButton (_actionsTabTouch, &BrowserComponent::onActionsChosen);
  makeButton (_setsTabTouch, &BrowserComponent::onSetsChosen);
  makeButton (_filterTouch, &BrowserComponent::onFilterPressed);
  makeButton (_renameTouch, &BrowserComponent::onRenamePressed);
  makeButton (_saveTouch, &BrowserComponent::onSavePressed);
  makeButton (_saveAsTouch, &BrowserComponent::onSaveAsPressed);
  makeButton (_deleteTouch, &BrowserComponent::onDeletePressed);

  // The rename types into the row, so the row's own component has to be the
  // one holding the keys.
  setWantsKeyboardFocus (true);
}

BrowserComponent::~BrowserComponent () = default;

void
BrowserComponent::applyTheme ()
{
  resized ();
  repaint ();
}

void
BrowserComponent::resized ()
{
  _layout = layOutBrowser (getLocalBounds (), fingertipSize,
                           theme ().fontSize (FontRole::Body));

  for (size_t i = 0; i < _rowTouch.size (); ++i)
    {
      auto const shown = i < _layout.rows.size ()
                         && static_cast<int> (i) + _scrollOffset
                                < _names.size ();
      _rowTouch[i]->setVisible (shown);
      if (shown)
        _rowTouch[i]->setBounds (_layout.rows[i]);
    }

  _clipsTabTouch->setBounds (_layout.clipsTab);
  _shapesTabTouch->setBounds (_layout.shapesTab);
  _actionsTabTouch->setBounds (_layout.actionsTab);
  _setsTabTouch->setBounds (_layout.setsTab);
  _filterTouch->setBounds (_layout.filterButton);
  _renameTouch->setBounds (_layout.renameButton);
  _saveTouch->setBounds (_layout.saveButton);
  _saveAsTouch->setBounds (_layout.saveAsButton);
  _deleteTouch->setBounds (_layout.deleteButton);
}

void
BrowserComponent::setEntries (juce::StringArray const &names,
                              std::vector<bool> const &settingsOnly)
{
  if (names == _names && settingsOnly == _settingsOnly)
    return;

  _names = names;
  _settingsOnly = settingsOnly;
  _scrollOffset = juce::jlimit (
      0, juce::jmax (0, _names.size () - _layout.visibleRows), _scrollOffset);
  resized ();
  repaint ();
}

void
BrowserComponent::setScrollOffset (int firstRow)
{
  auto const clamped = juce::jlimit (
      0, juce::jmax (0, _names.size () - _layout.visibleRows), firstRow);
  if (clamped == _scrollOffset)
    return;

  _scrollOffset = clamped;
  resized ();
  repaint ();
}

void
BrowserComponent::setSelectedEntry (int index)
{
  if (index == _selectedEntry)
    return;

  _selectedEntry = index;

  // Brought into view if it is not already there. Only then: a tap on a row
  // you can see must not jerk the list, and scrolling to something already in
  // front of you is movement that says nothing.
  if (index >= 0 && _layout.visibleRows > 0)
    {
      if (index < _scrollOffset)
        setScrollOffset (index);
      else if (index >= _scrollOffset + _layout.visibleRows)
        setScrollOffset (index - _layout.visibleRows + 1);
    }

  repaint ();
}

void
BrowserComponent::mouseWheelMove (juce::MouseEvent const &,
                                  juce::MouseWheelDetails const &wheel)
{
  // The list follows the finger, like the main menu: pushing the list up
  // shows what is below it.
  auto const delta = wheel.deltaY > 0.f ? -1 : (wheel.deltaY < 0.f ? 1 : 0);
  if (delta == 0)
    return;

  setScrollOffset (_scrollOffset + delta);
  if (onScrolled)
    onScrolled (delta);
}

void
BrowserComponent::setShowingList (BrowserList list)
{
  if (list == _list)
    return;

  _list = list;
  repaint ();
}

void
BrowserComponent::paint (juce::Graphics &g)
{
  // Four words over the list: what a slot holds, the figures those are played
  // on, what ACT does to a slot, and the arrangement of all eight at once. All
  // four are chosen the same way, in the same place, so none of them is a mode
  // you have to remember being in.
  auto const paintListTab = [&g, this] (juce::Rectangle<int> bounds,
                                       juce::String const &label,
                                       bool active) {
    if (bounds.isEmpty ())
      return;

    g.setColour (active ? _channelColour.withAlpha (theme ().alphaInactive)
                        : toColour (theme ().textPrimary,
                                   theme ().alphaFill));
    g.fillRoundedRectangle (bounds.toFloat (), theme ().radiusControl);
    g.setColour (toColour (theme ().textPrimary,
                          active ? theme ().alphaDisabled
                                 : theme ().alphaOutline));
    g.drawRoundedRectangle (bounds.toFloat (), theme ().radiusControl,
                            theme ().strokeThin);

    g.setFont (juce::Font (juce::jmin (theme ().fontSize (FontRole::Body),
                                       bounds.getHeight () * 0.5f),
                           active ? juce::Font::bold : juce::Font::plain));
    // Full opacity for the active tab rather than an alpha rung: "active" has
    // always meant no dimming at all, which the alpha-less overload already
    // says. This used to be `active ? 1.f : 0.55f`; 1.f fits no rung, and the
    // maintainer still owes a call on whether full opacity deserves one of
    // its own. See issues/a3-motion-ui-metric-role-deviations.md (Task 16).
    g.setColour (active ? toColour (theme ().textPrimary)
                        : toColour (theme ().textPrimary,
                                   theme ().alphaInactive));
    g.drawFittedText (label, bounds, juce::Justification::centred, 1);
  };

  // Left to right in the order the work is done in: a set holds clips, a clip
  // holds a shape, and an action is what you reach for once all three are
  // standing.
  paintListTab (_layout.setsTab, "SETS", _list == BrowserList::Sessions);
  paintListTab (_layout.clipsTab, "CLIPS", _list == BrowserList::Clips);
  paintListTab (_layout.shapesTab, "SVG", _list == BrowserList::Shapes);
  paintListTab (_layout.actionsTab, "ACTIONS",
                _list == BrowserList::Actions);

  g.setColour (toColour (theme ().surface, theme ().alphaMuted));
  g.fillRoundedRectangle (_layout.listArea.toFloat (), theme ().radiusControl);

  for (int row = 0; row < static_cast<int> (_layout.rows.size ()); ++row)
    paintRow (g, row);

  paintButton (g, _layout.filterButton, _actionLabels[0], _actionEnabled[0]);
  paintButton (g, _layout.renameButton, _actionLabels[1], _actionEnabled[1]);
  paintButton (g, _layout.saveButton, _actionLabels[2], _actionEnabled[2]);
  paintButton (g, _layout.saveAsButton, _actionLabels[3], _actionEnabled[3]);
  paintButton (g, _layout.deleteButton, _actionLabels[4], _actionEnabled[4]);
}

void
BrowserComponent::paintRow (juce::Graphics &g, int row)
{
  auto const entry = _scrollOffset + row;
  if (entry < 0 || entry >= _names.size ())
    return;

  auto const bounds = _layout.rows[static_cast<size_t> (row)];
  auto const chosen = entry == _selectedEntry;
  auto const editing = chosen && _renaming;

  if (chosen)
    {
      g.setColour (_channelColour.withAlpha (theme ().alphaFillEmphasis));
      g.fillRoundedRectangle (
          bounds.toFloat ().reduced (selectedRowHighlightInset),
          theme ().radiusControl);
    }

  // Being typed into is a state of the row, so the row says so: an edge round
  // it, in the colour the rest of the bar uses for something unsaved.
  if (editing)
    {
      g.setColour (toColour (theme ().warning));
      g.drawRoundedRectangle (bounds.toFloat ().reduced (theme ().strokeThin),
                              theme ().radiusControl, theme ().strokeThick);
    }

  auto const text = bounds.reduced (bounds.getHeight () / 3, 0);
  auto const font = juce::Font (juce::jmin (theme ().fontSize (FontRole::Body),
                                            bounds.getHeight () * 0.55f),
                                juce::Font::plain);
  g.setFont (font);
  // Full opacity for the chosen row rather than an alpha rung, the same
  // restructuring as paintListTab() above; this used to be
  // `chosen ? 1.f : 0.7f`. 0.7f itself fits no rung either -- 0.10 from
  // alphaInactive, 0.15 from alphaTextStrong, too far from both -- so it
  // keeps its own name (unselectedRowNameOpacity) rather than snapping. See
  // issues/a3-motion-ui-metric-role-deviations.md (Task 16).
  g.setColour (chosen ? toColour (theme ().textPrimary)
                      : toColour (theme ().textPrimary,
                                 unselectedRowNameOpacity));
  g.drawFittedText (editing ? _renameText : _names[entry], text,
                    juce::Justification::centredLeft, 1);

  // The caret, where the next character goes. At the end of the text and
  // nowhere else: a name is short enough to retype, and a caret you can move
  // is a caret you have to be able to see moving.
  if (editing)
    {
      auto const width
          = juce::GlyphArrangement::getStringWidth (font, _renameText);
      auto const x = juce::jmin (text.getRight () - 1.f,
                                 text.getX () + width + 1.f);

      g.setColour (toColour (theme ().warning));
      g.fillRect (x, static_cast<float> (text.getY () + text.getHeight () / 6),
                  1.5f, static_cast<float> (text.getHeight () * 2 / 3));

      // Nothing else belongs on a row being typed into -- the preset dot is
      // about the file, and while it is being renamed it is about the name.
      return;
    }

  // A settings preset carries a dot on the right. A mark rather than a colour
  // because the selection already owns the accent, and a mark rather than a
  // word because the row is read at a glance or not at all.
  auto const index = static_cast<size_t> (entry);
  if (index < _settingsOnly.size () && _settingsOnly[index])
    {
      auto const dot = bounds.getHeight () / 5.f;
      // Same restructuring as above for the chosen case. The unselected
      // branch does not snap to a rung: see unselectedPresetDotOpacity above.
      g.setColour (chosen ? toColour (theme ().accent)
                          : toColour (theme ().accent,
                                     unselectedPresetDotOpacity));
      g.fillEllipse (bounds.getRight () - dot * 2.5f,
                     bounds.getCentreY () - dot / 2.f, dot, dot);
    }
}

void
BrowserComponent::setActions (juce::StringArray const &labels,
                              std::array<bool, 5> const &enabled)
{
  if (labels == _actionLabels && enabled == _actionEnabled)
    return;

  _actionLabels = labels;
  _actionEnabled = enabled;
  repaint ();
}

void
BrowserComponent::paintButton (juce::Graphics &g, juce::Rectangle<int> bounds,
                               juce::String const &label, bool enabled)
{
  // An empty label is a key that does not exist yet -- nothing is drawn at
  // all, rather than an outline with nothing in it, which reads as a fault.
  if (bounds.isEmpty () || label.isEmpty ())
    return;

  g.setColour (toColour (theme ().textPrimary, theme ().alphaFill));
  g.fillRoundedRectangle (bounds.toFloat (), theme ().radiusControl);
  g.setColour (toColour (theme ().textPrimary, theme ().alphaOutline));
  g.drawRoundedRectangle (bounds.toFloat (), theme ().radiusControl,
                          theme ().strokeThin);

  g.setFont (juce::Font (juce::jmin (theme ().fontSize (FontRole::Body),
                                     bounds.getHeight () * 0.5f),
                         juce::Font::plain));
  g.setColour (enabled ? toColour (theme ().textPrimary,
                                   theme ().alphaTextStrong)
                       : toColour (theme ().textPrimary,
                                  theme ().alphaFillEmphasis));
  g.drawFittedText (label, bounds, juce::Justification::centred, 1);
}


void
BrowserComponent::beginRename (juce::String const &name)
{
  _renaming = true;
  _renameText = name;
  _renameWas = name;

  grabKeyboardFocus ();
  if (onRenameEditingChanged)
    onRenameEditingChanged (true);

  repaint ();
}

void
BrowserComponent::cancelRename ()
{
  endRename (false);
}

void
BrowserComponent::commitRename ()
{
  endRename (true);
}

void
BrowserComponent::endRename (bool keep)
{
  if (!_renaming)
    return;

  _renaming = false;

  // Read before the callback runs: whatever it does may come back through
  // here, and a name half torn down is a name that arrives empty.
  auto const wanted = _renameText.trim ();
  auto const changed = keep && wanted.isNotEmpty () && wanted != _renameWas;

  _renameText = {};
  _renameWas = {};

  if (onRenameEditingChanged)
    onRenameEditingChanged (false);

  if (changed && onRenamed)
    onRenamed (wanted);

  repaint ();
}

bool
BrowserComponent::keyPressed (juce::KeyPress const &key)
{
  if (!_renaming)
    return false;

  if (key == juce::KeyPress::escapeKey)
    {
      endRename (false);
      return true;
    }

  if (key == juce::KeyPress::returnKey)
    {
      endRename (true);
      return true;
    }

  if (key == juce::KeyPress::backspaceKey)
    {
      _renameText = _renameText.dropLastCharacters (1);
      repaint ();
      return true;
    }

  // What a name may carry is the engine's rule, not this component's -- see
  // nameCharacterIsAllowed(), where it is written down once and tested.
  auto const character = key.getTextCharacter ();
  if (!nameCharacterIsAllowed (character)
      || _renameText.length () >= maxTypedNameLength)
    return true;

  _renameText += juce::String::charToString (character);
  repaint ();
  return true;
}

void
BrowserComponent::focusLost (FocusChangeType)
{
  // The keyboard follows the focus, so losing it ends the edit whatever took
  // it away -- and ends it by keeping nothing, because walking away from a
  // half-typed name is not a way of asking for it.
  endRename (false);
}



void
BrowserComponent::setChannelColour (juce::Colour colour)
{
  if (colour == _channelColour)
    return;

  _channelColour = colour;
  repaint ();
}

}
