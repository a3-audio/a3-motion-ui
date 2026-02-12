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

  // Background based on selection state
  if (cell.cellSelected)
    {
      g.setColour (colour.withAlpha (0.4f));
      g.fillRect (bounds);
    }
  else if (cell.rowHighlighted)
    {
      g.setColour (colour.withAlpha (0.15f));
      g.fillRect (bounds);
    }

  // Draw trajectory figure icon or "---" for empty
  auto iconAlpha = cell.cellSelected ? 1.0f
                   : cell.rowHighlighted ? 0.9f
                                         : 0.4f;
  auto iconColour = colour.withAlpha (iconAlpha);

  if (cell.trajectoryType == TrajectoryType::Empty && !cell.hasTickData)
    {
      g.setColour (iconColour);
      g.setFont (LayoutHints::fontSize * 0.7f);
      g.drawText (cell.label, bounds, juce::Justification::centred, false);
    }
  else
    {
      // Square icon area centered in the cell
      auto const h = static_cast<float> (bounds.getHeight ());
      auto const iconSize = h * 0.7f;
      auto iconArea = juce::Rectangle<float> (iconSize, iconSize)
                          .withCentre (bounds.getCentre ().toFloat ());

      // Prefer tick data icon when available
      if (cell.hasTickData)
        drawTickDataIcon (g, iconArea, iconColour, channel);
      else
        drawTrajectoryIcon (g, iconArea, cell.trajectoryType, iconColour);
    }

  // Thin baseline
  g.setColour (colour.withAlpha (0.1f));
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
      // Continuous path
      bool started = false;
      for (size_t i = 0; i < ticks.size (); i += static_cast<size_t> (step))
        {
          if (!ticks[i].isValid ())
            {
              started = false; // break the path at invalid ticks
              continue;
            }
          float nx = (ticks[i].x () - centreX) / (range * 0.5f);
          float ny = (ticks[i].y () - centreY) / (range * 0.5f);
          if (!started)
            {
              cell.tickPath.startNewSubPath (nx, ny);
              started = true;
            }
          else
            {
              cell.tickPath.lineTo (nx, ny);
            }
        }
    }

  cell.hasTickData = true;
  repaint ();
}

void
PadRowDisplay::drawTickDataIcon (juce::Graphics &g,
                                 juce::Rectangle<float> area,
                                 juce::Colour colour, int channel)
{
  auto const &cell = _cells[static_cast<size_t> (channel)];
  auto const cx = area.getCentreX ();
  auto const cy = area.getCentreY ();
  auto const r = area.getWidth () * 0.45f;
  auto const strokeThickness = 1.5f;

  g.setColour (colour);

  if (cell.hasJumpTicks)
    {
      // Draw dots at each jump position
      auto const dotR = r * 0.22f;
      for (auto const &p : cell.jumpPoints)
        {
          auto const x = cx + p.first * r;
          auto const y = cy + p.second * r;
          g.fillEllipse (x - dotR, y - dotR, dotR * 2.f, dotR * 2.f);
        }
    }
  else
    {
      // Scale the normalised [-1,1] path to the icon area
      auto transform = juce::AffineTransform::scale (r, r)
                           .translated (cx, cy);
      g.strokePath (cell.tickPath,
                    juce::PathStrokeType (strokeThickness),
                    transform);
    }
}

void
PadRowDisplay::drawTrajectoryIcon (juce::Graphics &g,
                                   juce::Rectangle<float> area,
                                   TrajectoryType type,
                                   juce::Colour colour)
{
  auto const cx = area.getCentreX ();
  auto const cy = area.getCentreY ();
  auto const r = area.getWidth () * 0.45f;

  g.setColour (colour);
  auto const strokeThickness = 1.5f;

  switch (type)
    {
    case TrajectoryType::Circle:
      {
        // Simple circle
        g.drawEllipse (cx - r, cy - r, r * 2.f, r * 2.f, strokeThickness);
        break;
      }
    case TrajectoryType::FigureOfEight:
      {
        // Lemniscate / figure-8 drawn as a path
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
        g.strokePath (path, juce::PathStrokeType (strokeThickness));
        break;
      }
    case TrajectoryType::CornerStep:
      {
        // 4-corner step: ball jumps between corners (no connecting lines)
        auto const d = r * 0.7f;
        auto const dotR = r * 0.22f;

        // Just 4 dots at the corners
        g.fillEllipse (cx - d - dotR, cy - d - dotR, dotR * 2.f, dotR * 2.f);
        g.fillEllipse (cx + d - dotR, cy - d - dotR, dotR * 2.f, dotR * 2.f);
        g.fillEllipse (cx + d - dotR, cy + d - dotR, dotR * 2.f, dotR * 2.f);
        g.fillEllipse (cx - d - dotR, cy + d - dotR, dotR * 2.f, dotR * 2.f);
        break;
      }
    case TrajectoryType::Spiral:
      {
        // Out-and-back spiral (matches actual pattern)
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
        g.strokePath (path, juce::PathStrokeType (strokeThickness));
        break;
      }
    case TrajectoryType::Lissajous:
      {
        // Lissajous 3:2 with phase offset
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
        g.strokePath (path, juce::PathStrokeType (strokeThickness));
        break;
      }
    case TrajectoryType::Rose:
      {
        // 3-petal rose: r = cos(3*theta)
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
        g.strokePath (path, juce::PathStrokeType (strokeThickness));
        break;
      }
    case TrajectoryType::Zigzag:
      {
        // Circle with zigzag perturbation
        juce::Path path;
        auto constexpr numPoints = 80;
        for (int i = 0; i <= numPoints; ++i)
          {
            auto const t
                = static_cast<float> (i) / static_cast<float> (numPoints);
            auto const angle = t * juce::MathConstants<float>::twoPi;
            // Triangle wave for zigzag
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
        g.strokePath (path, juce::PathStrokeType (strokeThickness));
        break;
      }
    case TrajectoryType::Ellipse:
      {
        // Horizontal ellipse
        g.drawEllipse (cx - r, cy - r * 0.4f, r * 2.f, r * 0.8f,
                       strokeThickness);
        break;
      }
    case TrajectoryType::Pendulum:
      {
        // Horizontal line with ball ends (swing back and forth)
        auto const lineY = cy;
        g.drawLine (cx - r, lineY, cx + r, lineY, strokeThickness);
        auto const dotR = r * 0.18f;
        g.fillEllipse (cx - r - dotR, lineY - dotR, dotR * 2.f, dotR * 2.f);
        g.fillEllipse (cx + r - dotR, lineY - dotR, dotR * 2.f, dotR * 2.f);
        break;
      }
    case TrajectoryType::Triangle:
      {
        // Equilateral triangle path
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
        g.strokePath (path, juce::PathStrokeType (strokeThickness));
        break;
      }
    case TrajectoryType::Square:
      {
        // Square outline
        auto const d = r * 0.7f;
        g.drawRect (juce::Rectangle<float> (cx - d, cy - d, d * 2.f, d * 2.f),
                    strokeThickness);
        break;
      }
    case TrajectoryType::Star:
      {
        // 5-pointed star
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
        g.strokePath (path, juce::PathStrokeType (strokeThickness));
        break;
      }
    case TrajectoryType::Bounce:
      {
        // 3 dots at 120° (jump pattern)
        auto const dotR = r * 0.22f;
        for (int i = 0; i < 3; ++i)
          {
            auto const angle
                = static_cast<float> (i) / 3.f
                  * juce::MathConstants<float>::twoPi;
            auto const x = cx + r * 0.65f * std::cos (angle);
            auto const y = cy + r * 0.65f * std::sin (angle);
            g.fillEllipse (x - dotR, y - dotR, dotR * 2.f, dotR * 2.f);
          }
        break;
      }
    case TrajectoryType::Helix:
      {
        // Double spiral (out and back)
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
        g.strokePath (path, juce::PathStrokeType (strokeThickness));
        break;
      }
    case TrajectoryType::Orbit:
      {
        // Eccentric ellipse (egg-shaped)
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
        g.strokePath (path, juce::PathStrokeType (strokeThickness));
        break;
      }
    case TrajectoryType::Cross:
      {
        // 4 dots on axes (jump pattern)
        auto const dotR = r * 0.22f;
        g.fillEllipse (cx + r * 0.65f - dotR, cy - dotR, dotR * 2.f, dotR * 2.f);
        g.fillEllipse (cx - dotR, cy - r * 0.65f - dotR, dotR * 2.f, dotR * 2.f);
        g.fillEllipse (cx - r * 0.65f - dotR, cy - dotR, dotR * 2.f, dotR * 2.f);
        g.fillEllipse (cx - dotR, cy + r * 0.65f - dotR, dotR * 2.f, dotR * 2.f);
        break;
      }
    case TrajectoryType::Wave:
      {
        // Flower/sun shape (circle with bumps)
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
        g.strokePath (path, juce::PathStrokeType (strokeThickness));
        break;
      }
    case TrajectoryType::Hypo:
      {
        // Hypotrochoid (Spirograph shape)
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
        g.strokePath (path, juce::PathStrokeType (strokeThickness));
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
