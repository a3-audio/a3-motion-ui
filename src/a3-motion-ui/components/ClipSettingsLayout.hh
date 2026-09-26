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

#include <a3-motion-engine/ClipSettings.hh>
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

/** One channel face per channel, at the top of the global section. */
constexpr int numChannelColumns = numChannelsInitial;

/** The lengths a take can be given, as powers of two of a bar, and how they
 *  are worded. Eight buttons rather than a list: the whole range from 1/128
 *  to 16 bars was a dropdown nobody wanted to scroll, and these are the ones
 *  anybody reaches for. This table is the authority on what a take's length
 *  may be — it reaches 32 bars, one step past the speed control's range. */

// speedLog2Min/Max come from ClipSettings.hh, beside the value they bound.
// They lived here until 2026-09-21, where the engine could not see them --
// and an action script therefore clamped to a different pair.

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

/** A length, written as the ticks you can count on the indicator.
 *
 *  Everything in the bar that says how long something is says it in these:
 *  the record lengths, the speed keys, the Motion section's readout. They used
 *  to be counted in bars and in ratios, which are two different units on two
 *  keys a finger's width apart -- and neither is the thing a person actually
 *  counts while watching the take go round.
 *
 *  Exact rather than rounded. A length is a whole number of beats or a
 *  power-of-two fraction of one, and a six-beat take at a sixteenth of its
 *  length really is three eighths of a beat: "3/8", not a tidied 1/4 that the
 *  indicator would then contradict. */
juce::String beatsName (float beats);

/** What a speed key says, for the take it would be applied to.
 *
 *  The key is a **ratio** -- `2^speedLog2` of the take's own recorded length
 *  -- so the same key is four ticks on a four-beat take and eight on an
 *  eight-beat one. Naming it with a fixed number would be true for one clip
 *  and a lie for the next; naming it with the ratio ("1" for "as recorded")
 *  was honest and unreadable, because 1 is not a number of anything you can
 *  see. So it is computed, and it changes when the clip does.
 *
 *  A take of no length has no answer, and a key showing a number it cannot
 *  honour is worse than one showing none: that reads `--`. */
juce::String speedKeyName (int speedLog2, float patternLengthBeats);


/** Which row to stand on after the one at `row` has been thrown away.
 *
 *  The row that took its place -- the same number, held inside the list that
 *  is left. A delete used to put the selection back on row 0, which is the
 *  library's "Empty" and has no file, and the list back at its top: in seventy
 *  rows that loses your place, and the row you reach for next is one of the
 *  shipped ones where the Delete key is correctly dark. Reported as the key
 *  working twice and then not at all.
 *
 *  `remaining` is how many rows are left *after* the removal. */
int selectionAfterRemoving (int row, int remaining);
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

/** Which of the bar's two pages is showing.
 *
 *  Here rather than inside ClipSettingsComponent because two components and
 *  the orchestrator between them all speak it, and burying it in one of them
 *  would drag that component's whole header into the other two. */
enum class BarPage
{
  Clip,
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
  /** The take about to be made: Shape as CLIP shows it, beside one card with
   *  the rec mode, fade and bias. The bar's own sections, so it covers
   *  nothing. */
  Record,
  /** The clip's movement: Motion's knobs and Elevation's, which stood beside
   *  the shape on CLIP until 2026-09-26. The bar's own sections, so it covers
   *  nothing. */
  Motion,
};

constexpr int numBarPages = 7;

/** Every page, once. `BarPages.EveryPageAppearsInTheOrderExactlyOnce` fails
 *  if a page is missing from here or listed twice -- nothing in the compiler
 *  checks that on its own, since this is data, not a case of an enum. Kept
 *  beside pageCoversClipArea and pageDescribesAClip because a page has to be
 *  added here too, alongside a case in each of them, and a forgotten one
 *  answers wrong quietly forever if this test does not walk it. */
constexpr std::array<BarPage, numBarPages> barPageOrder{
  BarPage::Clip,       BarPage::Action,
  BarPage::Controller, BarPage::Mixer,  BarPage::Browser,
  BarPage::Record,    BarPage::Motion,
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
    case BarPage::Motion:
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
    case BarPage::Action:
    case BarPage::Mixer:
    case BarPage::Browser:
    case BarPage::Record:
    case BarPage::Motion:
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

/** Whether a tap on this control already steps its value on, rather than
 *  only selecting it. True for the few-valued ones — direction, end-action
 *  and the global strip's rec mode — which wrap, so every tap arrives
 *  somewhere new. False for anything continuous, and for the lists
 *  (pattern, speed, record length, seam) where tapping through would turn
 *  into tapping and tapping. */
bool tapAdvancesValue (int sectionIndex, int subIndex);

/** The circle the elevation graphic draws, inside whatever cell it is given.
 *
 *  A picture again since 2026-09-26: the middle of the trajectory is set by
 *  the elv knob, not by a finger on the circle. */
juce::Rectangle<int> elevationCircleBounds (juce::Rectangle<int> cell);


/** Pull a base that is nearly at ear height exactly onto it.
 *
 *  The equator is where a sound is level with the listener, and it is the one
 *  place in the circle worth hitting exactly -- in a circle a few dozen
 *  pixels tall it is otherwise a single row, which is not a target. The pull
 *  lets go a little further out, or the equator would become a hole you
 *  cannot set a value beside. */
float snapElevationBase (float base);

/** The base elv sets: the knob turned the way a level is (clockwise is
 *  higher, where the base counts from the top), held inside the clip band
 *  and snapped like the graphic's finger was. */
float elevationBaseForKnob (float knob, float clipTop, float clipBottom);

/** Where elv stands for a base -- the base the other way up. */
constexpr float
knobForElevationBase (float base)
{
  return 1.f - base;
}


/** How many of Motion's controls stand on the MOTION page: the first eight.
 *  Its last two, fade (8) and bias (9), stand on REC since 2026-09-26 and
 *  keep their sub-indices, so the encoders and the take reach them as before. */
constexpr std::size_t motionSubsOnTheMotionPage = 8;

/** Whether a control stands on a page. */
constexpr bool
controlIsOnPage (int section, int sub, BarPage page)
{
  switch (section)
    {
    case 0: // Shape: all four on CLIP; on REC the picker and the picture.
      return page == BarPage::Clip || (page == BarPage::Record && sub < 2);
    case 1: // Elevation
      return page == BarPage::Motion;
    case 2: // Motion: the first eight on MOTION, fade and bias on REC
      return static_cast<std::size_t> (sub) < motionSubsOnTheMotionPage
                 ? page == BarPage::Motion
                 : page == BarPage::Record;
    case 3: // the rec mode
      return page == BarPage::Record;
    default:
      return false;
    }
}

/** Whether a page's tab is lit: the page on show, unless the big mixer is
 *  over the sphere -- then MAINMIX is the lit tab. */
constexpr bool
pageTabIsLit (BarPage tab, BarPage shown, bool mainMixOpen)
{
  return !mainMixOpen && tab == shown;
}

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

  /** The two blocks of the global strip, each in a frame of its own: the
   *  channel faces above, the transport below -- whose clip, then what to do
   *  to it. */
  juce::Rectangle<int> transportFrame;
  /** The Elevation section's side-view sphere. */
  juce::Rectangle<int> elevationGraphic;
  /** The grey field the picture stands in, at the top of the global strip,
   *  like the faces' and the transport's. Touched, it selects the picture:
   *  while selected, the big sphere turns the camera. */
  juce::Rectangle<int> elevationFrame;
  /** The little camera in the frame's top right corner: what touching the
   *  picture selects. */
  juce::Rectangle<int> elevationCameraMark;
  /** The Shape section's pictogram and the name under it. */
  juce::Rectangle<int> trajectoryIcon;
  juce::Rectangle<int> trajectoryName;
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
   *  choosing by. It also carries how long the next take will be -- see
   *  RecordingLength.hh: the length is the shown clip's, so it is written
   *  where the clip is named rather than on keys of its own. */
  juce::Rectangle<int> clipField;
  /** Rec, stop, play and act, two by two in the global strip under the
   *  channel faces, as a clip's pads stand on PADS: play and stop on top,
   *  act and rec under them. Indexed like transportKeyOrder. */
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
   *  already on turns it over.
   *
   *  At the top of the global strip since 2026-09-26, where the 4x3 grid of
   *  3D, FREQ and Q stood until those moved into the mixer strips. Which
   *  clip the bar describes is a choice for the whole device, like the
   *  transport under them, so they stand with it rather than in the clip's
   *  own header. */
  std::array<juce::Rectangle<int>, numChannelColumns> channelFaces;

  /** The frame the four faces stand in, the way the transport under them
   *  stands in one. */
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
  /** The clip's fourth view: what ACT does, and the envelope behind it. */
  juce::Rectangle<int> tabAction;
  juce::Rectangle<int> tabController;
  /** One channel's mixer strip, between PADS and the folder. It is a view of
   *  the channel whose clip the bar is describing, so it stands with the
   *  clip's own views rather than after the way out of them. */
  juce::Rectangle<int> tabMixer;
  /** Not a view: opens and closes the big mixer over the sphere, and is lit
   *  while it is open. It stood in the status bar as MIX until 2026-09-26. */
  juce::Rectangle<int> tabMainMix;
  juce::Rectangle<int> tabRecord;
  juce::Rectangle<int> tabMotion;

  /** CLIP's middle and right columns (2026-09-26): dir and end, and the four
   *  lengths. The left one is Shape's card, sectionCards[0]. */
  juce::Rectangle<int> playCard;
  juce::Rectangle<int> lengthCard;
  /** Their title rows. */
  juce::Rectangle<int> playLabel;
  juce::Rectangle<int> lengthLabel;


  /** The REC page's card, across the two columns Elevation and Motion take
   *  on CLIP, and its title row. */
  juce::Rectangle<int> recordCard;
  juce::Rectangle<int> recordLabel;
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

  /** The rec mode's key, at the top of the REC page's card. CLOCK, MENU and
   *  TAP went to the status bar on 2026-09-26; REC and SHIFT left the screen
   *  (the transport and the panel carry them). */
  juce::Rectangle<int> recModeButton;
};

/** The card a control is drawn in: its section's; CLIP's middle card for dir
 *  and end; the REC page's for fade, bias and the rec mode. */
juce::Rectangle<int> cardOfControl (ClipSettingsLayout const &layout,
                                    int section, int sub);

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

/** A share of the bar's height, written as the divisor that is actually
 *  divided by.
 *
 *  It carries both forms on purpose. layOutClipSettings() spends these by
 *  integer division -- `of (height)`, truncating, which is what decides the
 *  pixels -- while clipSettingsPreferredHeight() has to solve for the same
 *  chrome as a fraction of the whole, which is `asFraction ()`. Those two
 *  were written out by hand in two different files, as `height / 40` in one
 *  and `2.f / 40.f` in the other, and nothing said they were the same
 *  number. They had to change together and the connection was named
 *  nowhere. */
struct HeightShare
{
  int divisor;

  constexpr int
  of (int height) const
  {
    return height / divisor;
  }

  constexpr float
  asFraction () const
  {
    return 1.f / static_cast<float> (divisor);
  }
};

/** The panel's own margin, top and bottom -- the vertical counterpart of
 *  paddingH(). A skinned paddingSmall can widen it; this is its floor. */
constexpr HeightShare barPadding{ 40 };

/** The gap between the header row and the sections under it. */
constexpr HeightShare barHeaderGap{ 50 };

/** The header row's height. A ninth of the bar, but never shorter than a
 *  fingertip: every control in that row is pressed mid-set by a hand that is
 *  also doing something else. At the sizes the device ships with the ninth
 *  decides; at the smallest font and pot the fingertip does. */
constexpr HeightShare barHeader{ 9 };

/** ...and never taller than this, whatever the bar's height. */
constexpr HeightShare barHeaderMax{ 6 };

/** The gap between the keys *within* the header row -- a share of that row,
 *  not of the bar. */
constexpr HeightShare headerGapOfHeader{ 12 };

/** The tallest a row of buttons may get. Also a sixth, and deliberately its
 *  own name rather than barHeaderMax: the two are different quantities that
 *  happen to share a number, and folding them together would tie the button
 *  rows to the header's ceiling for no reason. */
constexpr HeightShare barButtonMax{ 6 };

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
