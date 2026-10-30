// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <a3-motion-ui/components/StatusBar.hh>

using namespace a3;

// The PRO-DJ LIVE badge on the bar: there only while a remote pointer may
// touch the screen, and a tap on it switches LIVE off -- never on.

namespace
{
struct Bar
{
  juce::Value bpm{ 120.0 };
  StatusBar bar{ bpm };
  int taps = 0;

  Bar ()
  {
    bar.setBounds (0, 0, 768, bar.preferredHeight ());
    bar.onLiveBadgeTapped = [this] { ++taps; };
  }

  void
  tapAt (juce::Point<int> at)
  {
    auto const source = juce::Desktop::getInstance ().getMainMouseSource ();
    juce::MouseEvent const event (
        source, at.toFloat (), {}, juce::MouseInputSource::defaultPressure,
        0.f, 0.f, 0.f, 0.f, &bar, &bar, juce::Time::getCurrentTime (),
        at.toFloat (), juce::Time::getCurrentTime (), 1, false);
    bar.mouseUp (event);
  }
};
}

TEST (StatusBarLiveBadge, ThereIsNoBadgeUntilLive)
{
  Bar b;

  EXPECT_TRUE (b.bar.liveBadgeArea ().isEmpty ());
  b.bar.setProdjLive (true);
  EXPECT_FALSE (b.bar.liveBadgeArea ().isEmpty ());
  b.bar.setProdjLive (false);
  EXPECT_TRUE (b.bar.liveBadgeArea ().isEmpty ());
}

TEST (StatusBarLiveBadge, ATapOnTheBadgeIsReported)
{
  Bar b;
  b.bar.setProdjLive (true);

  b.tapAt (b.bar.liveBadgeArea ().getCentre ());

  EXPECT_EQ (b.taps, 1);
}

TEST (StatusBarLiveBadge, WithoutTheBadgeNothingAnswersWhereItWas)
{
  Bar b;
  b.bar.setProdjLive (true);
  auto const where = b.bar.liveBadgeArea ().getCentre ();
  b.bar.setProdjLive (false);

  b.tapAt (where);

  EXPECT_EQ (b.taps, 0);
}

TEST (StatusBarLiveBadge, TheBarPaintsWithAndWithoutTheBadge)
{
  Bar b;
  juce::Image image (juce::Image::ARGB, b.bar.getWidth (),
                     b.bar.getHeight (), true);
  juce::Graphics g (image);

  b.bar.paint (g);
  b.bar.setProdjLive (true);
  b.bar.paint (g);

  SUCCEED ();
}
