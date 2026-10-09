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


#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <a3-motion-ui/components/ControllerComponent.hh>
#include <a3-motion-ui/components/LookAndFeel.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

using namespace a3;

// The PADS page's end keys, painted: each stands where the panel's key does,
// wears the look its LED shows, and under SHIFT shows the layer under it.

namespace
{
struct Page
{
  Page ()
  {
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    page.setBounds (0, 0, 768, 586);
  }

  ~Page () { juce::LookAndFeel::setDefaultLookAndFeel (nullptr); }

  juce::Image
  paint (FunctionKeyLook const &keys, RoomLook const &room)
  {
    page.setEndKeyLook (keys, room);
    return page.createComponentSnapshot (page.getLocalBounds ());
  }

  juce::Rectangle<int>
  keyAt (PanelSide side, int row) const
  {
    auto const layout = layOutController (page.getLocalBounds (), 0.f, 0);
    for (std::size_t i = 0; i < numPanelKeys; ++i)
      if (panelKeyPlaces[i].side == side && panelKeyPlaces[i].row == row)
        return layout.keys[i];
    return {};
  }

  LookAndFeel_A3 lookAndFeel;
  ControllerComponent page;
};

/** A point inside a key, clear of its rounded corner and of the mark in its
 *  middle: the ground the key is filled with. */
juce::Colour
groundOf (juce::Image const &image, juce::Rectangle<int> key)
{
  return image.getPixelAt (key.getX () + key.getWidth () / 8,
                           key.getCentreY ());
}

/** How many of a key's pixels are close to `colour`. */
int
pixelsLike (juce::Image const &image, juce::Rectangle<int> key,
            juce::Colour colour)
{
  auto count = 0;
  for (int y = key.getY (); y < key.getBottom (); ++y)
    for (int x = key.getX (); x < key.getRight (); ++x)
      {
        auto const p = image.getPixelAt (x, y);
        if (std::abs (p.getRed () - colour.getRed ()) < 24
            && std::abs (p.getGreen () - colour.getGreen ()) < 24
            && std::abs (p.getBlue () - colour.getBlue ()) < 24)
          ++count;
      }
  return count;
}
}

TEST (PadsPagePaint, PlayAllIsFilledWithItsLookOnBothSides)
{
  Page p;
  RoomLook room;
  room.anyClip = true;
  room.anythingPlays = true;
  auto const image = p.paint ({}, room);
  auto const lit = endKeyColour (EndKey::PlayAll, {}, room);

  for (auto const side : { PanelSide::Left, PanelSide::Right })
    EXPECT_EQ (groundOf (image, p.keyAt (side, 2)), lit);
}

// A1 on the left and A2 beside it on the right are two keys, each showing its
// own action: A2 running is white, A1 waiting at the idle shade.
TEST (PadsPagePaint, EachActionKeyShowsItsOwnAction)
{
  Page p;
  RoomLook room;
  room.actionAssigned = { true, true, false, false, false, false };
  room.actionRuns[1] = true;
  auto const image = p.paint ({}, room);

  EXPECT_EQ (groundOf (image, p.keyAt (PanelSide::Left, 3)),
             endKeyColour (EndKey::Action1, {}, room));
  EXPECT_EQ (groundOf (image, p.keyAt (PanelSide::Right, 3)),
             endKeyColour (EndKey::Action2, {}, room));
  EXPECT_NE (groundOf (image, p.keyAt (PanelSide::Left, 3)),
             groundOf (image, p.keyAt (PanelSide::Right, 3)));
}

// Holding SHIFT shows where the four moved keys are: PLAY all says REC in
// REC's red, and the free row 4 goes bare.
TEST (PadsPagePaint, ShiftShowsTheLayerUnderIt)
{
  Page p;
  FunctionKeyLook keys;
  RoomLook const room;
  auto const plain = p.paint (keys, room);
  keys.shiftHeld = true;
  auto const shifted = p.paint (keys, room);

  auto const red = functionKeyColour (FunctionKey::Record, keys);
  auto const playAll = p.keyAt (PanelSide::Right, 2);
  EXPECT_EQ (pixelsLike (plain, playAll, red), 0);
  EXPECT_GT (pixelsLike (shifted, playAll, red), 10) << "the word REC";

  EXPECT_EQ (groundOf (shifted, p.keyAt (PanelSide::Left, 4)),
             toColour (theme ().surface));
  EXPECT_NE (groundOf (plain, p.keyAt (PanelSide::Left, 4)),
             toColour (theme ().surface));
}
