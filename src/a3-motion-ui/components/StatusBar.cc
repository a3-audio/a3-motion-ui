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

#include <a3-motion-ui/components/LookAndFeel.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

#include <sstream>

namespace
{
auto constexpr beatsPerBar = 4; // TODO read from tempoclock
}

namespace a3
{

StatusBar::StatusBar (juce::Value &valueBPM)
    : _tickIndicator (beatsPerBar), _valueBPM (valueBPM)
{
  addChildComponent (_tickIndicator);
  _tickIndicator.setVisible (true);
  
  addChildComponent (_labelOrientation);
  _labelOrientation.setVisible (true);
  _labelOrientation.setJustificationType (juce::Justification::centredLeft);
  _labelOrientation.setText (juce::CharPointer_UTF8 ("0\xc2\xb0"), juce::dontSendNotification);

  addChildComponent (_labelBPM);
  _labelBPM.setVisible (true);
  _labelBPM.setJustificationType (juce::Justification::centredLeft);
  _labelBPM.setText ("BPM 60.0", juce::dontSendNotification);  // Default tempo
  
  // Register for BPM value changes
  _valueBPM.addListener (this);
  
  addChildComponent (_labelBeatClock);
  _labelBeatClock.setVisible (true);
  _labelBeatClock.setJustificationType (juce::Justification::centredRight);
  _labelBeatClock.setText ("1.1", juce::dontSendNotification);  // Initial beat/bar
  
  addChildComponent (_labelClockMode);
  _labelClockMode.setVisible (true);
  _labelClockMode.setJustificationType (juce::Justification::centredRight);
  _labelClockMode.setText ("INT", juce::dontSendNotification);

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
StatusBar::applyTheme ()
{
  // The clock readouts are accent; the orientation is a quieter aside beside
  // them. Set here rather than in the constructor: a skin loaded afterwards
  // has to reach them, and a Label keeps whatever colour it was given.
  for (auto *label : { &_labelBPM, &_labelBeatClock, &_labelClockMode })
    label->setColour (juce::Label::textColourId, toColour (theme ().accent));

  _labelOrientation.setColour (
      juce::Label::textColourId,
      toColour (theme ().textMuted, theme ().alphaInactive));

  refreshFonts ();
}

void
StatusBar::refreshFonts ()
{
  // The status bar is a header, and so are the section titles it sits above.
  // Before they shared a role the bar took juce's default label height and
  // came out larger than the headings below it.
  auto const font = juce::Font (juce::FontOptions (headerFontSize ()));

  for (auto *label : { &_labelOrientation, &_labelBPM, &_labelBeatClock,
                       &_labelClockMode })
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

  // The two end labels are as wide as their text needs. Fixed widths cut
  // "0\xc2\xb0" and "INT" down to an ellipsis as soon as the header size grew.
  auto const glyphWidth = headerFontSize () * 0.62f;

  // Orientation label on the far left
  auto orientArea = bounds.removeFromLeft (
      juce::jmax (40, static_cast<int> (glyphWidth * 4.f
                                        + LayoutHints::padding)));
  _labelOrientation.setBounds (orientArea.withTrimmedLeft (LayoutHints::padding));

  // BPM label next to orientation
  auto leftArea = bounds.removeFromLeft (bounds.getWidth () / 4);
  _labelBPM.setBounds (leftArea.withTrimmedLeft (LayoutHints::padding));

  // Clock mode label on the far right
  // The keyboard toggle sits at the very edge, right of everything else, so
  // it is reachable with a thumb without covering a reading.
  _keyboardIconArea = bounds.removeFromRight (bounds.getHeight ());

  auto clockModeArea = bounds.removeFromRight (
      juce::jmax (50, static_cast<int> (glyphWidth * 3.f
                                        + LayoutHints::padding)));
  _labelClockMode.setBounds (clockModeArea.withTrimmedRight (LayoutHints::padding));

  // Beat clock label next to clock mode
  auto rightArea = bounds.removeFromRight (bounds.getWidth () / 3);
  _labelBeatClock.setBounds (rightArea.withTrimmedRight (LayoutHints::padding));

  // Tick indicator in the center
  auto boundsTicks = bounds.withSizeKeepingCentre (bounds.getWidth () * 0.8f,
                                                   bounds.getHeight () * 0.6f);
  _tickIndicator.setBounds (boundsTicks);
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
StatusBar::mouseUp (juce::MouseEvent const &event)
{
  if (_keyboardIconArea.contains (event.getPosition ())
      && onKeyboardIconTapped)
    onKeyboardIconTapped ();
}

void
StatusBar::paint (juce::Graphics &g)
{
  // The window behind this component paints with juce's stock look, which no
  // skin can reach — the band under the clock stayed the same grey in every
  // skin. It is painted here instead, from the role that describes it.
  g.fillAll (toColour (theme ().surfaceRaised));

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
  g.drawRoundedRectangle (face.toFloat (), 2.f, 1.f);

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

      auto const bpm = static_cast<float> (value.getValue ());
      auto stringStream = std::stringstream ();
      stringStream.precision (1);
      stringStream << "BPM " << std::fixed << bpm;

      _labelBPM.setText (stringStream.str (), juce::dontSendNotification);
    }
}

void
StatusBar::beatCallback (Measure measure)
{
  // Only update tick indicator when in internal clock mode
  if (_clockMode != 0)
    return;
    
  _tickIndicator.setCurrentTick (measure.beat ());
  
  // Show as beat/4 (beatsPerBar is fixed to 4)
  auto text = juce::String (measure.beat () + 1) + "/4";
  _labelBeatClock.setText (text, juce::dontSendNotification);
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
    auto colour = safeThis->_clockMode == 2 ? toColour (theme ().accent)
                                            : toColour (theme ().warning);
    safeThis->_labelBPM.setText (str, juce::dontSendNotification);
    safeThis->_labelBPM.setColour (juce::Label::textColourId, colour);
  });
}

void
StatusBar::setBeatClock (int beat, int bar)
{
  _beatClockBeat = beat;
  _beatClockBar = bar;
  
  // Show as beat/4 (beatsPerBar is fixed to 4)
  auto text = juce::String (beat) + "/4";
  
  // Update on message thread (beat is 1-based from external, convert to 0-based for tick indicator)
  // Check clock mode inside lambda to avoid race condition when mode
  // switches between queuing and execution
  int tickBeat = (beat - 1) % 4;  // Convert 1-4 to 0-3
  juce::Component::SafePointer<StatusBar> safeThis (this);
  juce::MessageManager::callAsync ([safeThis, text, tickBeat] () {
    if (safeThis == nullptr) return;
    if (safeThis->_clockMode == 0)
      return;
    auto colour = safeThis->_clockMode == 2 ? toColour (theme ().accent)
                                            : toColour (theme ().warning);
    safeThis->_tickIndicator.setCurrentTick (tickBeat);
    safeThis->_labelBeatClock.setText (text, juce::dontSendNotification);
    safeThis->_labelBeatClock.setColour (juce::Label::textColourId, colour);
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
        // EXT (1) = orange, PIO (2) = cyan
        auto colour = mode == 2 ? toColour (theme ().accent)
                                            : toColour (theme ().warning);
        auto label  = mode == 2 ? "PIO" : "EXT";
        
        self->_labelClockMode.setText (label, juce::dontSendNotification);
        self->_labelClockMode.setColour (juce::Label::textColourId, colour);
        self->_labelBPM.setColour (juce::Label::textColourId, colour);
        self->_labelBeatClock.setColour (juce::Label::textColourId, colour);
        
        // Show external BPM if available
        float extBpm = self->_externalBPM.load ();
        if (extBpm > 0.f)
          {
            auto stringStream = std::stringstream ();
            stringStream.precision (1);
            stringStream << std::fixed << extBpm << " BPM";
            self->_labelBPM.setText (stringStream.str (), juce::dontSendNotification);
          }
        
        // Show external beat clock if available
        int beat = self->_beatClockBeat.load ();
        if (beat > 0)
          {
            auto text = juce::String (beat) + "/4";
            self->_labelBeatClock.setText (text, juce::dontSendNotification);
            int tickBeat = (beat - 1) % 4;
            self->_tickIndicator.setCurrentTick (tickBeat);
          }
      }
    else
      {
        self->_labelClockMode.setText ("INT", juce::dontSendNotification);
        self->_labelClockMode.setColour (juce::Label::textColourId, toColour (theme ().accent));
        self->_labelBPM.setColour (juce::Label::textColourId, toColour (theme ().accent));
        self->_labelBeatClock.setColour (juce::Label::textColourId, toColour (theme ().accent));
        
        // Show internal BPM
        if (self->_valueBPM.getValue ().isDouble ())
          {
            auto const bpm = static_cast<float> (self->_valueBPM.getValue ());
            auto stringStream = std::stringstream ();
            stringStream.precision (1);
            stringStream << "BPM " << std::fixed << bpm;
            self->_labelBPM.setText (stringStream.str (), juce::dontSendNotification);
          }
        
        // Reset tick indicator and beat counter to show internal state
        // (will be updated on next internal beat via beatCallback)
        self->_tickIndicator.setCurrentTick (0);
        self->_labelBeatClock.setText ("1/4", juce::dontSendNotification);
      }
  });
}

}
