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

#include <atomic>
#include <set>

namespace a3
{
enum class PanelRoute
{
  /** The panel does what it does: pads fire, keys are keys. */
  Panel,
  /** The key types. */
  Keyboard,
};

/** Who a panel button belongs to while the keyboard may be up (2026-09-30:
 *  "den hardware controller komplett übernehmen wenn aktiviert").
 *
 *  Decided where the buttons arrive, in the order they arrive: the app hears
 *  pads and keys through asynchronous Values, and a key that puts the
 *  keyboard away (ESC stands on TAP) must not have its release land on the
 *  panel afterwards. So a press goes wherever the panel belongs at that
 *  moment, and its release follows it there.
 *
 *  `route` is called on the adapter's thread only; `setOwnedByKeyboard`
 *  from anywhere. */
class PanelOwnership
{
public:
  void setOwnedByKeyboard (bool owned);
  PanelRoute route (int button, bool pressed);

private:
  std::atomic<bool> _ownedByKeyboard{ false };
  /** Buttons pressed while the keyboard owned the panel, until released. */
  std::set<int> _typing;
};
}
