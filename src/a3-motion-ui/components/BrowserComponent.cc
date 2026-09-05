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

#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

namespace a3
{

namespace
{
/** The library is longer than any list of rows, so the rows are a window onto
 *  it -- as many as fit, hit-sized, rather than all of them squeezed in. */
constexpr int maxVisibleRowTouches = 24;
}

BrowserComponent::BrowserComponent ()
{
  for (index_t channel = 0; channel < numChannelColumns; ++channel)
    for (index_t slot = 0; slot < numPadSlots; ++slot)
      {
        auto touch = std::make_unique<TouchControl> ();
        touch->setIdentity (static_cast<int> (channel),
                            static_cast<int> (slot));
        touch->onTap = [this] (int tappedChannel, int tappedSlot) {
          if (onFieldChosen)
            onFieldChosen (static_cast<index_t> (tappedChannel),
                           static_cast<index_t> (tappedSlot));
        };
        addAndMakeVisible (*touch);
        _fieldTouch[channel][slot] = std::move (touch);
      }

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

  makeButton (_sessionTouch, &BrowserComponent::onSessionPressed);
  makeButton (_clipsTabTouch, &BrowserComponent::onClipsChosen);
  makeButton (_actionsTabTouch, &BrowserComponent::onActionsChosen);
  makeButton (_renameTouch, &BrowserComponent::onRenamePressed);
  makeButton (_saveTouch, &BrowserComponent::onSavePressed);
  makeButton (_loadTouch, &BrowserComponent::onLoadSessionPressed);
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

  for (index_t channel = 0; channel < numChannelColumns; ++channel)
    for (index_t slot = 0; slot < numPadSlots; ++slot)
      _fieldTouch[channel][slot]->setBounds (_layout.fields[channel][slot]);

  for (size_t i = 0; i < _rowTouch.size (); ++i)
    {
      auto const shown = i < _layout.rows.size ()
                         && static_cast<int> (i) + _scrollOffset
                                < _names.size ();
      _rowTouch[i]->setVisible (shown);
      if (shown)
        _rowTouch[i]->setBounds (_layout.rows[i]);
    }

  _sessionTouch->setBounds (_layout.sessionField);
  _clipsTabTouch->setBounds (_layout.clipsTab);
  _actionsTabTouch->setBounds (_layout.actionsTab);
  _renameTouch->setBounds (_layout.renameButton);
  _saveTouch->setBounds (_layout.saveSessionButton);
  _loadTouch->setBounds (_layout.loadSessionButton);
}

void
BrowserComponent::setField (index_t channel, index_t slot,
                            juce::String const &name, juce::Colour colour)
{
  if (channel >= numChannelColumns || slot >= numPadSlots)
    return;
  if (_fieldNames[channel][slot] == name
      && _fieldColours[channel][slot] == colour)
    return;

  _fieldNames[channel][slot] = name;
  _fieldColours[channel][slot] = colour;
  repaint (_layout.fields[channel][slot]);
}

void
BrowserComponent::setSessionName (juce::String const &name)
{
  if (name == _sessionName)
    return;

  _sessionName = name;
  repaint (_layout.sessionField);
}

void
BrowserComponent::setFieldDrifted (index_t channel, index_t slot,
                                   bool drifted)
{
  if (channel >= numChannelColumns || slot >= numPadSlots)
    return;
  if (drifted == _fieldDrifted[channel][slot])
    return;

  _fieldDrifted[channel][slot] = drifted;
  repaint (_layout.fields[channel][slot]);
}

void
BrowserComponent::setSelectedField (int channel, int slot)
{
  if (channel == _selectedChannel && slot == _selectedSlot)
    return;

  _selectedChannel = channel;
  _selectedSlot = slot;
  repaint ();
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
BrowserComponent::setShowingActions (bool actions)
{
  if (actions == _showingActions)
    return;

  _showingActions = actions;
  repaint ();
}

void
BrowserComponent::paint (juce::Graphics &g)
{
  // Two words over the list: what a slot holds, and what ACT does to it. Both
  // are chosen the same way, in the same place, so neither is a mode you have
  // to remember being in.
  auto const paintListTab = [&g] (juce::Rectangle<int> bounds,
                                  juce::String const &label, bool active) {
    if (bounds.isEmpty ())
      return;

    g.setColour (active ? toColour (theme ().accent, 0.35f)
                        : toColour (theme ().textPrimary, 0.06f));
    g.fillRoundedRectangle (bounds.toFloat (), 3.f);
    g.setColour (toColour (theme ().textPrimary, active ? 0.35f : 0.15f));
    g.drawRoundedRectangle (bounds.toFloat (), 3.f, 1.f);

    g.setFont (juce::Font (juce::jmin (theme ().fontSize (FontRole::Body),
                                       bounds.getHeight () * 0.5f),
                           active ? juce::Font::bold : juce::Font::plain));
    g.setColour (toColour (theme ().textPrimary, active ? 1.f : 0.55f));
    g.drawFittedText (label, bounds, juce::Justification::centred, 1);
  };

  paintListTab (_layout.clipsTab, "CLIPS", !_showingActions);
  paintListTab (_layout.actionsTab, "ACTION", _showingActions);

  // The set, over the eight clips it filled.
  {
    auto const bounds = _layout.sessionField;
    if (!bounds.isEmpty ())
      {
        g.setColour (toColour (theme ().textPrimary, 0.06f));
        g.fillRoundedRectangle (bounds.toFloat (), 3.f);
        g.setColour (toColour (theme ().notice, 0.4f));
        g.drawRoundedRectangle (bounds.toFloat (), 3.f, 1.f);

        g.setFont (juce::Font (
            juce::jmin (theme ().fontSize (FontRole::Body),
                        bounds.getHeight () * 0.5f),
            juce::Font::plain));
        g.setColour (toColour (theme ().notice));
        g.drawFittedText (_sessionName.isEmpty ()
                              ? juce::String ("Set: none")
                              : "Set: " + _sessionName,
                          bounds.reduced (bounds.getHeight () / 3, 0),
                          juce::Justification::centredLeft, 1);
      }
  }

  for (index_t channel = 0; channel < numChannelColumns; ++channel)
    for (index_t slot = 0; slot < numPadSlots; ++slot)
      paintField (g, channel, slot);

  g.setColour (toColour (theme ().surface, 0.5f));
  g.fillRoundedRectangle (_layout.listArea.toFloat (), 3.f);

  for (int row = 0; row < static_cast<int> (_layout.rows.size ()); ++row)
    paintRow (g, row);

  paintButton (g, _layout.renameButton, _actionLabels[0], _actionEnabled[0]);
  paintButton (g, _layout.saveSessionButton, _actionLabels[1],
               _actionEnabled[1]);
  paintButton (g, _layout.loadSessionButton, _actionLabels[2],
               _actionEnabled[2]);
}

void
BrowserComponent::paintField (juce::Graphics &g, index_t channel,
                              index_t slot)
{
  auto const bounds = _layout.fields[channel][slot];
  if (bounds.isEmpty ())
    return;

  auto const chosen = static_cast<int> (channel) == _selectedChannel
                      && static_cast<int> (slot) == _selectedSlot;

  // Until the page is opened nobody has said which channel this is, and a
  // colour invented here would be one the skin cannot reach.
  auto const stored = _fieldColours[channel][slot];
  auto const colour = stored.isTransparent () ? toColour (theme ().textMuted)
                                              : stored;

  // The chosen one is filled: it is where the next clip you touch will go, and
  // that has to be answerable at a glance rather than by remembering what you
  // last pressed.
  g.setColour (chosen ? colour.withAlpha (0.45f) : colour.withAlpha (0.12f));
  g.fillRoundedRectangle (bounds.toFloat (), 3.f);
  g.setColour (chosen ? colour : toColour (theme ().textPrimary, 0.15f));
  g.drawRoundedRectangle (bounds.toFloat (), 3.f, chosen ? 2.f : 1.f);

  auto text = bounds.reduced (bounds.getWidth () / 12, 2);
  auto const heading = text.removeFromTop (text.getHeight () / 3);

  g.setFont (juce::Font (juce::jmin (theme ().fontSize (FontRole::Body),
                                     heading.getHeight () * 0.9f),
                         juce::Font::plain));
  g.setColour (colour);
  g.drawFittedText (juce::String (channel + 1) + "." + juce::String (slot + 1),
                    heading, juce::Justification::centredLeft, 1);

  g.setFont (juce::Font (juce::jmin (theme ().fontSize (FontRole::Body),
                                     text.getHeight () * 0.6f),
                         juce::Font::plain));
  g.setColour (toColour (theme ().textPrimary,
                         _fieldNames[channel][slot].isEmpty () ? 0.3f : 0.9f));
  g.drawFittedText (_fieldNames[channel][slot].isEmpty ()
                        ? juce::String ("empty")
                        : _fieldNames[channel][slot],
                    text, juce::Justification::centredLeft, 1);

  if (_fieldDrifted[channel][slot])
    {
      g.setColour (toColour (theme ().warning));
      g.fillEllipse (driftMark (bounds).toFloat ());
    }
}

void
BrowserComponent::paintRow (juce::Graphics &g, int row)
{
  auto const entry = _scrollOffset + row;
  if (entry < 0 || entry >= _names.size ())
    return;

  auto const bounds = _layout.rows[static_cast<size_t> (row)];
  auto const chosen = entry == _selectedEntry;

  if (chosen)
    {
      g.setColour (toColour (theme ().accent, 0.25f));
      g.fillRoundedRectangle (bounds.toFloat ().reduced (1.f), 3.f);
    }

  g.setFont (juce::Font (juce::jmin (theme ().fontSize (FontRole::Body),
                                     bounds.getHeight () * 0.55f),
                         juce::Font::plain));
  g.setColour (toColour (theme ().textPrimary, chosen ? 1.f : 0.7f));
  g.drawFittedText (_names[entry], bounds.reduced (bounds.getHeight () / 3, 0),
                    juce::Justification::centredLeft, 1);

  // A settings preset carries a dot on the right. A mark rather than a colour
  // because the selection already owns the accent, and a mark rather than a
  // word because the row is read at a glance or not at all.
  auto const index = static_cast<size_t> (entry);
  if (index < _settingsOnly.size () && _settingsOnly[index])
    {
      auto const dot = bounds.getHeight () / 5.f;
      g.setColour (toColour (theme ().accent, chosen ? 1.f : 0.75f));
      g.fillEllipse (bounds.getRight () - dot * 2.5f,
                     bounds.getCentreY () - dot / 2.f, dot, dot);
    }
}

void
BrowserComponent::setActions (juce::StringArray const &labels,
                              std::array<bool, 3> const &enabled)
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

  g.setColour (toColour (theme ().textPrimary, 0.06f));
  g.fillRoundedRectangle (bounds.toFloat (), 3.f);
  g.setColour (toColour (theme ().textPrimary, 0.15f));
  g.drawRoundedRectangle (bounds.toFloat (), 3.f, 1.f);

  g.setFont (juce::Font (juce::jmin (theme ().fontSize (FontRole::Body),
                                     bounds.getHeight () * 0.5f),
                         juce::Font::plain));
  g.setColour (toColour (theme ().textPrimary, enabled ? 0.9f : 0.3f));
  g.drawFittedText (label, bounds, juce::Justification::centred, 1);
}

}
