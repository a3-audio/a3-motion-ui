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

#include "StatusBar.hh"

#include <a3-motion-ui/components/TickPlayheads.hh>

#include <a3-motion-ui/components/LookAndFeel.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

#include <algorithm>
#include <sstream>

namespace
{
auto constexpr beatsPerBar = 4; // TODO read from tempoclock

/** Whether a meter would be drawn the same twice.
 *
 *  The whole point of asking is the repaint it saves, so it compares the two
 *  numbers a meter is drawn from and nothing else. */
bool
sameLevel (a3::VuLevel a, a3::VuLevel b)
{
  return juce::approximatelyEqual (a.peak, b.peak)
         && juce::approximatelyEqual (a.rms, b.rms);
}
}

namespace a3
{

StatusBar::StatusBar (juce::Value &valueBPM)
    : _tickIndicator (beatsPerBar), _valueBPM (valueBPM)
{
  addChildComponent (_tickIndicator);
  _tickIndicator.setVisible (true);
  

  addChildComponent (_labelBPM);
  _labelBPM.setVisible (true);
  _labelBPM.setJustificationType (juce::Justification::centredLeft);
  _labelBPM.setText ("BPM 60.0", juce::dontSendNotification);

  // Right-aligned, so it grows leftwards into the gap rather than towards the
  // keyboard icon a thumb is reaching for.
  addChildComponent (_labelReadout);
  _labelReadout.setVisible (true);
  _labelReadout.setJustificationType (juce::Justification::centredRight);
  
  // Register for BPM value changes
  _valueBPM.addListener (this);
  
  


  applyTheme ();
}

namespace
{
// resized() keeps a sixth of the bar free above and below the labels, so a
// label is given two thirds of the height, and a text row is drawn a quarter
// taller than its font.
constexpr float labelShare = 2.f / 3.f;
constexpr float rowHeightFactor = 1.25f;
}

float
StatusBar::headerFontSize () const
{
  // The header size, unless the height this bar was actually given is
  // smaller — then the text would be clipped top and bottom rather than
  // drawn larger. preferredHeight() is what keeps the two in step.
  auto const wanted = theme ().fontSize (FontRole::Header);
  if (getHeight () <= 0)
    return wanted;

  auto const room
      = static_cast<float> (getHeight ()) * labelShare / rowHeightFactor;

  return juce::jmin (wanted, room);
}

int
StatusBar::preferredHeight () const
{
  auto const needed = theme ().fontSize (FontRole::Header) * rowHeightFactor
                      / labelShare;

  return juce::jmax (getMinimumHeight (), static_cast<int> (needed));
}

void
StatusBar::refreshClockReadout ()
{
  auto const mode = juce::jlimit (0, 2, _clockMode.load ());

  auto const bpm = mode == 0
                       ? (_valueBPM.getValue ().isDouble ()
                              ? static_cast<float> (_valueBPM.getValue ())
                              : 0.f)
                       : _externalBPM.load ();

  // The number only. Which clock it comes from is written on the clock key
  // itself now, in that mode's colour, on the screen and under the hand —
  // saying it a third time up here was three places to keep in step and one
  // more thing to read.
  //
  // The colour stays: the reading is still somebody else's tempo or ours, and
  // that is worth knowing at a glance about the number itself.
  juce::String text;
  if (bpm > 0.f)
    text << "BPM " << juce::String (bpm, 1);

  _labelBPM.setText (text, juce::dontSendNotification);
  _labelBPM.setColour (juce::Label::textColourId, clockReadoutColour ());
}

juce::Colour
StatusBar::clockReadoutColour () const
{
  return Colours::clockMode (_clockMode);
}

void
StatusBar::applyTheme ()
{
  // Set here rather than in the constructor: a skin loaded afterwards has to
  // reach them, and a Label keeps whatever colour it was given. Through the
  // one colour, not plain accent — resetting both to accent here is what
  // left the mode reading green while the BPM beside it went orange on the
  // next external beat, which only recoloured the BPM.
  refreshClockReadout ();
  refreshFonts ();
}

void
StatusBar::refreshFonts ()
{
  // The status bar is a header, and so are the section titles it sits above.
  // Before they shared a role the bar took juce's default label height and
  // came out larger than the headings below it.
  auto const font = juce::Font (juce::FontOptions (headerFontSize ()));

  for (auto *label : { &_labelBPM, &_labelReadout })
    label->setFont (font);
}

StatusBar::~StatusBar ()
{
  _valueBPM.removeListener (this);
}

void
StatusBar::resized ()
{
  // The size the labels are drawn at is a function of the height this bar was
  // just given, so it is read here rather than pushed from outside. Setting it
  // before the new height arrived was the bug: raising Header Size in the menu
  // sized the text against the bar's *old* height and left it too small until
  // the next start, which then looked like the restart was wrong.
  refreshFonts ();

  auto bounds = getLocalBounds ();

  // Symmetrical padding above and below the clock/timer display
  auto const verticalPadding = bounds.getHeight () / 6.f;
  bounds.removeFromTop (verticalPadding);
  bounds.removeFromBottom (verticalPadding);

  // The keyboard toggle sits at the very edge, right of everything else, so
  // it is reachable with a thumb without covering a reading. Half again as
  // wide as it is tall: the face inside is inset on all four sides, and at a
  // square it came out small enough to have to aim at.
  _keyboardIconArea = bounds.removeFromRight (
      static_cast<int> (bounds.getHeight () * 1.5f));

  // Left of the keyboard icon and the same size, so the two read as a pair of
  // keys at the end of the bar rather than as two unrelated marks. What it
  // costs is width off the band the two labels share; the readout absorbs
  // most of it, being right-aligned and growing leftwards into the gap, and
  // it is the one thing here that can give width up — text that is read
  // rather than a target that is hit.
  _mixIconArea = bounds.removeFromRight (_keyboardIconArea.getWidth ());

  // Everything left on the bar comes out of one calculation with a test of
  // its own, the way the clip settings bar and the controller page do it: the
  // two readings, the nine meters and the beat display are placed against
  // each other rather than each carving what it wants off the band, and
  // paint() then draws into the rectangles the test checked.
  _layout = statusBarLayout (bounds, getWidth (),
                             juce::roundToInt (theme ().paddingSmall));

  _labelBPM.setBounds (_layout.bpm);
  _labelReadout.setBounds (_layout.readout);
  _tickIndicator.setBounds (_layout.tick);
}

void
StatusBar::setVuLevels (std::array<VuLevel, numChannelsInitial> const &inputs,
                        std::array<VuLevel, numOutputMeters> const &outputs)
{
  // Asked block by block, and the repaint is per block. This bar is on screen
  // for the whole of a set: a plain repaint() here would redraw the beat
  // display, both labels and both icons at the timer's rate forever, for the
  // sake of nine bars that between them cover a fifteenth of it.
  if (!std::equal (inputs.begin (), inputs.end (), _inputLevels.begin (),
                   sameLevel))
    {
      _inputLevels = inputs;
      repaint (_layout.inputBlock);
    }

  if (!std::equal (outputs.begin (), outputs.end (), _outputLevels.begin (),
                   sameLevel))
    {
      _outputLevels = outputs;
      repaint (_layout.outputBlock);
    }
}

void
StatusBar::paintVuMeters (juce::Graphics &g)
{
  // The mixer's own picture at a fraction of the size, not a second meter
  // drawn to a rule of its own: green, yellow and red down a bar is one
  // language, and a small one that spoke it differently would be read wrong
  // exactly once, at the moment it mattered.
  //
  // The bands do survive the shrinking, which was the open question. Measured
  // on the device at the shipped skin the bars come out six pixels across and
  // twenty-four tall, so the red band is about two pixels and the yellow
  // about five -- but both sit at the *head* of the bar against a dark track,
  // where a change of hue is legible long before a length is. What is lost is
  // reading a number off the scale, which is what the mixer page is for.
  //
  // **One thing does differ, and it has to.** On the ground the mixer's own
  // meters stand on, these were invisible until something arrived: that page
  // fills itself with `surface` and the track is `surfaceRaised`, which is the
  // colour this bar *is*. Sunk into it instead, so nine empty meters still
  // read as nine meters -- a meter has to be findable before it has anything
  // to say. See paintVuMeter's four-argument form.
  auto const track = toColour (theme ().surface);

  for (std::size_t i = 0; i < _inputLevels.size (); ++i)
    paintVuMeter (g, _layout.inputMeters[i], _inputLevels[i], track);

  for (std::size_t i = 0; i < _outputLevels.size (); ++i)
    paintVuMeter (g, _layout.outputMeters[i], _outputLevels[i], track);
}

void
StatusBar::setControlReadout (juce::String const &text)
{
  if (text == _labelReadout.getText ())
    return;

  _labelReadout.setText (text, juce::dontSendNotification);
  _labelReadout.setColour (juce::Label::textColourId,
                           toColour (theme ().textMuted));
}

void
StatusBar::paintOverChildren (juce::Graphics &g)
{
  auto const tick = _tickIndicator.getBounds ().toFloat ();

  // The playheads are drawn after the recording fill, not under it: while a
  // take runs on one channel the others keep playing, and a mark hidden by
  // the fill would be missing in exactly the moment both are worth knowing.
  paintPlayheads (g, tick);

  // The count-in: the same mark, in the same place, before there is anything
  // to fill. A beat blinks it; between beats it is gone, so the bar is not
  // carrying a light that never changes. Drawn at the left edge because that
  // is where the fill will start -- the blink becomes the fill rather than
  // being replaced by it.
  if (_countingIn)
    {
      if (_countInLit)
        {
          g.setColour (_recordingColour.withAlpha (theme ().alphaSecondary));
          g.fillRoundedRectangle (
              tick.withWidth (juce::jmin (tick.getHeight (),
                                          tick.getWidth ())),
              theme ().radiusTick);
        }
      return;
    }

  if (_recordingProgress < 0.f)
    return;

  // How far the running take has got, laid over the tick indicator itself
  // rather than beside it: the beat display is the widest thing on this bar
  // and sits over the sphere, where the eye already is while recording. Kept
  // translucent so the beats stay readable through it, and drawn over the
  // children because the indicator is one of them.
  // 0.45 is 0.05 from alphaMuted (0.5), inside the snapping tolerance.
  g.setColour (_recordingColour.withAlpha (theme ().alphaMuted));
  g.fillRoundedRectangle (tick.withWidth (tick.getWidth ()
                                          * _recordingProgress),
                          theme ().radiusTick);
}

void
StatusBar::setCountingIn (bool countingIn, juce::Colour colour)
{
  if (countingIn == _countingIn && colour == _recordingColour)
    return;

  _countingIn = countingIn;
  _recordingColour = colour;

  // Leaving it lit would carry the last blink into the take.
  if (!countingIn)
    _countInLit = false;

  repaint ();
}

void
StatusBar::pulseCountInOnBeat ()
{
  if (!_countingIn)
    return;

  _countInLit = true;
  repaint ();

  // Long enough to catch out of the corner of an eye, short enough that the
  // bar is dark again before the next beat at any tempo this device runs at.
  juce::Timer::callAfterDelay (
      90, [safe = juce::Component::SafePointer<StatusBar> (this)] {
        if (safe == nullptr)
          return;
        safe->_countInLit = false;
        safe->repaint ();
      });
}

void
StatusBar::setRecordingProgress (float fraction, juce::Colour colour)
{
  if (juce::approximatelyEqual (fraction, _recordingProgress)
      && colour == _recordingColour)
    return;

  _recordingProgress = fraction;
  _recordingColour = colour;
  repaint ();
}

void
StatusBar::setChannelPlayheads (
    std::array<float, numChannelsInitial> const &positions,
    std::array<juce::Colour, numChannelsInitial> const &colours)
{
  if (positions == _playheads && colours == _playheadColours)
    return;

  _playheads = positions;
  _playheadColours = colours;
  repaint ();
}

void
StatusBar::paintPlayheads (juce::Graphics &g, juce::Rectangle<float> tick)
{
  for (size_t channel = 0; channel < _playheads.size (); ++channel)
    {
      auto const head = playheadBounds (tick, _playheads[channel],
                                        theme ().strokeThick);
      if (head.isEmpty ())
        continue;

      g.setColour (_playheadColours[channel]);
      g.fillRect (head);
    }
}

void
StatusBar::setKeyboardState (KeyboardState state)
{
  if (_keyboardState == state)
    return;

  _keyboardState = state;
  repaint ();
}

void
StatusBar::setMixOpen (bool open)
{
  if (_mixOpen == open)
    return;

  _mixOpen = open;
  repaint (_mixIconArea);
}

void
StatusBar::mouseUp (juce::MouseEvent const &event)
{
  if (_mixIconArea.contains (event.getPosition ()) && onMixIconTapped)
    {
      onMixIconTapped ();
      return;
    }

  if (_keyboardIconArea.contains (event.getPosition ())
      && onKeyboardIconTapped)
    onKeyboardIconTapped ();
}

void
StatusBar::paintMixKey (juce::Graphics &g)
{
  if (_mixIconArea.isEmpty ())
    return;

  // The same rule the strip's function keys are drawn by: the colour says
  // which key this is and the ground says what it is doing. Open, the accent
  // is washed into a face behind the word the way MENU wears the menu it is
  // inside of; closed, there is no face at all — this bar is not a card, and
  // a resting box here would put a permanent frame on a strip that has none.
  //
  // Muted while closed rather than tinted, because the key beside it already
  // says state that way: the keyboard icon is muted when it is merely
  // available and accented when it is up, and two neighbouring keys reading
  // by two rules is two rules to learn.
  if (_mixOpen)
    {
      g.setColour (toColour (theme ().accent, theme ().alphaFillEmphasis));
      g.fillRoundedRectangle (_mixIconArea.toFloat (),
                              theme ().radiusControl);
    }

  g.setFont (juce::Font (juce::FontOptions (headerFontSize ())));
  g.setColour (_mixOpen ? toColour (theme ().accent)
                        : toColour (theme ().textMuted));
  g.drawFittedText ("MIX", _mixIconArea, juce::Justification::centred, 1);
}

void
StatusBar::paint (juce::Graphics &g)
{
  // The window behind this component paints with juce's stock look, which no
  // skin can reach — the band under the clock stayed the same grey in every
  // skin. It is painted here instead, from the role that describes it.
  g.fillAll (toColour (theme ().surfaceRaised));

  // Before the two keys and after the ground: they are clipped to their own
  // blocks on a refresh, so what is drawn after them here costs nothing on
  // the frames that are actually paid for.
  paintVuMeters (g);

  paintMixKey (g);

  // A keyboard, drawn rather than typed: three rows of keys and a space bar,
  // small enough to read as an icon at this size.
  auto const face = _keyboardIconArea.reduced (_keyboardIconArea.getWidth () / 5,
                                               _keyboardIconArea.getHeight () / 3);
  if (face.isEmpty ())
    return;

  g.setColour (_keyboardState == KeyboardState::Shown
                   ? toColour (theme ().accent)
               : _keyboardState == KeyboardState::Available
                   ? toColour (theme ().textMuted)
                   : toColour (theme ().textMuted, theme ().alphaDisabled));
  g.drawRoundedRectangle (face.toFloat (), theme ().radiusTick,
                          theme ().strokeThin);

  auto const keyW = face.getWidth () / 5.f;
  auto const keyH = face.getHeight () / 4.f;
  for (int row = 0; row < 2; ++row)
    for (int column = 0; column < 4; ++column)
      g.fillRect (face.getX () + keyW * (column + 0.5f),
                  face.getY () + keyH * (row + 0.6f), keyW * 0.6f,
                  keyH * 0.6f);

  g.fillRect (face.getX () + keyW * 1.f,
              face.getY () + keyH * 2.7f, keyW * 3.f, keyH * 0.6f);
}

void
StatusBar::valueChanged (juce::Value &value)
{
  if (value.refersToSameSourceAs (_valueBPM))
    {
      // Only update BPM display when in internal clock mode
      if (_clockMode != 0)
        return;
        
      jassert (value.getValue ().isDouble ());

      // Through the one writer. Written here directly it came out without the
      // clock's colour, so the reading told you the tempo but not whose it
      // was — and only sometimes, depending which of three writers got there
      // last.
      refreshClockReadout ();
    }
}

void
StatusBar::beatCallback (Measure measure)
{
  // Only update tick indicator when in internal clock mode
  if (_clockMode != 0)
    return;
    
  _tickIndicator.setCurrentTick (measure.beat ());
  
}

void
StatusBar::setExternalBPM (float bpm)
{
  _externalBPM = bpm;
  
  auto stringStream = std::stringstream ();
  stringStream.precision (1);
  stringStream << std::fixed << bpm << " BPM";
  
  // Update on message thread – check clock mode inside lambda to avoid
  // race condition when mode switches between queuing and execution
  juce::Component::SafePointer<StatusBar> safeThis (this);
  juce::MessageManager::callAsync ([safeThis, str = stringStream.str ()] () {
    if (safeThis == nullptr) return;
    if (safeThis->_clockMode == 0)
      return;
    safeThis->refreshClockReadout ();
  });
}

void
StatusBar::setBeatClock (int beat, int bar)
{
  _beatClockBeat = beat;
  _beatClockBar = bar;
  
  // Update on message thread (beat is 1-based from external, convert to 0-based for tick indicator)
  // Check clock mode inside lambda to avoid race condition when mode
  // switches between queuing and execution
  int tickBeat = (beat - 1) % 4;  // Convert 1-4 to 0-3
  juce::Component::SafePointer<StatusBar> safeThis (this);
  // The bar/beat reading itself is gone from the panel — the tick indicator
  // says the same thing without a number to read.
  juce::MessageManager::callAsync ([safeThis, tickBeat] () {
    if (safeThis == nullptr) return;
    if (safeThis->_clockMode == 0)
      return;
    safeThis->_tickIndicator.setCurrentTick (tickBeat);
  });
}

void
StatusBar::setClockMode (int mode)
{
  _clockMode = mode;
  
  juce::Component::SafePointer<StatusBar> safeThis (this);
  juce::MessageManager::callAsync ([safeThis, mode] () {
    if (safeThis == nullptr) return;
    auto *self = safeThis.getComponent ();
    if (mode != 0)
      {
        self->refreshClockReadout ();

        
        // Show external beat clock if available
        int beat = self->_beatClockBeat.load ();
        if (beat > 0)
          {
            int tickBeat = (beat - 1) % 4;
            self->_tickIndicator.setCurrentTick (tickBeat);
          }
      }
    else
      {
        self->refreshClockReadout ();
      }
  });
}

}
