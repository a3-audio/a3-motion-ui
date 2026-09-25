/*

  A3 Motion UI
  Copyright (C) 2023 Patric Schmitz

*/

#include "GlobalSettingsComponent.hh"

#include <a3-motion-ui/components/ListScroll.hh>

#include <a3-motion-ui/theme/Theme.hh>

namespace a3
{

namespace
{
// Structural washes: the overlay over the panel behind it, and the three
// depths a settings row can have. State — browsed, inactive — comes from the
// theme's alphas instead.
// See-through on purpose: the menu is where a skin is chosen and edited,
// and the sphere behind it is most of what a skin actually changes.
constexpr float rowWash = 0.063f;
constexpr float browsedRowWash = 0.086f;
constexpr float armedRowWash = 0.133f;
constexpr float armedFrameWash = 0.18f;
constexpr float rowFrameWash = 0.08f;

// The panel's proportions. One copy, read by the drawing and by the hit
// areas alike.
constexpr int maxPanelW = 760;
constexpr int itemH = 52;
constexpr int rowGap = 6;
constexpr int paddingV = 24;
constexpr int paddingH = 32;
}

int
globalSettingsSideZoneWidth (juce::Rectangle<int> bounds)
{
  // A fifth of the width on each side, and never less than the padding the
  // panel used to have. These are drag zones, not margins — a finger has to
  // land in one, and 32px is narrower than a fingertip.
  return juce::jmax (paddingH, bounds.getWidth () / 5);
}

juce::Rectangle<int>
globalSettingsPanelBounds (juce::Rectangle<int> bounds, int numOptions)
{
  auto const panelW
      = juce::jmin (maxPanelW,
                    bounds.getWidth () - 2 * globalSettingsSideZoneWidth (bounds));
  auto const panelH
      = paddingV * 2 + numOptions * itemH + (numOptions - 1) * rowGap;

  return juce::Rectangle<int> ((bounds.getWidth () - panelW) / 2,
                               (bounds.getHeight () - panelH) / 2, panelW,
                               panelH);
}

juce::Rectangle<int>
globalSettingsRowBounds (juce::Rectangle<int> panel, int numOptions, int index)
{
  juce::ignoreUnused (numOptions);

  auto const rows = panel.reduced (paddingH, paddingV);

  return juce::Rectangle<int> (rows.getX (),
                               rows.getY () + index * (itemH + rowGap),
                               rows.getWidth (), itemH);
}

juce::Rectangle<int>
globalSettingsNameArea (juce::Rectangle<int> row)
{
  return row.removeFromLeft (row.getWidth () * 3 / 5)
      .reduced (juce::roundToInt (theme ().padding), 0);
}

juce::Rectangle<int>
globalSettingsValueArea (juce::Rectangle<int> row)
{
  row.removeFromLeft (row.getWidth () * 3 / 5);
  return row.reduced (juce::roundToInt (theme ().padding));
}

GlobalSettingsComponent::GlobalSettingsComponent ()
{
  // Not for itself, but for its children: the dimmed area beside the panel
  // goes on letting touches through to the sphere, while the panel's rows
  // catch them.
  setInterceptsMouseClicks (false, true);
  // The arrows, Enter and Escape drive it too.
  setWantsKeyboardFocus (true);
}

void
GlobalSettingsComponent::setOptions (std::vector<Option> options)
{
  _options = std::move (options);
  _optionIndex = juce::jlimit (0, (int) _options.size () - 1, _optionIndex);
  _selectedValueIndex = 0;
  rebuildRowTouch ();
  repaint ();
}

void
GlobalSettingsComponent::rebuildRowTouch ()
{
  _rowTouch.clear ();

  for (size_t i = 0; i < _options.size (); ++i)
    {
      auto const option = static_cast<int> (i);

      RowTouch touch;

      // Name and value answer the same way: a tap selects, a double tap
      // opens. Neither drags -- a drag over the rows is a scroll, and six
      // rows have nothing to scroll.
      touch.name = std::make_unique<TouchControl> ();
      touch.value = std::make_unique<TouchControl> ();
      for (auto *control : { touch.name.get (), touch.value.get () })
        {
          control->setIdentity (option);
          control->onTap = [this] (int tapped, int) {
            setOptionIndex (tapped);

            // A row that leads somewhere opens on the first press -- asked
            // for at the device on 2026-09-23, and what Option::opensSubmenu
            // has said in its comment all along without anybody asking it.
            // There is nothing to arm on such a row: its value field holds
            // one answer, so selecting it is a press that asks a question
            // with one possible reply.
            //
            // The rows that hold a value keep both steps. That is the rule
            // from 2026-09-17 -- "kein edit ohne eingabemaske, das kollidiert
            // mit scroll" -- and it is about *changing* something, which
            // walking into a page does not.
            auto const index = static_cast<size_t> (tapped);
            if (index < _options.size () && _options[index].opensSubmenu)
              {
                if (onRowOpened)
                  onRowOpened (tapped);
                return;
              }

            if (onRowTapped)
              onRowTapped (tapped);
          };
          control->onDoubleTap = [this] (int tapped, int) {
            setOptionIndex (tapped);
            if (onRowOpened)
              onRowOpened (tapped);
          };
        }

      addAndMakeVisible (*touch.name);
      addAndMakeVisible (*touch.value);
      _rowTouch.push_back (std::move (touch));
    }

  // As many as the tallest list could show. Which value each stands for
  // changes as the list scrolls, so layOut() re-labels them.
  _pickerTouch.clear ();
  for (int slot = 0; slot < 32; ++slot)
    {
      auto control = std::make_unique<TouchControl> ();
      control->onTap = [this] (int value, int) { choosePickerValue (value); };
      control->onDoubleTap
          = [this] (int value, int) { choosePickerValue (value); };
      control->onDragIncrement
          = [this] (int, int, int increment) { scrollPicker (increment); };
      control->setFingerLatch (&FingerLatch::forGroup (FingerLatch::menuList));
      // One value per value's height of finger, so the list follows the hand.
      control->setPixelsPerStep (rowPitch ());
      addChildComponent (*control);
      _pickerTouch.push_back (std::move (control));
    }

  resized ();
}

int
GlobalSettingsComponent::pickerValueCount () const
{
  if (_options.empty ())
    return 0;
  return (int)_options[static_cast<size_t> (_optionIndex)].values.size ();
}

int
GlobalSettingsComponent::pickerRowsShown () const
{
  // As many rows as the area holds with the panel's padding, and never more
  // than there are values.
  auto const room = getHeight () - 2 * paddingV + rowGap;
  auto const fit = juce::jmax (1, room / (itemH + rowGap));
  return juce::jlimit (1, juce::jmax (1, pickerValueCount ()),
                       juce::jmin (fit, (int)_pickerTouch.size ()));
}

void
GlobalSettingsComponent::openPicker ()
{
  setValueFieldSelected (true);
}

void
GlobalSettingsComponent::cancelPicker ()
{
  if (!_valueFieldSelected)
    return;
  setValueFieldSelected (false);
  if (onPickerCancelled)
    onPickerCancelled ();
}

void
GlobalSettingsComponent::choosePickerValue (int value)
{
  if (!_valueFieldSelected || value < 0 || value >= pickerValueCount ())
    return;

  _selectedValueIndex = value;
  if (onPickerChosen)
    onPickerChosen (value);
  // The owner normally closes it while applying; closed here as well so the
  // list never outlives the choice.
  if (_valueFieldSelected)
    setValueFieldSelected (false);
}

void
GlobalSettingsComponent::scrollPicker (int steps)
{
  if (!_valueFieldSelected)
    return;
  _pickerTop = scrollBy (_pickerTop, steps, pickerRowsShown (),
                         pickerValueCount ());
  layOut ();
  repaint ();
}

bool
GlobalSettingsComponent::keyPressed (juce::KeyPress const &key)
{
  auto const up = key == juce::KeyPress::upKey;
  auto const down = key == juce::KeyPress::downKey;

  if (_valueFieldSelected)
    {
      if (up || down)
        {
          auto const next = juce::jlimit (0, pickerValueCount () - 1,
                                          _selectedValueIndex + (down ? 1 : -1));
          if (next != _selectedValueIndex)
            {
              _selectedValueIndex = next;
              _pickerTop = scrollToShow (_pickerTop, next, pickerRowsShown (),
                                         pickerValueCount ());
              layOut ();
              repaint ();
              if (onPickerBrowsed)
                onPickerBrowsed (next);
            }
          return true;
        }
      if (key == juce::KeyPress::returnKey)
        {
          choosePickerValue (_selectedValueIndex);
          return true;
        }
      if (key == juce::KeyPress::escapeKey)
        {
          cancelPicker ();
          return true;
        }
      return false;
    }

  if (up || down)
    {
      if (_options.empty ())
        return true;
      setOptionIndex (_optionIndex + (down ? 1 : -1));
      if (onRowTapped)
        onRowTapped (_optionIndex);
      return true;
    }
  if (key == juce::KeyPress::returnKey)
    {
      if (onRowOpened && !_options.empty ())
        onRowOpened (_optionIndex);
      return true;
    }
  return false;
}

void
GlobalSettingsComponent::mouseWheelMove (juce::MouseEvent const &,
                                         juce::MouseWheelDetails const &wheel)
{
  // What a two-finger scroll arrives as.
  auto const delta = wheel.deltaY > 0.f ? -1 : (wheel.deltaY < 0.f ? 1 : 0);
  if (delta != 0)
    scrollPicker (delta);
}

void
GlobalSettingsComponent::visibilityChanged ()
{
  // The arrows and Enter have to land here.
  if (isShowing ())
    grabKeyboardFocus ();
}

juce::Rectangle<int>
GlobalSettingsComponent::panelBounds () const
{
  if (_valueFieldSelected)
    return globalSettingsPanelBounds (getLocalBounds (), pickerRowsShown ());
  return globalSettingsPanelBounds (getLocalBounds (),
                                    juce::jmax (1, (int) _options.size ()));
}

void
GlobalSettingsComponent::resized ()
{
  layOut ();
}

void
GlobalSettingsComponent::layOut ()
{
  auto const numOptions = static_cast<int> (_rowTouch.size ());
  auto const picking = _valueFieldSelected;

  for (int i = 0; i < numOptions; ++i)
    {
      auto &touch = _rowTouch[static_cast<size_t> (i)];
      touch.name->setVisible (!picking);
      touch.value->setVisible (!picking);
      if (picking)
        continue;

      auto const panel
          = globalSettingsPanelBounds (getLocalBounds (), numOptions);
      auto const row = globalSettingsRowBounds (panel, numOptions, i);
      touch.name->setBounds (globalSettingsNameArea (row));
      touch.value->setBounds (globalSettingsValueArea (row));
    }

  auto const rows = picking ? pickerRowsShown () : 0;
  auto const panel = panelBounds ();
  for (size_t slot = 0; slot < _pickerTouch.size (); ++slot)
    {
      auto const value = _pickerTop + static_cast<int> (slot);
      auto const shown = picking && static_cast<int> (slot) < rows
                         && value < pickerValueCount ();
      auto &control = *_pickerTouch[slot];
      control.setVisible (shown);
      if (!shown)
        continue;
      control.setIdentity (value);
      control.setBounds (
          globalSettingsRowBounds (panel, rows, static_cast<int> (slot)));
    }
}

void
GlobalSettingsComponent::setOptionIndex (int index)
{
  if (_options.empty ())
    return;
  _optionIndex = juce::jlimit (0, (int) _options.size () - 1, index);
  repaint ();
}

void
GlobalSettingsComponent::setActiveValueIndex (int optionIndex, int activeIndex)
{
  if (optionIndex < 0 || optionIndex >= (int) _options.size ())
    return;
  auto &option = _options[static_cast<size_t> (optionIndex)];
  option.activeIndex = juce::jlimit (0, (int) option.values.size () - 1, activeIndex);
  repaint ();
}

void
GlobalSettingsComponent::navigateOption (int delta)
{
  if (_options.empty ())
    return;
  int n = static_cast<int> (_options.size ());
  _optionIndex = (_optionIndex + delta % n + n) % n;
  repaint ();
}

void
GlobalSettingsComponent::navigateValue (int delta)
{
  if (_options.empty ())
    return;
  auto const &values = _options[static_cast<size_t> (_optionIndex)].values;
  if (values.empty ())
    return;
  int n = static_cast<int> (values.size ());
  _selectedValueIndex = (_selectedValueIndex + delta % n + n) % n;
  repaint ();
}

void
GlobalSettingsComponent::setValueFieldSelected (bool selected)
{
  _valueFieldSelected = selected;
  if (selected && !_options.empty ())
    {
      _selectedValueIndex
          = _options[static_cast<size_t> (_optionIndex)].activeIndex;
      _pickerTop = scrollToShow (0, _selectedValueIndex, pickerRowsShown (),
                                 pickerValueCount ());
    }
  layOut ();
  repaint ();
}

void
GlobalSettingsComponent::paint (juce::Graphics &g)
{
  // ── dim around the panel, solid under it ──────────────────────────────────
  //
  // Two fills rather than one: the scrim says "the sphere is still there and
  // you are not looking at it", the panel says "read this". One value for
  // both made the rows see-through at 0.55 and took the whole sphere away at
  // 1 -- fillAll is the component, not the card.
  g.fillAll (toColour (theme ().surface, theme ().overlayScrim));

  if (_options.empty ())
    return;

  int const numOptions = static_cast<int> (_options.size ());
  auto const panelBounds
      = globalSettingsPanelBounds (getLocalBounds (), numOptions);

  // The panel on top of the scrim: its own value, see-through by default so
  // the sphere shows through the main menu -- see Theme::menuPanelOpacity.
  g.setColour (toColour (theme ().surface, theme ().menuPanelOpacity));
  g.fillRoundedRectangle (this->panelBounds ().toFloat (),
                          theme ().radiusPanel);

  // ── the list of one row's values, in place of the rows ───────────────────
  if (_valueFieldSelected)
    {
      auto const panel = this->panelBounds ();
      g.setColour (toColour (theme ().textPrimary, rowWash));
      g.fillRoundedRectangle (panel.toFloat (), theme ().radiusPanel);

      auto const &option = _options[static_cast<size_t> (_optionIndex)];
      auto const rows = pickerRowsShown ();
      for (int slot = 0; slot < rows; ++slot)
        {
          auto const value = _pickerTop + slot;
          if (value >= (int)option.values.size ())
            break;

          auto const row = globalSettingsRowBounds (panel, rows, slot);
          auto const isCandidate = value == _selectedValueIndex;
          auto const isActive = value == option.activeIndex;
          auto const &item = option.values[static_cast<size_t> (value)];

          g.setColour (toColour (theme ().textPrimary,
                                 isCandidate ? armedRowWash : rowWash));
          g.fillRoundedRectangle (row.toFloat (), theme ().radiusRow);

          // Which row this list belongs to, on the left; the value, with the
          // one that is in force now in bold.
          auto text = row.reduced (juce::roundToInt (theme ().padding), 0);
          g.setFont (juce::Font (theme ().fontSize (FontRole::Body),
                                 isActive ? juce::Font::bold
                                          : juce::Font::plain));
          g.setColour (isCandidate || isActive
                           ? item.colour
                           : toColour (theme ().textPrimary,
                                       theme ().alphaInactive));
          g.drawText (item.value, text, juce::Justification::centred, true);
          if (slot == 0)
            {
              g.setColour (toColour (theme ().textPrimary,
                                     theme ().alphaInactive));
              g.setFont (juce::Font (theme ().fontSize (FontRole::Body),
                                     juce::Font::plain));
              g.drawText (option.name, text, juce::Justification::centredLeft,
                          true);
            }
        }
      return;
    }

  // ── panel background ──────────────────────────────────────────────────────
  g.setColour (toColour (theme ().textPrimary, rowWash)); // a barely visible edge
  g.fillRoundedRectangle (panelBounds.toFloat (), theme ().radiusPanel);

  // ── one row per Option ───────────────────────────────────────────────────
  for (int i = 0; i < numOptions; ++i)
    {
      auto const row = globalSettingsRowBounds (panelBounds, numOptions, i);

      auto const &option = _options[static_cast<size_t> (i)];
      if (option.values.empty ())
        continue;

      bool const isBrowsedRow = (i == _optionIndex);
      bool const isArmedRow = isBrowsedRow && _valueFieldSelected;
      int const shownValueIndex = isArmedRow ? _selectedValueIndex : option.activeIndex;
      auto const &item = option.values[static_cast<size_t> (shownValueIndex)];

      g.setColour (toColour (theme ().textPrimary,
                             isArmedRow     ? armedRowWash
                             : isBrowsedRow ? browsedRowWash
                                            : rowWash));
      g.fillRoundedRectangle (row.toFloat (), theme ().radiusRow);

      auto const labelArea = globalSettingsNameArea (row);
      auto const valueArea = globalSettingsValueArea (row);

      g.setFont (juce::Font (theme ().fontSize (FontRole::Body),
                             juce::Font::plain));
      // Full opacity rather than an alpha rung: the browsed row's name has
      // always meant no dimming at all, which the alpha-less overload already
      // says. This used to be `isBrowsedRow ? 1.f : theme ().alphaInactive`;
      // 1.f fits no rung, and full opacity is the absence of an emphasis
      // decision rather than one of its rungs, so it deliberately gets no
      // role of its own. See issues/a3-motion-ui-metric-role-deviations.md
      // (Task 16).
      g.setColour (isBrowsedRow
                       ? toColour (theme ().textPrimary)
                       : toColour (theme ().textPrimary,
                                   theme ().alphaInactive));
      g.drawText (option.name, labelArea, juce::Justification::centredLeft, true);

      g.setColour (isArmedRow
                       ? toColour (theme ().textPrimary, armedFrameWash)
                       : toColour (theme ().textPrimary, rowFrameWash));
      g.fillRoundedRectangle (valueArea.toFloat (), theme ().radiusRow);

      g.setFont (juce::Font (theme ().fontSize (FontRole::Body),
                             juce::Font::bold));
      g.setColour (isBrowsedRow
                       ? item.colour
                       : toColour (theme ().textPrimary,
                                   theme ().alphaInactive));
      g.drawText (option.opensSubmenu ? juce::String (">") : item.value,
                  valueArea, juce::Justification::centred, true);
    }
}


bool
GlobalSettingsComponent::opensSubmenu (int index) const
{
  if (index < 0 || index >= (int)_options.size ())
    return false;

  return _options[static_cast<size_t> (index)].opensSubmenu;
}

int
GlobalSettingsComponent::rowPitch () const
{
  return itemH + rowGap;
}

} // namespace a3
