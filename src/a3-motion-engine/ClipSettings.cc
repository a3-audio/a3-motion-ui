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

#include "ClipSettings.hh"

#include <a3-motion-engine/Pattern.hh>

namespace a3
{

ClipSettings
clipSettingsFrom (Pattern const &pattern)
{
  ClipSettings settings;

  settings.speedLog2 = pattern.getSpeedLog2 ();
  settings.rotate = pattern.getRotate ();

  settings.reach = pattern.getReach ();
  settings.clipTop = pattern.getClipTop ();
  settings.clipBottom = pattern.getClipBottom ();
  settings.mirrorSouth = pattern.getMirrorSouth ();
  settings.flat = pattern.getFlat ();
  settings.flatElevation = pattern.getFlatElevation ();

  settings.spin = pattern.getSpin ();
  settings.reachLfo = pattern.getReachLfo ();
  settings.envelopeAttack = pattern.getEnvelopeAttack ();
  settings.envelopeDecay = pattern.getEnvelopeDecay ();
  settings.envelopeMax = pattern.getEnvelopeMax ();
  settings.filterAttack = pattern.getFilterAttack ();
  settings.filterDecay = pattern.getFilterDecay ();
  settings.filterMax = pattern.getFilterMax ();
  settings.actMode = pattern.getActMode ();

  settings.direction = pattern.getPlayDirection ();
  settings.endAction = pattern.getEndAction ();

  settings.fadeReach = pattern.getFadeReach ();
  settings.bridgeBias = pattern.getBridgeBias ();

  return settings;
}

void
applyClipSettings (Pattern &pattern, ClipSettings const &settings)
{
  pattern.setSpeedLog2 (settings.speedLog2);
  pattern.setRotate (settings.rotate);

  pattern.setReach (settings.reach);
  pattern.setClipTop (settings.clipTop);
  pattern.setClipBottom (settings.clipBottom);
  pattern.setMirrorSouth (settings.mirrorSouth);
  pattern.setFlat (settings.flat);
  pattern.setFlatElevation (settings.flatElevation);

  pattern.setSpin (settings.spin);
  pattern.setReachLfo (settings.reachLfo);
  pattern.setEnvelopeAttack (settings.envelopeAttack);
  pattern.setEnvelopeDecay (settings.envelopeDecay);
  pattern.setEnvelopeMax (settings.envelopeMax);
  pattern.setFilterAttack (settings.filterAttack);
  pattern.setFilterDecay (settings.filterDecay);
  pattern.setFilterMax (settings.filterMax);
  pattern.setActMode (settings.actMode);

  pattern.setPlayDirection (settings.direction);
  pattern.setEndAction (settings.endAction);

  pattern.setFadeReach (settings.fadeReach);
  pattern.setBridgeBias (settings.bridgeBias);
}

}
