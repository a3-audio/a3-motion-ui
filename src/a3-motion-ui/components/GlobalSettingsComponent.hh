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

#include <a3-motion-ui/theme/ThemeColours.hh>

namespace a3
{

/**
 * GlobalSettingsComponent
 *
 * Device-wide settings menu (Clockmode, Pot Size, Font Size), opened by the Menu
 * button. Shares the bottom-quarter "settings area" of the screen with
 * ClipSettingsComponent — the two are never shown at once, this one is
 * drawn on top while open. Every Option is shown as its own row
 * simultaneously, sized to fit however many Options are supplied.
 *
 * Navigation is driven by a single rotary encoder: two-level, one input.
 * Turning it while no row is armed calls navigateOption() to move the
 * highlighted row (browse level); pushing arms that row's value field
 * (setValueFieldSelected(true), which also seeds the candidate value from
 * the row's current active value); turning it while armed calls
 * navigateValue() to cycle that Option's values (edit level). The caller
 * reads getSelectedValueIndex() to apply the chosen value.
 */
class GlobalSettingsComponent : public juce::Component
{
public:
  struct ValueItem
  {
    juce::String value;
    /** Read at construction, so an item built after a skin change carries
     *  the new skin's text colour. */
    juce::Colour colour = toColour (theme ().textPrimary);
  };

  struct Option
  {
    juce::String name;
    std::vector<ValueItem> values;
    int activeIndex = 0;
  };

  explicit GlobalSettingsComponent ();

  void setOptions (std::vector<Option> options);

  // Which Option is currently shown/browsed.
  void setOptionIndex (int index);
  int  getOptionIndex () const { return _optionIndex; }

  // The candidate value index within the current Option, while armed.
  int  getSelectedValueIndex () const { return _selectedValueIndex; }

  // Update an Option's applied ("active") value, e.g. after confirming.
  void setActiveValueIndex (int optionIndex, int activeIndex);

  // Move between Options (browse level). Wraps around.
  void navigateOption (int delta);

  // Move between values of the current Option (edit level). Wraps around.
  void navigateValue (int delta);

  // Arming (selected=true) seeds the candidate value from the current
  // Option's active value, so turning the encoder starts from what's
  // currently applied.
  void setValueFieldSelected (bool selected);

  void paint (juce::Graphics &g) override;

private:
  std::vector<Option> _options;
  int _optionIndex        = 0;
  int _selectedValueIndex = 0;
  bool _valueFieldSelected = false;

  static constexpr int maxPanelW = 760;
  static constexpr int itemH     = 52;
  static constexpr int rowGap    = 6;
  static constexpr int paddingV  = 24;
  static constexpr int paddingH  = 32;
};

} // namespace a3
