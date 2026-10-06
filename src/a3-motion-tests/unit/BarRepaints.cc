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

using namespace a3;

namespace
{
// What a component asks to have redrawn, caught where JUCE hands it on: a
// component with a cached image passes every repaint of its own and of its
// children to that image first. Answering false stops it there, so nothing
// needs a window.
class RepaintRecorder : public juce::CachedComponentImage
{
public:
  explicit RepaintRecorder (int &requests) : _requests (requests) {}

  void paint (juce::Graphics &) override {}
  bool invalidateAll () override
  {
    ++_requests;
    return false;
  }
  bool invalidate (juce::Rectangle<int> const &) override
  {
    ++_requests;
    return false;
  }
  void releaseResources () override {}

private:
  int &_requests;
};

TrajectoryIconData
someIcon ()
{
  juce::Path path;
  path.addEllipse (-1.f, -1.f, 2.f, 2.f);
  return trajectoryIconFromPath (path, {});
}

// Everything A3MotionUIComponent::updateClipSettingsDisplay() hands the bar,
// with one clip's values. It runs on the 20 Hz timer while anything plays.
void
describeOneClip (ClipSettingsComponent &bar)
{
  bar.setTarget (1, 0, juce::Colours::orange);
  bar.setTrajectoryIcon (someIcon ());
  bar.setTrajectoryName ("Circle");
  bar.setClipName ("Closing Sunset", false);
  bar.setSlotDrifted (0, false);
  bar.setRecMode (RecMode::Latch);
  bar.setElevationSubIndex (1);

  std::array<juce::Colour, numChannelColumns> colours;
  std::array<juce::String, numChannelColumns> names;
  colours.fill (juce::Colours::orange);
  names.fill ("Closing Sunset");
  bar.setChannelFaces (colours, names, 1);

  bar.setElevationReach (0.4f, 0.2f);
  bar.setElevationBase (0.3f, 0.25f);
  bar.setElevationClipTop (0.7f);
  bar.setElevationClipBottom (0.1f);
  bar.setElevationChannels ({});
  bar.setSphereCamera ({});
  bar.setMotionSpeed (0.5f, "1");
  bar.setMotionDirection (0);
  bar.setMotionEndAction (0, {});
  bar.setMotionFadeReach (0.25f);
  bar.setMotionBridgeBias (1);
  bar.setMotionSqueeze (0.2f, -0.2f, 0.1f, -2.f);
  bar.setKnobsLaneDriven ({});
  bar.setKnobsWriting ({});
  bar.setMotionStretch (1, 0);
  bar.setMotionLean (0.1f, 0.f, std::nullopt, std::nullopt, 0, 0);
  bar.setSweeps (1, 0, 0);
  bar.setMotionEnvelope (2, 3);
  bar.setMotionEnvelopeMax (0.8f);
  bar.setMotionActMode (0);
  bar.setShapeSpeed (0);
  bar.setShapeRotate (0.25f, 0.f);
  bar.setBeatsPerBar (4);
  bar.setPatternLengthBeats (16.f);
  bar.setTransportState (true, false, false);
  bar.setRecArmed (false);
  bar.setRecording (false);
  bar.setActionActive (false);
  bar.setTrajectorySubIndex (0);
  bar.setMotionSubIndex (2);
  for (int ch = 0; ch < static_cast<int> (numChannelColumns); ++ch)
    bar.setChannelPots (ch, { { 0.5f, 0.5f, 0.f }, { 0.5f, 0.5f, 0.f } });
  bar.setSelectedParameterIndex (ClipSettingsComponent::motionIndex);
}

struct Bar
{
  Bar ()
  {
    bar.setBounds (0, 0, 768, 329);
    bar.setVisible (true);
    describeOneClip (bar);
    bar.setCachedComponentImage (new RepaintRecorder (requests));
    // Putting the recorder in is itself a repaint of everything.
    requests = 0;
  }

  ClipSettingsComponent bar;
  int requests = 0;
};
}

// The same clip described again asks for nothing to be drawn (#64). The bar
// is told everything on every timer tick while a clip plays, and every
// setter that repainted regardless redrew all 768x329 pixels of it for
// values that had not moved -- several milliseconds a tick on the device's
// message thread, which is where the finger on a knob waits.
TEST (BarRepaints, TheSameClipDescribedAgainRepaintsNothing)
{
  Bar b;

  describeOneClip (b.bar);

  EXPECT_EQ (b.requests, 0);
}

// The other half, or the test above is passed by a bar that never repaints.
TEST (BarRepaints, AValueThatMovesStillRepaints)
{
  Bar b;

  b.bar.setElevationClipTop (0.6f);
  EXPECT_GT (b.requests, 0);

  b.requests = 0;
  b.bar.setTrajectoryName ("Figure Eight");
  EXPECT_GT (b.requests, 0);

  b.requests = 0;
  b.bar.setMotionSpeed (0.5f, "2");
  EXPECT_GT (b.requests, 0);
}
