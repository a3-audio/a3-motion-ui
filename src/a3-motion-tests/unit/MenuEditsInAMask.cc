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

// "es ist schwierig im main menu werte zu ändern … kein edit ohne
// eingabemaske, das kollidiert mit scroll." (2026-09-17)
//
// On every menu page: a drag scrolls and never edits, a tap selects and does
// nothing else, a double tap or Enter opens the mask for that row, and only a
// mask changes a value. See .claude/notes/2026-09-17-menu-eingabemaske.md in
// the workspace.

#include <gtest/gtest.h>

#include <ShippedSkin.hh>

#include <a3-motion-ui/components/FingerLatch.hh>
#include <a3-motion-ui/components/GlobalSettingsComponent.hh>
#include <a3-motion-ui/components/SkinEditorComponent.hh>
#include <a3-motion-ui/components/TouchControl.hh>
#include <a3-motion-ui/theme/SkinParameters.hh>

using namespace a3;

namespace
{
juce::var
manyNumbers ()
{
  juce::String json = "{\"sphereScale\": 0.62";
  for (int i = 0; i < 40; ++i)
    json << ", \"probe" << i << "\": " << i;
  json << "}";
  return juce::JSON::parse (json);
}

/** The editor's own hit areas for a row, as a finger would reach them:
 *  name first, value second. */
struct RowControls
{
  TouchControl *name = nullptr;
  TouchControl *value = nullptr;
};

RowControls
controlsFor (juce::Component &parent, int row)
{
  RowControls found;
  for (auto *child : parent.getChildren ())
    {
      auto *control = dynamic_cast<TouchControl *> (child);
      // The list's scroll area behind the rows carries identity 0 as well,
      // and has no tap: only a row's own hit areas answer to one.
      if (control == nullptr || !control->isVisible ()
          || control->primary () != row || !control->onTap)
        continue;
      if (found.name == nullptr
          || control->getX () < found.name->getX ())
        {
          found.value = found.name;
          found.name = control;
        }
      else
        found.value = control;
    }
  return found;
}

int
rowOf (SkinEditorComponent &editor, juce::String const &path)
{
  for (int row = 0; row < 200; ++row)
    {
      editor.browseRow (row);
      if (editor.browsedPath () == path)
        return row;
    }
  return -1;
}

struct Editor
{
  SkinEditorComponent editor;
  int changes = 0;

  explicit Editor (juce::var skin)
  {
    editor.setSkin (std::move (skin), "probe");
    editor.setBounds (0, 0, 768, 600);
    editor.onValueChanged = [this] { ++changes; };
  }

  double value (juce::String const &path) const
  {
    return skinValue (editor.getSkin (), path);
  }
};
}

TEST (MenuSkinEditor, ATapOnAValueOnlySelectsTheRow)
{
  Editor e (manyNumbers ());
  auto const row = rowOf (e.editor, "sphereScale");
  e.editor.browseRow (row + 1);

  auto controls = controlsFor (e.editor, row);
  ASSERT_NE (controls.value, nullptr);
  controls.value->onTap (row, -1);

  EXPECT_EQ (e.editor.browsedRowIndex (), row);
  EXPECT_FALSE (e.editor.isNaming ());
  EXPECT_FALSE (e.editor.isEditing ()) << "nothing is armed by a tap";
}

TEST (MenuSkinEditor, ADragOnAValueScrollsAndNeverEdits)
{
  Editor e (manyNumbers ());
  auto const row = rowOf (e.editor, "sphereScale");
  e.editor.browseRow (row);
  auto const top = e.editor.firstVisibleRow ();

  auto controls = controlsFor (e.editor, row);
  ASSERT_NE (controls.value, nullptr);
  for (int i = 0; i < 5; ++i)
    controls.value->onDragIncrement (row, -1, 1);

  EXPECT_NEAR (e.value ("sphereScale"), 0.62, 1e-9);
  EXPECT_EQ (e.changes, 0);
  EXPECT_GT (e.editor.firstVisibleRow (), top) << "the drag scrolls the list";
}

TEST (MenuSkinEditor, ADoubleTapOnAValueOpensItsMask)
{
  Editor e (manyNumbers ());
  auto const row = rowOf (e.editor, "sphereScale");

  auto controls = controlsFor (e.editor, row);
  ASSERT_NE (controls.value, nullptr);
  controls.value->onDoubleTap (row, -1);

  EXPECT_TRUE (e.editor.isNaming ());
}

TEST (MenuSkinEditor, EnterOpensTheMaskOfTheSelectedRow)
{
  Editor e (manyNumbers ());
  rowOf (e.editor, "sphereScale");

  EXPECT_TRUE (e.editor.keyPressed (juce::KeyPress (juce::KeyPress::returnKey)));
  EXPECT_TRUE (e.editor.isNaming ());
}

TEST (MenuSkinEditor, EnterInTheMaskKeepsWhatWasTyped)
{
  Editor e (manyNumbers ());
  rowOf (e.editor, "sphereScale");
  e.editor.keyPressed (juce::KeyPress (juce::KeyPress::returnKey));

  for (int i = 0; i < 12; ++i)
    e.editor.backspaceName ();
  for (auto c : { '0', '.', '9' })
    e.editor.typeIntoName ((juce::juce_wchar)c);
  e.editor.keyPressed (juce::KeyPress (juce::KeyPress::returnKey));

  EXPECT_FALSE (e.editor.isNaming ());
  EXPECT_NEAR (e.value ("sphereScale"), 0.9, 1e-9);
}

TEST (MenuSkinEditor, EscapeInTheMaskLeavesTheValueAsItWas)
{
  Editor e (manyNumbers ());
  rowOf (e.editor, "sphereScale");
  e.editor.keyPressed (juce::KeyPress (juce::KeyPress::returnKey));

  for (int i = 0; i < 12; ++i)
    e.editor.backspaceName ();
  e.editor.typeIntoName ((juce::juce_wchar)'9');
  e.editor.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));

  EXPECT_FALSE (e.editor.isNaming ());
  EXPECT_NEAR (e.value ("sphereScale"), 0.62, 1e-9);
}

TEST (MenuSkinEditor, PlusAndMinusInTheMaskChangeTheValueLive)
{
  // Dialling a value while watching the sphere is why a skin is edited on the
  // device at all. It stays -- inside the mask.
  Editor e (manyNumbers ());
  rowOf (e.editor, "sphereScale");
  e.editor.keyPressed (juce::KeyPress (juce::KeyPress::returnKey));

  e.editor.stepTypedNumber (1);
  auto const up = e.value ("sphereScale");
  EXPECT_GT (up, 0.62);
  EXPECT_GT (e.changes, 0) << "live, not on Enter";
  EXPECT_NEAR (e.editor.typedText ().trim ().getDoubleValue (), up, 1e-9)
      << "the field shows what the value now is";

  e.editor.stepTypedNumber (-1);
  e.editor.stepTypedNumber (-1);
  EXPECT_LT (e.value ("sphereScale"), 0.62);
}

TEST (MenuSkinEditor, EscapeAfterPlusPutsTheValueBack)
{
  Editor e (manyNumbers ());
  rowOf (e.editor, "sphereScale");
  e.editor.keyPressed (juce::KeyPress (juce::KeyPress::returnKey));

  e.editor.stepTypedNumber (1);
  e.editor.stepTypedNumber (1);
  auto const changesBefore = e.changes;
  e.editor.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));

  EXPECT_NEAR (e.value ("sphereScale"), 0.62, 1e-9);
  EXPECT_GT (e.changes, changesBefore) << "the sphere has to go back too";
}

TEST (MenuSkinEditor, EscapeRestoresAValueTheSkinFileNeverStated)
{
  // Found on the device: custom-2 does not state alphaActive, so the row
  // shows the theme's 0.95 while the document holds nothing. Plus, then
  // Escape, wrote 0 -- the raw document's answer for a missing key -- and
  // closing the editor would have saved that into the skin file.
  Editor e (juce::JSON::parse (R"({"sphereScale": 0.62})"));
  ASSERT_GE (rowOf (e.editor, "alphaActive"), 0)
      << "the theme's defaults are rows too";
  auto const before = juce::JSON::toString (e.editor.getSkin ());

  e.editor.keyPressed (juce::KeyPress (juce::KeyPress::returnKey));
  ASSERT_TRUE (e.editor.isNaming ());
  e.editor.stepTypedNumber (1);
  e.editor.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));

  EXPECT_EQ (juce::JSON::toString (e.editor.getSkin ()), before)
      << "the document is exactly what it was, missing key and all";
}

TEST (MenuSkinEditor, UpAndDownInANumberMaskAreMinusAndPlus)
{
  Editor e (manyNumbers ());
  rowOf (e.editor, "sphereScale");
  e.editor.keyPressed (juce::KeyPress (juce::KeyPress::returnKey));

  e.editor.keyPressed (juce::KeyPress (juce::KeyPress::upKey));
  EXPECT_GT (e.value ("sphereScale"), 0.62);
  e.editor.keyPressed (juce::KeyPress (juce::KeyPress::downKey));
  e.editor.keyPressed (juce::KeyPress (juce::KeyPress::downKey));
  EXPECT_LT (e.value ("sphereScale"), 0.62);
}

TEST (MenuSkinEditor, TheMaskHasPlusAndMinusKeysToTouch)
{
  Editor e (manyNumbers ());
  rowOf (e.editor, "sphereScale");
  e.editor.keyPressed (juce::KeyPress (juce::KeyPress::returnKey));

  TouchControl *minus = nullptr;
  TouchControl *plus = nullptr;
  for (auto *child : e.editor.getChildren ())
    if (auto *control = dynamic_cast<TouchControl *> (child);
        control != nullptr && control->isVisible () && control->onTap)
      {
        if (control->primary () == SkinEditorComponent::maskMinusKey)
          minus = control;
        if (control->primary () == SkinEditorComponent::maskPlusKey)
          plus = control;
      }
  ASSERT_NE (plus, nullptr);
  ASSERT_NE (minus, nullptr);
  EXPECT_FALSE (plus->getBounds ().isEmpty ());

  plus->onTap (SkinEditorComponent::maskPlusKey, -1);
  EXPECT_GT (e.value ("sphereScale"), 0.62);
  minus->onTap (SkinEditorComponent::maskMinusKey, -1);
  minus->onTap (SkinEditorComponent::maskMinusKey, -1);
  EXPECT_LT (e.value ("sphereScale"), 0.62);
}

TEST (MenuSkinEditor, ATapOnAnActionRowDoesNotFireIt)
{
  // Save, Rename, Delete, Reset: a tap while scrolling must not do any of
  // them. They answer to a double tap or Enter like everything else.
  Editor e (manyNumbers ());
  int fired = 0;
  e.editor.onSave = [&fired] { ++fired; };
  e.editor.onSaveAsNew = [&fired] { ++fired; };
  e.editor.onReset = [&fired] { ++fired; };
  e.editor.onDelete = [&fired] { ++fired; };

  for (int row = 0; row < 5; ++row)
    {
      auto controls = controlsFor (e.editor, row);
      if (controls.name != nullptr)
        controls.name->onTap (row, -1);
      if (controls.value != nullptr)
        controls.value->onTap (row, -1);
    }
  EXPECT_EQ (fired, 0);
  EXPECT_FALSE (e.editor.isNaming ());

  // Row 0 is Save, row 1 Save as new.
  e.editor.browseRow (0);
  e.editor.keyPressed (juce::KeyPress (juce::KeyPress::returnKey));
  EXPECT_EQ (fired, 1) << "Enter fires the selected action";

  auto controls = controlsFor (e.editor, 1);
  ASSERT_NE (controls.name, nullptr);
  controls.name->onDoubleTap (1, -1);
  EXPECT_EQ (fired, 2) << "so does a double tap";
}

// ── The main menu ──────────────────────────────────────────────────────────

namespace
{
struct Menu
{
  GlobalSettingsComponent menu;
  std::vector<int> tapped, opened, browsed, chosen;
  int cancelled = 0;

  explicit Menu (int height = 600)
  {
    GlobalSettingsComponent::Option skin;
    skin.name = "Skin";
    for (auto name : { "a", "b", "c", "d", "e", "f", "g", "h", "i", "j",
                       "k", "l", "m", "n" })
      skin.values.push_back ({ name });
    skin.activeIndex = 2;

    GlobalSettingsComponent::Option editor;
    editor.name = "Skin Editor";
    editor.values.push_back ({ ">" });
    editor.opensSubmenu = true;

    GlobalSettingsComponent::Option sphere;
    sphere.name = "Sphere in Menu";
    sphere.values.push_back ({ "on" });
    sphere.values.push_back ({ "off" });

    menu.setBounds (0, 0, 768, height);
    menu.setOptions ({ skin, editor, sphere });
    menu.onRowTapped = [this] (int i) { tapped.push_back (i); };
    menu.onRowOpened = [this] (int i) { opened.push_back (i); };
    menu.onPickerBrowsed = [this] (int i) { browsed.push_back (i); };
    menu.onPickerChosen = [this] (int i) { chosen.push_back (i); };
    menu.onPickerCancelled = [this] { ++cancelled; };
  }

  bool key (int code)
  {
    return menu.keyPressed (juce::KeyPress (code));
  }
};

std::vector<TouchControl *>
visibleControls (juce::Component &parent)
{
  std::vector<TouchControl *> found;
  for (auto *child : parent.getChildren ())
    if (auto *control = dynamic_cast<TouchControl *> (child);
        control != nullptr && control->isVisible ())
      found.push_back (control);
  return found;
}
}

TEST (MenuMain, ATapOnARowOnlySelectsIt)
{
  Menu m;
  auto controls = controlsFor (m.menu, 2);
  ASSERT_NE (controls.value, nullptr);

  controls.value->onTap (2, -1);

  EXPECT_EQ (m.tapped, std::vector<int>{ 2 });
  EXPECT_TRUE (m.opened.empty ());
  EXPECT_FALSE (m.menu.isPickerOpen ());
}

TEST (MenuMain, ADragOverTheRowsChangesNothing)
{
  Menu m;
  for (auto *control : visibleControls (m.menu))
    if (control->onDragIncrement)
      for (int i = 0; i < 4; ++i)
        control->onDragIncrement (control->primary (), -1, 1);

  EXPECT_TRUE (m.opened.empty ());
  EXPECT_TRUE (m.chosen.empty ());
  EXPECT_FALSE (m.menu.isPickerOpen ());
}

TEST (MenuMain, ADoubleTapOpensTheRow)
{
  Menu m;
  auto controls = controlsFor (m.menu, 1);
  ASSERT_NE (controls.name, nullptr);

  controls.name->onDoubleTap (1, -1);

  EXPECT_EQ (m.opened, std::vector<int>{ 1 });
}

TEST (MenuMain, ArrowsWalkTheRowsAndEnterOpensOne)
{
  Menu m;
  EXPECT_TRUE (m.key (juce::KeyPress::downKey));
  EXPECT_TRUE (m.key (juce::KeyPress::downKey));
  EXPECT_EQ (m.menu.getOptionIndex (), 2);
  EXPECT_EQ (m.tapped.back (), 2) << "the owner hears about the selection";

  EXPECT_TRUE (m.key (juce::KeyPress::returnKey));
  EXPECT_EQ (m.opened, std::vector<int>{ 2 });
}

TEST (MenuMain, TheListOfValuesOpensOnTheActiveOne)
{
  Menu m;
  m.menu.setOptionIndex (0);
  m.menu.openPicker ();

  ASSERT_TRUE (m.menu.isPickerOpen ());
  EXPECT_EQ (m.menu.getSelectedValueIndex (), 2);
}

TEST (MenuMain, TappingAValueInTheListChoosesItAndClosesTheList)
{
  Menu m;
  m.menu.setOptionIndex (0);
  m.menu.openPicker ();

  auto controls = controlsFor (m.menu, 5);
  ASSERT_NE (controls.name, nullptr) << "a hit area for value 5";
  controls.name->onTap (5, -1);

  EXPECT_EQ (m.chosen, std::vector<int>{ 5 });
  EXPECT_FALSE (m.menu.isPickerOpen ());
}

TEST (MenuMain, ArrowsWalkTheValuesAndEnterChoosesOne)
{
  Menu m;
  m.menu.setOptionIndex (0);
  m.menu.openPicker ();

  m.key (juce::KeyPress::downKey);
  EXPECT_EQ (m.browsed, std::vector<int>{ 3 }) << "so the skin can be previewed";
  m.key (juce::KeyPress::returnKey);

  EXPECT_EQ (m.chosen, std::vector<int>{ 3 });
  EXPECT_FALSE (m.menu.isPickerOpen ());
}

TEST (MenuMain, EscapeClosesTheListWithoutChoosing)
{
  Menu m;
  m.menu.setOptionIndex (0);
  m.menu.openPicker ();
  m.key (juce::KeyPress::downKey);

  EXPECT_TRUE (m.key (juce::KeyPress::escapeKey));

  EXPECT_EQ (m.cancelled, 1);
  EXPECT_TRUE (m.chosen.empty ());
  EXPECT_FALSE (m.menu.isPickerOpen ());
}

TEST (MenuMain, ADragInALongListScrollsItAndChoosesNothing)
{
  Menu m (320);
  m.menu.setOptionIndex (0);
  m.menu.openPicker ();
  auto const top = m.menu.pickerFirstVisible ();

  for (auto *control : visibleControls (m.menu))
    if (control->onDragIncrement)
      {
        for (int i = 0; i < 6; ++i)
          control->onDragIncrement (control->primary (), -1, 1);
        break;
      }

  EXPECT_GT (m.menu.pickerFirstVisible (), top);
  EXPECT_TRUE (m.chosen.empty ());
  EXPECT_TRUE (m.menu.isPickerOpen ());
}

// Two fingers scroll a menu list as one: every scrollable hit area on the menu
// pages shares one FingerLatch, or the second finger would scroll it again.
TEST (MenuTwoFingers, EveryListAreaSharesOneLatch)
{
  auto const &latch = FingerLatch::forGroup (FingerLatch::menuList);

  Editor e (manyNumbers ());
  int checked = 0;
  for (auto *child : e.editor.getChildren ())
    if (auto *control = dynamic_cast<TouchControl *> (child);
        control != nullptr && control->onDragIncrement)
      {
        EXPECT_EQ (control->fingerLatch (), &latch);
        ++checked;
      }
  EXPECT_GT (checked, 1);

  Menu m;
  m.menu.setOptionIndex (0);
  m.menu.openPicker ();
  checked = 0;
  for (auto *control : visibleControls (m.menu))
    if (control->onDragIncrement)
      {
        EXPECT_EQ (control->fingerLatch (), &latch);
        ++checked;
      }
  EXPECT_GT (checked, 0);
}

// ── Scrolling with one finger, on the list and beside it ───────────────────
// "ich will mit einem finger sowohl links neben dem menu als auch auf dem menu
// gut scrollen können" (2026-09-17)

namespace
{
TouchControl *
firstRowControl (juce::Component &parent)
{
  for (auto *child : parent.getChildren ())
    if (auto *control = dynamic_cast<TouchControl *> (child);
        control != nullptr && control->isVisible () && control->onTap
        && control->onDragIncrement)
      return control;
  return nullptr;
}
}

TEST (MenuScroll, TheAreaUnderTheFingerStaysThereWhileTheListScrolls)
{
  // A drag keeps going to the component the finger went down on -- as long as
  // that component stays visible. Scrolling re-labels the row areas, and one
  // that came to stand for a heading was hidden, which ended the drag mid-way:
  // on the list, never beside it.
  SkinEditorComponent editor;
  editor.setSkin (shippedSkin (), "default");
  editor.setBounds (0, 0, 768, 600);

  auto *under = firstRowControl (editor);
  ASSERT_NE (under, nullptr);

  for (int step = 0; step < 60; ++step)
    {
      under->onDragIncrement (under->primary (), -1, 1);
      ASSERT_TRUE (under->isVisible ()) << "hidden after " << step + 1
                                        << " rows of scrolling";
    }
}

TEST (MenuScroll, TheSkinEditorsListMovesOneRowPerRowOfFinger)
{
  // A step every 12 px while a row is 34 px tall ran the list three times
  // faster than the finger, a row at a time.
  SkinEditorComponent editor;
  editor.setSkin (shippedSkin (), "default");
  editor.setBounds (0, 0, 768, 600);

  int checked = 0;
  for (auto *child : editor.getChildren ())
    if (auto *control = dynamic_cast<TouchControl *> (child);
        control != nullptr && control->onDragIncrement)
      {
        EXPECT_EQ (control->pixelsPerStep (), editor.rowPitch ());
        ++checked;
      }
  EXPECT_GT (checked, 1);
  EXPECT_GT (editor.rowPitch (), 12);
}

TEST (MenuScroll, TheMainMenusListOfValuesMovesOneRowPerRowOfFinger)
{
  Menu m (320);
  m.menu.setOptionIndex (0);
  m.menu.openPicker ();

  int checked = 0;
  for (auto *control : visibleControls (m.menu))
    if (control->onDragIncrement)
      {
        EXPECT_EQ (control->pixelsPerStep (), m.menu.rowPitch ());
        ++checked;
      }
  EXPECT_GT (checked, 0);
}
