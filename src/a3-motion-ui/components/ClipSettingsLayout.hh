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

/** Four sections for the clip, then the global strip. */
constexpr int numClipSettingsSections = 5;

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

  /** The Elevation section's side-view sphere. */
  juce::Rectangle<int> elevationGraphic;
  /** The Shape section's pictogram and the name under it. */
  juce::Rectangle<int> trajectoryIcon;
  juce::Rectangle<int> trajectoryName;
  /** The bar's own header row: "Slot N" left, the readout right. */
  juce::Rectangle<int> slotLabel;
  juce::Rectangle<int> readout;
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
