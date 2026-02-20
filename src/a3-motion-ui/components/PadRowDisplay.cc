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

#include "PadRowDisplay.hh"

#include <a3-motion-ui/components/LookAndFeel.hh>

#include <cmath>
#include <limits>

namespace a3
{

PadRowDisplay::PadRowDisplay (int rowIndex) : _rowIndex (rowIndex)
{
}

void
PadRowDisplay::resized ()
{
}

void
PadRowDisplay::paint (juce::Graphics &g)
{
  auto bounds = getLocalBounds ();

  // Grey background matching the StatusBar
  g.setColour (Colours::statusBar);
  g.fillRect (bounds);

  // Divide into 4 equal channel sections
  auto const sectionWidth = bounds.getWidth () / numChannels;

  for (int ch = 0; ch < numChannels; ++ch)
    {
      auto sectionBounds
          = bounds.removeFromLeft (ch < numChannels - 1 ? sectionWidth
                                                        : bounds.getWidth ());
      paintCell (g, sectionBounds, ch);

      // Draw thin separator line between sections
      if (ch < numChannels - 1)
        {
          g.setColour (Colours::background);
          g.drawVerticalLine (sectionBounds.getRight (),
                              static_cast<float> (sectionBounds.getY ()),
                              static_cast<float> (sectionBounds.getBottom ()));
        }
    }
}

void
PadRowDisplay::paintCell (juce::Graphics &g, juce::Rectangle<int> bounds,
                          int channel)
{
  auto const &cell = _cells[static_cast<size_t> (channel)];
  auto const colour = cell.colour;

  // Background: fill with channel colour, intensity depends on state
  auto bgAlpha = cell.cellSelected ? 0.85f
                 : cell.rowHighlighted ? 0.55f
                                       : 0.25f;
  g.setColour (colour.withAlpha (bgAlpha));
  g.fillRect (bounds);

  if (cell.trajectoryType == TrajectoryType::Empty && !cell.hasTickData)
    {
      // "---" label: dark outline + channel colour fill
      g.setFont (LayoutHints::fontSize * 0.7f);
      auto const outlineColour = juce::Colours::black.withAlpha (0.4f);
      g.setColour (outlineColour);
      for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy)
          if (dx != 0 || dy != 0)
            g.drawText (cell.label, bounds.translated (dx, dy),
                        juce::Justification::centred, false);
      g.setColour (colour);
      g.drawText (cell.label, bounds, juce::Justification::centred, false);
    }
  else
    {
      // Layout: [prefix] [beats] [icon]
      // prefix = "s"/"u" on the left, beats number in the middle-left,
      // icon on the right
      auto const h = static_cast<float> (bounds.getHeight ());
      auto boundsF = bounds.toFloat ();

      // --- Category prefix (far left) ---
      auto const prefixWidth = h * 0.45f;
      if (cell.categoryPrefix.isNotEmpty ())
        {
          auto const fontSize = h * 0.7f;
          g.setFont (fontSize);
          auto prefixArea = boundsF.removeFromLeft (prefixWidth)
                                .withTrimmedLeft (2.f);

          // Dark outline for readability
          g.setColour (juce::Colours::black.withAlpha (0.5f));
          for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy)
              if (dx != 0 || dy != 0)
                g.drawText (cell.categoryPrefix,
                            prefixArea.translated (
                                static_cast<float> (dx),
                                static_cast<float> (dy)),
                            juce::Justification::centredLeft, false);
          g.setColour (colour.brighter (0.2f));
          g.drawText (cell.categoryPrefix, prefixArea,
                      juce::Justification::centredLeft, false);
        }
      else
        {
          boundsF.removeFromLeft (prefixWidth);
        }

      // --- Beats count (left of icon) ---
      auto const beatsWidth = h * 0.45f;
      if (cell.lengthBeats > 0)
        {
          auto beatsStr = juce::String (cell.lengthBeats);
          auto const fontSize = h * 0.7f;
          g.setFont (fontSize);
          auto beatsArea = boundsF.removeFromLeft (beatsWidth);

          // Dark outline for readability
          g.setColour (juce::Colours::black.withAlpha (0.5f));
          for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy)
              if (dx != 0 || dy != 0)
                g.drawText (beatsStr,
                            beatsArea.translated (
                                static_cast<float> (dx),
                                static_cast<float> (dy)),
                            juce::Justification::centred, false);
          g.setColour (colour.brighter (0.3f));
          g.drawText (beatsStr, beatsArea,
                      juce::Justification::centred, false);
        }
      else
        {
          boundsF.removeFromLeft (beatsWidth);
        }

      // --- Icon (fills remaining space on the right) ---
      auto const iconSize = h * 0.7f;
      auto iconCentre = boundsF.getCentre ();
      auto iconArea = juce::Rectangle<float> (iconSize, iconSize)
                          .withCentre (iconCentre);

      // Prefer tick data icon when available
      if (cell.hasTickData)
        drawTickDataIcon (g, iconArea, channel);
      else
        drawTrajectoryIcon (g, iconArea, cell.trajectoryType, channel);
    }

  // White border when row is highlighted (hovered by encoder)
  if (cell.rowHighlighted)
    {
      g.setColour (juce::Colours::white.withAlpha (0.8f));
      g.drawRect (bounds, 2);
    }

  // Thin baseline
  g.setColour (juce::Colours::black.withAlpha (0.15f));
  g.drawHorizontalLine (bounds.getBottom () - 1,
                         static_cast<float> (bounds.getX ()),
                         static_cast<float> (bounds.getRight ()));
}

void
PadRowDisplay::setTrajectoryType (int channel, TrajectoryType type)
{
  jassert (channel >= 0 && channel < numChannels);
  _cells[static_cast<size_t> (channel)].trajectoryType = type;
  repaint ();
}

void
PadRowDisplay::setTickData (int channel, std::vector<Pos> const &ticks)
{
  jassert (channel >= 0 && channel < numChannels);
  auto &cell = _cells[static_cast<size_t> (channel)];

  cell.tickPath.clear ();
  cell.jumpPoints.clear ();
  cell.hasTickData = false;
  cell.hasJumpTicks = false;

  if (ticks.empty ())
    {
      repaint ();
      return;
    }

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
    {
      repaint ();
      return;
    }

  // Normalise to [-1, 1] range with aspect ratio preserved
  auto rangeX = maxX - minX;
  auto rangeY = maxY - minY;
  auto range = std::max (rangeX, rangeY);
  if (range < 1e-6f)
    range = 1.f;

  auto centreX = (minX + maxX) * 0.5f;
  auto centreY = (minY + maxY) * 0.5f;

  // Downsample: take at most 128 points for the icon path
  auto const maxIconPoints = 128;
  auto const step = std::max (1, static_cast<int> (ticks.size ()) / maxIconPoints);

  // Check if this is a jump-only pattern (majority of ticks are invalid)
  int invalidCount = static_cast<int> (ticks.size ()) - validCount;
  bool jumpPattern = invalidCount > validCount / 2;

  if (jumpPattern)
    {
      // For jump patterns, collect the distinct valid positions as dots
      cell.hasJumpTicks = true;
      for (size_t i = 0; i < ticks.size (); i += static_cast<size_t> (step))
        {
          if (!ticks[i].isValid ())
            continue;
          float nx = (ticks[i].x () - centreX) / (range * 0.5f);
          float ny = (ticks[i].y () - centreY) / (range * 0.5f);

          // Check if this point is already close to an existing one
          bool duplicate = false;
          for (auto const &p : cell.jumpPoints)
            {
              if (std::abs (p.first - nx) < 0.05f
                  && std::abs (p.second - ny) < 0.05f)
                {
                  duplicate = true;
                  break;
                }
            }
          if (!duplicate)
            cell.jumpPoints.push_back ({ nx, ny });
        }
    }
  else
    {
      // Collect downsampled normalised points, splitting at invalid ticks
      std::vector<std::vector<juce::Point<float>>> segments;
      segments.emplace_back ();

      for (size_t i = 0; i < ticks.size (); i += static_cast<size_t> (step))
        {
          if (!ticks[i].isValid ())
            {
              // Start a new segment after a gap
              if (!segments.back ().empty ())
                segments.emplace_back ();
              continue;
            }
          float nx = (ticks[i].x () - centreX) / (range * 0.5f);
          float ny = (ticks[i].y () - centreY) / (range * 0.5f);
          segments.back ().push_back ({ nx, ny });
        }

      // Build Catmull-Rom cubic Bézier path for each segment
      for (auto const &pts : segments)
        {
          if (pts.size () < 2)
            continue;

          cell.tickPath.startNewSubPath (pts[0]);

          // Check if the segment forms a closed loop
          auto const dist = pts.front ().getDistanceFrom (pts.back ());
          bool closed = dist < 0.1f && pts.size () > 4;

          auto const n = static_cast<int> (pts.size ());
          for (int i = 0; i < n - 1; ++i)
            {
              // Catmull-Rom: P0, P1, P2, P3
              // For endpoints, mirror or wrap
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

              // Convert Catmull-Rom to cubic Bézier control points
              auto cp1 = p1 + (p2 - p0) / 6.f;
              auto cp2 = p2 - (p3 - p1) / 6.f;

              cell.tickPath.cubicTo (cp1, cp2, p2);
            }
        }
    }

  cell.hasTickData = true;
  repaint ();
}

void
PadRowDisplay::setIconPath (int channel, juce::Path const &path,
                            std::vector<std::pair<float,float>> const &jumpDots)
{
  jassert (channel >= 0 && channel < numChannels);
  auto &cell = _cells[static_cast<size_t> (channel)];

  cell.tickPath.clear ();
  cell.jumpPoints.clear ();
  cell.hasTickData = false;
  cell.hasJumpTicks = false;

  if (path.isEmpty () && jumpDots.empty ())
    {
      repaint ();
      return;
    }

  if (!jumpDots.empty () && path.isEmpty ())
    {
      // Jump-dot pattern: store the dots directly
      cell.hasJumpTicks = true;
      cell.jumpPoints = jumpDots;
    }
  else
    {
      // Continuous path: use SVG path directly
      cell.tickPath = path;
    }

  cell.hasTickData = true;
  repaint ();
}

void
PadRowDisplay::drawTickDataIcon (juce::Graphics &g,
                                 juce::Rectangle<float> area,
                                 int channel)
{
  auto const &cell = _cells[static_cast<size_t> (channel)];
  auto const cx = area.getCentreX ();
  auto const cy = area.getCentreY ();
  auto const r = area.getWidth () * 0.45f;
  auto const strokeThickness = 1.5f;
  auto const outlineThickness = strokeThickness + 2.0f;
  auto const iconColour = cell.colour;
  auto const outlineColour = juce::Colours::black.withAlpha (0.6f);

  if (cell.hasJumpTicks)
    {
      // Draw dots: dark outline then channel colour fill
      // HOA→JUCE: screen x = -HOA_y, screen y = -HOA_x
      auto const dotR = r * 0.22f;
      auto const dotOutR = dotR + 1.0f;
      for (auto const &p : cell.jumpPoints)
        {
          auto const x = cx - p.second * r;
          auto const y = cy - p.first * r;
          g.setColour (outlineColour);
          g.fillEllipse (x - dotOutR, y - dotOutR,
                         dotOutR * 2.f, dotOutR * 2.f);
          g.setColour (iconColour);
          g.fillEllipse (x - dotR, y - dotR, dotR * 2.f, dotR * 2.f);
        }
    }
  else
    {
      // The SVG path is in HOA normalised [-1,1] space.
      // Convert to JUCE screen coords: JUCE x = -HOA y, JUCE y = -HOA x
      // then scale to the icon area.
      auto transform = juce::AffineTransform (
           0.f, -r, cx,   // JUCE x = -HOA_y * r + cx
          -r,  0.f, cy);  // JUCE y = -HOA_x * r + cy
      // Dark outline for readability
      g.setColour (outlineColour);
      g.strokePath (cell.tickPath,
                    juce::PathStrokeType (outlineThickness),
                    transform);
      // Channel colour stroke on top
      g.setColour (iconColour);
      g.strokePath (cell.tickPath,
                    juce::PathStrokeType (strokeThickness),
                    transform);
    }
}

void
PadRowDisplay::drawTrajectoryIcon (juce::Graphics &g,
                                   juce::Rectangle<float> area,
                                   TrajectoryType type,
                                   int channel)
{
  auto const cx = area.getCentreX ();
  auto const cy = area.getCentreY ();
  auto const r = area.getWidth () * 0.45f;

  auto const &cell = _cells[static_cast<size_t> (channel)];
  auto const iconColour = cell.colour;
  auto const outlineColour = juce::Colours::black.withAlpha (0.6f);
  auto const strokeThickness = 1.5f;
  auto const outlineThickness = strokeThickness + 2.0f;

  // Helper lambdas for outline+fill drawing pattern
  auto strokeOutlined = [&] (juce::Path const &path) {
    g.setColour (outlineColour);
    g.strokePath (path, juce::PathStrokeType (outlineThickness));
    g.setColour (iconColour);
    g.strokePath (path, juce::PathStrokeType (strokeThickness));
  };

  auto ellipseOutlined = [&] (float ex, float ey, float ew, float eh) {
    juce::Path ep;
    ep.addEllipse (ex, ey, ew, eh);
    strokeOutlined (ep);
  };

  auto dotOutlined = [&] (float dx, float dy, float dotR) {
    auto const dotOut = dotR + 1.0f;
    g.setColour (outlineColour);
    g.fillEllipse (dx - dotOut, dy - dotOut, dotOut * 2.f, dotOut * 2.f);
    g.setColour (iconColour);
    g.fillEllipse (dx - dotR, dy - dotR, dotR * 2.f, dotR * 2.f);
  };

  switch (type)
    {
    case TrajectoryType::Circle:
      {
        ellipseOutlined (cx - r, cy - r, r * 2.f, r * 2.f);
        break;
      }
    case TrajectoryType::FigureOfEight:
      {
        juce::Path path;
        auto constexpr numPoints = 64;
        for (int i = 0; i <= numPoints; ++i)
          {
            auto const t
                = static_cast<float> (i) / static_cast<float> (numPoints);
            auto const angle = t * juce::MathConstants<float>::twoPi;
            auto const x = cx + r * std::sin (angle * 2.f);
            auto const y = cy + r * std::sin (angle);
            if (i == 0)
              path.startNewSubPath (x, y);
            else
              path.lineTo (x, y);
          }
        path.closeSubPath ();
        strokeOutlined (path);
        break;
      }
    case TrajectoryType::CornerStep:
      {
        auto const d = r * 0.7f;
        auto const dotR = r * 0.22f;
        dotOutlined (cx - d, cy - d, dotR);
        dotOutlined (cx + d, cy - d, dotR);
        dotOutlined (cx + d, cy + d, dotR);
        dotOutlined (cx - d, cy + d, dotR);
        break;
      }
    case TrajectoryType::Spiral:
      {
        juce::Path path;
        auto constexpr numPoints = 80;
        for (int i = 0; i <= numPoints; ++i)
          {
            auto const t
                = static_cast<float> (i) / static_cast<float> (numPoints);
            auto const angle = t * 4.f * juce::MathConstants<float>::twoPi;
            auto const phase = t < 0.5f ? t * 2.f : (1.f - t) * 2.f;
            auto const rad = r * phase;
            auto const x = cx + rad * std::cos (angle);
            auto const y = cy + rad * std::sin (angle);
            if (i == 0)
              path.startNewSubPath (x, y);
            else
              path.lineTo (x, y);
          }
        strokeOutlined (path);
        break;
      }
    case TrajectoryType::Lissajous:
      {
        juce::Path path;
        auto constexpr numPoints = 80;
        for (int i = 0; i <= numPoints; ++i)
          {
            auto const t
                = static_cast<float> (i) / static_cast<float> (numPoints);
            auto const angle = t * juce::MathConstants<float>::twoPi;
            auto const x = cx + r * std::sin (3.f * angle
                                              + juce::MathConstants<float>::halfPi);
            auto const y = cy + r * std::sin (2.f * angle);
            if (i == 0)
              path.startNewSubPath (x, y);
            else
              path.lineTo (x, y);
          }
        path.closeSubPath ();
        strokeOutlined (path);
        break;
      }
    case TrajectoryType::Rose:
      {
        juce::Path path;
        auto constexpr numPoints = 80;
        bool started = false;
        for (int i = 0; i <= numPoints; ++i)
          {
            auto const t
                = static_cast<float> (i) / static_cast<float> (numPoints);
            auto const theta = t * juce::MathConstants<float>::twoPi;
            auto const rad = r * std::cos (3.f * theta);
            auto const x = cx + rad * std::cos (theta);
            auto const y = cy + rad * std::sin (theta);
            if (!started)
              {
                path.startNewSubPath (x, y);
                started = true;
              }
            else
              path.lineTo (x, y);
          }
        path.closeSubPath ();
        strokeOutlined (path);
        break;
      }
    case TrajectoryType::Zigzag:
      {
        juce::Path path;
        auto constexpr numPoints = 80;
        for (int i = 0; i <= numPoints; ++i)
          {
            auto const t
                = static_cast<float> (i) / static_cast<float> (numPoints);
            auto const angle = t * juce::MathConstants<float>::twoPi;
            auto const tri
                = 2.f
                      * std::abs (2.f * (t * 4.f
                                         - std::floor (t * 4.f + 0.5f)))
                  - 1.f;
            auto const x = cx + r * std::cos (angle) + r * 0.3f * tri;
            auto const y = cy + r * std::sin (angle);
            if (i == 0)
              path.startNewSubPath (x, y);
            else
              path.lineTo (x, y);
          }
        path.closeSubPath ();
        strokeOutlined (path);
        break;
      }
    case TrajectoryType::Ellipse:
      {
        ellipseOutlined (cx - r, cy - r * 0.4f, r * 2.f, r * 0.8f);
        break;
      }
    case TrajectoryType::Pendulum:
      {
        auto const lineY = cy;
        juce::Path linePath;
        linePath.startNewSubPath (cx - r, lineY);
        linePath.lineTo (cx + r, lineY);
        strokeOutlined (linePath);
        auto const dotR = r * 0.18f;
        dotOutlined (cx - r, lineY, dotR);
        dotOutlined (cx + r, lineY, dotR);
        break;
      }
    case TrajectoryType::Triangle:
      {
        juce::Path path;
        for (int i = 0; i <= 3; ++i)
          {
            auto const angle
                = static_cast<float> (i % 3) / 3.f
                      * juce::MathConstants<float>::twoPi
                  - juce::MathConstants<float>::halfPi;
            auto const x = cx + r * std::cos (angle);
            auto const y = cy + r * std::sin (angle);
            if (i == 0)
              path.startNewSubPath (x, y);
            else
              path.lineTo (x, y);
          }
        path.closeSubPath ();
        strokeOutlined (path);
        break;
      }
    case TrajectoryType::Square:
      {
        auto const d = r * 0.7f;
        juce::Path rectPath;
        rectPath.addRectangle (cx - d, cy - d, d * 2.f, d * 2.f);
        strokeOutlined (rectPath);
        break;
      }
    case TrajectoryType::Star:
      {
        juce::Path path;
        for (int i = 0; i <= 5; ++i)
          {
            auto const angle
                = static_cast<float> (i * 2 % 5) / 5.f
                      * juce::MathConstants<float>::twoPi
                  - juce::MathConstants<float>::halfPi;
            auto const x = cx + r * std::cos (angle);
            auto const y = cy + r * std::sin (angle);
            if (i == 0)
              path.startNewSubPath (x, y);
            else
              path.lineTo (x, y);
          }
        path.closeSubPath ();
        strokeOutlined (path);
        break;
      }
    case TrajectoryType::Bounce:
      {
        auto const dotR = r * 0.22f;
        for (int i = 0; i < 3; ++i)
          {
            auto const angle
                = static_cast<float> (i) / 3.f
                  * juce::MathConstants<float>::twoPi;
            auto const x = cx + r * 0.65f * std::cos (angle);
            auto const y = cy + r * 0.65f * std::sin (angle);
            dotOutlined (x, y, dotR);
          }
        break;
      }
    case TrajectoryType::Helix:
      {
        juce::Path path;
        auto constexpr numPoints = 80;
        for (int i = 0; i <= numPoints; ++i)
          {
            auto const t
                = static_cast<float> (i) / static_cast<float> (numPoints);
            auto const angle = t * 4.f * juce::MathConstants<float>::twoPi;
            auto const phase = t < 0.5f ? t * 2.f : (1.f - t) * 2.f;
            auto const rad = r * phase;
            auto const x = cx + rad * std::cos (angle);
            auto const y = cy + rad * std::sin (angle);
            if (i == 0)
              path.startNewSubPath (x, y);
            else
              path.lineTo (x, y);
          }
        strokeOutlined (path);
        break;
      }
    case TrajectoryType::Orbit:
      {
        juce::Path path;
        auto constexpr numPoints = 64;
        auto constexpr e = 0.6f;
        for (int i = 0; i <= numPoints; ++i)
          {
            auto const t
                = static_cast<float> (i) / static_cast<float> (numPoints);
            auto const angle = t * juce::MathConstants<float>::twoPi;
            auto const rad = r * (1.f - e * e) / (1.f + e * std::cos (angle));
            auto const x = cx + rad * std::cos (angle);
            auto const y = cy + rad * std::sin (angle);
            if (i == 0)
              path.startNewSubPath (x, y);
            else
              path.lineTo (x, y);
          }
        path.closeSubPath ();
        strokeOutlined (path);
        break;
      }
    case TrajectoryType::Cross:
      {
        auto const dotR = r * 0.22f;
        dotOutlined (cx + r * 0.65f, cy, dotR);
        dotOutlined (cx, cy - r * 0.65f, dotR);
        dotOutlined (cx - r * 0.65f, cy, dotR);
        dotOutlined (cx, cy + r * 0.65f, dotR);
        break;
      }
    case TrajectoryType::Wave:
      {
        juce::Path path;
        auto constexpr numPoints = 80;
        for (int i = 0; i <= numPoints; ++i)
          {
            auto const t
                = static_cast<float> (i) / static_cast<float> (numPoints);
            auto const angle = t * juce::MathConstants<float>::twoPi;
            auto const rad = r * (0.6f + 0.4f * std::sin (6.f * angle));
            auto const x = cx + rad * std::cos (angle);
            auto const y = cy + rad * std::sin (angle);
            if (i == 0)
              path.startNewSubPath (x, y);
            else
              path.lineTo (x, y);
          }
        path.closeSubPath ();
        strokeOutlined (path);
        break;
      }
    case TrajectoryType::Hypo:
      {
        juce::Path path;
        auto constexpr numPoints = 80;
        auto constexpr R = 5.f, rv = 3.f, d = 5.f;
        auto const scale = r / (R - rv + d);
        for (int i = 0; i <= numPoints; ++i)
          {
            auto const t
                = static_cast<float> (i) / static_cast<float> (numPoints);
            auto const angle = t * juce::MathConstants<float>::twoPi;
            auto const x = cx + scale * ((R - rv) * std::cos (angle)
                                         + d * std::cos ((R - rv) / rv * angle));
            auto const y = cy + scale * ((R - rv) * std::sin (angle)
                                         - d * std::sin ((R - rv) / rv * angle));
            if (i == 0)
              path.startNewSubPath (x, y);
            else
              path.lineTo (x, y);
          }
        path.closeSubPath ();
        strokeOutlined (path);
        break;
      }
    case TrajectoryType::Empty:
    default:
      break;
    }
}

void
PadRowDisplay::setLabel (int channel, juce::String label)
{
  jassert (channel >= 0 && channel < numChannels);
  _cells[static_cast<size_t> (channel)].label = std::move (label);
  repaint ();
}

void
PadRowDisplay::setLengthBeats (int channel, int beats)
{
  jassert (channel >= 0 && channel < numChannels);
  _cells[static_cast<size_t> (channel)].lengthBeats = beats;
  repaint ();
}

void
PadRowDisplay::setCategoryPrefix (int channel, juce::String prefix)
{
  jassert (channel >= 0 && channel < numChannels);
  _cells[static_cast<size_t> (channel)].categoryPrefix = std::move (prefix);
  repaint ();
}

void
PadRowDisplay::setRowHighlighted (int channel, bool highlighted)
{
  jassert (channel >= 0 && channel < numChannels);
  _cells[static_cast<size_t> (channel)].rowHighlighted = highlighted;
  repaint ();
}

void
PadRowDisplay::setCellSelected (int channel, bool selected)
{
  jassert (channel >= 0 && channel < numChannels);
  _cells[static_cast<size_t> (channel)].cellSelected = selected;
  repaint ();
}

void
PadRowDisplay::setChannelColour (int channel, juce::Colour colour)
{
  jassert (channel >= 0 && channel < numChannels);
  _cells[static_cast<size_t> (channel)].colour = colour;
}

}
