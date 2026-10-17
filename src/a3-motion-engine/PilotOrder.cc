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

#include "PilotOrder.hh"

namespace a3
{

namespace
{
constexpr PilotGame allGames[] = { PilotGame::None,      PilotGame::FakeOut,
                                   PilotGame::Formation, PilotGame::HideAndSeek,
                                   PilotGame::CallAndResponse, PilotGame::Slap,
                                   PilotGame::Climb,     PilotGame::Graze,
                                   PilotGame::Encircle };
constexpr PilotStyle allStyles[] = { PilotStyle::Auto, PilotStyle::Hard, PilotStyle::Rising,
                                     PilotStyle::Soft, PilotStyle::Groove, PilotStyle::None };
constexpr PilotRecruit allRecruits[] = { PilotRecruit::Self, PilotRecruit::Nearest,
                                         PilotRecruit::All };
constexpr PilotTargetKind namedKinds[] = { PilotTargetKind::Nearest, PilotTargetKind::Crowd,
                                           PilotTargetKind::Hotspot };
}

juce::String
pilotWord (PilotGame game)
{
  switch (game)
    {
    case PilotGame::None:
      return "none";
    case PilotGame::FakeOut:
      return "fakeout";
    case PilotGame::Formation:
      return "formation";
    case PilotGame::HideAndSeek:
      return "hideseek";
    case PilotGame::CallAndResponse:
      return "callresponse";
    case PilotGame::Slap:
      return "slap";
    case PilotGame::Climb:
      return "climb";
    case PilotGame::Graze:
      return "graze";
    case PilotGame::Encircle:
      return "encircle";
    }
  return "none";
}

juce::String
pilotWord (PilotTarget target)
{
  switch (target.kind)
    {
    case PilotTargetKind::Nearest:
      return "nearest";
    case PilotTargetKind::Group:
      return "group" + juce::String (target.groupId + 1);
    case PilotTargetKind::Crowd:
      return "crowd";
    case PilotTargetKind::Hotspot:
      return "hotspot";
    }
  return "nearest";
}

juce::String
pilotWord (PilotRecruit with)
{
  switch (with)
    {
    case PilotRecruit::Self:
      return "self";
    case PilotRecruit::Nearest:
      return "nearest";
    case PilotRecruit::All:
      return "all";
    }
  return "self";
}

std::optional<PilotGame>
pilotGameNamed (juce::String const &word)
{
  for (auto const game : allGames)
    if (word == pilotWord (game))
      return game;
  return std::nullopt;
}

std::optional<PilotTarget>
pilotTargetNamed (juce::String const &word)
{
  for (auto const kind : namedKinds)
    if (word == pilotWord (PilotTarget{ kind, -1 }))
      return PilotTarget{ kind, -1 };
  // Spelled exactly as written back, so "group01" is not group 1.
  for (auto id = 0; id < pilotGroups; ++id)
    {
      PilotTarget const group{ PilotTargetKind::Group, id };
      if (word == pilotWord (group))
        return group;
    }
  return std::nullopt;
}

std::optional<PilotRecruit>
pilotRecruitNamed (juce::String const &word)
{
  for (auto const with : allRecruits)
    if (word == pilotWord (with))
      return with;
  return std::nullopt;
}

juce::String
pilotWord (PilotStyle style)
{
  switch (style)
    {
    case PilotStyle::Auto:
      return "auto";
    case PilotStyle::Hard:
      return "hard";
    case PilotStyle::Rising:
      return "rising";
    case PilotStyle::Soft:
      return "soft";
    case PilotStyle::Groove:
      return "groove";
    case PilotStyle::None:
      return "none";
    }
  return "auto";
}

std::optional<PilotStyle>
pilotStyleNamed (juce::String const &word)
{
  for (auto const style : allStyles)
    if (word == pilotWord (style))
      return style;
  return std::nullopt;
}

std::optional<PilotFamily>
familyOf (PilotStyle style)
{
  switch (style)
    {
    case PilotStyle::Hard:
      return PilotFamily::Hard;
    case PilotStyle::Rising:
      return PilotFamily::Rising;
    case PilotStyle::Soft:
      return PilotFamily::Soft;
    case PilotStyle::Groove:
      return PilotFamily::Groove;
    case PilotStyle::Auto:
    case PilotStyle::None:
      return std::nullopt;
    }
  return std::nullopt;
}

bool
isContrastFigure (PilotGame game)
{
  switch (game)
    {
    case PilotGame::Slap:
    case PilotGame::Climb:
    case PilotGame::Graze:
    case PilotGame::Encircle:
      return true;
    case PilotGame::None:
    case PilotGame::FakeOut:
    case PilotGame::Formation:
    case PilotGame::HideAndSeek:
    case PilotGame::CallAndResponse:
      return false;
    }
  return false;
}

}
