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

#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <a3-motion-ui/components/ClipSettingsComponent.hh>
#include <a3-motion-ui/components/ControllerLayout.hh>
#include <a3-motion-ui/components/LookAndFeel.hh>
#include <a3-motion-ui/components/MixerControls.hh>

#include <algorithm>

using namespace a3;

namespace
{
// The face pots of a bar, left to right: the children that are knobs
// captioned 3D, FREQ or Q.
std::vector<PotKnob *>
facePots (ClipSettingsComponent &bar)
{
  std::vector<PotKnob *> pots;
  for (auto *child : bar.getChildren ())
    if (auto *knob = dynamic_cast<PotKnob *> (child))
      for (auto const pot : channelPotOrder)
        if (knob->label () == channelPotLabel (pot))
          pots.push_back (knob);

  std::sort (pots.begin (), pots.end (), [] (auto *a, auto *b) {
    return a->getX () < b->getX ();
  });
  return pots;
}

struct Bar
{
  explicit Bar (BarPage page)
  {
    // The device's panel as JUCE reports it: a fingertip is 69 px there.
    useDisplayForFingertip (195.0, 1.0);
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

    bar.setPage (page);
    bar.setBounds (0, 0, 768, bar.preferredHeight (768));
    bar.setVisible (true);

    std::array<juce::Colour, numChannelColumns> colours{
      juce::Colours::orange, juce::Colours::deepskyblue,
      juce::Colours::limegreen, juce::Colours::hotpink
    };
    std::array<juce::String, numChannelColumns> names{
      "Closing Sunset", "Tribal Drift", "", "Acid Spiral"
    };
    bar.setChannelFaces (colours, names, 1);
    bar.setChannelProgress ({ 0.25f, -1.f, -1.f, 0.8f });
  }

  ~Bar ()
  {
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    useDisplayForFingertip (unknownDisplayDpi, 1.0);
  }

  LookAndFeel_A3 lookAndFeel;
  ClipSettingsComponent bar;
};
}

// Each face's 3D, FREQ and Q are columns as tall as a fingertip at least,
// side by side across the face (#65), and the knobs on screen are those
// columns: the touch area is the column, not the drawn ring.
TEST (ChannelFaces, ThePotsAreFingertipTallColumns)
{
  Bar b (BarPage::Clip);
  auto const pots = facePots (b.bar);
  ASSERT_EQ (pots.size (), numChannelColumns * numChannelPots);

  for (std::size_t i = 0; i < pots.size (); ++i)
    {
      EXPECT_TRUE (pots[i]->isVisible ()) << i;
      EXPECT_GE (pots[i]->getHeight (), displayFingertip ()) << i;
      EXPECT_GT (pots[i]->getHeight (), pots[i]->getWidth ())
          << "a column, " << i;

      // The three of one face stand edge to edge.
      if (i % numChannelPots != 0)
        EXPECT_LE (pots[i]->getX () - pots[i - 1]->getRight (), 1) << i;
    }

  // The whole range is four fingertips of travel on every one of them.
  EXPECT_EQ (pots[0]->getMouseDragSensitivity (),
             juce::roundToInt (fingertipsForTheWholeRange
                               * static_cast<float> (displayFingertip ())));
}

// The face row paints, on every page it stands on (a page test must paint).
TEST (ChannelFaces, TheFaceRowPaintsOnEveryPage)
{
  for (auto const page : { BarPage::Clip, BarPage::Motion, BarPage::Action,
                           BarPage::Mixer, BarPage::Record })
    {
      Bar b (page);
      auto const image
          = b.bar.createComponentSnapshot (b.bar.getLocalBounds ());
      EXPECT_EQ (image.getWidth (), b.bar.getWidth ());
      EXPECT_EQ (image.getHeight (), b.bar.getHeight ());
    }
}
