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

#include <array>
#include <functional>
#include <memory>
#include <vector>

#include <a3-motion-engine/RecMode.hh>

#include <a3-motion-ui/theme/Theme.hh>

#include <a3-motion-ui/components/ClipSettingsCaptions.hh>
#include <a3-motion-ui/components/ClipSettingsLayout.hh>
#include <a3-motion-ui/components/TouchControl.hh>
#include <a3-motion-ui/components/TrajectoryIcon.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>
#include <a3-motion-ui/theme/ThemedComponent.hh>

namespace a3
{

/**
 * ClipSettingsComponent
 *
 * Permanent bottom panel showing the last-selected clip's settings, as 4
 * vertical sections side by side (Shape, Elevation, Motion, Filter). The
 * leftmost section (Shape) shows a pictogram of the loaded pattern with
 * its name underneath; the second (Elevation) shows a small side-view
 * sphere graphic (listener's head at the centre, grey bands for the
 * clip-top/clip-bottom excluded zones, and a solid marker line at the
 * reach cone's edge — or, in flat mode, a single flat elevation line
 * instead) on top of six small controls: reach, clip-top, clip-bottom
 * (rotary knobs), mirror-south, flat (toggles), and flat-elevation (rotary
 * knob, only meaningful while flat is on) — see Pattern::getReach()/
 * getMirrorSouth()/getClipTop()/getClipBottom()/getFlat()/
 * getFlatElevation(). Strictly monotonic model: the centre of the recorded
 * 2D trajectory always maps to the pole (mirror-south picks which one),
 * the disc's edge always maps to `reach`'s point — no folding or
 * base-point interaction, unlike the wrap/elevation scheme this replaced.
 * All Elevation controls are visible in parallel, with whichever one the
 * Pot-Encoder currently edits highlighted (the graphic highlights too,
 * whenever the highlighted control affects it). Motion (Speed/Direction/
 * End-Action) and Filter (Freq/Q) use the same small-control style —
 * Freq/Q as rotary knobs, Speed/Direction/End-Action as value displays
 * showing their current name or note value instead of a knob (Speed is
 * quantized to note values, and a knob angle says nothing a reader of
 * "1/4" does not already know) — laid out in a single row per section
 * since there's no graphic to share space with. Every knob/toggle
 * across every section (Elevation included) is given the same fixed-size
 * bounds — see controlBounds() — sized to fit Elevation's tightest layout
 * (graphic + 2x3 grid) and reused as-is (centred, with extra margin) by
 * Motion/Filter's roomier single-row layouts, rather than each stretching
 * to fill its own section's grid cell; a control's caption sits directly
 * below it, and a value display's value directly above its caption. That
 * shared size scales with the skin's potSize, and its text separately via
 * the theme's font scale — both driven by their own Global Settings
 * options ("Pot Size"/"Font Size"), adjustable live without a rebuild.
 * Every caption in the bar is drawn at one size, from the tightest
 * caption box across all sections (see sharedCaptionSize()); fitting each
 * caption to its own box instead made short captions like "Q" several
 * times the height of the long ones beside them. The values share a
 * second size the same way (see sharedValueSize()), which is what brought
 * the lone "1" in the Motion section back into proportion.
 * Section headings are deliberately small (see paintSectionLabel()) so
 * most of each section's height goes to its controls. Driven by two
 * per-channel encoders in A3MotionUIComponent: the Motion-Encoder scrolls
 * which section is selected, the Pot-Encoder changes that section's
 * (sub-)value — this component only renders whatever it's told via
 * setTrajectoryIcon()/setElevationReach()/setMotionSpeed()/
 * setFilterSweep()/setSelectedParameterIndex() etc., it holds no logic of
 * its own. Sizing is derived from whatever bounds the parent gives it (see
 * A3MotionUIComponent::resized()), not a fixed pixel height.
 */
class ClipSettingsComponent : public juce::Component,
                              public ThemedComponent,
                              private juce::Timer
{
public:
  /** How many sections describe the shown clip. They and the global strip
   *  beside them share the bar's width equally. */
  static constexpr int numClipSections = 3;

  /** Stops the Motion-Encoder scrolls through: the clip's sections, then the
   *  global strip. */
  static constexpr int numParameters = numClipSections + 1;

  static constexpr int trajectoryIndex = 0; // leftmost, pictogram section
  static constexpr int elevationIndex = 1;  // sphere/coverage section
  static constexpr int motionIndex = 2;     // speed/direction/end-action
  static constexpr int globalIndex = 3;     // rightmost, not the clip's

  /** How wide one of the clip's four sections is, given the width the row of
   *  sections has to share.
   *
   *  Five equal parts: the four sections and the global strip beside them.
   *  The strip used to be half a section — an aside rather than a section of
   *  its own — but it now carries the Menu/Rec/Tap buttons, and a finger
   *  needs a target the size of a finger. */
  static constexpr int
  clipSectionWidth (int rowWidth)
  {
    return rowWidth * 3 / 4 / numClipSections;
  }

  explicit ClipSettingsComponent ();

  /** Which channel/clip-slot the panel currently describes, and that
   *  channel's colour (drawn as a frame + section accents). */
  void setTarget (int channel, int slot, juce::Colour channelColour);

  /** Pictogram + name shown in the Shape section. */
  /** The recording mode, shown in the global strip. Not a clip setting: it is
   *  the same for every channel, which is why it sits apart from them. */
  void setRecMode (RecMode mode);

  void setTrajectoryIcon (TrajectoryIconData const &icon);
  void setTrajectoryName (juce::String const &name);

  /** Elevation section values, shown as six small controls, always visible
   *  together. reach/clipTop/clipBottom/flatElevation are unipolar
   *  (0..1); mirrorSouth/flat are booleans. */
  void setElevationReach (float reach);
  void setElevationMirrorSouth (bool mirrorSouth);
  void setElevationClipTop (float clipTop);
  void setElevationClipBottom (float clipBottom);
  void setElevationFlat (bool flat);
  void setElevationFlatElevation (float flatElevation);

  /** Which of the Elevation section's 6 controls (0 = reach, 1 = clip-top,
   *  2 = clip-bottom, 3 = mirror-south, 4 = flat, 5 = flat-elevation) the
   *  Pot-Encoder currently edits, cycled by pressing it. All six are always
   *  shown; this only controls which one is highlighted. */
  void setElevationSubIndex (int subIndex);

  /** Motion section values. Speed is quantized to musical note-value
   *  fractions/multiples of a bar rather than a free value — the caller
   *  (A3MotionUIComponent, which owns the actual speedLog2 range) passes
   *  an already-normalized knob position (0..1) plus the formatted label
   *  to show above the knob (e.g. "1/4", "2"), so this component doesn't
   *  need to know the underlying range. direction/endAction are discrete
   *  (0..2), shown as toggles with their state name. */
  void setMotionSpeed (float normalizedFrac, juce::String const &label);
  void setMotionDirection (int direction);
  void setMotionEndAction (int endAction);

  /** Which of the Motion section's 3 controls (0 = speed, 1 = direction,
   *  2 = end-action) the Pot-Encoder currently edits, cycled by pressing
   *  it. All three are always shown; this only controls highlighting. */
  void setMotionSubIndex (int subIndex);

  /** Which stretch-filling a take will use — 0 glide, 1 hard. */
  void setMotionFade (int sixteenths);

  /** The length the next take will have, already worded ("2", "1/4"), and
   *  which of the shape section's two elements is armed. */
  void setRecordLength (juce::String const &label);
  /** Every length the next take can have, worded as setRecordLength words
   *  the current one. Supplied once: it is a fixed range, and the list has
   *  to know all of it to offer it. */
  void setRecordLengthValues (juce::StringArray values);
  void setTrajectorySubIndex (int subIndex);


  /** One channel's three values in the global section's grid — freq, Q and
   *  the third one ("3d"), all unipolar (0..1). These belong to the channel,
   *  not to the clip the bar happens to show, which is why they sit in the
   *  global section rather than in a section of the clip's. */
  void setChannelValues (int channel, float freq, float q, float threeD);

  /** Which section (0..numParameters-1) is currently selected/highlighted. */
  void setSelectedParameterIndex (int index);

  /** Scale factor for every knob/toggle's shared size (see class doc) —
   *  1.0 = default. Set from A3MotionUIComponent's Global Settings "Pot
   *  Size" option, so it's adjustable live on the device without a


  /** One-line terminal-style readout of the last-operated control, shown
   *  top-right (e.g. "CH2 POT1 0.73"). Global, independent of setTarget(). */
  void setLastControlReadout (juce::String const &text);

  /** A control was tapped: select its section and sub-element in one go —
   *  what the encoders reach by scrolling and pressing. */
  std::function<void (int section, int sub)> onControlTapped;
  /** A control was dragged, by one increment. Same increment the
   *  Pot-Encoder produces, so both go through one handler. */
  std::function<void (int section, int sub, int increment)> onControlDragged;

  /** A cell of the per-channel grid was dragged. `row` is in channelRow*
   *  order. */
  std::function<void (int channel, int row, int increment)>
      onChannelValueDragged;

  /** The global strip's action buttons. Device-wide functions the hardware
   *  has its own keys for — this is the way to them with a finger. */
  /** The rec mode steps on — what its encoder used to do. */
  std::function<void ()> onRecModePressed;
  std::function<void ()> onMenuPressed;
  std::function<void ()> onRecordPressed;
  std::function<void ()> onTapPressed;

  /** Whether the Rec button should read as armed. */
  void setRecording (bool recording);

  /** A beat went by. TAP lights up for it — the beat is the one thing on
   *  this device you cannot see, and a button that only reacts to being
   *  pressed tells you nothing about whether it landed. */
  void beatPulse ();

  void paint (juce::Graphics &g) override;
  void resized () override;

  /** The bar caches its whole geometry, and that geometry is built from
   *  the skin's font and pot sizes — so a skin change is a re-layout here,
   *  not only a repaint. */
  void applyTheme () override;

  /** How tall this bar wants to be at the current font and pot sizes, given
   *  the width it will get. The caller clamps it — see
   *  clipSettingsHeightWithin(). */
  int preferredHeight (int width) const;

private:
  /** The card behind a section plus its title — every section opens with
   *  it, and it is the only thing they all draw the same way. */
  void paintSectionCard (juce::Graphics &g, int sectionIndex, bool isSelected);

  /** Recomputes _layout from the current bounds and theme. Called by both
   *  paint() and resized(), so the picture and the hit areas can never
   *  disagree. */
  void updateLayout ();

  void paintTrajectorySection (juce::Graphics &g, bool isSelected);
  void paintElevationSection (juce::Graphics &g, bool isSelected);
  /** Side-view sphere graphic for the Elevation section: circle + head dot
   *  at the pole (mirror-south picks which one), grey clip-top/clip-bottom
   *  excluded bands, and a solid marker line at reach's edge — or, while
   *  flat is on, a single flat elevation line instead of the cone/head-dot.
   *  `isActive` highlights it exactly like a knob would, whenever the
   *  Pot-Encoder currently edits a control that affects this graphic
   *  (reach, mirror-south, flat, or flat-elevation). */
  void paintElevationGraphic (juce::Graphics &g, juce::Rectangle<int> bounds,
                              bool isActive, bool isSelected);
  /** Speed (knob) / Direction / End-Action (toggles), single row, always
   *  visible in parallel — same style as the Elevation controls. */
  void paintMotionSection (juce::Graphics &g, bool isSelected);
  /** Freq / Q (knobs), single row. */
  /** The global section's 4x3 grid, each column in its channel's colour. */
  void paintChannelGrid (juce::Graphics &g);
  void paintGridKnob (juce::Graphics &g, juce::Rectangle<int> bounds,
                      ControlMetrics metrics, float value,
                      juce::Colour colour);
  /** Small, deliberately unobtrusive section title (see class doc) — most
   *  of a section's height goes to its controls, not this label. */
  void paintGlobalSection (juce::Graphics &g, bool isSelected);
  /** One action button: a filled, labelled box. Not paintMiniToggle — that
   *  shows a value under a caption, and these have no value, only a name
   *  and the fact that they can be pressed. */
  void paintActionButton (juce::Graphics &g, juce::Rectangle<int> bounds,
                          juce::String const &label, bool isActive);
  /** The bar's one button face — see the definition. `caption` may be empty
   *  for a button that names itself. */
  void paintBarButton (juce::Graphics &g, juce::Rectangle<int> bounds,
                       juce::String const &label, juce::String const &caption,
                       bool isActive, bool isSelected,
                       bool opensList = false);

  void paintSectionLabel (juce::Graphics &g, juce::Rectangle<int> labelArea,
                          juce::String const &text, bool isSelected);


  /** A section card's fill: the channel's colour while the section is
   *  selected, a barely-there wash otherwise. */
  juce::Colour cardColour (bool isSelected) const;
  /** What a control is drawn in — its arc, its icon, its value. Takes the
   *  channel's colour in the selected section so that the section the
   *  encoders act on is the one that carries the colour. */
  juce::Colour controlColour (bool isSelected) const;
  /** The caption naming a control: quieter than the control itself, and
   *  quieter again outside the selected section. */
  juce::Colour captionColour (bool isSelected) const;

  /** Largest size for `role` at which `text` still fits inside `area`. */
  float fontFor (FontRole role, juce::Rectangle<int> area,
                 juce::String const &text) const;

  /** Small labelled rotary knob (Ableton/Bitwig-style), sized to fill
   *  `bounds` (a fixed box from controlBounds(), unaffected by Font
   *  Size — see its comment). `angleFrac` is -1..1, mapped onto the
   *  knob's -135deg..+135deg sweep (0 = straight up). `fillFromZero`
   *  picks the fill style: true draws the value arc from centre (for
   *  bipolar params), false draws it from the sweep's start (for
   *  unipolar params like clip-top/clip-bottom, whose 0..1 range the
   *  caller has already remapped to -1..1). `isActive` highlights it as
   *  the one the Pot-Encoder currently edits. A knob shows no value text:
   *  a continuous param is read off the angle, and the ones whose exact
   *  value matters (Speed) are drawn as value displays instead — see
   *  paintMiniToggle(). The caption's size comes from the bar, not from
   *  this box; the knob's own position and size are unaffected by Font
   *  Size. */
  void paintMiniKnob (juce::Graphics &g, juce::Rectangle<int> bounds,
                      ControlMetrics metrics,
                      juce::String const &label, float angleFrac,
                      bool fillFromZero, bool isActive, bool isSelected);
  /** Small labelled value display (mirror-south, flat, direction,
   *  end-action, speed), styled to match paintMiniKnob: the current state
   *  or value as centred text instead of an arc, with the caption below
   *  it. */
  void paintMiniToggle (juce::Graphics &g, juce::Rectangle<int> bounds,
                        ControlMetrics metrics,
                        juce::String const &label,
                        juce::String const &stateText,
                        bool isActive, bool isSelected);

  int _channel = 0;
  int _slot = 0;
  juce::Colour _channelColour; // set from the theme in the constructor
  juce::String _lastControlText;
  TrajectoryIconData _trajectoryIcon;
  RecMode _recMode = RecMode::Touch;
  juce::String _trajectoryName{ "Empty" };
  float _elevationReach = 0.5f;
  bool _elevationMirrorSouth = false;
  float _elevationClipTop = 0.0f;
  float _elevationClipBottom = 0.0f;
  bool _elevationFlat = false;
  float _elevationFlatElevation = 0.5f;
  int _elevationSubIndex = 0;
  float _motionSpeedFrac = 0.5f;
  juce::String _motionSpeedLabel{ "1" };
  int _motionDirection = 0;
  int _motionEndAction = 0;
  int _motionSubIndex = 0;
  int _motionFade = 0;
  int _trajectorySubIndex = 0;
  juce::String _recordLengthLabel { "1" };
  juce::StringArray _recordLengthValues;
  std::array<float, numChannelColumns> _channelFreq{};
  std::array<float, numChannelColumns> _channelQ{};
  std::array<float, numChannelColumns> _channelThreeD{};

  /** Which list is open, by section and sub-element; -1 for none. While one
   *  is open it takes over its section's controls — a list cannot open
   *  outside the bar, because MotionComponent's GL context composites above
   *  anything over it. */
  int _openDropdownSection = -1;
  int _openDropdown = -1;
  /** How many entries the open list has, and where they sit. Rebuilt when a
   *  list opens, so paint() and the hit areas read the same rectangles. */
  std::vector<juce::Rectangle<int>> _dropdownEntries;
  std::vector<std::unique_ptr<TouchControl>> _dropdownTouch;

  void openDropdown (int section, int sub);
  /** Whether this control is a list rather than something to turn. */
  static bool opensList (int section, int sub);
  void closeDropdown ();
  void layOutDropdown ();
  void paintDropdown (juce::Graphics &g);
  /** The values the given Motion sub-element can take. */
  juce::StringArray dropdownValues (int section, int sub) const;
  int dropdownCurrentIndex (int section, int sub) const;
  int _selectedIndex = 0;

  /** Every rectangle in the bar, recomputed by updateLayout(). */
  ClipSettingsLayout _layout;

  /** Invisible hit areas over what paint() draws: one per section card,
   *  and one per control on top of it. The cards are created first so the
   *  controls sit in front of them — a tap on a knob must not be caught by
   *  the card it lies on. */
  std::array<std::unique_ptr<TouchControl>, numParameters> _sectionTouch;
  /** Over the Elevation section's sphere graphic, in front of that
   *  section's card, with no callbacks at all: the graphic is a picture of
   *  what the controls below it do, and touching a picture should do
   *  nothing. Without it the card underneath would answer. */
  std::unique_ptr<TouchControl> _elevationGraphicTouch;
  /** One hit area per grid cell, [channel][row]. */
  std::array<std::array<std::unique_ptr<TouchControl>, numChannelRows>,
             numChannelColumns>
      _gridTouch;
  std::unique_ptr<TouchControl> _recModeTouch;
  std::unique_ptr<TouchControl> _menuTouch;
  std::unique_ptr<TouchControl> _recTouch;
  std::unique_ptr<TouchControl> _tapTouch;
  bool _recording = false;
  /** TAP is lit: on the beat, and while a finger is on it. */
  bool _tapLit = false;
  void timerCallback () override;
  std::array<std::vector<std::unique_ptr<TouchControl>>, numParameters>
      _controlTouch;

  /** Builds the hit areas once; resized() only moves them afterwards. */
  void createTouchControls ();

  static constexpr char const *parameterNames[numParameters] = {
    "Shape", "Elevation", "Motion", "Global",
  };
};

}
