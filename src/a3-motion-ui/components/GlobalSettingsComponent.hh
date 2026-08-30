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
#include <a3-motion-ui/components/TouchControl.hh>

#include <functional>
#include <memory>

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
/** Where the menu's panel sits inside whatever area it was given, and where
 *  each of its rows sits inside that panel. Pulled out of paint() so the hit
 *  areas are placed by the same arithmetic that draws them. */
juce::Rectangle<int> globalSettingsPanelBounds (juce::Rectangle<int> bounds,
                                                int numOptions);
juce::Rectangle<int> globalSettingsRowBounds (juce::Rectangle<int> panel,
                                              int numOptions, int index);
/** A row's two halves: the name, which browses, and the value field, which
 *  arms and then drags. */
juce::Rectangle<int> globalSettingsNameArea (juce::Rectangle<int> row);
juce::Rectangle<int> globalSettingsValueArea (juce::Rectangle<int> row);

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
    /** A row that leads somewhere rather than holding a value. It has nothing
     *  to choose between, so arming its value field is a press that asks a
     *  question with one answer — it opens on the first press instead, and
     *  says so with a chevron rather than the word "open". */
    bool opensSubmenu = false;
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

  /** Whether the row at `index` leads to a submenu rather than holding a
   *  value — the caller opens it on the first press. */
  bool opensSubmenu (int index) const;

  // Arming (selected=true) seeds the candidate value from the current
  // Option's active value, so turning the encoder starts from what's
  // currently applied.
  void setValueFieldSelected (bool selected);

  /** A row's name was tapped: browse that row, disarm. */
  std::function<void (int option)> onRowTapped;
  /** A row's value field was tapped: arm it — or, on a row that leads
   *  somewhere, open it. */
  std::function<void (int option)> onValueArmed;
  /** The armed row's value field was dragged, by one increment. */
  std::function<void (int option, int increment)> onValueDragged;
  /** The finger came off after such a drag: apply what it landed on. */
  std::function<void (int option)> onValueReleased;

  void paint (juce::Graphics &g) override;
  void resized () override;

private:
  /** Two hit areas per option — the name and the value field. Rebuilt
   *  whenever setOptions() changes how many rows there are. */
  struct RowTouch
  {
    std::unique_ptr<TouchControl> name;
    std::unique_ptr<TouchControl> value;
  };
  std::vector<RowTouch> _rowTouch;

  void rebuildRowTouch ();

  std::vector<Option> _options;
  int _optionIndex        = 0;
  int _selectedValueIndex = 0;
  bool _valueFieldSelected = false;

};

} // namespace a3
