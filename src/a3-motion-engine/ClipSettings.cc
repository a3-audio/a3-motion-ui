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
  settings.elevationBase = pattern.getElevationBase ();
  settings.mirrorSouth = pattern.getMirrorSouth ();
  settings.flat = pattern.getFlat ();
  settings.flatElevation = pattern.getFlatElevation ();

  settings.spin = pattern.getSpin ();
  settings.reachLfo = pattern.getReachLfo ();
  settings.envelopeAttack = pattern.getEnvelopeAttack ();
  settings.envelopeDecay = pattern.getEnvelopeDecay ();
  settings.envelopeMax = pattern.getEnvelopeMax ();
  settings.freqAttack = pattern.getFreqAttack ();
  settings.freqDecay = pattern.getFreqDecay ();
  settings.freqMax = pattern.getFreqMax ();
  settings.qAttack = pattern.getQAttack ();
  settings.qDecay = pattern.getQDecay ();
  settings.qMax = pattern.getQMax ();
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
  pattern.setElevationBase (settings.elevationBase);
  pattern.setMirrorSouth (settings.mirrorSouth);
  pattern.setFlat (settings.flat);
  pattern.setFlatElevation (settings.flatElevation);

  pattern.setSpin (settings.spin);
  pattern.setReachLfo (settings.reachLfo);
  pattern.setEnvelopeAttack (settings.envelopeAttack);
  pattern.setEnvelopeDecay (settings.envelopeDecay);
  pattern.setEnvelopeMax (settings.envelopeMax);
  pattern.setFreqAttack (settings.freqAttack);
  pattern.setFreqDecay (settings.freqDecay);
  pattern.setFreqMax (settings.freqMax);
  pattern.setQAttack (settings.qAttack);
  pattern.setQDecay (settings.qDecay);
  pattern.setQMax (settings.qMax);
  pattern.setActMode (settings.actMode);

  pattern.setPlayDirection (settings.direction);
  pattern.setEndAction (settings.endAction);

  pattern.setFadeReach (settings.fadeReach);
  pattern.setBridgeBias (settings.bridgeBias);
}

ClipSettings
actionOver (ClipSettings const &current, ClipSettings const &action)
{
  auto fired = action;

  // Named one by one rather than copied as a block: this is the list the
  // ACTION page owns, and a field that joins it has to be added here too.
  fired.envelopeAttack = current.envelopeAttack;
  fired.envelopeDecay = current.envelopeDecay;
  fired.envelopeMax = current.envelopeMax;
  fired.freqAttack = current.freqAttack;
  fired.freqDecay = current.freqDecay;
  fired.freqMax = current.freqMax;
  fired.qAttack = current.qAttack;
  fired.qDecay = current.qDecay;
  fired.qMax = current.qMax;
  fired.actMode = current.actMode;

  return fired;
}

}
