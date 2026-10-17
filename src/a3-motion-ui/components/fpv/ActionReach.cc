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
      {
        if (order.game)
          return order;
        auto asked = order;
        asked.style = order.style.value_or (PilotStyle::Auto);
        if (*asked.style == PilotStyle::None)
          return std::nullopt;
        return asked;
      }
    }
  return std::nullopt;
}

PilotOrder
releasingItsShip ()
{
  PilotOrder order;
  order.game = PilotGame::None;
  return order;
}

juce::String
pilotStyleReadout (int channel, juce::String const &padName, PilotStyle style)
{
  return "CH" + juce::String (channel + 1) + " " + padName + " "
         + pilotWord (style).toUpperCase ();
}

juce::String
readoutWord (PilotGame game)
{
  if (game == PilotGame::Encircle)
    return "CIRCLE";
  return pilotWord (game).toUpperCase ();
}

juce::String
pilotReadout (int channel, juce::String const &padName, PilotGame game)
{
  auto const lead = "CH" + juce::String (channel + 1) + " " + padName + " ";
  if (game == PilotGame::None)
    return lead + "NO GAME";
  return lead + "GAME " + readoutWord (game);
}

}
