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

#include <JuceHeader.h>

#include <vector>

#include <a3-motion-ui/components/SphereProjection.hh>

namespace a3
{

/** The listener, as they look from wherever the room is being looked at.
 *
 *  Every picture on this device is of one room with one person in the middle
 *  of it, and until now none of them said so. A person is the one mark that
 *  answers both questions a turned view raises at once -- which way round am I
 *  looking, and how far over -- without a key to read: which way the room is
 *  turned is which way they face, and how far it is tipped is how much of them
 *  you can see. From straight down that is the top of a head; from the horizon
 *  it is a figure standing.
 *
 *  A real silhouette rather than a stick: the body is a few ellipsoids, each
 *  one's outline is whatever it comes to from this angle, and what is returned
 *  is the outline of all of them together. A stick figure has to be drawn
 *  differently for every view and looks wrong in most of them; a solid has one
 *  description and looks like itself from anywhere.
 *
 *  Returned centred on the origin, in units where `height` is the figure's
 *  full standing height. Screen axes, so it can be filled straight into a
 *  Graphics after a translate.
 */
juce::Path listenerSilhouette (SphereCamera camera, float height);

/** The outline of a set of points, anticlockwise from the leftmost.
 *
 *  Exposed for the silhouette's own tests: a hull that is wrong shows up as a
 *  figure with a corner cut off, which is hard to see and easy to measure. */
std::vector<juce::Point<float> >
outlineOf (std::vector<juce::Point<float> > points);

}
