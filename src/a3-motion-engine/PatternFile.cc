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

#include "PatternFile.hh"

#include "RecordingSeam.hh"
#include "SvgPathTokens.hh"
#include "TrajectoryShape.hh"

#include <a3-motion-engine/tempo/TempoClock.hh>

#include <cmath>
#include <algorithm>
#include <iostream>
#include <sstream>

namespace a3
{

// ---------------------------------------------------------------------------
//  Helpers — simple 2D point for path building (no juce::Point in engine)
// ---------------------------------------------------------------------------

struct Vec2
{
  float x = 0.f, y = 0.f;
  Vec2 () = default;
  Vec2 (float x_, float y_) : x (x_), y (y_) {}
  Vec2 operator+ (Vec2 const &o) const { return { x + o.x, y + o.y }; }
  Vec2 operator- (Vec2 const &o) const { return { x - o.x, y - o.y }; }
  Vec2 operator/ (float s) const { return { x / s, y / s }; }
  float distTo (Vec2 const &o) const
  {
    auto dx = x - o.x;
    auto dy = y - o.y;
    return std::sqrt (dx * dx + dy * dy);
  }
};

static std::string
fts (float v)
{
  // Compact float representation: max 4 decimal places, strip trailing zeros
  char buf[32];
  std::snprintf (buf, sizeof (buf), "%.4f", static_cast<double> (v));
  std::string s (buf);
  if (s.find ('.') != std::string::npos)
    {
      auto last = s.find_last_not_of ('0');
      if (s[last] == '.')
        --last;
      s.erase (last + 1);
    }
  return s;
}

// ---------------------------------------------------------------------------
//  buildSvgPathData — tick data → SVG path string with Catmull-Rom Bézier
// ---------------------------------------------------------------------------

static std::string
buildSvgPathData (std::vector<Pos> const &ticks,
                  std::vector<std::pair<float,float>> &outJumpDots)
{
  outJumpDots.clear ();

  if (ticks.empty ())
    return {};

  // ── Bounding box of valid ticks (XY only) ──
  float minX =  std::numeric_limits<float>::max ();
  float maxX =  std::numeric_limits<float>::lowest ();
  float minY =  std::numeric_limits<float>::max ();
  float maxY =  std::numeric_limits<float>::lowest ();
  int validCount = 0;

  for (auto const &pos : ticks)
    {
      if (!pos.isValid ())
        continue;
      if (pos.x () < minX) minX = pos.x ();
      if (pos.x () > maxX) maxX = pos.x ();
      if (pos.y () < minY) minY = pos.y ();
      if (pos.y () > maxY) maxY = pos.y ();
      ++validCount;
    }

  if (validCount < 2)
    return {};

  auto rangeX = maxX - minX;
  auto rangeY = maxY - minY;
  auto range  = std::max (rangeX, rangeY);
  if (range < 1e-6f)
    range = 1.f;

  auto centreX = (minX + maxX) * 0.5f;
  auto centreY = (minY + maxY) * 0.5f;

  // ── Downsample to max 128 points ──
  auto const maxPts = 128;
  auto const step = std::max (1, static_cast<int> (ticks.size ()) / maxPts);

  // Split into segments wherever the trajectory stops travelling: a gap in
  // the data, or a teleport. Splitting on gaps alone was enough only while a
  // take was sparse; a finished one has its seams closed and no gaps left, so
  // a tapped take came out as one path with a straight line drawn across
  // every jump — and stayed that way, because this is what goes to disk.
  std::vector<std::vector<Vec2>> segments;

  for (auto const &run : trajectorySegments (ticks))
    {
      auto const runStep = std::max (
          size_t{ 1 }, run.size () / static_cast<size_t> (maxPts));

      std::vector<Vec2> pts;
      for (size_t i = 0; i < run.size (); i += runStep)
        pts.push_back ({ (run[i].x () - centreX) / (range * 0.5f),
                         (run[i].y () - centreY) / (range * 0.5f) });

      if (!pts.empty ())
        segments.push_back (std::move (pts));
    }

  // Remove trailing empty segment
  while (!segments.empty () && segments.back ().empty ())
    segments.pop_back ();

  // ── Detect whether the original tick data forms a closed loop ──
  // When the first and last *valid* ticks are close, the pattern is
  // inherently closed.  Downsampling can enlarge the gap between the
  // first and last *sampled* points, which would fool the per-segment
  // closed-path detection below.  We record the ground truth here and
  // pass it through.
  bool tickDataIsClosed = false;
  if (segments.size () == 1 && !segments[0].empty ()
      && ticks.front ().isValid () && ticks.back ().isValid ())
    {
      float origDist = std::sqrt (
          std::pow (ticks.front ().x () - ticks.back ().x (), 2.f)
          + std::pow (ticks.front ().y () - ticks.back ().y (), 2.f));
      // Threshold in original (un-normalised) coordinate space.
      // 0.08 * (range * 0.5) maps to 0.08 in normalised space.
      tickDataIsClosed = origDist < 0.08f * range * 0.5f
                         && segments[0].size () > 4;
    }

  // ── Tapped rather than drawn ──
  // Decided on the shape of the tick data, not on how many ticks are missing
  // from it. The old test needed at least two segments, which only ever
  // happened while gaps were still in the data.
  if (isTappedTrajectory (ticks))
    {
      for (auto const &held : trajectoryPlateaus (ticks))
        {
          float const nx = (held.x () - centreX) / (range * 0.5f);
          float const ny = (held.y () - centreY) / (range * 0.5f);

          bool duplicate = false;
          for (auto const &d : outJumpDots)
            {
              if (std::abs (d.first - nx) < 0.05f
                  && std::abs (d.second - ny) < 0.05f)
                {
                  duplicate = true;
                  break;
                }
            }
          if (!duplicate)
            outJumpDots.push_back ({ nx, ny });
        }
      return {};
    }

  // ── Palindrome: for non-closed segments, append reversed ──
  for (auto &pts : segments)
    {
      if (pts.size () < 2)
        continue;

      auto const dist = pts.front ().distTo (pts.back ());
      bool closed = (dist < 0.08f && pts.size () > 4)
                    || tickDataIsClosed;

      if (!closed)
        {
          auto const origSize = pts.size ();
          for (int i = static_cast<int> (origSize) - 2; i > 0; --i)
            pts.push_back (pts[static_cast<size_t> (i)]);
        }
    }

  // ── Build Catmull-Rom → cubic Bézier SVG path string ──
  std::ostringstream out;

  for (auto const &pts : segments)
    {
      if (pts.size () < 2)
        continue;

      // The separator matters: a closed subpath ends in Z, and writing the
      // next subpath's M straight after it produced "ZM", which is not a
      // command any more. The reader dropped it, the two runs became one, and
      // a straight line was drawn across the gap between them.
      if (out.tellp () > 0)
        out << ' ';

      out << "M " << fts (pts[0].x) << ' ' << fts (pts[0].y);

      auto const dist = pts.front ().distTo (pts.back ());
      bool closed = (dist < 0.08f && pts.size () > 4)
                    || tickDataIsClosed;

      auto const n = static_cast<int> (pts.size ());
      // For closed paths we need n segments (including the wrap-around
      // from the last point back to the first) so the curve has no gap.
      // For open paths n-1 segments suffice.
      auto const numSegments = closed ? n : (n - 1);
      for (int i = 0; i < numSegments; ++i)
        {
          Vec2 p0, p1, p2, p3;
          p1 = pts[static_cast<size_t> (i)];
          p2 = pts[static_cast<size_t> ((i + 1) % n)];

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

          out << " C " << fts (cp1.x) << ' ' << fts (cp1.y)
              << ' ' << fts (cp2.x) << ' ' << fts (cp2.y)
              << ' ' << fts (p2.x) << ' ' << fts (p2.y);
        }

      if (closed)
        out << " Z";
    }

  return out.str ();
}

// ---------------------------------------------------------------------------
//  Adaptive cubic Bezier flattening -- matches JUCE PathFlatteningIterator
//  Recursively subdivides until the control-point deviation from the chord
//  is below the given tolerance.  This ensures the playback polyline matches
//  the display polyline pixel-for-pixel.
// ---------------------------------------------------------------------------

static void
flattenCubic (Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3,
              std::vector<Vec2> &out, float tolerance, int depth = 0)
{
  // Maximum recursion depth to avoid infinite loops
  if (depth > 12)
    {
      out.push_back (p3);
      return;
    }

  // Flatness test: max distance of control points from the chord p0->p3
  auto const dx = p3.x - p0.x;
  auto const dy = p3.y - p0.y;
  auto const lenSq = dx * dx + dy * dy;

  float d1, d2;
  if (lenSq < 1e-12f)
    {
      // Degenerate: chord has zero length, use distance from p0
      d1 = std::sqrt ((p1.x - p0.x) * (p1.x - p0.x)
                      + (p1.y - p0.y) * (p1.y - p0.y));
      d2 = std::sqrt ((p2.x - p0.x) * (p2.x - p0.x)
                      + (p2.y - p0.y) * (p2.y - p0.y));
    }
  else
    {
      // Distance of control points from chord (perpendicular distance)
      d1 = std::abs ((p1.x - p0.x) * dy - (p1.y - p0.y) * dx)
           / std::sqrt (lenSq);
      d2 = std::abs ((p2.x - p0.x) * dy - (p2.y - p0.y) * dx)
           / std::sqrt (lenSq);
    }

  if (d1 <= tolerance && d2 <= tolerance)
    {
      out.push_back (p3);
      return;
    }

  // De Casteljau split at t=0.5
  auto const m01  = Vec2 ((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f);
  auto const m12  = Vec2 ((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);
  auto const m23  = Vec2 ((p2.x + p3.x) * 0.5f, (p2.y + p3.y) * 0.5f);
  auto const m012 = Vec2 ((m01.x + m12.x) * 0.5f, (m01.y + m12.y) * 0.5f);
  auto const m123 = Vec2 ((m12.x + m23.x) * 0.5f, (m12.y + m23.y) * 0.5f);
  auto const mid  = Vec2 ((m012.x + m123.x) * 0.5f,
                           (m012.y + m123.y) * 0.5f);

  flattenCubic (p0, m01, m012, mid, out, tolerance, depth + 1);
  flattenCubic (mid, m123, m23, p3, out, tolerance, depth + 1);
}

static void
flattenQuadratic (Vec2 p0, Vec2 p1, Vec2 p2,
                  std::vector<Vec2> &out, float tolerance, int depth = 0)
{
  if (depth > 12)
    {
      out.push_back (p2);
      return;
    }

  // Flatness: distance of control point from chord
  auto const dx = p2.x - p0.x;
  auto const dy = p2.y - p0.y;
  auto const lenSq = dx * dx + dy * dy;

  float d;
  if (lenSq < 1e-12f)
    d = std::sqrt ((p1.x - p0.x) * (p1.x - p0.x)
                   + (p1.y - p0.y) * (p1.y - p0.y));
  else
    d = std::abs ((p1.x - p0.x) * dy - (p1.y - p0.y) * dx)
        / std::sqrt (lenSq);

  if (d <= tolerance)
    {
      out.push_back (p2);
      return;
    }

  auto const m01 = Vec2 ((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f);
  auto const m12 = Vec2 ((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);
  auto const mid = Vec2 ((m01.x + m12.x) * 0.5f, (m01.y + m12.y) * 0.5f);

  flattenQuadratic (p0, m01, mid, out, tolerance, depth + 1);
  flattenQuadratic (mid, m12, p2, out, tolerance, depth + 1);
}

// ---------------------------------------------------------------------------
//  sampleSvgPathToTicks -- SVG path string -> tick positions
//  Uses adaptive Bezier flattening (same tolerance as display pipeline)
//  to produce an identical polyline, then resamples at uniform arc-length.
// ---------------------------------------------------------------------------

static void
sampleSvgPathToTicks (std::string const &pathData,
                      std::vector<std::pair<float,float>> const &jumpDots,
                      std::size_t numTicks,
                      std::vector<Pos> &outTicks)
{
  outTicks.clear ();
  outTicks.reserve (numTicks);

  if (!jumpDots.empty ())
    {
      // Jump pattern: distribute dots evenly
      auto const dotsN = jumpDots.size ();
      auto const ticksPerDot = numTicks / dotsN;
      for (std::size_t t = 0; t < numTicks; ++t)
        {
          auto const dotIdx = t / ticksPerDot;
          auto const posInDot = t % ticksPerDot;
          if (posInDot == ticksPerDot - 1 || dotIdx >= dotsN)
            outTicks.push_back (Pos::invalid);
          else
            outTicks.push_back (
                Pos::fromCartesian (jumpDots[dotIdx].first,
                                   jumpDots[dotIdx].second, 0.f));
        }
      return;
    }

  if (pathData.empty ())
    {
      for (std::size_t t = 0; t < numTicks; ++t)
        outTicks.push_back (Pos::invalid);
      return;
    }

  // Parse the SVG path into a polyline via adaptive Bezier flattening.
  // Uses the SAME tolerance (0.005) as the display pipeline
  // (PathFlatteningIterator in MotionComponent) so both pipelines
  // produce identical polylines from the same SVG data.
  static constexpr float kFlattenTolerance = 0.005f;

  std::vector<Vec2> polyline;

  // Where each subpath begins. The polyline is flat, so without these the
  // stretch from the end of one subpath to the start of the next is just
  // another segment and gets walked like any other -- the blob crossing a gap
  // the finger left, in a straight line, which is what it looked like at the
  // end of a trajectory.
  std::vector<std::size_t> subPathStarts;

  auto tokens = svgPathTokens (juce::String (pathData));

  int idx = 0;
  auto nextF = [&]() -> float {
    if (idx < tokens.size ())
      return tokens[idx++].getFloatValue ();
    return 0.f;
  };

  Vec2 cur{ 0.f, 0.f };
  Vec2 subPathStart{ 0.f, 0.f };

  while (idx < tokens.size ())
    {
      auto cmd = tokens[idx].toStdString ();
      if (cmd == "M" || cmd == "m")
        {
          ++idx;
          cur = { nextF (), nextF () };
          subPathStart = cur;
          subPathStarts.push_back (polyline.size ());
          polyline.push_back (cur);
        }
      else if (cmd == "L" || cmd == "l")
        {
          ++idx;
          cur = { nextF (), nextF () };
          polyline.push_back (cur);
        }
      else if (cmd == "C" || cmd == "c")
        {
          ++idx;
          auto x1 = nextF (), y1 = nextF ();
          auto x2 = nextF (), y2 = nextF ();
          auto x3 = nextF (), y3 = nextF ();

          flattenCubic (cur, { x1, y1 }, { x2, y2 }, { x3, y3 },
                        polyline, kFlattenTolerance);
          cur = { x3, y3 };
        }
      else if (cmd == "Q" || cmd == "q")
        {
          ++idx;
          auto x1 = nextF (), y1 = nextF ();
          auto x2 = nextF (), y2 = nextF ();

          flattenQuadratic (cur, { x1, y1 }, { x2, y2 },
                            polyline, kFlattenTolerance);
          cur = { x2, y2 };
        }
      else if (cmd == "Z" || cmd == "z")
        {
          ++idx;
          // Close the sub-path: add the start point so the polyline
          // includes the closing segment back to the beginning.
          if (!polyline.empty ()
              && subPathStart.distTo (cur) > 1e-6f)
            {
              polyline.push_back (subPathStart);
              cur = subPathStart;
            }
        }
      else
        {
          ++idx;
        }
    }

  if (polyline.size () < 2)
    {
      for (std::size_t t = 0; t < numTicks; ++t)
        outTicks.push_back (Pos::invalid);
      return;
    }

  // Build cumulative arc-length table
  std::vector<float> arcLen (polyline.size (), 0.f);
  for (std::size_t i = 1; i < polyline.size (); ++i)
    arcLen[i] = arcLen[i - 1] + polyline[i].distTo (polyline[i - 1]);

  auto const totalLen = arcLen.back ();
  if (totalLen < 1e-6f)
    {
      for (std::size_t t = 0; t < numTicks; ++t)
        outTicks.push_back (
            Pos::fromCartesian (polyline[0].x, polyline[0].y, 0.f));
      return;
    }

  // Sample at uniform arc-length intervals
  for (std::size_t t = 0; t < numTicks; ++t)
    {
      auto const targetDist
          = static_cast<float> (t) / static_cast<float> (numTicks) * totalLen;

      // Binary search for the segment containing targetDist
      auto it = std::lower_bound (arcLen.begin (), arcLen.end (), targetDist);
      auto seg = static_cast<std::size_t> (
          std::max (static_cast<int> (it - arcLen.begin ()) - 1, 0));
      if (seg >= polyline.size () - 1)
        seg = polyline.size () - 2;

      auto const segLen = arcLen[seg + 1] - arcLen[seg];
      auto const frac
          = (segLen > 1e-8f)
                ? (targetDist - arcLen[seg]) / segLen
                : 0.f;

      // A segment that ends on a subpath's first point is not part of any
      // shape: it is the gap between two of them. Nothing was played there,
      // and nothing is written.
      auto const isBridge
          = std::find (subPathStarts.begin (), subPathStarts.end (), seg + 1)
            != subPathStarts.end ();
      if (isBridge)
        {
          outTicks.push_back (Pos::invalid);
          continue;
        }

      auto const px = polyline[seg].x + (polyline[seg + 1].x - polyline[seg].x) * frac;
      auto const py = polyline[seg].y + (polyline[seg + 1].y - polyline[seg].y) * frac;
      outTicks.push_back (Pos::fromCartesian (px, py, 0.f));
    }
}

// ---------------------------------------------------------------------------
//  save
// ---------------------------------------------------------------------------

bool
PatternFile::save (std::shared_ptr<Pattern> const &pattern,
                   juce::File const &file)
{
  if (!pattern)
    return false;

  auto ticks = pattern->getTicks ();

  // The take as it was played, not as it currently reads with a closing move
  // laid over it. The fade is a setting, so what goes on disk has to be the
  // thing the setting applies to -- otherwise reopening the clip bakes the
  // last length in and it can never be shortened again.
  auto const baseline = pattern->getFadeBaseline ();
  if (baseline.size () == ticks.positions.size () && !baseline.empty ())
    ticks.positions = baseline;

  auto const numTicks = ticks.positions.size ();
  auto const lengthBeats
      = static_cast<int> (numTicks) / TempoClock::getTicksPerBeat ();

  // Build the SVG path data string (with palindrome for seamless loops)
  std::vector<std::pair<float,float>> jumpDots;
  auto pathData = buildSvgPathData (ticks.positions, jumpDots);

  // Build SVG XML
  auto svg = std::make_unique<juce::XmlElement> ("svg");
  svg->setAttribute ("xmlns", "http://www.w3.org/2000/svg");
  svg->setAttribute ("viewBox", "-1 -1 2 2");
  svg->setAttribute ("data-name", juce::String (pattern->getName ()));
  svg->setAttribute ("data-beats", lengthBeats);
  // Where the take's seam lies. Not something that can be worked out again
  // from the file — once filled, the stretch looks like any other run of
  // ticks — and how it is filled is a playback setting, so it has to survive
  // a restart or there is no way back from the last fill.
  auto const seam = pattern->getSeamSpan ();
  if (seam.length > 0)
    {
      svg->setAttribute ("data-seam-begin", static_cast<int> (seam.begin));
      svg->setAttribute ("data-seam-length", static_cast<int> (seam.length));
    }

  // Where the take stopped. Not something that can be worked out again from
  // the file -- once the closing move is written, that stretch looks like any
  // other run of ticks -- and the fade is a playback setting, so without this
  // turning it after a restart would have nothing to take hold of.
  if (auto const join = pattern->getSeamJoin ())
    {
      svg->setAttribute ("data-seam-join", static_cast<int> (*join));
      svg->setAttribute ("data-fade", static_cast<int> (pattern->getFade ()));
    }
  svg->setAttribute ("data-ppqn", TempoClock::getTicksPerBeat ());

  if (!pathData.empty ())
    {
      auto *pathEl = svg->createNewChildElement ("path");
      pathEl->setAttribute ("d", juce::String (pathData));
      pathEl->setAttribute ("fill", "none");
      pathEl->setAttribute ("stroke", "black");
    }

  for (auto const &dot : jumpDots)
    {
      auto *circleEl = svg->createNewChildElement ("circle");
      circleEl->setAttribute ("cx", juce::String (dot.first, 4));
      circleEl->setAttribute ("cy", juce::String (dot.second, 4));
      circleEl->setAttribute ("r", "0.05");
    }

  file.getParentDirectory ().createDirectory ();
  return file.replaceWithText (svg->toString ());
}

// ---------------------------------------------------------------------------
//  load
// ---------------------------------------------------------------------------

std::shared_ptr<Pattern>
PatternFile::load (juce::File const &file)
{
  if (!file.existsAsFile ())
    return nullptr;

  auto xml = juce::XmlDocument::parse (file);
  if (!xml || xml->getTagName () != "svg")
    return nullptr;

  auto name = xml->getStringAttribute ("data-name").toStdString ();
  auto lengthBeats = xml->getIntAttribute ("data-beats", 0);
  auto ppqn = xml->getIntAttribute ("data-ppqn",
                                     TempoClock::getTicksPerBeat ());

  if (lengthBeats <= 0)
    return nullptr;

  std::string pathData;
  std::vector<std::pair<float,float>> jumpDots;

  for (auto *child : xml->getChildIterator ())
    {
      if (child->getTagName () == "path")
        {
          auto d = child->getStringAttribute ("d");
          if (d.isNotEmpty ())
            pathData = d.toStdString ();
        }
      else if (child->getTagName () == "circle")
        {
          auto cx = child->getStringAttribute ("cx").getFloatValue ();
          auto cy = child->getStringAttribute ("cy").getFloatValue ();
          jumpDots.push_back ({ cx, cy });
        }
    }

  auto pattern = std::make_shared<Pattern> ();
  pattern->setName (name);
  pattern->resize (static_cast<index_t> (lengthBeats * ppqn));

  // A file that does not mention a seam has none — a shipped shape, or a take
  // that filled its whole loop.
  auto const seamLength = xml->getIntAttribute ("data-seam-length", 0);
  if (xml->hasAttribute ("data-seam-join"))
    pattern->setSeamJoin (static_cast<index_t> (
        xml->getIntAttribute ("data-seam-join", 0)));

  if (seamLength > 0)
    pattern->setSeamSpan (
        { static_cast<index_t> (xml->getIntAttribute ("data-seam-begin", 0)),
          static_cast<index_t> (seamLength) });

  auto const numTicks = pattern->getNumTicks ();

  std::vector<Pos> sampled;
  sampleSvgPathToTicks (pathData, jumpDots, numTicks, sampled);

  for (index_t t = 0; t < numTicks && t < sampled.size (); ++t)
    pattern->setTick (t, sampled[t]);

  // What came out of the file is the take as played; the closing move is laid
  // over it here, from the length the file carries.
  pattern->setFadeBaseline (pattern->getTicks ().positions);
  if (pattern->getSeamJoin ())
    applyFade (*pattern, static_cast<index_t> (
                             xml->getIntAttribute ("data-fade", 0)));

  pattern->setStatus (Pattern::Status::Idle);
  return pattern;
}

// ---------------------------------------------------------------------------
//  peek
// ---------------------------------------------------------------------------

PatternFile::PeekResult
PatternFile::peek (juce::File const &file)
{
  PeekResult result;

  if (!file.existsAsFile ())
    return result;

  auto xml = juce::XmlDocument::parse (file);
  if (!xml || xml->getTagName () != "svg")
    return result;

  result.name = xml->getStringAttribute ("data-name").toStdString ();
  result.lengthBeats = xml->getIntAttribute ("data-beats", 0);

  for (auto *child : xml->getChildIterator ())
    {
      if (child->getTagName () == "path")
        {
          auto d = child->getStringAttribute ("d");
          if (d.isNotEmpty ())
            result.pathData = d.toStdString ();
        }
      else if (child->getTagName () == "circle")
        {
          auto cx = child->getStringAttribute ("cx").getFloatValue ();
          auto cy = child->getStringAttribute ("cy").getFloatValue ();
          result.jumpDots.push_back ({ cx, cy });
        }
    }

  return result;
}

}
