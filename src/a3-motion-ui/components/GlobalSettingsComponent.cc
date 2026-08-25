/*

  A3 Motion UI
  Copyright (C) 2023 Patric Schmitz

*/

#include "GlobalSettingsComponent.hh"

#include <a3-motion-ui/theme/Theme.hh>

namespace a3
{

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
  g.fillAll (juce::Colour (0, 0, 0).withAlpha (0.72f));

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
  g.setColour (juce::Colour (0x10ffffff)); // lower alpha for less visible edge
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

      g.setColour (isArmedRow ? juce::Colour (0x22ffffff)
                    : isBrowsedRow ? juce::Colour (0x16ffffff)
                                   : juce::Colour (0x10ffffff));
      g.fillRoundedRectangle (row.toFloat (), 6.f);

      auto labelArea = row.removeFromLeft (row.getWidth () * 3 / 5).reduced (8, 0);
      auto valueArea = row.reduced (8, 8);

      g.setFont (juce::Font (theme ().fontSize (FontRole::Label),
                             juce::Font::plain));
      g.setColour (juce::Colours::white.withAlpha (isBrowsedRow ? 0.90f : 0.55f));
      g.drawText (option.name, labelArea, juce::Justification::centredLeft, true);

      g.setColour (isArmedRow
                       ? juce::Colours::white.withAlpha (0.18f)
                       : juce::Colours::white.withAlpha (0.08f));
      g.fillRoundedRectangle (valueArea.toFloat (), 5.f);

      g.setFont (juce::Font (theme ().fontSize (FontRole::Value),
                             juce::Font::bold));
      g.setColour (isBrowsedRow ? item.colour : juce::Colours::white.withAlpha (0.6f));
      g.drawText (item.value, valueArea, juce::Justification::centred, true);
    }
}

} // namespace a3
