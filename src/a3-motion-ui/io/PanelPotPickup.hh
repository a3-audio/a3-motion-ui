// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <optional>

namespace a3
{

/** The panel's pots after a hand-over, as StemDeck's AUX pots have them (P44).
 *
 *  Until the panel has been gained from the other deck nothing is armed and a
 *  reading passes through unchanged -- Motion alone behaves as it always did.
 *  After a gain a pot does nothing until it has crossed the value it would
 *  change (or come within a hair of it); the reading from before the
 *  hand-over is forgotten, because it was the other deck's hand. Once picked
 *  up, a stored value that moved away from the pot (a touch, a clip or set
 *  load) arms it again. Message thread only. */
class PanelPotPickup
{
public:
  static constexpr int count = 4;

  /** The panel came back from the other deck: every pot has to find its value
   *  again. */
  void panelGained ();

  /** The value to apply for this reading, or nothing while the pot has not
   *  picked up. `stored` is the value the pot would change. */
  std::optional<float> feed (int pot, float reading, float stored);

  /** What the stored value is after a fed reading was applied, so a later
   *  difference can be told from the pot's own doing. */
  void wrote (int pot, float stored);

private:
  struct Pot
  {
    bool armed = false; // a hand-over has happened
    bool pickedUp = false;
    std::optional<float> lastReading;
    float applied = 0.f;
  };

  std::array<Pot, count> _pots;
};

}
