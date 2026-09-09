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

#include <a3-motion-ui/components/ControllerLayout.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

namespace a3
{

namespace
{
// Fractions of the bounds, not measurements. The track is a slot a third of
// its column wide -- measured against REAPER's own mixer (a screenshot of
// REAPER v7.78, one "1-channelbus" strip, 92px wide, read by pixel profile)
// this is already close to its ~20-28%, so it is left alone: the fault
// REAPER's picture points at is the cap, not the track.
constexpr float trackWidthOfBounds = 1.f / 3.f;
constexpr float captionHeightOfBounds = 1.f / 5.f;

// The same REAPER measurement: the cap's height is 15% of the *track's*
// height. The old formula (`capH = min(trackW, trackHeight)`) made the cap
// square on the track's width instead, which is why a 176px throw drew a
// 59px cap in a 118px track -- it read as a fill level, not a handle. A
// fader's cap is flat because a small, fixed share of the track's length
// leaves the same several-cap-heights of travel whatever the track's width
// happens to be, where a share of the width shrinks the travel to nothing as
// soon as the track gets tall and narrow.
constexpr float capHeightOfTrackHeight = 0.15f;
}

FaderGeometry
faderGeometry (juce::Rectangle<int> bounds, ControlMetrics metrics,
               float fraction)
{
  if (bounds.isEmpty ())
    return {};

  // Half a knob is the floor under both shares that follow, and it is the
  // same floor for both reasons: a caption that shrank with the cell would be
  // unreadable before the cell was too small to draw, and a track that shrank
  // with it would be too narrow to land on first. Pot Size is the nearest
  // thing this device has to a statement of how big a thing has to be to be
  // read and aimed at, so half of it is what a part of a control gets when
  // its share of the cell comes out smaller. VuMeter.cc floors its output
  // block's caption the same way, for the first of those two reasons.
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

  // Flat, and exactly the track's own width -- a fader's cap does not
  // overhang the track (see capHeightOfTrackHeight above for the
  // measurement this rests on). Clamped to at least a pixel and never past
  // the whole track, so a track too short to hold a real cap still returns
  // one rather than dividing by nothing.
  auto const trackH = track.getHeight ();
  auto const capH = trackH > 0
                        ? juce::jlimit (1, trackH,
                                        juce::roundToInt (
                                            trackH * capHeightOfTrackHeight))
                        : 0;
  auto const travel = trackH - capH;
  auto const top = track.getY ()
                   + juce::roundToInt (travel * (1.f - juce::jlimit (0.f, 1.f,
                                                                    fraction)));
  auto const cap = juce::Rectangle<int> (track.getX (), top, trackW, capH);

  // The trap in a flatter cap: it is a smaller *drawn* target, and a smaller
  // drawn target must not be a smaller one to *touch*. Grown around the
  // cap's own centre up to fingertipSize in both dimensions rather than
  // shrunk to match the picture, and kept inside `bounds` -- not just the
  // track -- since there is room to spare in the caption band below and a
  // hit area escaping the control's own cell would start covering its
  // neighbour's.
  auto const hitArea
      = juce::Rectangle<int> (juce::jmax (fingertipSize, trackW),
                              juce::jmax (fingertipSize, capH))
            .withCentre (cap.getCentre ())
            .constrainedWithin (bounds);

  return { track, cap, hitArea, caption };
}

int
faderHeightForThrow (int width, int maxHeight, ControlMetrics metrics)
{
  if (width <= 0)
    return 0;

  // Both the travel and the cap grow monotonically with the height -- one
  // extra pixel of cell adds at most one to the caption -- so the first
  // height that satisfies the test is also the smallest, and the walk can
  // stop there. Not proven by a test (see the docstring above), and a flat
  // cap makes it less obviously true than a square one did, since the hit
  // area's floor stops growing with the height until the cap itself clears
  // fingertipSize.
  for (auto height = 1; height <= maxHeight; ++height)
    {
      auto const bounds = juce::Rectangle<int> (width, height);
      auto const bottom = faderGeometry (bounds, metrics, 0.f);
      auto const top = faderGeometry (bounds, metrics, 1.f);

      // The cap has to exist before its travel means anything -- a cell too
      // short to hold one at all comes back with a cap of zero height (see
      // faderGeometry's capH). And it is minTravelInHitAreaHeights times the
      // *hit area's* height the travel has to clear, not the cap's own: a
      // flat cap has almost no height of its own, so a cell just tall enough
      // to nudge it a few pixels would otherwise count as a fader nobody's
      // fingertip could actually land on and follow.
      if (!bottom.cap.isEmpty ()
          && static_cast<float> (bottom.cap.getY () - top.cap.getY ())
                 >= bottom.hitArea.getHeight () * minTravelInHitAreaHeights)
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
