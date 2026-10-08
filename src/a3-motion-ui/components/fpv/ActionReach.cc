/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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

#include "ActionReach.hh"

namespace a3
{

std::optional<PilotOrder>
pilotOrderAtPress (AppView view, PilotOrder const &order)
{
  switch (view)
    {
    case AppView::Full:
      return std::nullopt;
    case AppView::Fpv:
      if (!order.game)
        return std::nullopt;
      return order;
    }
  return std::nullopt;
}

juce::String
pilotReadout (int channel, juce::String const &padName, PilotGame game)
{
  auto const lead = "CH" + juce::String (channel + 1) + " " + padName + " ";
  if (game == PilotGame::None)
    return lead + "NO GAME";
  return lead + "GAME " + pilotWord (game).toUpperCase ();
}

}
