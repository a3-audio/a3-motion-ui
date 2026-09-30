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

#include <a3-motion-ui/theme/ThemeColours.hh>

namespace a3
{

namespace
{
// A list key against the bar keys it opens from: twice as wide, so the
// longest name fits at the header size, and taller, since it is aimed at
// with a finger over the sphere.
constexpr float keyWidthOfAnchor = 2.f;
constexpr float keyHeightOfAnchor = 1.6f;
constexpr float gapOfAnchorHeight = 0.2f;
}

WorkspaceList::WorkspaceList () { setVisible (false); }

void
WorkspaceList::show (std::vector<workspaces::Workspace> entries,
                     juce::Rectangle<int> anchor)
{
  _entries = std::move (entries);
  _anchor = anchor;
  setVisible (!_entries.empty ());
  if (isVisible ())
    toFront (false);
  repaint ();
}

int
WorkspaceList::keyCount () const
{
  return static_cast<int> (_entries.size ());
}

juce::Rectangle<int>
WorkspaceList::keyArea (int index) const
{
  auto const anchorHeight = static_cast<float> (_anchor.getHeight ());
  auto const width
      = juce::roundToInt (static_cast<float> (_anchor.getWidth ())
                          * keyWidthOfAnchor);
  auto const height = juce::roundToInt (anchorHeight * keyHeightOfAnchor);
  auto const gap = juce::roundToInt (anchorHeight * gapOfAnchorHeight);

  auto const column
      = juce::Rectangle<int> (_anchor.getRight () - width,
                              _anchor.getBottom () + gap, width,
                              keyCount () * (height + gap))
            .constrainedWithin (getLocalBounds ());

  return column.withY (column.getY () + index * (height + gap))
      .withHeight (height);
}

void
WorkspaceList::paint (juce::Graphics &g)
{
  g.setFont (juce::Font (
      juce::FontOptions (theme ().fontSize (FontRole::Header))));

  for (int i = 0; i < keyCount (); ++i)
    {
      auto const &entry = _entries[static_cast<size_t> (i)];
      auto const face = keyArea (i).toFloat ();

      // Solid, not washed: the sphere moves behind it.
      g.setColour (toColour (theme ().surfaceRaised));
      g.fillRoundedRectangle (face, theme ().radiusControl);
      if (entry.current)
        {
          g.setColour (toColour (theme ().accent, theme ().alphaFillEmphasis));
          g.fillRoundedRectangle (face, theme ().radiusControl);
        }
      g.setColour (toColour (theme ().textPrimary, theme ().alphaOutline));
      g.drawRoundedRectangle (face, theme ().radiusControl,
                              theme ().strokeThin);

      g.setColour (entry.current ? toColour (theme ().accent)
                                 : toColour (theme ().textPrimary));
      g.drawFittedText (entry.label, keyArea (i), juce::Justification::centred,
                        1);
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
