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

#pragma once

#include <array>

#include <juce_gui_basics/juce_gui_basics.h>

#include <a3-motion-engine/Config.hh>

#include <a3-motion-ui/components/ClipSettingsLayout.hh>

namespace a3
{

/** How many meters the output section carries.
 *
 *  One subwoofer and four speakers — the maintainer's "1.4 ch output VU", and
 *  exactly what arrives: `/vu/4` is the sub and `/vu/5..8` are the speakers.
 *  A separate count from the channels' because it answers a different
 *  question: how many ways the room is driven, not how many decks are on the
 *  device. */
constexpr int numOutputMeters = 5;

/** Where each kind of output meter sits in that block.
 *
 *  The subwoofer first, then the four speakers — the order they arrive in
 *  (`/vu/4`, then `/vu/5..8`) and the order "1.4" is said in. Named rather
 *  than written as 0 and 1 at the two call sites, because the two callbacks
 *  that fill this block are in a different file from the one that draws it. */
constexpr int subwooferMeterIndex = 0;
constexpr int firstSpeakerMeterIndex = 1;

/** How often a page carrying meters redraws them.
 *
 *  Twenty-five a second, chosen for the eye rather than for the wire: past
 *  about that, a bar's movement stops reading as movement and starts reading
 *  as the same picture drawn again, and a booth is not the place to spend a
 *  machine on that.
 *
 *  **It deliberately does not track the rate the meters arrive at, and must
 *  not be rewritten to.** VuLevels keeps only the latest sample, so a page
 *  redrawing slower than the sender shows the newest value and one redrawing
 *  faster shows the same value twice — neither is a fault, and neither needs
 *  the two numbers to agree. That independence is the point: how fast the
 *  meters arrive is somebody else's setting on another machine, and a
 *  constant here derived from it would be one that goes quietly wrong when
 *  that setting changes.
 *
 *  What the *arriving* rate does explain is why this is a timer at all rather
 *  than a repaint per message. Twelve meters at the rate they currently come
 *  is a few hundred messages a second; a repaint each would be a few hundred
 *  repaints. See `vuNowMs` for the other half of that separation. */
constexpr int vuMeterRefreshHz = 25;

/** One meter's two numbers, as they arrive on the wire.
 *
 *  Linear amplitude in 0..1, peak and rms of the same frame — one `/vu/N`
 *  message carries both floats, and the OSC reference gives both as [0-1].
 *  Kept as they arrive rather than
 *  converted on receipt: the sphere's corona reads the same values through a
 *  curve of its own, and two units in flight is how the two pictures would
 *  come to disagree. */
struct VuLevel
{
  float peak = 0.f;
  float rms = 0.f;
};

/** The bottom of the meter, in decibels below full scale.
 *
 *  Sixty is what a console's meter shows, and the span matters more than it
 *  looks: these are linear amplitudes, so a passage sitting at a comfortable
 *  -20 dBFS arrives as 0.1 and would stand one tenth up a linear bar. Every
 *  level anybody actually plays would live in the bottom fifth of the meter,
 *  which is the same as having no meter. */
constexpr float vuMeterFloorDb = -60.f;

/** How long the peak's mark stands still before it follows the signal again.
 *
 *  A second and a half, and the number is about a person rather than about a
 *  frame rate. A transient occupies one frame however fast the frames come,
 *  which is to say nobody sees it; the hold has to be long enough that the
 *  mark can be found, read and believed with both hands busy, and short
 *  enough that it still answers to what is playing now rather than to what
 *  played a chorus ago.
 *
 *  In milliseconds rather than in frames for the same reason
 *  `vuMeterRefreshHz` is not derived from the sending rate: a hold counted in
 *  frames would mean a different length of time on every setup. */
constexpr juce::int64 vuPeakHoldMs = 1500;

/** The clock the peak's hold is measured on.
 *
 *  Monotonic milliseconds since the app started, not the wall clock: a system
 *  clock stepped by NTP mid-set would either freeze a mark or expire one
 *  early. One function rather than a call at each site, so the callback that
 *  writes a level and the paint that reads it cannot end up on two clocks. */
juce::int64 vuNowMs ();

/** Where a level lands on a meter: 0 at the foot, 1 at the head.
 *
 *  Logarithmic, with `vuMeterFloorDb` at the foot and full scale at the head.
 *  Anything at or below silence, and anything that is not a number at all,
 *  comes back as the foot — `log10 (0)` is minus infinity and a fraction
 *  computed before that is said is not something a clamp can rescue. */
float vuMeterFraction (float amplitude);

/** The same place on the meter, asked for in decibels below full scale.
 *
 *  `vuMeterFraction` is handed the linear amplitude that arrives on the wire;
 *  this is the half of that mapping the *bands* are stated in, since where a
 *  band begins is a decibel figure a mixing engineer already knows and not an
 *  amplitude anybody would recognise. One function rather than the arithmetic
 *  written twice, so a change to the floor moves the fill and the bands
 *  together. */
constexpr float
vuFractionForDb (float db)
{
  return (db - vuMeterFloorDb) / (0.f - vuMeterFloorDb);
}

/** Where the meter stops reading as programme and starts reading as headroom.
 *
 *  -18 dBFS is the alignment level digital gear is lined up on -- it is what
 *  the analogue 0 VU a console's green ends at maps to, and it is where a mix
 *  sitting correctly on this system's inputs lives. Green up to here therefore
 *  means "this is the level you meant", rather than "this is quiet". */
constexpr float vuGreenCeilingDb = -18.f;

/** Where what is left of the headroom runs out.
 *
 *  -6 dBFS, a few decibels short of full scale: past it a transient that the
 *  rms has not caught up with yet can reach the ceiling inside one frame, so
 *  the red is a warning about the next moment rather than a report on this
 *  one. Below it there is a doubling of amplitude still in hand, which is the
 *  margin a hand needs to be able to correct in. */
constexpr float vuYellowCeilingDb = -6.f;

/** The meter's three colour bands, foot to head.
 *
 *  Indices rather than an enum because they index an array of rectangles that
 *  the picture and the test both walk in order, and an enum would be cast at
 *  every one of those sites. */
constexpr int numVuMeterBands = 3;
constexpr std::size_t vuGreenBand = 0;
constexpr std::size_t vuYellowBand = 1;
constexpr std::size_t vuRedBand = 2;

/** Where the parts of one meter are drawn.
 *
 *  Public so the picture and the test can read the same rectangles — the
 *  lesson ClipSettingsLayout stands on. `rms` and `peak` come back empty for
 *  a meter that has nothing to show, which is what a meter nobody has sent
 *  anything to must look like. */
struct VuMeterGeometry
{
  /** The whole bar, drawn even when it is empty: a meter has to be findable
   *  before it has anything to say. */
  juce::Rectangle<int> track;
  /** The fill, standing on the foot of the track. */
  juce::Rectangle<int> rms;
  /** The thin mark, laid across the track where the peak reached. */
  juce::Rectangle<int> peak;
  /** The fill again, cut into its three colour bands foot to head.
   *
   *  **A band is a stretch of the track, not a state of the signal.** They are
   *  the fill intersected with three fixed zones, so a bar filled into the red
   *  is green at its foot, yellow through its middle and red only at its head
   *  -- which is what lets it say *how far* over you are. A meter that changed
   *  colour as a whole would be a warning light, and a warning light answers
   *  only yes or no.
   *
   *  Together they are exactly `rms`: a band that came out empty is one the
   *  fill has not reached. Cut here rather than in paint() so the boundaries
   *  can be checked without a screen. */
  std::array<juce::Rectangle<int>, numVuMeterBands> bands;
};

VuMeterGeometry vuMeterGeometry (juce::Rectangle<int> bounds, VuLevel level);

/** A strip, split into its meter column and the controls standing beside it.
 *
 *  Measured off REAPER's own mixer on this machine — the meter takes 28 of a
 *  92 px strip, just under a third, and runs the *full height* of it to the
 *  left of everything else. The height is the point: the scale runs down the
 *  meter's length, so length is the resolution it is read with, and a meter
 *  occupying one row of seven could say "loud" and nothing more. */
struct StripColumns
{
  juce::Rectangle<int> meter;
  juce::Rectangle<int> controls;
};

StripColumns splitStripForMeter (juce::Rectangle<int> strip);

/** The five output meters across one block, and the word under them.
 *
 *  Thin bars side by side in one block, which is how REAPER draws a
 *  multi-channel meter — five separate widgets would read as five outputs to
 *  compare one at a time, where what this says is one thing: how the room is
 *  being driven. */
struct OutputMeterBlock
{
  std::array<juce::Rectangle<int>, numOutputMeters> bars;
  juce::Rectangle<int> caption;
};

OutputMeterBlock outputMeterBlock (juce::Rectangle<int> block,
                                   ControlMetrics metrics);

/** Every level the mixer's meters read, and the peak lingering over each.
 *
 *  **Why this exists at all**, given that `/vu/0..3` already lands in
 *  `ChannelUIState::vuPeak`/`vuLevel`: those are two bare atomics read by the
 *  GL thread for the corona, with no memory of when a value arrived. A peak
 *  mark that stands still for a moment needs that memory, and it has to be
 *  one memory — the overlay and the bar's MIX tab draw the same channel's
 *  meter, and two holds would be two marks disagreeing about the same sound.
 *  So the channel levels are written here *beside* the existing store, never
 *  instead of it, and the same goes for the outputs: `/vu/4` and `/vu/5..8`
 *  still reach the sphere's glow and the speaker lights, which have no other
 *  source.
 *
 *  Time is a parameter rather than something this reads, so the hold can be
 *  tested without waiting for it.
 *
 *  Not thread-safe, and does not need to be: every `/vu` message arrives on
 *  the message thread (`OSCReceiver::MessageLoopCallback`) and every reader
 *  paints on it. */
class VuLevels
{
public:
  void setChannel (int channel, VuLevel level, juce::int64 nowMs);
  void setOutput (int meter, VuLevel level, juce::int64 nowMs);

  /** The latest rms, and the peak as it should be *drawn* — held while its
   *  hold lasts, and the current frame's once it is up. */
  VuLevel channel (int channel, juce::int64 nowMs) const;
  VuLevel output (int meter, juce::int64 nowMs) const;

private:
  struct Meter
  {
    VuLevel latest;
    float heldPeak = 0.f;
    juce::int64 heldAtMs = 0;
  };

  static void set (Meter &meter, VuLevel level, juce::int64 nowMs);
  static VuLevel read (Meter const &meter, juce::int64 nowMs);

  std::array<Meter, static_cast<std::size_t> (numChannelsInitial)> _channel;
  std::array<Meter, static_cast<std::size_t> (numOutputMeters)> _output;
};

/** One meter, drawn into `bounds`.
 *
 *  A free function beside `paintBarKnob`, for the reason it is: the overlay
 *  draws nine of these and the bar's MIX tab draws one, and the same meter
 *  has to be the same picture in both.
 *
 *  **It takes no colour.** The fill used to be the channel's own, which is how
 *  this device says whose something is everywhere else — but green, yellow and
 *  red down a level meter is a language older than this device and it does not
 *  survive being said in four hues at once. Whose meter it is, the strip it
 *  stands in already says: the wash behind it and every knob beside it are in
 *  the channel's colour. */
void paintVuMeter (juce::Graphics &g, juce::Rectangle<int> bounds,
                   VuLevel level);

}
