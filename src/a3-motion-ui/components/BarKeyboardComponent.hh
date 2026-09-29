/*

  A3 Motion UI
  Copyright (C) 2026 Patric Schmitz

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

#include <a3-motion-ui/components/BarKeyboardLayout.hh>
#include <a3-motion-ui/components/BarKeyboardModel.hh>
#include <a3-motion-ui/theme/ThemedComponent.hh>

#include <array>
#include <functional>
#include <map>

namespace a3
{

/** The in-app keyboard, in the bar's clip content (2026-09-28).
 *
 *  Replaces Onboard: asked for as "ein keyboard welches nur den bereich vom
 *  clipsettingeditor ausfüllt und auf unser hardware controller layout
 *  passt". Where the keys stand is BarKeyboardLayout's, what a press means
 *  BarKeyboardModel's; this only draws them and counts fingers.
 *
 *  **It never takes the keyboard focus** -- the field being typed into
 *  keeps it, and the rename row and the script editor end their edit when
 *  they lose it. A key goes out through `onKey` as an ordinary KeyPress.
 *
 *  Opaque, on the bar's own ground: the fields under it are the bar's and
 *  would otherwise show through. */
class BarKeyboardComponent : public juce::Component,
                             public ThemedComponent,
                             private juce::Timer
{
public:
  BarKeyboardComponent ();

  /** The encoders' eight fields in this component's coordinates, and the
   *  bar's type sizes the keys are lettered in. */
  void setFields (std::array<juce::Rectangle<int>, 8> const &fields,
                  ControlMetrics metrics);

  /** The panel: an encoder walks the keys of the field it stands under,
   *  its press types the one it stands on. `row` 0 is the upper encoder. */
  void turnEncoder (int column, int row, int increment);
  void pressEncoder (int column, int row);

  std::function<void (juce::KeyPress const &)> onKey;
  std::function<void ()> onHide;
  /** SHIFT held -- the panel's or the screen's. */
  std::function<bool ()> isShiftHeld;

  void paint (juce::Graphics &g) override;
  void mouseDown (juce::MouseEvent const &e) override;
  void mouseDrag (juce::MouseEvent const &e) override;
  void mouseUp (juce::MouseEvent const &e) override;
  void visibilityChanged () override;

  /** Asked on a skin change, after the bar has laid itself out again: the
   *  fields and the type sizes follow the skin's fonts and Pot Size, and
   *  the bar's bounds do not always change with them. */
  std::function<void ()> onPlacementStale;
  void applyTheme () override;

private:
  void timerCallback () override;
  void relayout ();
  void fire (int keyIndex);
  bool keyIsLit (int keyIndex) const;
  int panelKeyOfBlock (int block) const;
  void paintKey (juce::Graphics &g, int keyIndex) const;

  std::array<juce::Rectangle<int>, 8> _fields{};
  ControlMetrics _metrics{ 0, 0.f, 0.f };
  KeyboardState _state;
  std::vector<KeyCap> _keys;

  /** Which key each finger is on, by touch source; -1 once it slid off. */
  std::map<int, int> _pressed;
  int _repeatingKey = -1;
  int _repeatingSource = -1;

  /** Per field, the key its encoder stands on, and whether it has been
   *  turned since the keyboard came up -- only then is it marked. */
  std::array<int, 8> _panelKey{};
  std::array<bool, 8> _panelShown{};
};

}
