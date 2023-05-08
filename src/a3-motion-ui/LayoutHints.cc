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

#include "LayoutHints.hh"

#include <a3-motion-ui/components/ChannelHeader.hh>

namespace a3
{

float const LayoutHints::Channels::widthMin = 100.f;
float const LayoutHints::Channels::heightFooter = 150.f;

float const LayoutHints::MotionComponent::heightMin = 100.f;

float const LayoutHints::padding = 5.f;
float const LayoutHints::lineHeight = 50.f;

float
LayoutHints::Channels::heightHeader ()
{
  return ChannelHeader::numSlidersFX * LayoutHints::lineHeight
         + 2 * LayoutHints::padding;
}

}
