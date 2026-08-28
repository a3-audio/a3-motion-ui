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

#include "TrajectoryIcon.hh"

#include <a3-motion-engine/TrajectoryShape.hh>

#include <a3-motion-ui/theme/ThemeColours.hh>

#include <algorithm>
#include <cmath>
#include <limits>

namespace a3
{

TrajectoryIconData
trajectoryIconFromPath (juce::Path const &path,
                        std::vector<std::pair<float, float>> const &jumpDots)
{
  TrajectoryIconData data;

  if (path.isEmpty () && jumpDots.empty ())
    return data;

  if (!jumpDots.empty () && path.isEmpty ())
    {
      data.hasJumpDots = true;
      data.jumpDots = jumpDots;
    }
  else
    {
      data.path = path;
    }

  data.hasIcon = true;
  return data;
}

TrajectoryIconData
trajectoryIconFromTicks (std::vector<Pos> const &ticks)
{
  TrajectoryIconData data;

  if (ticks.empty ())
    return data;

  // Find bounding box of valid ticks (XY only)
  float minX = std::numeric_limits<float>::max ();
  float maxX = std::numeric_limits<float>::lowest ();
  float minY = std::numeric_limits<float>::max ();
  float maxY = std::numeric_limits<float>::lowest ();
  int validCount = 0;

  for (auto const &pos : ticks)
    {
      if (!pos.isValid ())
        continue;
      auto x = pos.x ();
      auto y = pos.y ();
      if (x < minX) minX = x;
      if (x > maxX) maxX = x;
      if (y < minY) minY = y;
      if (y > maxY) maxY = y;
      ++validCount;
    }

  if (validCount < 2)
    return data;

  // Normalise to [-1, 1] range with aspect ratio preserved
  auto rangeX = maxX - minX;
  auto rangeY = maxY - minY;
  auto range = std::max (rangeX, rangeY);
  if (range < 1e-6f)
    range = 1.f;

  auto centreX = (minX + maxX) * 0.5f;
  auto centreY = (minY + maxY) * 0.5f;

  auto const normalise = [centreX, centreY, range] (Pos const &pos) {
    return juce::Point<float>{ (pos.x () - centreX) / (range * 0.5f),
                               (pos.y () - centreY) / (range * 0.5f) };
  };

  // Tapped or drawn is a question about the shape of the data, not about how
  // much of it is missing. Counting invalid ticks only worked while a take was
  // still being played in; once its seams are closed every tick is valid, and
  // a tap take then went down the drawn branch and got a straight line put
  // across every one of its jumps.
  if (isTappedTrajectory (ticks))
    {
      data.hasJumpDots = true;
      for (auto const &held : trajectoryPlateaus (ticks))
        {
          auto const point = normalise (held);

          // Two taps a hair apart are one dot: the icon is a few millimetres
          // across and cannot show the difference anyway.
          auto const isDuplicate = std::any_of (
              data.jumpDots.begin (), data.jumpDots.end (),
              [&point] (auto const &p) {
                return std::abs (p.first - point.x) < 0.05f
                       && std::abs (p.second - point.y) < 0.05f;
              });
          if (!isDuplicate)
            data.jumpDots.push_back ({ point.x, point.y });
        }
    }
  else
    {
      // At most this many points per stroke; beyond it the icon gains no
      // detail the eye can find at this size.
      auto const maxIconPoints = 128;

      for (auto const &segment : trajectorySegments (ticks))
        {
          auto const step = std::max (
              size_t{ 1 }, segment.size () / static_cast<size_t> (maxIconPoints));

          std::vector<juce::Point<float> > pts;
          for (size_t i = 0; i < segment.size (); i += step)
            pts.push_back (normalise (segment[i]));

          if (pts.size () < 2)
            continue;

          data.path.startNewSubPath (pts[0]);

          auto const dist = pts.front ().getDistanceFrom (pts.back ());
          bool const closed = dist < 0.1f && pts.size () > 4;

          auto const n = static_cast<int> (pts.size ());
          for (int i = 0; i < n - 1; ++i)
            {
              juce::Point<float> p0, p1, p2, p3;
              p1 = pts[static_cast<size_t> (i)];
              p2 = pts[static_cast<size_t> (i + 1)];

              if (closed)
                {
                  p0 = pts[static_cast<size_t> ((i - 1 + n) % n)];
                  p3 = pts[static_cast<size_t> ((i + 2) % n)];
                }
              else
                {
                  p0 = (i > 0) ? pts[static_cast<size_t> (i - 1)]
                               : p1 + (p1 - p2);
                  p3 = (i + 2 < n) ? pts[static_cast<size_t> (i + 2)]
                                   : p2 + (p2 - p1);
                }

              auto cp1 = p1 + (p2 - p0) / 6.f;
              auto cp2 = p2 - (p3 - p1) / 6.f;
              data.path.cubicTo (cp1, cp2, p2);
            }
        }

      if (data.path.isEmpty ())
        return data;
    }

  data.hasIcon = true;
  return data;
}

void
drawTrajectoryIcon (juce::Graphics &g, juce::Rectangle<float> area,
                    TrajectoryIconData const &data, juce::Colour colour)
{
  if (!data.hasIcon)
    return;

  auto const cx = area.getCentreX ();
  auto const cy = area.getCentreY ();
  auto const r = area.getWidth () * 0.45f;
  auto const strokeThickness = 1.5f;
  auto const outlineThickness = strokeThickness + 2.0f;
  // A dark outline behind the stroke, so the icon stays readable on a
  // channel-coloured pad. A structure, not a state.
  constexpr float outlineOpacity = 0.6f;
  auto const outlineColour = toColour (theme ().surface, outlineOpacity);

  if (data.hasJumpDots)
    {
      // HOA→JUCE: screen x = -HOA_y, screen y = -HOA_x
      // Small enough that four of them read as four marks rather than as one
      // cluster: at 0.22 of the icon's radius they crowded the field.
      auto const dotR = r * 0.12f;
      auto const dotOutR = dotR + 1.0f;
      for (auto const &p : data.jumpDots)
        {
          auto const x = cx - p.second * r;
          auto const y = cy - p.first * r;
          g.setColour (outlineColour);
          g.fillEllipse (x - dotOutR, y - dotOutR, dotOutR * 2.f, dotOutR * 2.f);
          g.setColour (colour);
          g.fillEllipse (x - dotR, y - dotR, dotR * 2.f, dotR * 2.f);
        }
    }
  else
    {
      // The path is in HOA normalised [-1,1] space. Convert to JUCE screen
      // coords: JUCE x = -HOA_y, JUCE y = -HOA_x, then scale to the icon area.
      auto transform = juce::AffineTransform (
           0.f, -r, cx,   // JUCE x = -HOA_y * r + cx
          -r,  0.f, cy);  // JUCE y = -HOA_x * r + cy
      g.setColour (outlineColour);
      g.strokePath (data.path, juce::PathStrokeType (outlineThickness), transform);
      g.setColour (colour);
      g.strokePath (data.path, juce::PathStrokeType (strokeThickness), transform);
    }
}

}
