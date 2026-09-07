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
  auto const towards = params.reach < 0.f ? -1.f : 1.f;

  auto reach = std::abs (params.reach);
  if (pattern.getReachLfo () != 0)
    reach = lfoSweep (reach, pattern.getReachLfo (),
                      pattern.getReachLfoPhase ());

  // The base is held inside the clips before anything else is asked of it.
  //
  // The control that sets it keeps it in the band, but the clips move
  // afterwards: turn the ceiling down past where the base already stands and
  // the base is outside the room it is supposed to be in. Every point of the
  // figure then clamps onto the cut, and a whole trajectory piled onto one
  // height is a straight line drawn across the picture.
  {
    auto const floor = 1.f - std::clamp (params.clipBottom, 0.f, 1.f);
    auto const ceiling = std::clamp (params.clipTop, 0.f, 1.f);

    // Ordered before clamping, so clips pushed past each other pin the base to
    // where they crossed rather than inverting the range -- the same rule the
    // graphic draws its band by.
    params.elevationBase
        = std::clamp (params.elevationBase, std::min (ceiling, floor),
                      std::max (ceiling, floor));
  }

  // The base travels to the cut and turns back there, not at the pole.
  //
  // The clips are a hard clamp on where the sound may go, so a sway that swept
  // past one spent part of every cycle standing on the cut while the number
  // behind it carried on: the movement stopped and the reading did not, which
  // reads as the sway travelling through the part of the room that was taken
  // away. Where there is no cut there is no bound and it still goes pole to
  // pole -- the bound is the cut, not a second opinion about the sweep.
  if (pattern.getElevationLfo () != 0)
    {
      auto const to = pattern.getElevationLfo () > 0
                          ? 1.f - std::clamp (params.clipBottom, 0.f, 1.f)
                          : std::clamp (params.clipTop, 0.f, 1.f);

      auto const from = std::clamp (params.elevationBase, 0.f, 1.f);

      params.elevationBase
          = from
            + (to - from) * lfoTravel (pattern.getElevationLfoPhase ());
    }

  // A cut bounds the reach -- afterwards, because the sway is what moves the
  // base it is measured from. The clips are a hard clamp on where the sound
  // may go: a point pushed past one keeps its bearing and gives up its
  // height, so a reach that runs into a cut is not a bigger figure, it is a
  // figure piled onto the cut, drawn as a straight bar across the picture.
  //
  // A pole is not a cut, and this used to treat it as one -- the reach was
  // held to `1 - base` whether or not anything had been cut away. That
  // forbade the one thing the band model is for: a figure that runs *over the
  // outer wall*, carrying on past a pole rather than stopping at it. The
  // price was paid where elevation is most often left. Based a tenth off the
  // floor, a reach of 0.65 came out as 0.108: the figure shrank to a splinter
  // while the run from the pad's middle to the pole kept its full length, and
  // that run was then the whole picture -- four straight arms across the room
  // that are in no shape.
  //
  // So: a reach the hand set is left exactly where the hand set it, unless a
  // clip is actually in the way.
  {
    auto const base = std::clamp (params.elevationBase, 0.f, 1.f);
    auto const cut = towards > 0.f ? std::clamp (params.clipBottom, 0.f, 1.f)
                                   : std::clamp (params.clipTop, 0.f, 1.f);

    if (cut > 0.f)
      {
        auto const wall = towards > 0.f ? 1.f - cut : cut;
        auto const room = std::max (
            0.f, towards > 0.f ? wall - base : base - wall);

        reach = std::min (reach, room);
      }

    params.reach = towards * reach;
  }

  return params;
}

float
defaultReach (float current)
{
  juce::ignoreUnused (current);
  return 0.f;
}

float
defaultElevationBase (float clipTop, float clipBottom)
{
  auto const ceiling = std::clamp (clipTop, 0.f, 1.f);
  auto const floor = 1.f - std::clamp (clipBottom, 0.f, 1.f);

  return (std::min (ceiling, floor) + std::max (ceiling, floor)) * 0.5f;
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
