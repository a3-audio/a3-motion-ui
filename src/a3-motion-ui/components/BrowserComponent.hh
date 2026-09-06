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

/** Which of the three folders the browser is listing.
 *
 *  It lived inside A3MotionUIComponent, which is what *decides* the list; this
 *  is what shows it, and a page that draws three tabs cannot be handed a bool
 *  saying which of two it is on. */
enum class BrowserList
{
  Clips,
  /** Action clips -- what ACT does to a slot. Structurally a clip with no
   *  shape: a set of settings, kept in actions/ rather than clips/ because
   *  what it is for is different even though what it holds is the same. */
  Actions,
  /** The arrangement of all eight clips at once. */
  Sessions,
};

/** The browser page: the library, filling whatever clip the bar is showing.
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

  /** The rows of the library list.
   *
   *  @param settingsOnly  Parallel to @p names: a row that changes how a slot
   *                       is played without touching what it plays. Marked,
   *                       because such a row does something different from
   *                       the one above it and a list you have to read to
   *                       tell them apart is a list you cannot use in the
   *                       dark. Empty means no row is one.
   */
  void setEntries (juce::StringArray const &names,
                   std::vector<bool> const &settingsOnly = {});
  void setScrollOffset (int firstRow);
  int getScrollOffset () const { return _scrollOffset; }
  int getVisibleRows () const { return _layout.visibleRows; }
  int getNumEntries () const { return _names.size (); }
  /** The name in a row, or empty for a row that is not there. */
  juce::String entryName (int index) const
  {
    return juce::isPositiveAndBelow (index, _names.size ()) ? _names[index]
                                                           : juce::String{};
  }

  /** Which library row is highlighted -- what rename acts on. */
  void setSelectedEntry (int index);
  int getSelectedEntry () const { return _selectedEntry; }

  /** Which folder the tabs and the list are showing. */
  void setShowingList (BrowserList list);

  /** Which folder the list should show. One callback per tab rather than one
   *  carrying the choice: the tabs are three separate things a finger lands
   *  on, and the page that decides what each means is the one that owns them. */
  std::function<void ()> onClipsChosen;
  std::function<void ()> onActionsChosen;
  std::function<void ()> onSetsChosen;
  std::function<void (int index)> onEntryChosen;
  /** Say what just happened, in the list rather than in the status bar.
   *
   *  The bar's readout is at the top of a thousand pixels and the keys are at
   *  the bottom: a word up there is a word nobody standing over the keys
   *  reads. This shows over the foot of the list, a finger's width from the
   *  key that was pressed, and takes itself away again. */
  void showMessage (juce::String const &text);

  /** What the three keys under the list say, and which of them can be
   *  pressed. Driven from outside rather than fixed here: a key that is drawn
   *  as though it worked and does nothing is worse than one that is plainly
   *  not available, and the set of things you can do changes with what is
   *  chosen. An empty label draws no key at all. */
  void setActions (juce::StringArray const &labels,
                   std::array<bool, 5> const &enabled);

  /** Narrow what the list shows. What the states are and what they are
   *  called is the page's, not this component's -- it draws the word it is
   *  given and says a key was pressed. */
  std::function<void ()> onFilterPressed;
  std::function<void ()> onRenamePressed;
  /** Write the clip on show back to its file, or keep the current action or
   *  set -- what it means follows the folder the list is on. */
  std::function<void ()> onSavePressed;
  /** Write what is on show to a new file rather than over the one it came
   *  from. What "on show" means is the page's to decide. */
  std::function<void ()> onSaveAsPressed;
  /** Throw the chosen row's file away. What that takes is the page's to
   *  decide, and so is asking twice: this fires on every press, armed or
   *  not. */
  std::function<void ()> onDeletePressed;
  std::function<void (int delta)> onScrolled;

  /** Type a new name over the chosen row.
   *
   *  In the row rather than in a field somewhere else: what you are renaming
   *  is a row of a list, and a name typed anywhere but where the name is
   *  makes you look in two places to see whether you got it right. */
  void beginRename (juce::String const &name);
  void cancelRename ();
  /** Keep what has been typed, as Enter does. The key under the list says
   *  "Keep" while a row is open, so there are two ways to finish and neither
   *  is a keyboard nobody can see. */
  void commitRename ();
  bool isRenaming () const { return _renaming; }
  /** Enter was pressed on a name that is not the one it started as. */
  std::function<void (juce::String const &name)> onRenamed;
  /** The row was opened or closed for typing -- the page above shows and
   *  hides the system keyboard on it, the way it does for the script
   *  editor. */
  std::function<void (bool editing)> onRenameEditingChanged;

private:
  void paintRow (juce::Graphics &g, int row);
  void paintMessage (juce::Graphics &g);
  void paintButton (juce::Graphics &g, juce::Rectangle<int> bounds,
                    juce::String const &label, bool enabled);

  void mouseWheelMove (juce::MouseEvent const &event,
                       juce::MouseWheelDetails const &wheel) override;

  bool keyPressed (juce::KeyPress const &key) override;
  void focusLost (FocusChangeType cause) override;

  void endRename (bool keep);

  BrowserLayout _layout;

  juce::StringArray _actionLabels{ "", "Rename", "Save", "", "" };
  std::array<bool, 5> _actionEnabled{ false, false, false, false, false };
  juce::StringArray _names;
  int _scrollOffset = 0;
  int _selectedEntry = -1;
  std::vector<bool> _settingsOnly;
  /** Which of the three words over the list is lit. */
  BrowserList _list = BrowserList::Clips;

  std::vector<std::unique_ptr<TouchControl>> _rowTouch;
  std::unique_ptr<TouchControl> _clipsTabTouch;
  std::unique_ptr<TouchControl> _actionsTabTouch;
  std::unique_ptr<TouchControl> _setsTabTouch;
  std::unique_ptr<TouchControl> _filterTouch;
  std::unique_ptr<TouchControl> _renameTouch;
  std::unique_ptr<TouchControl> _saveTouch;
  std::unique_ptr<TouchControl> _saveAsTouch;
  std::unique_ptr<TouchControl> _deleteTouch;

  juce::String _message;
  /** Long enough to read a short word without looking away from the keys,
   *  short enough not to still be there next time you glance down. */
  static constexpr int messageMillis = 1600;
  int _messageGeneration = 0;

  bool _renaming = false;
  juce::String _renameText;
  juce::String _renameWas;
};

}
