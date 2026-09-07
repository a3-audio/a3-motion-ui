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

#include <algorithm>

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
  // The swell moves how far the figure spreads, not which way: the size is
  // swept and the sign is left alone. Swept signed, a reach set upwards would
  // pass through nothing and come out spreading downwards, which is the
  // figure turning inside out rather than breathing.
  //
  // And it stays in the room the base leaves it. A reach of one puts the
  // outer edge a whole half-turn from the base, which only fits when the base
  // is at the pole it is growing away from -- so a clip based a third of the
  // way down, swelling, ran past the floor and wrapped back over it. That was
  // safe while the cone chose a pole for itself, because it always grew
  // towards the further one and there was always room; once it grew the way
  // it was told, the room stopped being guaranteed and nothing was watching.
  //
  // A bound on the sweep, not a second opinion about the knob: with no swell
  // on it the reach is left exactly where the hand put it.
  if (pattern.getReachLfo () != 0)
    {
      auto const towards = params.reach < 0.f ? -1.f : 1.f;
      auto const base = std::clamp (params.elevationBase, 0.f, 1.f);

      // The clips bound it as well as the poles do. They are a hard clamp --
      // a point pushed past one keeps its bearing and gives up its height --
      // so a swell that sweeps into one is not opening the figure out, it is
      // piling it onto the cut.
      auto const floor = 1.f - std::clamp (params.clipBottom, 0.f, 1.f);
      auto const ceiling = std::clamp (params.clipTop, 0.f, 1.f);
      auto const room = std::max (
          0.f, towards > 0.f ? std::min (1.f, floor) - base
                             : base - std::max (0.f, ceiling));

      auto const swept = lfoSweep (std::abs (params.reach),
                                   pattern.getReachLfo (),
                                   pattern.getReachLfoPhase ());

      params.reach = towards * std::min (swept, room);
    }
  // The base travels the whole way, pole to pole.
  params.elevationBase
      = lfoSweep (params.elevationBase, pattern.getElevationLfo (),
                  pattern.getElevationLfoPhase ());

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
