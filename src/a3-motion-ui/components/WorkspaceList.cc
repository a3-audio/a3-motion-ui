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

#include "WorkspaceList.hh"


namespace a3
{

namespace switcherLook
{
namespace
{
// StemDeck's Theme.h.
juce::Colour const keyFace{ 0xff26282c };
juce::Colour const currentFace{ 0xff58595c };
juce::Colour const outline{ 0xff34363b };
juce::Colour const text{ 0xffdcdcdc };
juce::Colour const currentText{ 0xff000000 };
juce::Colour const panel{ 0xff1c1d20 };
// LookAndFeel_V4's, not the skin's: the switch looks the same in both apps.
constexpr float cornerSize = 6.f;
constexpr float outlineWidth = 1.f;
constexpr float halfPixel = 0.5f;
}

void
paintKey (juce::Graphics &g, juce::Rectangle<int> area,
          juce::String const &label, bool current)
{
  // LookAndFeel_V4's drawButtonBackground and drawButtonText, which is what
  // StemDeck's keys are.
  auto const face = area.toFloat ().reduced (halfPixel);
  g.setColour (current ? currentFace : keyFace);
  g.fillRoundedRectangle (face, cornerSize);
  g.setColour (outline);
  g.drawRoundedRectangle (face, cornerSize, outlineWidth);

  auto const font = juce::Font (juce::FontOptions (
      juce::jmin (16.f, static_cast<float> (area.getHeight ()) * 0.6f)));
  auto const yIndent = juce::jmin (4, juce::roundToInt (static_cast<float> (area.getHeight ()) * 0.3f));
  auto const indent = juce::jmin (
      juce::roundToInt (font.getHeight () * 0.6f),
      2 + juce::jmin (area.getWidth (), area.getHeight ()) / 4);

  g.setFont (font);
  g.setColour (current ? currentText : text);
  g.drawFittedText (label, area.reduced (indent, yIndent),
                    juce::Justification::centred, 2);
}

void
paintPanel (juce::Graphics &g, juce::Rectangle<int> area)
{
  auto const face = area.toFloat ();
  g.setColour (panel);
  g.fillRoundedRectangle (face, cornerSize);
  g.setColour (outline);
  g.drawRoundedRectangle (face, cornerSize, outlineWidth);
}
}


WorkspaceList::WorkspaceList () { setVisible (false); }

void
WorkspaceList::show (std::vector<workspaces::Workspace> entries)
{
  _entries = std::move (entries);
  setVisible (!_entries.empty ());
  if (isVisible ())
    toFront (false);
  repaint ();
}

juce::Rectangle<int>
WorkspaceList::columnArea () const
{
  // Placed in the window's coordinates, where StemDeck's column stands,
  // though this list is the sphere's child and starts below the bar.
  auto const *window = getTopLevelComponent ();
  auto const switcher = workspaces::switcherGeometry (window->getWidth ());
  auto const height
      = keyCount () * (switcher.listKeyHeight + switcher.listGap)
        + switcher.listGap;
  auto const inWindow = juce::Rectangle<int> (
      window->getWidth () - switcher.margin - switcher.listWidth,
      switcher.listTop, switcher.listWidth, height);

  return getLocalArea (window, inWindow).constrainedWithin (getLocalBounds ());
}

int
WorkspaceList::keyCount () const
{
  return static_cast<int> (_entries.size ());
}

juce::Rectangle<int>
WorkspaceList::keyArea (int index) const
{
  auto const switcher = workspaces::switcherGeometry (
      getTopLevelComponent ()->getWidth ());
  auto const step = switcher.listKeyHeight + switcher.listGap;

  return columnArea ()
      .reduced (switcher.listGap)
      .withTrimmedTop (index * step)
      .withHeight (switcher.listKeyHeight);
}

void
WorkspaceList::paint (juce::Graphics &g)
{
  switcherLook::paintPanel (g, columnArea ());
  for (int i = 0; i < keyCount (); ++i)
    {
      auto const &entry = _entries[static_cast<size_t> (i)];
      switcherLook::paintKey (g, keyArea (i), entry.label, entry.current);
    }
}

void
WorkspaceList::mouseUp (juce::MouseEvent const &event)
{
  setVisible (false);

  for (int i = 0; i < keyCount (); ++i)
    if (keyArea (i).contains (event.getPosition ()))
      {
        if (onChosen)
          onChosen (_entries[static_cast<size_t> (i)].number);
        return;
      }
}

}
