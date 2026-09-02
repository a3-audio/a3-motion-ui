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
constexpr int channelRowThreeD = 0;
constexpr int channelRowFreq = 1;
constexpr int channelRowQ = 2;

/** How many controls a section holds. Must agree with
 *  A3MotionUIComponent::numSubElementsForSection — a tap addresses a
 *  control by the same sub-index the encoder does. */
int numControlsInSection (int sectionIndex);

/** Whether a tap on this control already steps its value on, rather than
 *  only selecting it. True for the few-valued ones — mirror-south, flat,
 *  direction, end-action and the global strip's rec mode. False for
 *  anything continuous, and for the lists (pattern, speed, record length,
 *  seam) where tapping through would turn into tapping and tapping. */
bool tapAdvancesValue (int sectionIndex, int subIndex);

/** Every rectangle in the bar, from one calculation. paint() draws into
 *  it, resized() puts the TouchControls on it. Two calculations would be
 *  two truths, and those drift apart. */
struct ClipSettingsLayout
{
  /** What paint() would otherwise have derived on its own. */
  ControlMetrics metrics;
  int headerHeight = 0;

  juce::Rectangle<int> clipBounds;
  juce::Rectangle<int> globalBounds;

  /** The card per section, indexed like ClipSettingsComponent::*Index. */
  std::array<juce::Rectangle<int>, numClipSettingsSections> sectionCards;
  /** The title row per section. */
  std::array<juce::Rectangle<int>, numClipSettingsSections> sectionLabels;

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
  /** The label above each channel column. */
  std::array<juce::Rectangle<int>, numChannelColumns> channelLabels;
  /** The row captions down the side: freq, Q, 3d. */
  std::array<juce::Rectangle<int>, numChannelRows> channelRowLabels;

  /** The Elevation section's side-view sphere. */
  juce::Rectangle<int> elevationGraphic;
  /** The Shape section's pictogram and the name under it. */
  juce::Rectangle<int> trajectoryIcon;
  juce::Rectangle<int> trajectoryName;
  /** The bar's own header row: "Slot N" left, the readout right. */
  juce::Rectangle<int> slotLabel;
  juce::Rectangle<int> readout;

  /** The global strip's three action buttons — Menu, Rec, Tap. Device-wide
   *  functions that the hardware has its own keys for; these are the way to
   *  them with a finger. Beside `controls`, not in it: no encoder reaches
   *  them, so they are not sub-elements of the section. */
  /** Where a section's dropdown opens: that section's content area, which
   *  the open list takes over. It cannot open outside the bar —
   *  MotionComponent's GL context composites above anything drawn over it. */
  std::array<juce::Rectangle<int>, numClipSettingsSections> dropdownArea;

  /** One height for every button in the bar, whatever section it is in.
   *  Buttons that sized themselves to their own cell came out three
   *  different heights in three sections. */
  int buttonHeight = 0;

  juce::Rectangle<int> recModeButton;
  juce::Rectangle<int> clockModeButton;
  juce::Rectangle<int> menuButton;
  juce::Rectangle<int> recButton;
  juce::Rectangle<int> tapButton;
};

/** Lays the whole bar out for the given bounds and the three sizes the
 *  user can actually change (header and body font size, Pot Size). Reads
 *  no theme of its own, so it can be checked at sizes nobody has dialled
 *  in yet. */
ClipSettingsLayout layOutClipSettings (juce::Rectangle<int> bounds,
                                       float headerSize, float bodySize,
                                       float potSizeScale);

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
