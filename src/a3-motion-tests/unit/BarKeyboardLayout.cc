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

#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <a3-motion-ui/components/BarKeyboardLayout.hh>
#include <a3-motion-ui/components/ClipSettingsCaptions.hh>
#include <a3-motion-ui/components/ClipSettingsLayout.hh>
#include <a3-motion-ui/components/ControllerLayout.hh>
#include <a3-motion-ui/components/PanelKeyboard.hh>

#include <set>

using namespace a3;

namespace
{

constexpr std::array<KeyboardPage, 2> bothPages{ KeyboardPage::Letters,
                                                 KeyboardPage::Symbols };

// The device: 768 wide, the bar at the height it asks for at the sizes the
// shipped skins use (header 18, body 14, pot 0.9), and at the smallest ones.
struct BarSizes
{
  float header;
  float body;
  float pot;
};

constexpr std::array<BarSizes, 3> barSizes{ BarSizes{ 18.f, 14.f, 0.9f },
                                            BarSizes{ 14.f, 12.f, 1.f },
                                            BarSizes{ 12.f, 10.f, 0.6f } };

ClipSettingsLayout
deviceBar (BarSizes s, int width = 768)
{
  auto const knobDiam = knobDiameterForFont (s.body, s.pot);
  // As ClipSettingsComponent sizes the bar: the PADS page no longer asks
  // for room in it, so the clip settings alone set its height.
  auto const height = clipSettingsPreferredHeight (s.header, s.body, knobDiam)
                      + channelRowHeight (knobDiam, width);
  return layOutClipSettings ({ 0, 0, width, height }, s.header, s.body, s.pot,
                             BarPage::Clip);
}

std::vector<KeyCap>
keysOn (ClipSettingsLayout const &bar, KeyboardPage page)
{
  // Where the keyboard stands: the bar's clip content.
  return layOutPanelKeyboard (bar.clipContent, page);
}

}

TEST (BarKeyboardLayout, EveryKeyLiesInsideTheBarsClipContent)
{
  for (auto sizes : barSizes)
    for (auto page : bothPages)
      {
        auto const bar = deviceBar (sizes);
        for (auto const &key : keysOn (bar, page))
          {
            EXPECT_FALSE (key.bounds.isEmpty ());
            EXPECT_TRUE (bar.clipContent.contains (key.bounds))
                << key.bounds.toString () << " not in "
                << bar.clipContent.toString ();
            // The global strip and the header stay free.
            EXPECT_FALSE (key.bounds.intersects (bar.globalBounds));
            EXPECT_FALSE (key.bounds.intersects (bar.tabClip));
          }
      }
}

TEST (BarKeyboardLayout, NoTwoKeysOverlap)
{
  for (auto sizes : barSizes)
    for (auto page : bothPages)
      {
        auto const keys = keysOn (deviceBar (sizes), page);
        for (size_t a = 0; a < keys.size (); ++a)
          for (size_t b = a + 1; b < keys.size (); ++b)
            EXPECT_FALSE (keys[a].bounds.intersects (keys[b].bounds))
                << keys[a].bounds.toString () << " / "
                << keys[b].bounds.toString ();
      }
}

TEST (BarKeyboardLayout, EveryKeyIsAFingertipAtTheShippedSizes)
{
  for (auto sizes : barSizes)
    for (auto page : bothPages)
      for (auto const &key : keysOn (deviceBar (sizes), page))
        {
          EXPECT_GE (key.bounds.getWidth (), fingertipSize)
              << key.bounds.toString ();
          EXPECT_GE (key.bounds.getHeight (), fingertipSize)
              << key.bounds.toString ();
        }
}

TEST (BarKeyboardLayout, BothPagesCarryTheEditingKeysInTheSamePlace)
{
  // Backspace, Enter, the cursor, Escape and Hide do not move when the page
  // changes -- a hand that has found them keeps finding them.
  for (auto action : { KeyAction::Backspace, KeyAction::Enter, KeyAction::Left,
                       KeyAction::Right, KeyAction::Escape, KeyAction::Hide,
                       KeyAction::Page, KeyAction::Shift })
    {
      auto const bar = deviceBar (barSizes[0]);
      auto const find = [&] (KeyboardPage page) {
        for (auto const &key : keysOn (bar, page))
          if (key.def.action == action)
            return key.bounds;
        return juce::Rectangle<int>{};
      };
      auto const letters = find (KeyboardPage::Letters);
      EXPECT_FALSE (letters.isEmpty ());
      EXPECT_EQ (letters, find (KeyboardPage::Symbols));
    }
}
