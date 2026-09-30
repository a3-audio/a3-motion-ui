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

#include <a3-motion-ui/components/MixerComponent.hh>
#include <a3-motion-ui/components/VuMeter.hh>
#include <a3-motion-ui/components/VuMeterView.hh>

#include <vector>

using namespace a3;

namespace
{
constexpr auto roomy = 900;

/** The master column's meters: the views turned a quarter, wider than tall.
 *  The channels' meters stand upright. */
std::vector<juce::Rectangle<int> >
outputBars (juce::Component const &mixer)
{
  std::vector<juce::Rectangle<int> > bars;
  for (auto *child : mixer.getChildren ())
    if (dynamic_cast<VuMeterView const *> (child) != nullptr
        && child->isVisible () && child->getWidth () > child->getHeight ())
      bars.push_back (child->getBounds ());
  return bars;
}

juce::Image
paintWithOutputs (float level)
{
  MixerState state;
  VuLevels levels;
  for (int meter = 0; meter < numOutputMeters; ++meter)
    levels.setOutput (meter, VuLevel{ level, level }, vuNowMs ());

  MixerComponent mixer (state, levels);
  mixer.setBounds (0, 0, roomy, roomy);
  return mixer.createComponentSnapshot (mixer.getLocalBounds ());
}

int
pixelsThatDiffer (juce::Image const &a, juce::Image const &b,
                  juce::Rectangle<int> area)
{
  auto differ = 0;
  for (int y = area.getY (); y < area.getBottom (); ++y)
    for (int x = area.getX (); x < area.getRight (); ++x)
      differ += a.getPixelAt (x, y) != b.getPixelAt (x, y) ? 1 : 0;
  return differ;
}
}

// A page test must paint (the ACTION page crashed on its first frame with a
// green suite, 2026-09-28). Ten meters since 2026-09-30, the main sub and the
// nine main tops: each one shows a signal where its bar is, and silence does
// not.
TEST (MasterColumnPaint, EveryOutputMeterShowsItsSignal)
{
  MixerState state;
  VuLevels levels;
  MixerComponent mixer (state, levels);
  mixer.setBounds (0, 0, roomy, roomy);
  auto const bars = outputBars (mixer);
  ASSERT_EQ (static_cast<int> (bars.size ()), numOutputMeters);

  auto const silent = paintWithOutputs (0.f);
  auto const loud = paintWithOutputs (1.f);

  for (std::size_t i = 0; i < bars.size (); ++i)
    EXPECT_GT (pixelsThatDiffer (silent, loud, bars[i]), 0) << "meter " << i;

  // A look at it, for whoever changes the column next: A3_SNAPSHOT_DIR.
  auto const dir = juce::SystemStats::getEnvironmentVariable (
      "A3_SNAPSHOT_DIR", {});
  if (dir.isNotEmpty ())
    {
      auto file = juce::File (dir).getChildFile ("master-column.png");
      file.deleteFile ();
      juce::FileOutputStream out (file);
      juce::PNGImageFormat ().writeImageToStream (loud, out);
    }
}
