// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later
#include "PanelPotPickup.hh"

#include <cmath>

namespace a3
{

namespace
{
/** Within this of the value counts as having found it (a hair, as StemDeck). */
constexpr float nearEnough = 0.01f;
/** A stored value that differs from what the pot wrote by more than float
 *  noise was moved by something else. */
constexpr float movedElsewhere = 1e-6f;

bool
inRange (int pot)
{
  return pot >= 0 && pot < PanelPotPickup::count;
}
}

void
PanelPotPickup::panelGained ()
{
  for (auto &pot : _pots)
    {
      pot.armed = true;
      pot.pickedUp = false;
      pot.lastReading.reset ();
    }
}

std::optional<float>
PanelPotPickup::feed (int pot, float reading, float stored)
{
  if (!inRange (pot) || !_pots[static_cast<std::size_t> (pot)].armed)
    return reading;

  auto &state = _pots[static_cast<std::size_t> (pot)];
  if (state.pickedUp && std::abs (stored - state.applied) > movedElsewhere)
    state.pickedUp = false;

  auto const before = state.lastReading;
  state.lastReading = reading;
  if (state.pickedUp)
    return reading;

  auto const close = std::abs (reading - stored) <= nearEnough;
  auto const crossed
      = before.has_value () && (*before - stored) * (reading - stored) <= 0.f;
  if (!close && !crossed)
    return std::nullopt;

  state.pickedUp = true;
  return reading;
}

void
PanelPotPickup::wrote (int pot, float stored)
{
  if (inRange (pot))
    _pots[static_cast<std::size_t> (pot)].applied = stored;
}

}
