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
#include <a3-motion-engine/TempoLfo.hh>

namespace a3
{

ClipSettings
clipSettingsFrom (Pattern const &pattern)
{
  ClipSettings settings;

  settings.speedLog2 = pattern.getSpeedLog2 ();
  settings.rotate = pattern.getRotate ();
  settings.squeezeX = pattern.getSqueezeX ();
  settings.squeezeY = pattern.getSqueezeY ();
  settings.squeezeXLfo = pattern.getSqueezeXLfo ();
  settings.squeezeYLfo = pattern.getSqueezeYLfo ();

  settings.reach = pattern.getReach ();
  settings.clipTop = pattern.getClipTop ();
  settings.clipBottom = pattern.getClipBottom ();
  settings.elevationBase = pattern.getElevationBase ();
  settings.mirrorSouth = pattern.getMirrorSouth ();
  settings.flat = pattern.getFlat ();
  settings.flatElevation = pattern.getFlatElevation ();

  settings.spin = pattern.getSpin ();
  settings.reachLfo = pattern.getReachLfo ();
  settings.elevationLfo = pattern.getElevationLfo ();
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
  pattern.setSqueezeX (settings.squeezeX);
  pattern.setSqueezeY (settings.squeezeY);
  pattern.setSqueezeXLfo (settings.squeezeXLfo);
  pattern.setSqueezeYLfo (settings.squeezeYLfo);

  pattern.setReach (settings.reach);
  pattern.setClipTop (settings.clipTop);
  pattern.setClipBottom (settings.clipBottom);
  pattern.setElevationBase (settings.elevationBase);
  pattern.setMirrorSouth (settings.mirrorSouth);
  pattern.setFlat (settings.flat);
  pattern.setFlatElevation (settings.flatElevation);

  pattern.setSpin (settings.spin);
  pattern.setReachLfo (settings.reachLfo);
  pattern.setElevationLfo (settings.elevationLfo);
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

ElevationParams
sweptElevation (ElevationParams params, Pattern const &pattern)
{
  // Out of where each was set and back, towards the end its sign points at.
  // At a step of zero lfoSweep() gives the value back untouched, so a clip
  // with no modulation projects through exactly what it was set to.
  params.reach
      = lfoSweep (params.reach, pattern.getReachLfo (),
                  pattern.getReachLfoPhase ());
  // Which way the cone grows is decided before the base moves, and then held.
  // Deciding it from the swept base -- which is what mapTo3D does on its own
  // -- turned the figure inside out every time the sweep crossed the equator.
  params.coneDirection = params.elevationBase <= 0.5f
                             ? ElevationParams::ConeDirection::South
                             : ElevationParams::ConeDirection::North;

  // ... and the base sweeps only as far as the cone still fits, not to the
  // pole. At the pole the cone has no room at all: every point of the figure
  // clamps onto it, the shape collapses to one direction and is drawn as a
  // line pressed against the rim. Holding the direction without this would
  // have made that the normal end of every sway.
  //
  // The swept reach, not the set one: when the swell opens the coverage the
  // base has to pull in to make room for it.
  auto const room = std::clamp (params.reach, 0.f, 1.f);
  auto const towardsSouth
      = params.coneDirection == ElevationParams::ConeDirection::South;

  params.elevationBase = lfoSweepBetween (
      params.elevationBase, pattern.getElevationLfo (),
      pattern.getElevationLfoPhase (), towardsSouth ? 0.f : room,
      towardsSouth ? 1.f - room : 1.f);

  return params;
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
