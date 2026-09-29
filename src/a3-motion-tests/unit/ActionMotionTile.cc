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

#include <a3-motion-ui/components/ActionComponent.hh>
#include <a3-motion-ui/components/ActionMotionKnobs.hh>
#include <a3-motion-ui/components/ClipSettingsCaptions.hh>
#include <a3-motion-ui/components/PotKnob.hh>
#include <a3-motion-ui/components/TouchControl.hh>

#include <optional>

using namespace a3;

// ── What a tile knob is ──────────────────────────────────────────────────

// Each knob is the MOTION page's own: its scale, its caption, which way it
// fills. One table, two pages.
TEST (ActionMotionKnobs, EachKnobIsTheMotionPagesOwn)
{
  EXPECT_STREQ (motionTileKnobSpec (MotionParam::Spin).label, caption::spin);
  EXPECT_STREQ (motionTileKnobSpec (MotionParam::Rotate).label, caption::rotate);
  EXPECT_TRUE (motionTileKnobSpec (MotionParam::Rotate).wraps);
  EXPECT_STREQ (motionTileKnobSpec (MotionParam::Elevation).label,
                caption::elevation);
  EXPECT_STREQ (motionTileKnobSpec (MotionParam::ClipBottom).label,
                caption::clipBottom);
  EXPECT_STREQ (motionTileKnobSpec (MotionParam::RollSweep).label,
                caption::rollSweep);
  EXPECT_TRUE (motionTileKnobSpec (MotionParam::Reach).bipolar);
}

TEST (ActionMotionKnobs, EveryValueHasACaption)
{
  for (auto const param : motionParamOrder)
    EXPECT_STRNE (motionParamCaption (param), "") << static_cast<int> (param);
  EXPECT_STREQ (motionParamCaption (MotionParam::Speed), caption::speed);
  EXPECT_STREQ (motionParamCaption (MotionParam::EndAction), caption::endAction);
}

// Speed, direction and end are fields, as on CLIP, not knobs.
TEST (ActionMotionKnobs, ClipsThreeAreFields)
{
  for (auto const param : motionParamOrder)
    EXPECT_EQ (motionParamIsAField (param),
               param == MotionParam::Speed || param == MotionParam::Direction
                   || param == MotionParam::EndAction);
}

// elv is the base the other way up, and held inside the clip band -- as the
// MOTION page turns it.
TEST (ActionMotionKnobs, ElvIsTheBaseTheOtherWayUp)
{
  EXPECT_FLOAT_EQ (motionKnobValue (MotionParam::Elevation, 0.2f), 0.8f);
  EXPECT_NEAR (motionValueForKnob (MotionParam::Elevation, 0.8, 0.f, 0.f),
               0.2f, 1e-5f);
  EXPECT_FLOAT_EQ (motionKnobValue (MotionParam::Spin, 3.f), 3.f);
}

TEST (ActionMotionKnobs, ASweepTurnsInWholeSteps)
{
  EXPECT_FLOAT_EQ (motionValueForKnob (MotionParam::Spin, 2.6, 0.f, 0.f), 3.f);
  EXPECT_FLOAT_EQ (motionValueForKnob (MotionParam::Reach, -0.35, 0.f, 0.f),
                   -0.35f);
}

TEST (ActionMotionKnobs, FieldsStepLikeClipsKeys)
{
  EXPECT_FLOAT_EQ (steppedMotionValue (MotionParam::Speed, 0.f, 1), 1.f);
  EXPECT_FLOAT_EQ (steppedMotionValue (MotionParam::Speed,
                                       static_cast<float> (speedLog2Max), 1),
                   static_cast<float> (speedLog2Max));
  // Direction and end come round, as a tap on CLIP brings them round.
  EXPECT_FLOAT_EQ (steppedMotionValue (MotionParam::Direction, 3.f, 1), 0.f);
  // Back from the first end action is the last one -- Clip since the end
  // action Clip came in, the same cycle as the CLIP page's.
  EXPECT_FLOAT_EQ (steppedMotionValue (MotionParam::EndAction, 0.f, -1),
                   static_cast<float> (value::numEndActions - 1));
}

// ── The page ─────────────────────────────────────────────────────────────

namespace
{
std::array<MotionShown, numMotionParams>
shownWith (MotionParam param, MotionSource source, float value)
{
  std::array<MotionShown, numMotionParams> shown{};
  shown[static_cast<size_t> (param)] = { value, source };
  return shown;
}

struct Page
{
  ActionComponent page;
  Page ()
  {
    // Visible, as it is on its page: a hidden component answers no
    // getComponentAt(), which is how the tests find what is under a finger.
    page.setVisible (true);
    page.setBounds (0, 0, 560, 268);
  }
};
}

TEST (ActionMotionTile, OneTileAtATime)
{
  Page p;
  EXPECT_EQ (p.page.tile (), ActionTile::Audio);
  p.page.setMotionTile ({}, true, 4.f);

  EXPECT_TRUE (p.page.audioKnob (ActionComponent::Attack)->isVisible ());
  for (auto const param : motionParamOrder)
    EXPECT_FALSE (p.page.motionControl (param)->isVisible ());

  p.page.setTile (ActionTile::Motion);
  EXPECT_FALSE (p.page.audioKnob (ActionComponent::Attack)->isVisible ());
  for (auto const param : motionParamOrder)
    EXPECT_TRUE (p.page.motionControl (param)->isVisible ());

  p.page.setTile (ActionTile::Audio);
  EXPECT_TRUE (p.page.audioKnob (ActionComponent::QMax)->isVisible ());
  EXPECT_FALSE (p.page.motionControl (MotionParam::Roll)->isVisible ());
}

// A button with no action has nothing to put on a clip: the tile says so
// instead of offering nineteen knobs that would do nothing.
TEST (ActionMotionTile, AnEmptyButtonHasNothingToEdit)
{
  Page p;
  p.page.setTile (ActionTile::Motion);
  p.page.setMotionTile ({}, false, 4.f);
  for (auto const param : motionParamOrder)
    EXPECT_FALSE (p.page.motionControl (param)->isVisible ());
}

// Three looks: grey where the button leaves the value alone (the clip's own
// shown as a hint), the channel's colour where the script sets it, and on a
// wash where it was turned on the button.
TEST (ActionMotionTile, AKnobSaysWhereItsValueComesFrom)
{
  Page p;
  p.page.setTile (ActionTile::Motion);

  auto const knobFor = [&p] (MotionParam param) {
    return dynamic_cast<PotKnob *> (p.page.motionControl (param));
  };

  p.page.setMotionTile (shownWith (MotionParam::Spin, MotionSource::Clip, 2.f),
                        true, 4.f);
  ASSERT_NE (knobFor (MotionParam::Spin), nullptr);
  EXPECT_FALSE (knobFor (MotionParam::Spin)->isSelected ());
  EXPECT_DOUBLE_EQ (knobFor (MotionParam::Spin)->getValue (), 2.0);

  p.page.setMotionTile (
      shownWith (MotionParam::Spin, MotionSource::Script, 5.f), true, 4.f);
  EXPECT_TRUE (knobFor (MotionParam::Spin)->isSelected ());
  EXPECT_FALSE (knobFor (MotionParam::Spin)->isActive ());
  EXPECT_DOUBLE_EQ (knobFor (MotionParam::Spin)->getValue (), 5.0);

  p.page.setMotionTile (
      shownWith (MotionParam::Spin, MotionSource::Button, -1.f), true, 4.f);
  EXPECT_TRUE (knobFor (MotionParam::Spin)->isSelected ());
  EXPECT_TRUE (knobFor (MotionParam::Spin)->isActive ());
}

TEST (ActionMotionTile, TurningAKnobMakesTheValueTheButtons)
{
  Page p;
  p.page.setTile (ActionTile::Motion);
  p.page.setMotionTile ({}, true, 4.f);

  std::optional<std::pair<MotionParam, float> > set;
  p.page.onMotionSet = [&set] (MotionParam param, float value) {
    set = { param, value };
  };

  auto *knob = dynamic_cast<PotKnob *> (p.page.motionControl (MotionParam::Reach));
  ASSERT_NE (knob, nullptr);
  knob->setValue (-0.5, juce::sendNotificationSync);

  ASSERT_TRUE (set.has_value ());
  EXPECT_EQ (set->first, MotionParam::Reach);
  EXPECT_FLOAT_EQ (set->second, -0.5f);
}

TEST (ActionMotionTile, ADoubleTapHandsTheValueBackToTheScript)
{
  Page p;
  p.page.setTile (ActionTile::Motion);
  p.page.setMotionTile (
      shownWith (MotionParam::Tilt, MotionSource::Button, 0.5f), true, 4.f);

  std::optional<MotionParam> unset;
  p.page.onMotionUnset = [&unset] (MotionParam param) { unset = param; };

  auto *knob = dynamic_cast<PotKnob *> (p.page.motionControl (MotionParam::Tilt));
  ASSERT_NE (knob, nullptr);
  ASSERT_TRUE (knob->onDoubleTapped);
  knob->onDoubleTapped ();

  ASSERT_TRUE (unset.has_value ());
  EXPECT_EQ (*unset, MotionParam::Tilt);
}

// ── The six fields choose; the panel fires (2026-09-28) ─────────────────

namespace
{
TouchControl *
touchAt (ActionComponent &page, juce::Rectangle<int> area)
{
  return dynamic_cast<TouchControl *> (page.getComponentAt (area.getCentre ()));
}
}

// A field only chooses its button -- what the rest of the page shows. It
// fires nothing: that is the pads' job, on the panel and the PADS page.
TEST (ActionMotionTile, AFieldOnlyChoosesItsButton)
{
  Page p;
  std::optional<int> chosen;
  p.page.onButtonChosen = [&chosen] (int button) { chosen = button; };

  auto *field = touchAt (p.page, p.page.layout ().actionFields[2]);
  ASSERT_NE (field, nullptr);
  ASSERT_TRUE (field->onPress);
  field->onPress (0, 0);
  ASSERT_TRUE (chosen.has_value ());
  EXPECT_EQ (*chosen, 2);
  EXPECT_FALSE (field->onRelease) << "nothing to let go of: nothing fired";
}

// The after key: a tap steps it on, two taps put it back to nothing.
TEST (ActionMotionTile, TheAfterKeyStepsAndClears)
{
  Page p;
  int stepped = 0;
  bool cleared = false;
  p.page.onAfterStepped = [&stepped] (int increment) { stepped += increment; };
  p.page.onAfterCleared = [&cleared] { cleared = true; };

  auto *key = touchAt (p.page, p.page.layout ().afterKey);
  ASSERT_NE (key, nullptr);
  ASSERT_TRUE (key->onTap);
  key->onTap (0, 0);
  EXPECT_EQ (stepped, 1);
  ASSERT_TRUE (key->onDoubleTap);
  key->onDoubleTap (0, 0);
  EXPECT_TRUE (cleared);
}

// The page crashed on its first paint (2026-09-28): the AUDIO tile's row
// names called themselves. The tile tests never painted, so they stayed
// green -- this one paints both tiles, the way the screen does.
TEST (ActionMotionTile, ThePagePaintsOnBothTiles)
{
  Page p;
  p.page.setMotionTile ({}, true, 4.f);
  for (auto const tile : { ActionTile::Audio, ActionTile::Motion })
    {
      p.page.setTile (tile);
      auto const image = p.page.createComponentSnapshot (
          p.page.getLocalBounds ());
      EXPECT_EQ (image.getWidth (), p.page.getWidth ());
    }
}
