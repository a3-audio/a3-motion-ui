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

#include <a3-motion-ui/components/BrowserLayout.hh>
#include <a3-motion-ui/components/TouchControl.hh>
#include <a3-motion-ui/theme/ThemedComponent.hh>

#include <vector>

namespace a3
{

/** The browser page: the device's eight clips down one side, the library down
 *  the other.
 *
 *  Knows nothing about where patterns come from -- it is handed a list of
 *  names and hands back which row was chosen. What that means is the
 *  orchestrator's business, the same way the pads page knows nothing about
 *  what a pad does.
 */
class BrowserComponent : public juce::Component, public ThemedComponent
{
public:
  BrowserComponent ();
  ~BrowserComponent () override;

  void paint (juce::Graphics &g) override;
  void resized () override;
  void applyTheme () override;

  /** What each field holds, and the colour of the channel it belongs to. */
  void setField (index_t channel, index_t slot, juce::String const &name,
                 juce::Colour colour);
  /** Which field a chosen clip would land in, or -1 for none. */
  void setSelectedField (int channel, int slot);
  /** Whether this field's slot has drifted from its clip -- the same mark the
   *  slot keys in the header carry, meaning the same thing. */
  void setFieldDrifted (index_t channel, index_t slot, bool drifted);

  /** The library, and where the window onto it starts. */
  void setEntries (juce::StringArray const &names);
  void setScrollOffset (int firstRow);
  int getScrollOffset () const { return _scrollOffset; }
  int getVisibleRows () const { return _layout.visibleRows; }
  int getNumEntries () const { return _names.size (); }

  /** Which library row is highlighted -- what rename acts on. */
  void setSelectedEntry (int index);
  int getSelectedEntry () const { return _selectedEntry; }

  /** The set that is loaded, shown over the eight clips it filled. */
  void setSessionName (juce::String const &name);

  std::function<void ()> onSessionPressed;
  std::function<void (index_t channel, index_t slot)> onFieldChosen;
  std::function<void (int index)> onEntryChosen;
  std::function<void ()> onRenamePressed;
  /** Write the chosen field's clip back. Sessions get their own pair of keys
   *  when sessions exist -- see the plan's step 3. */
  std::function<void ()> onSavePressed;
  std::function<void ()> onSaveSessionPressed;
  std::function<void ()> onLoadSessionPressed;
  std::function<void (int delta)> onScrolled;

private:
  void paintField (juce::Graphics &g, index_t channel, index_t slot);
  void paintRow (juce::Graphics &g, int row);
  void paintButton (juce::Graphics &g, juce::Rectangle<int> bounds,
                    juce::String const &label, bool enabled);

  void mouseWheelMove (juce::MouseEvent const &event,
                       juce::MouseWheelDetails const &wheel) override;

  BrowserLayout _layout;

  std::array<std::array<juce::String, numPadSlots>, numChannelColumns> _fieldNames;
  std::array<std::array<juce::Colour, numPadSlots>, numChannelColumns> _fieldColours;
  std::array<std::array<bool, numPadSlots>, numChannelColumns> _fieldDrifted{};
  int _selectedChannel = 0;
  int _selectedSlot = 0;

  juce::String _sessionName;
  juce::StringArray _names;
  int _scrollOffset = 0;
  int _selectedEntry = -1;

  std::array<std::array<std::unique_ptr<TouchControl>, numPadSlots>,
             numChannelColumns>
      _fieldTouch;
  std::vector<std::unique_ptr<TouchControl>> _rowTouch;
  std::unique_ptr<TouchControl> _sessionTouch;
  std::unique_ptr<TouchControl> _renameTouch;
  std::unique_ptr<TouchControl> _saveTouch;
  std::unique_ptr<TouchControl> _loadTouch;
};

}
