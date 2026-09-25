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

#include <a3-motion-ui/theme/TransportLook.hh>

using namespace a3;

// The four keys light by one rule, in the global strip and on the pads page
// alike. Written down here rather than inside a paint method, because "is this
// key lit" is a question two screens ask and a person asks in the dark.

TEST (TransportKeyGround, RecordLightsOnlyWhileItRecords)
{
  TransportState state;
  state.recording = true;
  EXPECT_EQ (transportKeyGround (TransportKey::Record, state),
             TransportGround::Lit);

  state.recording = false;
  EXPECT_EQ (transportKeyGround (TransportKey::Record, state),
             TransportGround::Dark);
}

// The ground is where play's state lives, because the glyph cannot hold it:
// there are three states -- running, held, stopped -- and a shape with two
// forms can only tell two of them apart. See drawTransportGlyph().
TEST (TransportKeyGround, PlayLightsWhileTheClipRuns)
{
  TransportState state;
  state.playing = true;
  EXPECT_EQ (transportKeyGround (TransportKey::PlayPause, state),
             TransportGround::Lit);

  state.playing = false;
  EXPECT_EQ (transportKeyGround (TransportKey::PlayPause, state),
             TransportGround::Dark);
}

// Both ends of the transport wait for the next beat -- starting and stopping
// alike -- and half a second of nothing after a press reads as a key that did
// not work. So it blinks while it waits, the way a deck's play key does.
TEST (TransportKeyGround, PlayBlinksWhileItWaitsForTheBeat)
{
  TransportState state;
  state.scheduled = true;
  EXPECT_EQ (transportKeyGround (TransportKey::PlayPause, state),
             TransportGround::Waiting);
}

// A scheduled stop leaves the clip running until the beat lands. What the key
// has to say in that moment is "your press was taken", not "still playing" --
// the blink is the newer fact, so it wins.
TEST (TransportKeyGround, WaitingBeatsPlayingWhileAStopIsPending)
{
  TransportState state;
  state.playing = true;
  state.scheduled = true;
  EXPECT_EQ (transportKeyGround (TransportKey::PlayPause, state),
             TransportGround::Waiting);
}

// Not "while the finger is down": the engine puts the clip's settings back
// when the envelope has finished falling, not when the hand lifts, and for a
// hold that moment is the middle of an audible decay. What the key shows is
// what is still moving.
TEST (TransportKeyGround, ActionLightsWhileTheActionIsStillRunning)
{
  TransportState state;
  state.actionActive = true;
  EXPECT_EQ (transportKeyGround (TransportKey::Action, state),
             TransportGround::Lit);

  state.actionActive = false;
  EXPECT_EQ (transportKeyGround (TransportKey::Action, state),
             TransportGround::Dark);
}

// Stop has no state to be in -- it is a way out, and a way out that stayed lit
// afterwards would be claiming to be somewhere. What it does have is the
// press: it acts instantly and unquantised, so without a flash the one key
// that never waits is also the one key that never answers.
TEST (TransportKeyGround, StopLightsOnlyWhileItIsPressed)
{
  for (bool recording : { false, true })
    for (bool playing : { false, true })
      for (bool scheduled : { false, true })
        {
          TransportState state;
          state.recording = recording;
          state.playing = playing;
          state.scheduled = scheduled;

          EXPECT_EQ (transportKeyGround (TransportKey::Stop, state),
                     TransportGround::Dark);

          state.stopPressed = true;
          EXPECT_EQ (transportKeyGround (TransportKey::Stop, state),
                     TransportGround::Lit);
        }
}

// Each key reads only its own fields. Recording must not light ACT, an accent
// must not light REC -- the four keys sit in one row and a light on the wrong
// one is worse than no light at all.
TEST (TransportKeyGround, EachKeyReadsOnlyItsOwnFields)
{
  TransportState state;
  state.playing = true;
  state.actionActive = true;
  state.stopPressed = true;
  EXPECT_EQ (transportKeyGround (TransportKey::Record, state),
             TransportGround::Dark);

  TransportState other;
  other.recording = true;
  other.scheduled = true;
  other.stopPressed = true;
  EXPECT_EQ (transportKeyGround (TransportKey::Action, other),
             TransportGround::Dark);
}

// An unsaved take turns REC into SAVE and ACT into DISCARD -- the two keys
// stay where they are, so the finger finds the same places.
TEST (TransportKeyGround, AnUnsavedTakeTurnsRecIntoSaveAndActIntoDiscard)
{
  TransportState state;
  state.unsaved = true;

  EXPECT_EQ (transportFace (TransportKey::Record, state), TransportFace::Save);
  EXPECT_EQ (transportFace (TransportKey::Action, state),
             TransportFace::Discard);
  EXPECT_EQ (transportFace (TransportKey::Stop, state), TransportFace::Stop);
  EXPECT_EQ (transportFace (TransportKey::PlayPause, state),
             TransportFace::PlayPause);
}

// While a new take runs on that slot, REC is what ends it -- SAVE would be a
// key that means something else in the middle of the take.
TEST (TransportKeyGround, WhileRecordingTheKeysAreTheirOwn)
{
  TransportState state;
  state.unsaved = true;
  state.recording = true;

  EXPECT_EQ (transportFace (TransportKey::Record, state),
             TransportFace::Record);
  EXPECT_EQ (transportFace (TransportKey::Action, state),
             TransportFace::Action);
}

TEST (TransportKeyGround, WithoutATakeTheKeysAreTheirOwn)
{
  TransportState state;

  EXPECT_EQ (transportFace (TransportKey::Record, state),
             TransportFace::Record);
  EXPECT_EQ (transportFace (TransportKey::Action, state),
             TransportFace::Action);
}

// SAVE is lit while there is something to save: it is the key the eye has to
// find. DISCARD is dark, and lights only once it has been pressed once.
TEST (TransportKeyGround, SaveIsLitAndDiscardLightsWhenArmed)
{
  TransportState state;
  state.unsaved = true;

  EXPECT_EQ (transportKeyGround (TransportKey::Record, state),
             TransportGround::Lit);
  EXPECT_EQ (transportKeyGround (TransportKey::Action, state),
             TransportGround::Dark);

  state.discardArmed = true;
  EXPECT_EQ (transportKeyGround (TransportKey::Action, state),
             TransportGround::Lit);
}

// Green keeps, red takes away -- the same rule the four keys already follow.
TEST (TransportKeyGround, SaveIsGreenAndDiscardIsRed)
{
  EXPECT_EQ (transportColour (TransportFace::Save),
             transportColour (TransportKey::PlayPause));
  EXPECT_EQ (transportColour (TransportFace::Discard),
             transportColour (TransportKey::Stop));
}
