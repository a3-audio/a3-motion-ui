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
constexpr float overlayOpacity = 0.72f;
constexpr float rowWash = 0.063f;
constexpr float browsedRowWash = 0.086f;
constexpr float armedRowWash = 0.133f;
constexpr float armedFrameWash = 0.18f;
constexpr float rowFrameWash = 0.08f;
}

GlobalSettingsComponent::GlobalSettingsComponent ()
{
  setInterceptsMouseClicks (false, false);
}

void
GlobalSettingsComponent::setOptions (std::vector<Option> options)
{
  _options = std::move (options);
  _optionIndex = juce::jlimit (0, (int) _options.size () - 1, _optionIndex);
  _selectedValueIndex = 0;
  repaint ();
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
  int const panelW = juce::jmin (maxPanelW, getWidth () - 2 * paddingH);
  int const panelH = paddingV * 2 + numOptions * itemH + (numOptions - 1) * rowGap;
  auto panelBounds = juce::Rectangle<int> (
      (getWidth () - panelW) / 2,
      (getHeight () - panelH) / 2,
      panelW, panelH);

  // ── panel background ──────────────────────────────────────────────────────
  g.setColour (toColour (theme ().textPrimary, rowWash)); // a barely visible edge
  g.fillRoundedRectangle (panelBounds.toFloat (), 10.f);

  // ── one row per Option ───────────────────────────────────────────────────
  auto rows = panelBounds.reduced (paddingH, paddingV);

  for (int i = 0; i < numOptions; ++i)
    {
      auto row = rows.removeFromTop (itemH);
      if (i < numOptions - 1)
        rows.removeFromTop (rowGap);

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

      auto labelArea = row.removeFromLeft (row.getWidth () * 3 / 5).reduced (8, 0);
      auto valueArea = row.reduced (8, 8);

      g.setFont (juce::Font (theme ().fontSize (FontRole::Label),
                             juce::Font::plain));
      g.setColour (toColour (theme ().textPrimary,
                         isBrowsedRow ? 1.f : theme ().alphaInactive));
      g.drawText (option.name, labelArea, juce::Justification::centredLeft, true);

      g.setColour (isArmedRow
                       ? toColour (theme ().textPrimary, armedFrameWash)
                       : toColour (theme ().textPrimary, rowFrameWash));
      g.fillRoundedRectangle (valueArea.toFloat (), 5.f);

      g.setFont (juce::Font (theme ().fontSize (FontRole::Value),
                             juce::Font::bold));
      g.setColour (isBrowsedRow
                       ? item.colour
                       : toColour (theme ().textPrimary,
                                   theme ().alphaInactive));
      g.drawText (item.value, valueArea, juce::Justification::centred, true);
    }
}

} // namespace a3
