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
OverlayMenuComponent::paint (juce::Graphics &g)
{
  // ── dim background ────────────────────────────────────────────────────────
  g.fillAll (juce::Colour (0, 0, 0).withAlpha (0.72f));

  if (_items.empty ())
    return;

  int panelH = paddingV * 2 + static_cast<int> (_items.size ()) * itemH;
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
  auto itemArea = panelBounds.reduced (paddingH, paddingV);
  for (int i = 0; i < static_cast<int> (_items.size ()); ++i)
    {
      auto row = itemArea.removeFromTop (itemH);
      bool isSelected = (i == _selectedIndex);
      bool isActive   = (i == _activeIndex);

      // highlight selected row
      if (isSelected)
        {
          g.setColour (_items[i].colour.withAlpha (0.25f));
          g.fillRoundedRectangle (row.toFloat (), 6.f);
          g.setColour (_items[i].colour.withAlpha (0.6f));
          g.drawRoundedRectangle (row.toFloat (), 6.f, 1.5f);
        }

      // active indicator (dot on the right)
      if (isActive)
        {
          auto dot = row.withWidth (12).withHeight (12)
                        .withRightX (row.getRight () - 8)
                        .withY (row.getCentreY () - 6);
          g.setColour (_items[i].colour);
          g.fillEllipse (dot.toFloat ());
        }

      // label
      g.setFont (juce::Font (22.f, isSelected ? juce::Font::bold : juce::Font::plain));
      g.setColour (isSelected ? _items[i].colour
                              : juce::Colours::white.withAlpha (0.55f));
      g.drawText (_items[i].label, row.reduced (4, 0),
                  juce::Justification::centredLeft, true);
    }
}

} // namespace a3
