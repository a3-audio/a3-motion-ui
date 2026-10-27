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
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

#include <ShippedSkin.hh>

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

// The face row in the shipped skin, written out for a look when
// A3_SNAPSHOT_DIR is set: every knob on it is StemDeck's (2026-10-16), the
// channel's colour in the arc, a light tick on a dark cap.
TEST (ChannelFaces, TheFaceRowPaintsInTheShippedSkin)
{
  auto const before = theme ();
  setTheme (loadTheme (shippedSkin ()));
  {
    Bar b (BarPage::Clip);
    std::array<juce::Colour, numChannelColumns> colours;
    for (size_t i = 0; i < colours.size (); ++i)
      colours[i] = toColour (theme ().channel[i]);
    b.bar.setChannelFaces (colours, { "Closing Sunset", "Tribal Drift", "",
                                      "Acid Spiral" },
                           1);

    auto const pots = facePots (b.bar);
    ASSERT_FALSE (pots.empty ());
    auto row = pots.front ()->getBounds ();
    for (auto *pot : pots)
      row = row.getUnion (pot->getBounds ());

    auto const image = b.bar.createComponentSnapshot (row);
    EXPECT_EQ (image.getWidth (), row.getWidth ());

    auto const dir
        = juce::SystemStats::getEnvironmentVariable ("A3_SNAPSHOT_DIR", {});
    if (dir.isNotEmpty ())
      {
        auto file = juce::File (dir).getChildFile ("channel-row.png");
        file.deleteFile ();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat ().writeImageToStream (image, out);
      }
  }
  setTheme (before);
}

namespace
{
bool
sameImage (juce::Image const &a, juce::Image const &b)
{
  if (a.getWidth () != b.getWidth () || a.getHeight () != b.getHeight ())
    return false;

  for (int y = 0; y < a.getHeight (); ++y)
    for (int x = 0; x < a.getWidth (); ++x)
      if (a.getPixelAt (x, y) != b.getPixelAt (x, y))
        return false;
  return true;
}

void
writeSnapshot (juce::Image const &image, juce::String const &name)
{
  auto const dir
      = juce::SystemStats::getEnvironmentVariable ("A3_SNAPSHOT_DIR", {});
  if (dir.isEmpty ())
    return;

  auto file = juce::File (dir).getChildFile (name);
  file.deleteFile ();
  juce::FileOutputStream out (file);
  juce::PNGImageFormat ().writeImageToStream (image, out);
}
}

// A channel on a deck shows its deck where the clip's name stood, and a deck
// Core does not report present is drawn differently from one that is (the
// face must paint, in every state, in the shipped skin).
TEST (ChannelFaces, ADeckChannelPaintsItsDeckAndAbsentIsMarked)
{
  auto const before = theme ();
  setTheme (loadTheme (shippedSkin ()));
  {
    Bar b (BarPage::Clip);
    std::array<juce::Colour, numChannelColumns> colours;
    for (size_t i = 0; i < colours.size (); ++i)
      colours[i] = toColour (theme ().channel[i]);
    b.bar.setChannelFaces (colours, { "Closing Sunset", "Tribal Drift", "",
                                      "Acid Spiral" },
                           1);

    auto const faces = b.bar.getLocalBounds ();
    auto const analog = b.bar.createComponentSnapshot (faces);

    b.bar.setChannelDecks ({ "DECK A", "", "", "" }, { false, false, false, false });
    auto const present = b.bar.createComponentSnapshot (faces);
    EXPECT_FALSE (sameImage (analog, present))
        << "the deck is not on the face";

    b.bar.setChannelDecks ({ "DECK A (absent)", "", "", "" },
                           { true, false, false, false });
    auto const absent = b.bar.createComponentSnapshot (faces);
    EXPECT_FALSE (sameImage (present, absent)) << "absent looks like present";

    b.bar.setChannelDecks ({ "", "", "", "" }, { false, false, false, false });
    EXPECT_TRUE (sameImage (analog, b.bar.createComponentSnapshot (faces)))
        << "analog again is not as before";

    writeSnapshot (present, "deck-channel-present.png");
    writeSnapshot (absent, "deck-channel-absent.png");
  }
  setTheme (before);
}
