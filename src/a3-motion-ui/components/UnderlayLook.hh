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

#pragma once

#include <a3-motion-ui/components/SphereProjection.hh>

#include <JuceHeader.h>

namespace a3
{

/** Everything the underlay's picture depends on.
 *
 *  The underlay is the take being written over, drawn faintly under the new
 *  one. It does not change while a take is recorded, and drawing it afresh
 *  every frame -- a braid of strokes, rasterised on the CPU -- was a sixth of
 *  the render thread (2026-09-25). So it is drawn once into an image of its
 *  own and kept for as long as this stays the same. Its braid stands still
 *  there, which was decided on purpose: it is ground to draw over, and a ghost
 *  that moves pulls the eye away from the finger.
 *
 *  The small dot running along it is not part of this: it moves with the
 *  write head and is drawn every frame. */
struct UnderlayLook
{
  /** Which take, by identity. The take being written over is not written to
   *  while a new one records, so its identity and its length are enough. */
  void const *take = nullptr;
  std::size_t ticks = 0;
  SphereCamera camera;
  int width = 0;
  int height = 0;
  juce::Colour colour;
  float opacity = 0.f;
  float thickness = 0.f;

  friend bool
  operator== (UnderlayLook const &a, UnderlayLook const &b)
  {
    return a.take == b.take && a.ticks == b.ticks
           && a.camera.pitch == b.camera.pitch && a.camera.turn == b.camera.turn
           && a.width == b.width && a.height == b.height
           && a.colour == b.colour && a.opacity == b.opacity
           && a.thickness == b.thickness;
  }

  friend bool
  operator!= (UnderlayLook const &a, UnderlayLook const &b)
  {
    return !(a == b);
  }
};

}
