/*

  A3 Motion UI
  Copyright (C) 2023 Patric Schmitz

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

*/

#pragma once

#include <vector>

#include <JuceHeader.h>

namespace a3
{

/**
 * OverlayMenuComponent
 *
 * Full-window semi-transparent overlay with a centred selection list.
 * Navigation via navigate(+1/-1), confirmation via confirmSelection().
 * Caller reads getSelectedIndex() to apply the chosen value.
 */
class OverlayMenuComponent : public juce::Component
{
public:
  struct Item
  {
    juce::String label;
    juce::Colour colour{ juce::Colours::white };
  };

  explicit OverlayMenuComponent ();

  void setItems (std::vector<Item> items);

  // The "active" index is the currently applied value (shown with a marker).
  void setActiveIndex (int index);
  int  getActiveIndex () const { return _activeIndex; }

  // The "selected" index is the highlighted candidate during navigation.
  void setSelectedIndex (int index);
  int  getSelectedIndex () const { return _selectedIndex; }

  // Move selection by delta steps (wraps around).
  void navigate (int delta);

  void paint (juce::Graphics &g) override;

private:
  std::vector<Item> _items;
  int _activeIndex   = 0;
  int _selectedIndex = 0;

  static constexpr int panelW     = 320;
  static constexpr int itemH      = 52;
  static constexpr int paddingV   = 24;
  static constexpr int paddingH   = 32;
};

} // namespace a3
