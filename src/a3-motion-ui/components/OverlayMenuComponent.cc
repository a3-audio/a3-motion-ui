/*

  A3 Motion UI
  Copyright (C) 2023 Patric Schmitz

*/

#include "OverlayMenuComponent.hh"

namespace a3
{

OverlayMenuComponent::OverlayMenuComponent ()
{
  setInterceptsMouseClicks (false, false);
}

void
OverlayMenuComponent::setItems (std::vector<Item> items)
{
  _items = std::move (items);
  _selectedIndex = juce::jlimit (0, (int) _items.size () - 1, _selectedIndex);
  _activeIndex   = juce::jlimit (0, (int) _items.size () - 1, _activeIndex);
  repaint ();
}

void
OverlayMenuComponent::setActiveIndex (int index)
{
  _activeIndex = index;
  repaint ();
}

void
OverlayMenuComponent::setSelectedIndex (int index)
{
  if (_items.empty ())
    return;
  _selectedIndex = juce::jlimit (0, (int) _items.size () - 1, index);
  repaint ();
}

void
OverlayMenuComponent::navigate (int delta)
{
  if (_items.empty ())
    return;
  int n = static_cast<int> (_items.size ());
  _selectedIndex = (_selectedIndex + delta % n + n) % n;
  repaint ();
}

void
OverlayMenuComponent::setValueFieldSelected (bool selected)
{
  _valueFieldSelected = selected;
  repaint ();
}

void
OverlayMenuComponent::paint (juce::Graphics &g)
{
  // ── dim background ────────────────────────────────────────────────────────
  g.fillAll (juce::Colour (0, 0, 0).withAlpha (0.72f));

  if (_items.empty ())
    return;

  int panelH = paddingV * 2 + itemH;
  auto panelBounds = juce::Rectangle<int> (
      (getWidth () - panelW) / 2,
      (getHeight () - panelH) / 2,
      panelW, panelH);

  // ── panel background ──────────────────────────────────────────────────────
  g.setColour (juce::Colour (0x22ffffff));
  g.fillRoundedRectangle (panelBounds.toFloat (), 10.f);
  g.setColour (juce::Colour (0x55ffffff));
  g.drawRoundedRectangle (panelBounds.toFloat (), 10.f, 1.f);

  // ── items ─────────────────────────────────────────────────────────────────
  auto row = panelBounds.reduced (paddingH, paddingV);
  auto const &item = _items[static_cast<size_t> (_selectedIndex)];
  bool const isActive = (_selectedIndex == _activeIndex);

  g.setColour (juce::Colour (0x22ffffff));
  g.fillRoundedRectangle (row.toFloat (), 6.f);

  auto labelArea = row.removeFromLeft (row.getWidth () * 3 / 5).reduced (8, 0);
  auto valueArea = row.reduced (8, 8);

  g.setFont (juce::Font (19.f, juce::Font::plain));
  g.setColour (juce::Colours::white.withAlpha (0.90f));
  g.drawText (item.description, labelArea, juce::Justification::centredLeft, true);

  auto valueBorderColour = _valueFieldSelected
                               ? juce::Colours::white.withAlpha (0.95f)
                               : juce::Colours::white.withAlpha (0.45f);
  g.setColour (valueBorderColour);
  g.drawRoundedRectangle (valueArea.toFloat (), 5.f, _valueFieldSelected ? 2.0f : 1.0f);

  g.setFont (juce::Font (18.f, juce::Font::bold));
  g.setColour (isActive ? item.colour : juce::Colours::white.withAlpha (0.9f));
  g.drawText (item.value, valueArea, juce::Justification::centred, true);
}

} // namespace a3
