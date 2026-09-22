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

#include "LookAndFeel.hh"

#include <a3-motion-ui/components/BarKnob.hh>
#include <a3-motion-ui/components/MixerComponent.hh>
#include <a3-motion-ui/components/PotKnob.hh>
#include <a3-motion-ui/components/VuMeter.hh>
#include <a3-motion-ui/theme/ThemedComponent.hh>

namespace a3
{

LookAndFeel_A3::LookAndFeel_A3 ()
{
  applyTheme (theme ());
}

void
LookAndFeel_A3::applyTheme (Theme const &theme)
{
  auto const surface = toColour (theme.surface);
  auto const surfaceRaised = toColour (theme.surfaceRaised);
  auto const background = toColour (theme.background);
  auto const textPrimary = toColour (theme.textPrimary);
  auto const textMuted = toColour (theme.textMuted);
  auto const textOnAccent = toColour (theme.textOnAccent);
  auto const accent = toColour (theme.accent);

  setColour (juce::ResizableWindow::backgroundColourId, background);
  setColour (juce::DocumentWindow::textColourId, textPrimary);

  setColour (juce::Label::textColourId, textPrimary);
  setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);

  // The knob keeps its own greys rather than taking the accent: it is a value
  // readout, and an accent-coloured one would compete with the state colours
  // sharing the screen with it.
  setColour (juce::Slider::thumbColourId, textMuted);
  setColour (juce::Slider::rotarySliderFillColourId, textMuted.darker ());
  setColour (juce::Slider::rotarySliderOutlineColourId, surfaceRaised);
  setColour (juce::Slider::backgroundColourId, surface);
  setColour (juce::Slider::trackColourId, accent);

  setColour (juce::TextButton::buttonColourId, surfaceRaised);
  setColour (juce::TextButton::buttonOnColourId, accent);
  setColour (juce::TextButton::textColourOffId, textPrimary);
  setColour (juce::TextButton::textColourOnId, textOnAccent);

  setColour (juce::ComboBox::backgroundColourId, surfaceRaised);
  setColour (juce::ComboBox::textColourId, textPrimary);
  setColour (juce::ComboBox::outlineColourId, surface);

  setColour (juce::PopupMenu::backgroundColourId, surfaceRaised);
  setColour (juce::PopupMenu::textColourId, textPrimary);
  setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
  setColour (juce::PopupMenu::highlightedTextColourId, textOnAccent);

  setColour (juce::ScrollBar::thumbColourId, textMuted);
}

namespace
{
juce::Font
scaled (juce::Font font)
{
  return font.withHeight (font.getHeight ()
                          * theme ().scaleFor (FontRole::Body));
}
}

juce::Font
LookAndFeel_A3::getLabelFont (juce::Label &label)
{
  return scaled (juce::LookAndFeel_V4::getLabelFont (label));
}

juce::Font
LookAndFeel_A3::getComboBoxFont (juce::ComboBox &box)
{
  return scaled (juce::LookAndFeel_V4::getComboBoxFont (box));
}

juce::Font
LookAndFeel_A3::getPopupMenuFont ()
{
  return scaled (juce::LookAndFeel_V4::getPopupMenuFont ());
}

juce::Font
LookAndFeel_A3::getTextButtonFont (juce::TextButton &button, int buttonHeight)
{
  return scaled (
      juce::LookAndFeel_V4::getTextButtonFont (button, buttonHeight));
}


int
LookAndFeel_A3::getSliderThumbRadius (juce::Slider &slider)
{
  if (slider.getSliderStyle () != juce::Slider::LinearVertical)
    return juce::LookAndFeel_V4::getSliderThumbRadius (slider);

  return vuFaderHandle (slider.getLocalBounds (), 0.f).getHeight () / 2;
}

void
LookAndFeel_A3::drawRotarySlider (juce::Graphics &g, int x, int y, int width,
                                  int height, float sliderPosProportional,
                                  float rotaryStartAngle,
                                  float rotaryEndAngle, juce::Slider &slider)
{
  auto *knob = dynamic_cast<PotKnob *> (&slider);
  if (knob == nullptr)
    {
      juce::LookAndFeel_V4::drawRotarySlider (g, x, y, width, height,
                                              sliderPosProportional,
                                              rotaryStartAngle, rotaryEndAngle,
                                              slider);
      return;
    }

  // -1..1 across the scale, which is what the arc is drawn from.
  auto const angle = sliderPosProportional * 2.f - 1.f;

  paintBarKnob (g, juce::Rectangle<int> (x, y, width, height),
                mixerControlMetrics (),
                slider.findColour (juce::Slider::thumbColourId),
                knob->label (), angle, knob->fillsFromTheMiddle (),
                knob->isActive (), knob->isSelected (), knob->reach (),
                knob->wraps ());
}

juce::Slider::SliderLayout
LookAndFeel_A3::getSliderLayout (juce::Slider &slider)
{
  if (slider.getSliderStyle () != juce::Slider::LinearVertical)
    return juce::LookAndFeel_V4::getSliderLayout (slider);

  juce::Slider::SliderLayout layout;
  layout.sliderBounds = slider.getLocalBounds ();
  return layout;
}

void
LookAndFeel_A3::drawLinearSlider (juce::Graphics &g, int x, int y, int width,
                                  int height, float sliderPos,
                                  float minSliderPos, float maxSliderPos,
                                  juce::Slider::SliderStyle style,
                                  juce::Slider &slider)
{
  if (style != juce::Slider::LinearVertical)
    {
      juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height,
                                              sliderPos, minSliderPos,
                                              maxSliderPos, style, slider);
      return;
    }

  // sliderPos is where the slider says its handle belongs, and it is the only
  // answer that stays under the finger: the slider maps its travel over the
  // track less the handle, where our own arithmetic mapped it over the whole
  // track and drifted away on the overlay's long faders.
  auto const bounds = juce::Rectangle<int> (x, y, width, height);
  auto const handle = vuFaderHandleAt (bounds, juce::roundToInt (sliderPos));

  paintVuFaderCap (g, bounds, handle,
                   slider.findColour (juce::Slider::thumbColourId));
}

void
applyThemeEverywhere (Theme loaded, juce::Component &inTree)
{
  // Nothing to carry over any more: the sizes are the skin's, so a reload
  // brings them along instead of having to be spared from it.
  setTheme (loaded);

  // The LookAndFeel does cache: findColour reads what setColour last wrote,
  // so a skin change has to reach it before anything repaints.
  if (auto *lookAndFeel
      = dynamic_cast<LookAndFeel_A3 *> (&inTree.getLookAndFeel ()))
    lookAndFeel->applyTheme (theme ());

  if (auto *root = inTree.getTopLevelComponent ())
    {
      // Most components read the theme while painting, so the repaint is all
      // they need. The ones that cache — juce::Labels take a colour and a
      // font once and keep them — have to be told first.
      applyThemeToTree (*root);
      root->repaint ();
    }
}

}
