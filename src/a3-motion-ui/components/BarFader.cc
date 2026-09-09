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

#include "BarFader.hh"

#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

namespace a3
{

namespace
{
// Fractions of the bounds, not measurements. The track is a slot the width of
// a third of its column; the cap is as tall as the track is wide, which keeps
// it square-ish at every aspect and therefore hittable without a rule of its
// own.
constexpr float trackWidthOfBounds = 1.f / 3.f;
constexpr float captionHeightOfBounds = 1.f / 5.f;
}

FaderGeometry
faderGeometry (juce::Rectangle<int> bounds, ControlMetrics metrics,
               float fraction)
{
  if (bounds.isEmpty ())
    return {};

  auto area = bounds;
  auto const caption = area.removeFromBottom (juce::jmax (
      metrics.knobDiam / 2,
      juce::roundToInt (bounds.getHeight () * captionHeightOfBounds)));

  auto const trackW = juce::jmax (
      metrics.knobDiam / 2,
      juce::roundToInt (bounds.getWidth () * trackWidthOfBounds));
  auto const track
      = juce::Rectangle<int> (trackW, area.getHeight ())
            .withCentre ({ area.getCentreX (), area.getCentreY () });

  // As tall as the track is wide: square-ish whatever the aspect, so it needs
  // no threshold of its own to stay hittable. Clamped into the track at both
  // ends, or the last part of the throw could not be reached.
  auto const capH = juce::jmin (trackW, track.getHeight ());
  auto const travel = track.getHeight () - capH;
  auto const top = track.getY ()
                   + juce::roundToInt (travel * (1.f - juce::jlimit (0.f, 1.f,
                                                                    fraction)));

  return { track, juce::Rectangle<int> (track.getX (), top, trackW, capH),
           caption };
}

int
faderHeightForThrow (int width, int maxHeight, ControlMetrics metrics)
{
  if (width <= 0)
    return 0;

  // Both the travel and the cap grow monotonically with the height -- one
  // extra pixel of cell adds at most one to the caption -- so the first
  // height that satisfies the test is also the smallest, and the walk can
  // stop there.
  for (auto height = 1; height <= maxHeight; ++height)
    {
      auto const bounds = juce::Rectangle<int> (width, height);
      auto const bottom = faderGeometry (bounds, metrics, 0.f).cap;
      auto const top = faderGeometry (bounds, metrics, 1.f).cap;

      // The cap has to exist before its travel means anything. A cell too
      // short to hold one at all comes back with a cap of zero height, and
      // "travelled at least as far as it is tall" is then true of standing
      // still -- which is how a one-pixel row first claimed to be a fader.
      if (!bottom.isEmpty ()
          && bottom.getY () - top.getY () >= bottom.getHeight ())
        return height;
    }

  return 0;
}

void
paintBarFader (juce::Graphics &g, juce::Rectangle<int> bounds,
               ControlMetrics metrics, juce::Colour channelColour,
               juce::String const &label, float fraction, bool isActive,
               bool isSelected)
{
  auto const geometry = faderGeometry (bounds, metrics, fraction);
  if (geometry.track.isEmpty ())
    return;

  auto const &t = theme ();

  g.setColour (toColour (t.surfaceRaised));
  g.fillRect (geometry.track);

  // Filled from the bottom, because that is where a fader's travel starts and
  // what is filled is how far it has come.
  auto filled = geometry.track;
  filled = filled.withTop (geometry.cap.getCentreY ());
  g.setColour (isActive ? channelColour
                        : channelColour.withMultipliedAlpha (t.padShadeIdle));
  g.fillRect (filled);

  // textMuted is this theme's "not the thing you are looking at" role --
  // paintBarKnob's captionColour() reaches for the same one when a control is
  // not selected, and a second name for the same idea is a name that could
  // drift from it.
  g.setColour (isSelected ? toColour (t.textPrimary) : toColour (t.textMuted));
  g.fillRect (geometry.cap);

  g.setFont (juce::Font (juce::FontOptions{}.withHeight (metrics.captionSize)));
  g.drawText (label, geometry.caption, juce::Justification::centred, false);
}

}
