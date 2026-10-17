/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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

namespace a3
{

/** How the heard direction is taken to a figure's new hold point. The floor
 *  point jumps; the ear is led, so a side change is heard as the side it
 *  is, not as a sweep through the listeners' heads.
 *  - Shortest: the short way round, height evenly.
 *  - ViaFront: left-right through the front, at one height -- the stage.
 *  - ViaSide: front-back round the side the ship is heard on.
 *  - Vertical: up-down in height only, the azimuth kept. */
enum class GlidePath
{
  Shortest,
  ViaFront,
  ViaSide,
  Vertical,
};

/** How a figure's ship stands this tick, besides the point it steers for. */
struct FigureStance
{
  /** The ship stands at the point: placed there, still, unsteered. */
  bool hold = false;
  /** The breath's stop holds the ship as it holds a patrol. */
  bool breathes = false;
  /** How the ear reaches a new hold point. */
  GlidePath glide = GlidePath::Shortest;
};

}
