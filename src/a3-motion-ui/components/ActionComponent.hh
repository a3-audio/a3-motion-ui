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

#include <a3-motion-ui/components/ActionLayout.hh>
#include <a3-motion-ui/components/TouchControl.hh>
#include <a3-motion-ui/theme/ThemedComponent.hh>

namespace a3
{

/** What the ACT key does, on a page of its own.
 *
 *  Which action clip the slot fires, and the envelope that fires it. The
 *  action itself is chosen in the file menu beside the clips -- one place
 *  where things are picked, rather than a second one here.
 *
 *  It decides nothing. A turn goes out as (control, increment) and lands in
 *  the same handler the clip bar's own controls reach.
 */
class ActionComponent : public juce::Component, public ThemedComponent
{
public:
  /** In the order they are laid out, which is the order the handler expects. */
  enum Control
  {
    Attack = 0,
    Decay,
    EnvelopeMax,
    FreqAttack,
    FreqDecay,
    FreqMax,
    QAttack,
    QDecay,
    QMax,
    /** Not a knob and not in a row -- it stands beside the action's name. */
    ActMode,
    numControls
  };

  ActionComponent ();
  ~ActionComponent () override;

  void paint (juce::Graphics &g) override;
  void resized () override;
  /** The page's geometry comes from the theme's header size, so a skin change
   *  has to re-lay it out rather than only repaint it. */
  void applyTheme () override;

  /** Which clip this page is showing, and the colour it wears. */
  void setTarget (int channel, int slot, juce::Colour channelColour);

  /** Steps 0..envelopeMaxStep, as the engine counts them. */
  void setEnvelope (int attackStep, int decayStep, float max);
  /** The two filter envelopes: the cutoff's, then the resonance's. */
  void setFreqEnvelope (int attackStep, int decayStep, float max);
  void setQEnvelope (int attackStep, int decayStep, float max);
  /** 0 = one-shot, 1 = hold. */
  void setActMode (int mode);

  /** Which action clip this slot fires; empty for none. */
  void setActionName (juce::String const &name);

  /** The script that action carries, shown under its name. Empty for none. */
  void setScript (juce::String const &script);

  /** Where the global strip's three channel rows stand, in the bar's own
   *  coordinates. The page puts its own rows on those so the two blocks of
   *  knobs read across at one height; an empty rectangle means lay out
   *  freely. */
  void setGridReference (juce::Rectangle<int> barCoordinates);

  std::function<void (int control, int increment)> onControlDragged;
  std::function<void (int control)> onControlDoubleTapped;
  std::function<void (int control)> onControlTapped;

private:
  void paintActionField (juce::Graphics &g);
  void paintScriptField (juce::Graphics &g);

  ActionLayout _layout;

  int _channel = 0;
  int _slot = 0;
  juce::Colour _channelColour;

  int _attack = 2;
  int _decay = 3;
  float _max = 1.f;
  int _freqAttack = 2;
  int _freqDecay = 3;
  float _freqMax = 0.f;
  int _qAttack = 2;
  int _qDecay = 3;
  float _qMax = 0.f;
  int _actMode = 0;
  juce::String _actionName;
  juce::String _script;
  juce::Rectangle<int> _gridReference;

  std::array<std::unique_ptr<TouchControl>, numControls> _touch;
};

}
