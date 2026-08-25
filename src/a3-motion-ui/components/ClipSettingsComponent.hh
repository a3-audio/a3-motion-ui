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

#include <a3-motion-ui/theme/Theme.hh>

#include <a3-motion-ui/components/TrajectoryIcon.hh>

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
 * End-Action) and Filter (Sweep/Q) use the same small-knob style — Speed/
 * Sweep/Q as rotary knobs, Direction/End-Action (discrete) as two-state-
 * style toggles showing their current name — laid out in a single row per
 * section since there's no graphic to share space with. Every knob/toggle
 * across every section (Elevation included) is given the same fixed-size
 * bounds — see controlBounds() — sized to fit Elevation's tightest layout
 * (graphic + 2x3 grid) and reused as-is (centred, with extra margin) by
 * Motion/Filter's roomier single-row layouts, rather than each stretching
 * to fill its own section's grid cell; a knob's optional value text
 * (Speed only, e.g. "1/4") sits directly above it, its label directly
 * below. That shared size scales with setPotSizeScale(), and its label/
 * value text separately via the theme's font scale — both driven by their
 * own Global Settings options ("Pot Size"/"Font Size"), adjustable live
 * without a rebuild.
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
class ClipSettingsComponent : public juce::Component
{
public:
  static constexpr int numParameters = 4;
  static constexpr int trajectoryIndex = 0; // leftmost, pictogram section
  static constexpr int elevationIndex = 1;  // sphere/coverage section
  static constexpr int motionIndex = 2;     // speed/direction/end-action
  static constexpr int filterIndex = 3;     // sweep/Q

  explicit ClipSettingsComponent ();

  /** Which channel/clip-slot the panel currently describes, and that
   *  channel's colour (drawn as a frame + section accents). */
  void setTarget (int channel, int slot, juce::Colour channelColour);

  /** Pictogram + name shown in the Shape section. */
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

  /** Filter section values, both unipolar (0..1). */
  void setFilterSweep (float sweep);
  void setFilterQ (float q);

  /** Which of the Filter section's 2 controls (0 = sweep, 1 = Q) the
   *  Pot-Encoder currently edits, cycled by pressing it. Both are always
   *  shown; this only controls highlighting. */
  void setFilterSubIndex (int subIndex);

  /** Which section (0..numParameters-1) is currently selected/highlighted. */
  void setSelectedParameterIndex (int index);

  /** Scale factor for every knob/toggle's shared size (see class doc) —
   *  1.0 = default. Set from A3MotionUIComponent's Global Settings "Pot
   *  Size" option, so it's adjustable live on the device without a
   *  rebuild. */
  void setPotSizeScale (float scale);

  /** Scale factor for every knob/toggle's label and value text — 1.0 =
   *  default. Independent of setPotSizeScale(), since a knob's circle and
   *  its text may need tuning separately (e.g. a small pot with big,

  /** One-line terminal-style readout of the last-operated control, shown
   *  top-right (e.g. "CH2 POT1 0.73"). Global, independent of setTarget(). */
  void setLastControlReadout (juce::String const &text);

  void paint (juce::Graphics &g) override;

private:
  void paintTrajectorySection (juce::Graphics &g, juce::Rectangle<int> bounds,
                               bool isSelected);
  void paintElevationSection (juce::Graphics &g, juce::Rectangle<int> bounds,
                              bool isSelected, int knobDiam);
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
  void paintMotionSection (juce::Graphics &g, juce::Rectangle<int> bounds,
                           bool isSelected, int knobDiam);
  /** Sweep / Q (knobs), single row. */
  void paintFilterSection (juce::Graphics &g, juce::Rectangle<int> bounds,
                           bool isSelected, int knobDiam);
  /** Small, deliberately unobtrusive section title (see class doc) — most
   *  of a section's height goes to its controls, not this label. */
  void paintSectionLabel (juce::Graphics &g, juce::Rectangle<int> labelArea,
                          juce::String const &text, bool isSelected);
  /** Fixed-size (knobDiam wide, tall enough for label + optional value
   *  text above), centred within `cell` — every knob/toggle across every
   *  section is given bounds built this way, so they all render at
   *  identical size regardless of how roomy their own section's grid cell
   *  happens to be (see class doc / setPotSizeScale()). */
  juce::Rectangle<int> controlBounds (juce::Rectangle<int> cell,
                                      int knobDiam) const;
  /** Height of a control's caption row, from the Label role. */
  int labelRowHeight (juce::Rectangle<int> content) const;

  /** Largest size for `role` at which `text` still fits inside `area`. */
  float fontFor (FontRole role, juce::Rectangle<int> area,
                 juce::String const &text) const;

  /** A control's cell at full grid width, knob height. */
  juce::Rectangle<int> textCell (juce::Rectangle<int> cell,
                                 int knobDiam) const;
  /** Small labelled rotary knob (Ableton/Bitwig-style), sized to fill
   *  `bounds` (a fixed box from controlBounds(), unaffected by Font
   *  Size — see its comment). `angleFrac` is -1..1, mapped onto the
   *  knob's -135deg..+135deg sweep (0 = straight up). `fillFromZero`
   *  picks the fill style: true draws the value arc from centre (for
   *  bipolar params), false draws it from the sweep's start (for
   *  unipolar params like clip-top/clip-bottom, whose 0..1 range the
   *  caller has already remapped to -1..1). `isActive` highlights it as
   *  the one the Pot-Encoder currently edits. `valueText` is optional:
   *  when non-empty, shown above the knob — for quantized/discrete
   *  params (currently just Speed) where the exact value matters and
   *  can't be read off the knob angle alone. Label/value text sizing is
   *  Font-Size-scaled, but drawn into a widened *fitting* target that
   *  doesn't affect the knob's own (fixed) position or size. */
  void paintMiniKnob (juce::Graphics &g, juce::Rectangle<int> bounds,
                      int knobDiam,
                      juce::String const &label, float angleFrac,
                      bool fillFromZero, bool isActive, bool isSelected,
                      juce::String const &valueText = {});
  /** Small labelled two-state toggle (mirror-south, flat), styled to match
   *  paintMiniKnob: label, then the current state as centred text instead
   *  of an arc. */
  void paintMiniToggle (juce::Graphics &g, juce::Rectangle<int> bounds,
                        int knobDiam,
                        juce::String const &label, juce::String const &stateText,
                        bool isActive, bool isSelected);

  int _channel = 0;
  int _slot = 0;
  juce::Colour _channelColour{ juce::Colours::white };
  juce::String _lastControlText;
  TrajectoryIconData _trajectoryIcon;
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
  float _filterSweep = 0.0f;
  float _filterQ = 0.0f;
  int _filterSubIndex = 0;
  int _selectedIndex = 0;
  float _potSizeScale = 1.0f;

  static constexpr int paddingH = 16;

  static constexpr char const *parameterNames[numParameters] = {
    "Shape", "Elevation", "Motion", "Filter",
  };
};

}
