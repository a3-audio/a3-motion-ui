/*

  A3 Motion UI
  Copyright (C) 2023 Patric Schmitz

*/

#include "GlobalSettingsComponent.hh"

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
constexpr float overlayOpacity = 0.55f;
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

juce::Rectangle<int>
globalSettingsPanelBounds (juce::Rectangle<int> bounds, int numOptions)
{
  auto const panelW = juce::jmin (maxPanelW, bounds.getWidth () - 2 * paddingH);
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
  return row.removeFromLeft (row.getWidth () * 3 / 5).reduced (8, 0);
}

juce::Rectangle<int>
globalSettingsValueArea (juce::Rectangle<int> row)
{
  row.removeFromLeft (row.getWidth () * 3 / 5);
  return row.reduced (8, 8);
}

GlobalSettingsComponent::GlobalSettingsComponent ()
{
  // Not for itself, but for its children: the dimmed area beside the panel
  // goes on letting touches through to the sphere, while the panel's rows
  // catch them.
  setInterceptsMouseClicks (false, true);
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

      touch.name = std::make_unique<TouchControl> ();
      touch.name->setIdentity (option);
      touch.name->onTap = [this] (int tapped, int) {
        if (onRowTapped)
          onRowTapped (tapped);
      };

      touch.value = std::make_unique<TouchControl> ();
      touch.value->setIdentity (option);
      touch.value->onTap = [this] (int tapped, int) {
        if (onValueArmed)
          onValueArmed (tapped);
      };
      touch.value->onDragIncrement
          = [this] (int dragged, int, int increment) {
              if (onValueDragged)
                onValueDragged (dragged, increment);
            };
      // Coming off the value field is what the second encoder press does:
      // it applies what the drag landed on.
      touch.value->onRelease = [this] (int released, int) {
        if (onValueReleased)
          onValueReleased (released);
      };

      addAndMakeVisible (*touch.name);
      addAndMakeVisible (*touch.value);
      _rowTouch.push_back (std::move (touch));
    }

  resized ();
}

void
GlobalSettingsComponent::resized ()
{
  auto const numOptions = static_cast<int> (_rowTouch.size ());
  if (numOptions == 0)
    return;

  auto const panel = globalSettingsPanelBounds (getLocalBounds (), numOptions);

  for (int i = 0; i < numOptions; ++i)
    {
      auto const row = globalSettingsRowBounds (panel, numOptions, i);
      auto &touch = _rowTouch[static_cast<size_t> (i)];
      touch.name->setBounds (globalSettingsNameArea (row));
      touch.value->setBounds (globalSettingsValueArea (row));
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
    _selectedValueIndex = _options[static_cast<size_t> (_optionIndex)].activeIndex;
  repaint ();
}

void
GlobalSettingsComponent::paint (juce::Graphics &g)
{
  // ── dim background ────────────────────────────────────────────────────────
  g.fillAll (toColour (theme ().surface, overlayOpacity));

  if (_options.empty ())
    return;

  int const numOptions = static_cast<int> (_options.size ());
  auto const panelBounds
      = globalSettingsPanelBounds (getLocalBounds (), numOptions);

  // ── panel background ──────────────────────────────────────────────────────
  g.setColour (toColour (theme ().textPrimary, rowWash)); // a barely visible edge
  g.fillRoundedRectangle (panelBounds.toFloat (), 10.f);

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
      g.fillRoundedRectangle (row.toFloat (), 6.f);

      auto const labelArea = globalSettingsNameArea (row);
      auto const valueArea = globalSettingsValueArea (row);

      g.setFont (juce::Font (theme ().fontSize (FontRole::Body),
                             juce::Font::plain));
      g.setColour (toColour (theme ().textPrimary,
                         isBrowsedRow ? 1.f : theme ().alphaInactive));
      g.drawText (option.name, labelArea, juce::Justification::centredLeft, true);

      g.setColour (isArmedRow
                       ? toColour (theme ().textPrimary, armedFrameWash)
                       : toColour (theme ().textPrimary, rowFrameWash));
      g.fillRoundedRectangle (valueArea.toFloat (), 5.f);

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

} // namespace a3
