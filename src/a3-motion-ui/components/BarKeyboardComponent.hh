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
#include <a3-motion-ui/io/PanelGrid.hh>
#include <a3-motion-ui/theme/ThemedComponent.hh>

#include <functional>
#include <map>
#include <optional>
#include <set>

namespace a3
{

/** The in-app keyboard, in the bar's clip content (2026-09-28).
 *
 *  Replaces Onboard. Since 2026-09-30 it is the panel: its 44 keys stand on
 *  the PADS grid, and while it is up the panel's 44 keys type
 *  (`pressPanelCell`) -- "den hardware controller komplett übernehmen".
 *  Which key is where is PanelKeyboard's, what a press means
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

  /** The bar's type sizes the keys are lettered in; the keys stand on the
   *  panel's grid in this component's bounds. */
  void setMetrics (ControlMetrics metrics);

  /** A panel key went down or came up while the keyboard owns the panel.
   *  Characters type on the press -- a key on the panel is a moment, as on
   *  a desk keyboard; Backspace and the cursor go on while held. */
  void pressPanelCell (PanelCell cell, bool down);

  std::function<void (juce::KeyPress const &)> onKey;
  std::function<void ()> onHide;
  /** SHIFT held -- the screen's. */
  std::function<bool ()> isShiftHeld;

  void paint (juce::Graphics &g) override;
  void resized () override;
  void mouseDown (juce::MouseEvent const &e) override;
  void mouseDrag (juce::MouseEvent const &e) override;
  void mouseUp (juce::MouseEvent const &e) override;
  void visibilityChanged () override;

  /** Asked on a skin change, after the bar has laid itself out again: the
   *  type sizes follow the skin's fonts and Pot Size, and the bar's bounds
   *  do not always change with them. */
  std::function<void ()> onPlacementStale;
  void applyTheme () override;

private:
  void timerCallback () override;
  void relayout ();
  void fire (KeyDef const &def);
  void startRepeating (KeyDef const &def);
  void stopRepeating ();
  bool keyIsLit (int keyIndex) const;
  void paintKey (juce::Graphics &g, int keyIndex) const;

  ControlMetrics _metrics{ 0, 0.f, 0.f };
  KeyboardState _state;
  std::vector<KeyCap> _keys;
  /** Which key each finger is on, by touch source; -1 once it slid off. */
  std::map<int, int> _pressed;
  /** The panel keys held down, lit on the screen while they are. */
  std::set<std::pair<int, int>> _panelHeld;

  /** What repeats while held, and who holds it: a finger (its source) or a
   *  panel key (its cell). */
  std::optional<KeyDef> _repeating;
  int _repeatingSource = -1;
  std::optional<PanelCell> _repeatingCell;
};

}
