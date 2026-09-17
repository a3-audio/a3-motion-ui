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
 * A value changes only in its mask: a tap selects a row, a double tap or
 * Enter opens it -- a row that leads somewhere opens that page, a row with a
 * value opens the list of its values in place of the rows. In that list a
 * tap or Enter chooses, Escape (and the owner's Back) leaves it as it was, and
 * the arrows walk it so a skin can be seen before it is chosen. A drag scrolls
 * and never edits: "kein edit ohne eingabemaske, das kollidiert mit scroll."
 *
 * It used to be the encoder's two levels laid out for a finger -- tap the
 * value to arm, drag it to change, let go to apply -- which put an edit one
 * drag away from every scroll.
 */
/** Where the menu's panel sits inside whatever area it was given, and where
 *  each of its rows sits inside that panel. Pulled out of paint() so the hit
 *  areas are placed by the same arithmetic that draws them. */
juce::Rectangle<int> globalSettingsPanelBounds (juce::Rectangle<int> bounds,
                                                int numOptions);
/** How wide the drag zones beside the panel are. Wide enough to land a
 *  finger in without looking. */
int globalSettingsSideZoneWidth (juce::Rectangle<int> bounds);
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

  /** A row was tapped, or the arrows moved the selection: it is selected. */
  std::function<void (int option)> onRowTapped;
  /** A row was double tapped, or Enter pressed on it: open it. */
  std::function<void (int option)> onRowOpened;
  /** The arrows moved through the list of values -- a candidate, not a
   *  choice. The skin row previews it. */
  std::function<void (int value)> onPickerBrowsed;
  /** A value in the list was tapped or Entered. The list has closed. */
  std::function<void (int value)> onPickerChosen;
  /** The list was left without choosing. The owner undoes any preview. */
  std::function<void ()> onPickerCancelled;

  /** The list of the selected row's values, opened on its active one. */
  void openPicker ();
  /** Leave the list without choosing; calls onPickerCancelled. */
  void cancelPicker ();
  bool isPickerOpen () const { return _valueFieldSelected; }
  /** The first value the list shows, and moving it. */
  int pickerFirstVisible () const { return _pickerTop; }
  void scrollPicker (int steps);
  /** How tall a row of the list is with its gap. */
  int rowPitch () const;

  bool keyPressed (juce::KeyPress const &key) override;
  void mouseWheelMove (juce::MouseEvent const &,
                       juce::MouseWheelDetails const &wheel) override;
  void visibilityChanged () override;

  /** Where the panel sits, so the side strips can be put beside it. */
  juce::Rectangle<int> panelBounds () const;

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
  /** One hit area per value the list can show at once; identity is the
   *  value's index. Only visible while the list is open. */
  std::vector<std::unique_ptr<TouchControl> > _pickerTouch;

  void rebuildRowTouch ();
  void choosePickerValue (int value);
  int pickerRowsShown () const;
  int pickerValueCount () const;
  void layOut ();

  std::vector<Option> _options;
  int _optionIndex        = 0;
  int _selectedValueIndex = 0;
  bool _valueFieldSelected = false;
  int _pickerTop = 0;

};

} // namespace a3
