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

#include <JuceHeader.h>

#include <a3-motion-engine/Config.hh>

#include <a3-motion-ui/io/PadFunctions.hh>

#include <array>
#include <vector>

namespace a3
{

/** The two text sizes and the knob diameter every control in the bar
 *  shares. Decided from the whole bar's geometry, not from an individual
 *  control's cell — that is what keeps neighbouring controls the same size
 *  as each other. Lived as a private member struct of
 *  ClipSettingsComponent and moved here so the layout can be computed, and
 *  checked, without a component. */
struct ControlMetrics
{
  int knobDiam;
  float captionSize;
  float valueSize;
};

/** Three sections for the clip, then the global one. Filter used to be a
 *  fourth: its Freq and Q were never the clip's, they wrote the same
 *  per-channel values the pots do, so they moved to the global section
 *  where they belong. */
constexpr int numClipSettingsSections = 4;

/** The global section's per-channel grid is one column per channel. */
constexpr int numChannelColumns = numChannelsInitial;

/** ... and three rows. The order they are read in, top to bottom. */
constexpr int numChannelRows = 3;

/** The lengths a take can be given, as powers of two of a bar, and how they
 *  are worded. Eight buttons rather than a list: the whole range from 1/128
 *  to 16 bars was a dropdown nobody wanted to scroll, and these are the ones
 *  anybody reaches for. This table is the authority on what a take's length
 *  may be — it reaches 32 bars, one step past the speed control's range. */
constexpr int numRecordLengths = 8;

/** How far a clip's speed may be pushed either way, as a power of two of a
 *  bar. Here rather than in A3MotionUIComponent, where they were private, so
 *  that the naming of a speed and the dragging of one can be computed -- and
 *  checked -- without a component. */
constexpr int speedLog2Min = -7; // 2^-7 bar = a 128th note
constexpr int speedLog2Max = 4;  // 2^4 bar = 16 bars

/** How many speeds the Shape section keeps under a finger.
 *
 *  Four, not the whole of speedLog2Min..Max. Twelve buttons said every value
 *  the range holds and took three rows of the section to do it; these are the
 *  four a hand wants at once, and the two rows they give back are what the
 *  clip field stands in.
 *
 *  Which four is the performer's to say: a key is tapped for the speed it
 *  carries and dragged to give it another, so every value in the range is a
 *  key away and stays on that key. The four a fresh device starts with live
 *  in AppSettings, because a favourite speed is a working habit rather than
 *  part of an arrangement. */
constexpr int numSpeedButtons = 4;

/** A speed worded the way a musician reads it: `1` for as recorded, `1/8` for
 *  eight times as fast, `16` for sixteen bars a cycle.
 *
 *  The one place a speed is put into words. The four keys used to carry their
 *  names beside their values as fixed strings, which only works while the
 *  values are fixed too. */
juce::String speedLog2Name (int speedLog2);

/** Where a drag leaves the speed key it started on. Clamped rather than
 *  wrapped: the ends of the range are ends, and a key that jumped from the
 *  fastest to the slowest under a finger would be a key nobody could aim. */
int draggedSpeedLog2 (int speedLog2, int increment);

/** No finger is on a speed key — the value speedKeyIsActive() takes when
 *  nothing is being dragged. */
constexpr int noSpeedKeyDragged = -1;

/** Whether the speed key at `index` reads as active: which one of the four
 *  wears the colour. `draggedIndex` is the key currently under a finger, or
 *  noSpeedKeyDragged. */
bool speedKeyIsActive (std::array<int, numSpeedButtons> const &keys, int index,
                       int clipSpeedLog2, int draggedIndex);
constexpr int recordLengthLog2[numRecordLengths] = { -2, -1, 0, 1, 2, 3, 4, 5 };
constexpr char const *recordLengthNames[numRecordLengths]
    = { "1/4", "1/2", "1", "2", "4", "8", "16", "32" };
/** The four things you do to a clip, as small keys in the bar's header.
 *
 *  The pads page has these already; the header carries them so the clip you
 *  are editing can be started and stopped without leaving the settings you
 *  came to change. Same order as they are read: arm it, stop it, run it, hit
 *  it. */
enum class TransportKey
{
  Record,
  Stop,
  PlayPause,
  Action,
};
constexpr int numTransportKeys = 4;
constexpr TransportKey transportKeyOrder[numTransportKeys]
    = { TransportKey::Record, TransportKey::Stop, TransportKey::PlayPause,
        TransportKey::Action };

constexpr int channelRowThreeD = 0;
constexpr int channelRowFreq = 1;
constexpr int channelRowQ = 2;

/** Which of the bar's two pages is showing.
 *
 *  Here rather than inside ClipSettingsComponent because two components and
 *  the orchestrator between them all speak it, and burying it in one of them
 *  would drag that component's whole header into the other two. */
enum class BarPage
{
  Clip,
  /** The clip page with the Shape section turned over: the take you are about
   *  to make rather than the clip as it plays. Only that one section changes
   *  — you are still looking at the elevation and the motion it will get. */
  Record,
  /** What ACT does: the envelope it fires and the mode it fires in. Its own
   *  page rather than a section, because the clip page's three columns are
   *  already as narrow as a fingertip allows -- a fourth would take the shape
   *  its picture. */
  Action,
  Controller,
  /** One channel's mixer strip: the channel of the clip this bar describes.
   *  The whole mixer is an overlay reached from the status bar -- this is the
   *  one-handed reach to the channel you are already looking at, without
   *  laying anything over the sphere. */
  Mixer,
  /** Somewhere else entirely: what is stored, rather than what is loaded. The
   *  eight clips of the device down one side and the library down the other,
   *  so a clip is put where it goes rather than dialled to. */
  Browser,
};

constexpr int numBarPages = 6;

/** Every page, once. `BarPages.EveryPageAppearsInTheOrderExactlyOnce` fails
 *  if a page is missing from here or listed twice -- nothing in the compiler
 *  checks that on its own, since this is data, not a case of an enum. Kept
 *  beside pageCoversClipArea and pageDescribesAClip because a page has to be
 *  added here too, alongside a case in each of them, and a forgotten one
 *  answers wrong quietly forever if this test does not walk it. */
constexpr std::array<BarPage, numBarPages> barPageOrder{
  BarPage::Clip,       BarPage::Record, BarPage::Action,
  BarPage::Controller, BarPage::Mixer,  BarPage::Browser,
};

/** Pages that cover the clip area with something of their own.
 *
 *  The bar's sections must not be drawn under them — a page that does not
 *  fill every pixel would otherwise show the clip settings through its own
 *  gaps, which is what the ACTION page did on its first evening.
 *
 *  Here rather than in ClipSettingsComponent because it is a property of the
 *  page, and because nothing in C++ warns about a page missing from an `if`
 *  chain. There were 23 such comparisons across three files. A `switch` with
 *  no `default:` is what makes a forgotten page loud instead: `-Wswitch-enum`
 *  names the case a new enumerator is missing from. Nothing in this project's
 *  own CMakeLists asks for it: it comes from JUCE's
 *  `juce::juce_recommended_warning_flags`, linked PUBLIC by the
 *  `juce_dependencies` target in `src/a3-motion-engine/CMakeLists.txt` and
 *  inherited from there by every target here (JUCE 9.0.1 defines it in
 *  `lib/cmake/JUCE-9.0.1/JUCEHelperTargets.cmake`; it is visible in
 *  `build/.../flags.make`). Said outright because grepping the repository for
 *  the flag finds nothing, and a reader who concludes it is not on concludes
 *  this whole guarantee is fictional. It is a warning, not a compile error --
 *  and it only
 *  knows about the enum's own cases, which is what `barPageOrder` above is
 *  for: a page absent from *that* list fails
 *  `BarPages.EveryPageAppearsInTheOrderExactlyOnce` instead. */
constexpr bool
pageCoversClipArea (BarPage page)
{
  switch (page)
    {
    case BarPage::Clip:
    case BarPage::Record:
      return false;
    case BarPage::Action:
    case BarPage::Controller:
    case BarPage::Mixer:
    case BarPage::Browser:
      return true;
    }
  // Every case above returns, so this is never reached -- it exists only to
  // stop -Wreturn-type complaining that the function might fall off the end,
  // which would otherwise drown out the one warning that matters here.
  __builtin_unreachable ();
}

/** Pages that are about one clip, so a tap on a channel face steps that
 *  channel's slot and leaves the page where it is.
 *
 *  PADS is the exception: it shows every slot at once, so reaching for a
 *  channel there is reaching for its clip, and the face brings the CLIP view
 *  back with it. FILES has a clip in mind too — the one a picked file is put
 *  into — so choosing the slot and then choosing the file is one errand, and
 *  being thrown back to CLIP halfway through it meant tabbing back and losing
 *  the list you were reading. MIX is the same errand from the other side: the
 *  strip on show is the shown clip's channel, so a face is how you get to the
 *  next channel's strip and being thrown to CLIP would undo the reach.
 *
 *  A `switch` with no `default:` for the same reason as pageCoversClipArea
 *  above -- `-Wswitch-enum` is what says a new page forgot to answer. */
constexpr bool
pageDescribesAClip (BarPage page)
{
  switch (page)
    {
    case BarPage::Clip:
    case BarPage::Record:
    case BarPage::Action:
    case BarPage::Mixer:
    case BarPage::Browser:
      return true;
    case BarPage::Controller:
      return false;
    }
  // Every case above returns, so this is never reached -- see the matching
  // comment in pageCoversClipArea.
  __builtin_unreachable ();
}

/** The area inside a section's card that its controls are laid out in —
 *  the card less its frame. Public because it is also what the shared
 *  caption and value sizes are fitted to: fonts sized against one width and
 *  drawn in another look cramped at widths where they are not. */
juce::Rectangle<int> sectionContentBounds (juce::Rectangle<int> card);

/** How many controls a section holds. Must agree with
 *  A3MotionUIComponent::numSubElementsForSection — a tap addresses a
 *  control by the same sub-index the encoder does. */
int numControlsInSection (int sectionIndex);

/** How far a grid knob's modulation arc reaches, given where the knob was set
 *  and where a modulation has carried it.
 *
 *  All three rows have one: 3d rides the accent, freq and Q ride their own
 *  envelopes, and the engine has always sent all three moving. Only 3d was
 *  drawn moving -- the other two were handed their own value as their reach,
 *  which is an arc of zero length, so two thirds of what the device was doing
 *  had to be taken on trust.
 *
 *  Never below where the knob was set: envelopeOver() only ever raises, and a
 *  ceiling dialled under the floor leaves the floor alone. An arc that ran
 *  backwards from the pointer would draw a modulation that cannot happen. */
float gridKnobReach (float set, float effective);

/** Whether a tap on this control already steps its value on, rather than
 *  only selecting it. True for the few-valued ones — direction, end-action
 *  and the global strip's rec mode — which wrap, so every tap arrives
 *  somewhere new. False for anything continuous, and for the lists
 *  (pattern, speed, record length, seam) where tapping through would turn
 *  into tapping and tapping. */
bool tapAdvancesValue (int sectionIndex, int subIndex);

/** The circle the elevation graphic draws, inside whatever cell it is given.
 *
 *  Its own function because the graphic is a control now: a finger on it sets
 *  where the middle of the trajectory sits, and the circle drawing and the
 *  circle being touched have to be the same circle or the line lands where
 *  the finger did not. */
juce::Rectangle<int> elevationCircleBounds (juce::Rectangle<int> cell);

/** The elevation a point in that cell stands for: 0 at the top of the circle
 *  (north pole), 1 at the bottom (south). Past either end it holds at the
 *  pole -- a finger sliding off the top must not wrap round to the bottom.
 *
 *  `bandLow`/`bandHigh` are what clip-top and clip-bottom have left of the
 *  sphere, and the axis stays inside them: a base outside the band would be a
 *  line you can see and the sound cannot reach. Crossed clips leave a band of
 *  nothing, and then the axis has exactly one place to be. */
float elevationBaseAt (juce::Rectangle<int> cell, int y, float bandLow = 0.f,
                       float bandHigh = 1.f);

/** Pull a base that is nearly at ear height exactly onto it.
 *
 *  The equator is where a sound is level with the listener, and it is the one
 *  place in the circle worth hitting exactly -- in a circle a few dozen
 *  pixels tall it is otherwise a single row, which is not a target. The pull
 *  lets go a little further out, or the equator would become a hole you
 *  cannot set a value beside. */
float snapElevationBase (float base);


/** Whether a tap on this control flips it. True for the two-state ones —
 *  pole and flat. They used to be stepped like the rest, but stepping is
 *  tied to a direction (an encoder turned right meant South) and a tap has
 *  none: it always said +1, so the value could be switched on and never
 *  back off. Mutually exclusive with tapAdvancesValue. */
bool tapTogglesValue (int sectionIndex, int subIndex);

/** Every rectangle in the bar, from one calculation. paint() draws into
 *  it, resized() puts the TouchControls on it. Two calculations would be
 *  two truths, and those drift apart. */
struct ClipSettingsLayout
{
  /** What paint() would otherwise have derived on its own. */
  ControlMetrics metrics;
  int headerHeight = 0;

  juce::Rectangle<int> clipBounds;
  /** What is left of the clip part under its header row — where the three
   *  sections are laid out, and what the controller page fills. The one
   *  statement of where the content begins: the page used to work the
   *  header's height out for itself and drew its top row of pads under the
   *  tabs that switch to it. */
  juce::Rectangle<int> clipContent;
  juce::Rectangle<int> globalBounds;
  /** What the global strip lays its contents out in: its card less the band
   *  the transport keys stand in. The card reaches up over them so the strip
   *  reads as one block, which is why the card and the content area are no
   *  longer the same rectangle minus a title. */
  juce::Rectangle<int> globalContent;

  /** The card per section, indexed like ClipSettingsComponent::*Index. */
  std::array<juce::Rectangle<int>, numClipSettingsSections> sectionCards;
  /** The title row per section. */
  std::array<juce::Rectangle<int>, numClipSettingsSections> sectionLabels;
  /** The lock, at the right end of that title row: a square the size of the
   *  row, so it is hit without aiming while the other hand is busy. Empty for
   *  the global strip, which is the device's and holds no clip. */
  std::array<juce::Rectangle<int>, numClipSettingsSections> sectionLocks;

  /** Per section its controls' cells, ordered **by sub-index**, not by
   *  where they sit. Elevation draws reach, mirror-south, clip-top, ...
   *  but is indexed reach, clip-top, clip-bottom, mirror-south, flat,
   *  flat-elevation. */
  std::array<std::vector<juce::Rectangle<int>>, numClipSettingsSections>
      controls;

  /** The global section's per-channel grid: [channel][row], the rows in
   *  channelRow* order. Not part of `controls` — these belong to a channel each,
   *  not to the clip the bar is showing, so they are dragged through their
   *  own callback. */
  std::array<std::array<juce::Rectangle<int>, numChannelRows>,
             numChannelColumns>
      channelGrid;
  /** Empty. The channel numbers over the grid are gone: each column already
   *  wears its channel's colour, and a colour is read without being read.
   *  Kept as a field so nothing has to special-case its absence. */
  std::array<juce::Rectangle<int>, numChannelColumns> channelLabels;

  /** The two blocks of the strip, each in a frame of its own: the knobs
   *  above, the transport below. Drawn slightly set off from the card so the
   *  strip reads as what it is -- values, then the things you do. */
  juce::Rectangle<int> channelGridFrame;
  juce::Rectangle<int> transportFrame;
  /** The row captions down the side: freq, Q, 3d. */
  std::array<juce::Rectangle<int>, numChannelRows> channelRowLabels;

  /** The Elevation section's side-view sphere. */
  juce::Rectangle<int> elevationGraphic;
  /** The Shape section's pictogram and the name under it. */
  juce::Rectangle<int> trajectoryIcon;
  juce::Rectangle<int> trajectoryName;
  /** The take's length, on the Shape section's back — in recordLengthLog2
   *  order. */
  std::array<juce::Rectangle<int>, numRecordLengths> lengthButtons;
  /** How fast the clip plays, on its front — one key per speed the
   *  performer has put there, left to right. */
  std::array<juce::Rectangle<int>, numSpeedButtons> speedButtons;

  /** Which way a pass runs and what it does when it runs out. Under the
   *  speeds, on the front face only: they came from Motion, because what a
   *  take does when it ends is a property of the take rather than of the
   *  movement it traces. */
  juce::Rectangle<int> directionButton;
  juce::Rectangle<int> endActionButton;

  /** The clip field: which clip is in the slot, and a place to scroll through
   *  them with a finger.
   *
   *  A second hit area on the shape control the picture above already is, not
   *  a control of its own -- a name you can push with your thumb where the
   *  name is written, rather than a list that covers the picture you are
   *  choosing by. Empty on the Record face, which is about the take you are
   *  making rather than the clip you are holding. */
  juce::Rectangle<int> clipField;
  /** The bar's own header row: the four transport keys, then "Slot N", then
   *  the page tabs closing it. */
  std::array<juce::Rectangle<int>, numTransportKeys> transportButtons;
  /** One key per slot, where the slot's name used to be written. A heading
   *  that says which clip you are looking at and a control that changes which
   *  clip you are looking at want to be in the same place -- and reading
   *  "Slot 1" gave you no way to get to slot 2 without leaving for the pads
   *  page. */
  std::array<juce::Rectangle<int>, numPadSlots> slotButtons;
  /** One face per channel, in its own colour, carrying that channel's slot
   *  number.
   *
   *  These replaced CLIP and the two shared slot keys. CLIP meant "show me
   *  the clip" and you had to remember whose; a face says the same thing,
   *  says whose, and says which of its two slots -- with all four on screen
   *  at once. The number in it *is* the slot, and touching the face you are
   *  already on turns it over. */
  std::array<juce::Rectangle<int>, numChannelColumns> channelFaces;

  /** The frame the four faces stand in, the way the global strip's knobs and
   *  transport each stand in one.
   *
   *  Nine keys in a row read as nine of the same thing, and they are not:
   *  five choose what the settings area shows, four choose *which clip* it is
   *  showing. The frame is what says so -- and it is what lets the faces be
   *  narrower than a view without reading as keys that came out wrong. */
  juce::Rectangle<int> channelFacesFrame;

  /** The clip's plainest view, and the head of the row of views.
   *
   *  It was removed when the faces arrived, on the reasoning that a face said
   *  "show me this channel's clip" and said whose. That held only while the
   *  clip view was the one thing a face could lead to. A face now selects the
   *  clip that REC, ACTION and CLIP alike describe, so it no longer answers
   *  "which view" at all -- and without this key there would be no way back
   *  to the clip's own page from the pads or the browser. */
  juce::Rectangle<int> tabClip;
  juce::Rectangle<int> tabRecord;
  /** The clip's fourth view: what ACT does, and the envelope behind it. */
  juce::Rectangle<int> tabAction;
  juce::Rectangle<int> tabController;
  /** One channel's mixer strip, between PADS and the folder. It is a view of
   *  the channel whose clip the bar is describing, so it stands with the
   *  clip's own views rather than after the way out of them. */
  juce::Rectangle<int> tabMixer;
  /** The way to the browser. A folder rather than a fourth word: the three
   *  tabs are views of the clip you are on, and this leaves it. */
  juce::Rectangle<int> tabBrowser;
  /** The last-operated control, at the top of the **global strip** — the one
   *  part of the bar that stands on both pages. */
  juce::Rectangle<int> readout;

  /** The global strip's three action buttons — Menu, Rec, Tap. Device-wide
   *  functions that the hardware has its own keys for; these are the way to
   *  them with a finger. Beside `controls`, not in it: no encoder reaches
   *  them, so they are not sub-elements of the section. */
  /** One height for every button in the bar, whatever section it is in.
   *  Buttons that sized themselves to their own cell came out three
   *  different heights in three sections. */
  int buttonHeight = 0;

  juce::Rectangle<int> recModeButton;
  juce::Rectangle<int> clockModeButton;
  juce::Rectangle<int> menuButton;
  juce::Rectangle<int> recButton;
  juce::Rectangle<int> tapButton;
  /** Held, not tapped: Shift+Action previews for as long as it is down. In
   *  the global strip because it modifies the whole device, and a modifier on
   *  a page you have to leave is one you cannot hold. */
  juce::Rectangle<int> shiftButton;
};

/** The signal dot's diameter, as a share of the smaller side of a channel
 *  face.
 *
 *  A fifth. Big enough to be caught out of the corner of an eye at arm's
 *  length, small enough that it cannot be mistaken for the face's own colour
 *  or crowd the slot number in the middle. */
constexpr float channelFaceDotOfFace = 1.f / 5.f;

/** How far the dot is held off the face's corner, as a share of its own
 *  diameter. Half, so the air around it is of its own size and it reads as
 *  sitting *in* the face rather than clipped to its edge. */
constexpr float channelFaceDotInsetOfDot = 0.5f;

/** Lays the whole bar out for the given bounds and the three sizes the
 *  user can actually change (header and body font size, Pot Size). Reads
 *  no theme of its own, so it can be checked at sizes nobody has dialled
 *  in yet. */
ClipSettingsLayout layOutClipSettings (juce::Rectangle<int> bounds,
                                       float headerSize, float bodySize,
                                       float potSizeScale,
                                       BarPage page = BarPage::Clip);

/** Where the "not saved" mark sits inside a key or a field.
 *
 *  One rule for both places. The slot key in the header and the field on the
 *  browser page mean the same thing by it, and a mark that sat differently in
 *  the two would read as two different marks -- which is worse than no mark,
 *  because you would look for the difference.
 *
 *  Top right, and small: it is a footnote on the control, not part of what the
 *  control says. Never smaller than three pixels, or on a shrunken bar it
 *  would be a stray pixel rather than a dot.
 */
juce::Rectangle<int> driftMark (juce::Rectangle<int> bounds);

/** A control's box: as tall as the knob box, but the cell's full width —
 *  the knob is drawn at its own diameter inside it while caption and value
 *  get the room the grid gives them. Never taller than the cell, or a
 *  row's captions land on the row beneath. */
juce::Rectangle<int> textCell (juce::Rectangle<int> cell, int knobDiam);

/** A section title's row height. Never more than a third of the section,
 *  or a large header setting leaves no room for the controls. */
int titleRowHeight (juce::Rectangle<int> content, float headerSize);

/** A text row's height at `size` — a caption below a control, or a value
 *  above it. */
int textRowHeight (juce::Rectangle<int> content, float size);

}
