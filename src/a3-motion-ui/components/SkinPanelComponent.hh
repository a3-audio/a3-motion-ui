/*

  A3 Motion UI
  Copyright (C) 2023 Patric Schmitz

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#pragma once

#include <JuceHeader.h>

#include <a3-motion-ui/components/SkinPanelLayout.hh>
#include <a3-motion-ui/theme/SkinSections.hh>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace a3
{

/**
 * SkinPanelComponent
 *
 * The skin editor, as a narrow panel at the left edge of the sphere: six
 * sections named after what a performer sees (SkinSections.hh), each with a
 * switch, each effect in it with a switch of its own and a bar for its
 * amount on the same row. One section open at a time.
 *
 * Asked for on 2026-09-28, replacing a list of every value in the skin that
 * covered the sphere it was changing and changed a value only by a double
 * tap and typing.
 *
 * **How a value is set.** Built from JUCE's own parts (see juce-pro):
 *
 * - a `juce::Slider` in `LinearBar` style is the row's bar. The drag is
 *   **relative and one to one**: the bar's own width is the whole range
 *   (`setMouseDragSensitivity` follows the width in resized()), and it does
 *   not jump to where the finger lands (`setSliderSnapsToMousePosition
 *   (false)`) -- touching a row to read it must not change it. The sphere
 *   follows every step of the drag.
 * - `−` and `+` beside it move a hundredth of the bar's travel, and repeat
 *   while held (`juce::Button::setRepeatSpeed`): the fine way, one finger,
 *   no second gesture to learn.
 * - a **double tap** on a bar puts back what it held when the editor opened
 *   (`setDoubleClickReturnValue`) -- the way back from a value dialled into
 *   a corner, one row at a time.
 * - bars are skewed where the useful end of a range is small
 *   (`juce::NormalisableRange`), so what ships sits near the middle.
 *
 * Typing stays available as the fallback: the footer opens the full list of
 * every value, the old editor, where a double tap still calls the keyboard.
 *
 * The panel's encoders were considered and left alone: they turn freq and
 * Q, whatever is on screen, since the touch rework (ARCHITECTURE.md) -- a
 * knob that means something else when a page is open is a knob you have to
 * look at.
 *
 * The component holds the edited skin; who applies it and who writes it is
 * the caller's business, as with SkinEditorComponent.
 */
class SkinPanelComponent : public juce::Component
{
public:
  SkinPanelComponent ();
  ~SkinPanelComponent () override;

  /** The skin to edit. Every bar remembers the value it has now, for a
   *  double tap to return to. */
  void setSkin (juce::var skin, juce::String const &name);
  juce::var const &getSkin () const { return _skin; }
  juce::String const &getSkinName () const { return _name; }

  /** Which section shows its rows. A tap on an open header closes it. */
  void openSection (std::optional<SkinSection> section);
  std::optional<SkinSection> openedSection () const { return _open; }

  /** What the switch and bar widgets do, as methods so a test can reach
   *  them without a finger. */
  void setSectionSwitch (SkinSection section, bool on);
  void setEffectSwitch (SkinSection section, int effect, bool on);
  void setTunable (SkinTunable const &tunable, double value);
  void stepTunable (SkinTunable const &tunable, int steps);
  double tunableValue (SkinTunable const &tunable) const;

  /** Whether an effect's bar can be moved: only while it is drawn. */
  bool isEffectBarEnabled (SkinSection section, int effect) const;

  /** Called whenever a value or a switch changed, so the caller can put the
   *  edited skin in force at once. */
  std::function<void ()> onValueChanged;
  /** A swatch was tapped: the path the picker writes back to, and the
   *  colour it should open on. */
  std::function<void (juce::String const &, juce::Colour)> onColourPicked;
  /** The footer: every value, and the skin's file actions. */
  std::function<void ()> onOpenFullList;

  /** A colour row's colour: what the file says, or the theme's default for
   *  a role it does not name. */
  juce::Colour colourAt (juce::String const &path) const;

  void paint (juce::Graphics &g) override;
  void resized () override;

private:
  class Content;
  class Bar;
  class SectionHeader;
  class Swatch;

  void rebuildRows ();
  void refreshEnabledStates ();
  void valueChanged ();

  juce::var _skin;
  juce::String _name;
  std::optional<SkinSection> _open;
  /** What each bar's path held when the skin was handed in. */
  std::map<juce::String, double> _valueAtOpen;

  juce::Viewport _viewport;
  std::unique_ptr<Content> _content;
};

}
