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
  return layOutBarKeyboard (keyboardFieldsOf (bar), page);
}

}

TEST (BarKeyboardLayout, EveryRowFillsTwelveSlotsOnBothPages)
{
  for (auto page : bothPages)
    for (int row = 0; row < keyboardRows; ++row)
      {
        int slots = 0;
        for (auto const &key : keyboardRow (page, row))
          slots += key.span;
        EXPECT_EQ (slots, keyboardSlotsPerRow) << "row " << row;
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
  // All three size sets: the bar grows with its fonts, but the controller
  // floor keeps four fingertip rows even at the smallest.
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

TEST (BarKeyboardLayout, KeysStandInTheEncodersFieldColumns)
{
  // The eight fields are the encoders' four by two. A key belongs to the
  // columns it spans and never crosses a column edge half way.
  for (auto sizes : barSizes)
    for (auto page : bothPages)
      {
        auto const bar = deviceBar (sizes);
        auto const fields = keyboardFieldsOf (bar);
        for (auto const &key : keysOn (bar, page))
          {
            auto const firstColumn = key.firstSlot / keyboardSlotsPerColumn;
            auto const lastColumn
                = (key.firstSlot + key.def.span - 1) / keyboardSlotsPerColumn;
            auto const fieldRow = key.row / 2;
            auto const &left
                = fields[static_cast<size_t> (fieldRow * 4 + firstColumn)];
            auto const &right
                = fields[static_cast<size_t> (fieldRow * 4 + lastColumn)];
            EXPECT_GE (key.bounds.getX (), left.getX ());
            EXPECT_LE (key.bounds.getRight (), right.getRight ());
            EXPECT_GE (key.bounds.getY (), left.getY ());
            EXPECT_LE (key.bounds.getBottom (), left.getBottom ());
          }
      }
}

TEST (BarKeyboardLayout, TheFieldsAreTheBarsOwnPageFields)
{
  auto const bar = deviceBar (barSizes[0]);
  auto const fields = keyboardFieldsOf (bar);
  for (size_t i = 0; i < fields.size (); ++i)
    EXPECT_EQ (fields[i], bar.pageFields[i]);
}

TEST (BarKeyboardLayout, TheFieldsDoNotDependOnThePageShown)
{
  // ACTION and CHMIX lay no fields out; the keyboard stands on them too.
  auto const sizes = barSizes[0];
  auto const knobDiam = knobDiameterForFont (sizes.body, sizes.pot);
  auto const height = clipSettingsPreferredHeight (sizes.header, sizes.body,
                                                   knobDiam)
                      + channelRowHeight (knobDiam, 768);
  auto const clip = layOutClipSettings ({ 0, 0, 768, height }, sizes.header,
                                        sizes.body, sizes.pot, BarPage::Clip);
  auto const action
      = layOutClipSettings ({ 0, 0, 768, height }, sizes.header, sizes.body,
                            sizes.pot, BarPage::Action);
  EXPECT_EQ (keyboardFieldsOf (clip), keyboardFieldsOf (action));
}

TEST (BarKeyboardLayout, EveryCharacterANameOrScriptNeedsIsReachable)
{
  std::set<juce::juce_wchar> reachable;
  for (auto page : bothPages)
    for (int row = 0; row < keyboardRows; ++row)
      for (auto const &key : keyboardRow (page, row))
        if (key.action == KeyAction::Character)
          {
            reachable.insert (key.character);
            reachable.insert (upperCaseOf (key.character));
          }

  juce::String const needed
      = juce::String ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
                      "0123456789 -_.")
        // What the shipped scripts, clips and sets are written with.
        + "/\":,~;=\\{}'()[]+*<>|"
        + juce::String (juce::CharPointer_UTF8 (
            "\xc3\xa4\xc3\xb6\xc3\xbc\xc3\x84\xc3\x96\xc3\x9c\xc3\x9f"));

  for (auto character : needed)
    EXPECT_TRUE (reachable.count (character) > 0)
        << "missing: " << juce::String::charToString (character);
}

TEST (BarKeyboardLayout, BothPagesCarryTheEditingKeysInTheSamePlace)
{
  // Backspace, Enter, the cursor, Escape and Hide do not move when the page
  // changes -- a hand that has found them keeps finding them.
  for (auto action : { KeyAction::Backspace, KeyAction::Enter, KeyAction::Left,
                       KeyAction::Right, KeyAction::Escape, KeyAction::Hide,
                       KeyAction::Page })
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

TEST (BarKeyboardLayout, EachEncoderOwnsTheKeysOfItsField)
{
  auto const bar = deviceBar (barSizes[0]);
  auto const keys = keysOn (bar, KeyboardPage::Letters);
  auto const fields = keyboardFieldsOf (bar);
  std::set<size_t> owned;
  for (int block = 0; block < 8; ++block)
    {
      auto const inBlock = keysOfBlock (keys, block);
      EXPECT_FALSE (inBlock.empty ()) << "block " << block;
      for (auto index : inBlock)
        {
          EXPECT_TRUE (keys[index].bounds.intersects (
              fields[static_cast<size_t> (block)]));
          owned.insert (index);
        }
    }
  // Every key is reachable from some encoder.
  EXPECT_EQ (owned.size (), keys.size ());
}

TEST (BarKeyboardLayout, BlockKeysAreInReadingOrder)
{
  auto const keys
      = keysOn (deviceBar (barSizes[0]), KeyboardPage::Letters);
  // Top left field: q w e / a s d.
  auto const block = keysOfBlock (keys, 0);
  ASSERT_EQ (block.size (), 6u);
  juce::String read;
  for (auto index : block)
    read += juce::String::charToString (keys[index].def.character);
  EXPECT_EQ (read, "qweasd");
}

TEST (BarKeyboardLayout, KeyAtFindsTheKeyUnderAPoint)
{
  auto const keys
      = keysOn (deviceBar (barSizes[0]), KeyboardPage::Letters);
  for (size_t i = 0; i < keys.size (); ++i)
    EXPECT_EQ (keyAt (keys, keys[i].bounds.getCentre ()),
               static_cast<int> (i));
  EXPECT_EQ (keyAt (keys, { -100, -100 }), -1);
}
