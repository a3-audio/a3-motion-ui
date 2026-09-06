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

#include <a3-motion-ui/io/OnScreenKeyboard.hh>
#include "A3MotionUIComponent.hh"

#include <a3-motion-engine/Envelope.hh>
#include <a3-motion-engine/TempoLfo.hh>
#include <a3-motion-engine/TrajectoryShaping.hh>
#include <a3-motion-engine/TrajectorySpin.hh>

#include <chrono>
#include <fstream>
#include <iostream>

#include <a3-motion-engine/Config.hh>
#include <a3-motion-engine/PlaybackRate.hh>
#include <a3-motion-engine/RecordingSeam.hh>
#include <a3-motion-engine/TrajectoryShape.hh>
#include <a3-motion-engine/RecordingSpans.hh>
#include <a3-motion-ui/components/PatternProgressBar.hh>
#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/PatternFile.hh>
#include <a3-motion-engine/ClipFile.hh>
#include <a3-motion-engine/ClipMigration.hh>
#include <a3-motion-engine/ActionScript.hh>
#include <a3-motion-engine/PatternLibrary.hh>
#include <a3-motion-engine/UserConfig.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-engine/elevation/HeightMap.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>

#include <a3-motion-ui/Config.hh>
#include <a3-motion-ui/Helpers.hh>
#include <a3-motion-ui/components/ChannelStrip.hh>
#include <a3-motion-ui/components/ChannelUIState.hh>
#include <a3-motion-ui/components/FilterDisplay.hh>
#include <a3-motion-ui/components/LayoutHints.hh>
#include <a3-motion-ui/components/LoopLengthDisplay.hh>
#include <a3-motion-ui/components/ElevationDisplay.hh>
#include <a3-motion-ui/components/MotionComponent.hh>
#include <a3-motion-ui/components/PadRowDisplay.hh>
#include <a3-motion-ui/components/GlobalSettingsComponent.hh>
#include <a3-motion-ui/components/ClipSettingsComponent.hh>
#include <a3-motion-ui/components/StatusBar.hh>

#include <a3-motion-ui/tests/TempoEstimatorTest.hh>

#include <a3-motion-ui/io/InputOutputAdapter.hh>
#ifdef HARDWARE_INTERFACE_V2
#include <a3-motion-ui/io/InputOutputAdapterV2.hh>
#endif
#ifdef HARDWARE_INTERFACE_V3
#include <a3-motion-ui/io/InputOutputAdapterV3.hh>
#endif

#include <algorithm>
#include <array>

namespace a3
{

namespace
{
/** The fade in ticks. Sixteenths of a beat are what the panel offers, because
 *  that is a length a musician can hear; ticks are what the pattern counts. */

}


A3MotionUIComponent::A3MotionUIComponent (unsigned int const numChannels)
    : _heightMap (std::make_unique<HeightMapSphere> ()),
      _engine (numChannels, *_heightMap)
{
  setLookAndFeel (&_lookAndFeel);

  _oscMessageHandler = std::make_unique<OscMessageHandler> (_engine, *this);
  applyOscAddresses (userConfig);

  if (runsOnHardware ())
    {
      createHardwareInterface ();
    }

  // Initialize pattern library (creates system/ and user/ dirs if needed)
  // Path is configurable via "patternDir" in config.json.
  auto patternsDir = userConfig.hasProperty ("patternDir")
      ? juce::File (userConfig["patternDir"].toString ())
      : juce::File ("/home/aaa/a3-motion-ui/pattern");
  // Before the library exists, not after: PatternLibrary scans in its own
  // constructor, so a migration that ran afterwards would leave the first
  // start of the day looking at shapes whose clips it had not seen. It showed
  // up in the log as "patterns loaded" printing before "ClipMigration".
  //
  // Non-destructive and idempotent: it runs on every start and does nothing
  // once every take has a clip.
  migrateCombinedPatterns (patternsDir);
  migrateSetToCurrent (patternsDir);

  _patternLibrary = std::make_unique<PatternLibrary> (patternsDir);
  _lastLibraryFingerprint = _patternLibrary->getDirectoryFingerprint ();

  initializePatterns ();
  // What was where last time, on top of the defaults initializePatterns just
  // laid down. A folder with a set and the takes it names is a gig on a stick.
  applySet ();

  createChannelsUI ();
  createMainUI ();
  createPadRowDisplays ();

  // Global Settings (hidden by default). A child of MotionComponent, not of
  // this component: MotionComponent's OpenGL context is attached with
  // component painting enabled, so it draws its own children over the
  // rendered image. That is the only way anything can sit on top of the
  // sphere — and the only way the menu can be see-through and still show
  // the skin it is editing behind it.
  // Editing an address in the menu writes config.json; the watcher picks
  // that up and this puts it in force without a restart.
  _motionComponent->onAppConfigReloaded
      = [this] (juce::var const &config) { applyOscAddresses (config); };

  // Back and close, over whatever is open. A child of MotionComponent like
  // the overlays themselves, so it composites above the GL context.
  _overlayButtons = std::make_unique<OverlayButtons> ();
  _overlayButtons->setAlwaysOnTop (true);
  _overlayButtons->onBack = [this] { toggleGlobalSettings (); };
  _overlayButtons->onClose = [this] { closeAllOverlays (); };
  _motionComponent->addChildComponent (*_overlayButtons);

  // The strips beside whatever page is open. One set for all of them: a page
  // that grows a long list should not have to grow a gesture too.
  _overlayStrips = std::make_unique<OverlaySideStrips> ();
  _overlayStrips->setAlwaysOnTop (true);

  _overlayStrips->onBrowse = [this] (int delta) {
    if (_colourPickerOpen)
      _colourPicker->navigate (delta);
    else if (_skinEditorOpen)
      // Scrolls the page, and does not move the selection: a row is chosen by
      // touching it, which is the one convention every hand in the room
      // already has. The strip used to walk the selection and the list
      // re-centred itself around it, so the row you were reaching for slid
      // away as you reached.
      _skinEditor->scrollList (delta);
    else if (_globalSettingsOpen)
      {
        _globalSettingsValueFieldSelected = false;
        _globalSettings->setValueFieldSelected (false);
        _globalSettings->navigateOption (delta > 0 ? 1 : -1);
        _globalSettingsOptionIndex = _globalSettings->getOptionIndex ();
      }
  };

  _overlayStrips->onValue = [this] (int delta) {
    if (_skinEditorOpen)
      {
        // Arms on the first increment — dragging here already means "change
        // this" — but only a row a drag may turn. Arming an action or a
        // colour row would fire it.
        if (!_skinEditor->isEditing ())
          {
            if (!_skinEditor->canTurnBrowsedRow ())
              return;
            _skinEditor->toggleEditing ();
          }
        _skinEditor->navigate (delta);
        return;
      }

    if (!_globalSettingsOpen)
      return;

    if (!_globalSettingsValueFieldSelected)
      {
        if (_globalSettings->opensSubmenu (_globalSettingsOptionIndex))
          return; // a row that leads somewhere has no value to turn
        _globalSettingsValueFieldSelected = true;
        _globalSettings->setValueFieldSelected (true);
      }

    _globalSettings->navigateValue (delta > 0 ? 1 : -1);

    // Seeing the skin while choosing it is the point of choosing it here.
    if (browsedMenuRow () == std::optional<MenuRow>{ MenuRow::Skin })
      previewSkin (_globalSettings->getSelectedValueIndex ());
  };

  _overlayStrips->onValueReleased = [this] {
    if (_skinEditorOpen)
      {
        if (_skinEditor->isEditing ())
          _skinEditor->toggleEditing ();
        return;
      }

    if (_globalSettingsOpen && _globalSettingsValueFieldSelected)
      confirmGlobalSettingsOption ();
  };

  _motionComponent->addChildComponent (*_overlayStrips);

  _globalSettings = std::make_unique<GlobalSettingsComponent> ();
  _globalSettings->setAlwaysOnTop (true);

  // The same two levels the channel-3 encoder drives, reached with a finger:
  // tapping a name browses, tapping a value field arms, dragging it chooses,
  // and letting go applies — the release standing in for the second press.
  _globalSettings->onRowTapped = [this] (int option) {
    _globalSettingsOptionIndex = option;
    _globalSettings->setOptionIndex (option);
    _globalSettingsValueFieldSelected = false;
    _globalSettings->setValueFieldSelected (false);
  };

  _globalSettings->onValueArmed = [this] (int option) {
    _globalSettingsOptionIndex = option;
    _globalSettings->setOptionIndex (option);

    // A row that leads somewhere has nothing to choose between, so arming it
    // would be a question with one answer: it opens instead.
    if (_globalSettings->opensSubmenu (option))
      {
        _globalSettingsValueFieldSelected = true;
        _globalSettings->setValueFieldSelected (true);
        confirmGlobalSettingsOption ();
        return;
      }

    _globalSettingsValueFieldSelected = true;
    _globalSettings->setValueFieldSelected (true);
  };

  // The strip left of the panel walks the rows; the strip right of it
  // changes the highlighted one. Two places instead of a press that switches
  // between two levels — which is what the encoder had to do, and what a
  // finger should not have to remember.
  _globalSettings->onBrowseDragged = [this] (int delta) {
    _globalSettingsValueFieldSelected = false;
    _globalSettings->setValueFieldSelected (false);
    _globalSettings->navigateOption (delta > 0 ? 1 : -1);
    _globalSettingsOptionIndex = _globalSettings->getOptionIndex ();
  };

  _globalSettings->onValueDragged = [this] (int, int increment) {
    // Arms on the first increment: dragging in the right-hand strip means
    // "change this", so asking for a tap first would be a step that says
    // nothing.
    if (!_globalSettingsValueFieldSelected)
      {
        if (_globalSettings->opensSubmenu (_globalSettingsOptionIndex))
          return; // a row that leads somewhere has no value to turn
        _globalSettingsValueFieldSelected = true;
        _globalSettings->setValueFieldSelected (true);
      }

    _globalSettings->navigateValue (increment > 0 ? 1 : -1);

    // Seeing the skin while choosing it is the point of choosing it here.
    if (browsedMenuRow () == std::optional<MenuRow>{ MenuRow::Skin })
      previewSkin (_globalSettings->getSelectedValueIndex ());
  };

  _globalSettings->onValueReleased = [this] (int) {
    if (!_globalSettingsValueFieldSelected)
      return;
    confirmGlobalSettingsOption ();
  };
  _motionComponent->addChildComponent (*_globalSettings);

  // The editor is a page of that menu and lives in the same place, for the
  // same reason: what it changes is mostly the sphere behind it.
  _skinEditor = std::make_unique<SkinEditorComponent> ();
  _skinEditor->setAlwaysOnTop (true);
  _motionComponent->addChildComponent (*_skinEditor);
  _skinEditor->onValueChanged = [this] { applyEditedSkin (); };
  _skinEditor->onSave = [this] { saveEditedSkin (); };
  _skinEditor->onSaveAsNew = [this] { saveSkinAsNew (); };
  _skinEditor->onRename = [this] (auto const &name) { renameEditedSkin (name); };
  _skinEditor->onReset = [this] { resetEditedSkinToDefault (); };
  _skinEditor->onDelete = [this] { deleteEditedSkin (); };

  // The keyboard is Onboard, the system's own — see io/OnScreenKeyboard.hh.
  // It types into whatever window has the focus, which is this one.
  _skinEditor->onNamingChanged = [this] (bool naming) { showKeyboard (naming); };
  _skinEditor->onColourPicked
      = [this] (auto const &path) { openColourPicker (path); };

  _colourPicker = std::make_unique<ColourPickerComponent> ();
  _colourPicker->setAlwaysOnTop (true);
  _motionComponent->addChildComponent (*_colourPicker);
  _colourPicker->onColourChanged = [this] { applyPickedColour (); };
  _colourPicker->onDone = [this] { closeColourPicker (); };

  // Clip Settings: permanent bottom panel, always visible.
  _clipSettings = std::make_unique<ClipSettingsComponent> ();
  _clipSettings->setAlwaysOnTop (true);

  // A finger reaches the same two handlers the encoders do. Tapping names
  // section and sub-element at once, which is what the Motion-Encoder's
  // scrolling and the Pot-Encoder's press reach one step at a time.
  _clipSettings->onControlTapped = [this] (int section, int sub) {
    // Section first: it resets the sub-index, so naming the sub-element
    // afterwards is what makes the tap land where it was aimed.
    // A tap on a section's free surface names no control (sub < 0). Within
    // the section that is already selected there is then nothing to do —
    // and it has to be caught before selectClipSettingsSection(), which
    // resets the sub-element itself. Going through it anyway armed reach
    // whenever the Elevation graphic — a picture, not a control — was
    // touched.
    if (sub < 0 && section == _clipSettingsMenuIndex)
      return;

    // Section first: it resets the sub-element, so naming the sub-element
    // afterwards is what makes the tap land where it was aimed.
    selectClipSettingsSection (section);
    selectClipSettingsSubElement (juce::jmax (0, sub));
  };
  // The grid's cells name their own channel — unlike everything else in the
  // bar, they are not about the clip on display.
  _clipSettings->onChannelValueDragged
      = [this] (int channel, int row, int increment) {
          handleChannelValueChange (static_cast<index_t> (channel), row,
                                    increment);
        };

  _clipSettings->onRecordLengthChosen = [this] (int index) {
    if (index < 0 || index >= numRecordLengths)
      return;

    // A setting for the next take, not a property of what is in the slot:
    // an existing pattern's length is its tick count, and changing that
    // would throw its data away.
    // The table decides what a length may be, not the speed control's range:
    // they are different settings that happened to share a constant, and 32
    // bars is one step past what speed offers.
    _clipUIParams[_clipSettingsChannel][_clipSettingsSlot].recordLengthLog2
        = recordLengthLog2[index];
    updateClipSettingsDisplay ();
  };

  _clipSettings->onRecModePressed = [this] {
    auto const count = static_cast<int> (recMenuModes.size ());
    applyRecMode ((recMenuIndex (_recMode) + 1) % count);
    updateClipSettingsDisplay ();
  };
  _clipSettings->onClockModePressed = [this] {
    // The same three the menu offers, in the same order.
    applyClockMode ((_clockMode + 1) % 3);
    _clipSettings->setClockMode (_clockMode);
  };
  _clipSettings->onMenuPressed = [this] { toggleGlobalSettings (); };
  _clipSettings->onRecordPressed = [this] {
    // The card turns over as the take is armed, so what you are recording is
    // drawn where what you are playing usually is.
    toggleRecordPage ();
    toggleRecordingOnShownClip ();
  };
  // The bar's four transport keys are the shown clip's pads, reached through
  // the pad handler rather than reimplemented: the timing rules live there
  // (play on the next beat, stop now, the accent while the finger is down)
  // and two routes to one function must not each keep their own copy.
  _clipSettings->onSlotSelected = [this] (index_t slot) {
    selectClip (_clipSettingsChannel, slot);
  };

  // A channel's face means "show me this channel's clip", and touched again
  // on the channel already shown it turns that channel's slot over. Two jobs
  // on one target because they are the same reach: you go to a channel to
  // see it, and once you are there the only other thing to say is which of
  // its two slots.
  //
  // Each channel keeps its own slot, so the same slot can be compared across
  // two channels in one move each. The two keys used to be shared, and
  // choosing slot 2 chose it for whichever channel you happened to be on.
  _clipSettings->onChannelFaceTapped = [this] (index_t channel) {
    // The faces select the clip the settings area is describing, whichever
    // view is open -- CLIP, REC and ACTION are three ways of looking at one
    // clip, and which clip that is is a question they share. A face used to
    // drag ACTION back to CLIP, which meant the one page where you most often
    // want to hear another channel's clip was the one page you could not stay
    // on while choosing it.
    //
    // FILES is here for the same reason. It has a clip in mind too -- the one
    // a picked file is put into -- so choosing the slot and then choosing the
    // file is one errand, and being thrown back to CLIP halfway through it
    // meant tabbing back and losing the list you were reading.
    auto const describesAClip
        = _barPage == BarPage::Clip || _barPage == BarPage::Record
          || _barPage == BarPage::Action || _barPage == BarPage::Browser;

    if (describesAClip && channel == _clipSettingsChannel)
      _channelSlot[channel]
          = static_cast<index_t> ((_channelSlot[channel] + 1) % numPadSlots);

    selectClip (channel, _channelSlot[channel]);

    // From the pads page there is no one clip on show to select into -- it
    // shows every slot at once -- so reaching for a channel there is reaching
    // for its clip, and the face brings that view back with it.
    if (!describesAClip)
      showBarPage (BarPage::Clip);
  };

  _clipSettings->onTransportTapped = [this] (TransportKey key) {
    switch (key)
      {
      case TransportKey::Record:
        toggleRecordPage ();
        toggleRecordingOnShownClip ();
        return;
      case TransportKey::Stop:
        handlePadPress (_clipSettingsChannel,
                        padIndexFor (PadFunction::Stop, _clipSettingsSlot));
        return;
      case TransportKey::PlayPause:
        handlePadPress (
            _clipSettingsChannel,
            padIndexFor (PadFunction::PlayPause, _clipSettingsSlot));
        return;
      case TransportKey::Action:
        return;
      }
  };
  _clipSettings->onTransportActionHeld = [this] (bool held) {
    auto const pad = padIndexFor (PadFunction::Action, _clipSettingsSlot);
    if (held)
      handlePadPress (_clipSettingsChannel, pad);
    else
      handlePadRelease (_clipSettingsChannel, pad);
  };

  _clipSettings->onTapPressed = [this] { handleScreenTap (); };
  // Held, not tapped: Shift+Action previews for as long as it is down. In the
  // global strip it stands on both pages, so it can be held while the other
  // hand works the pads.
  // The bar's ACT plays the shown clip's accent, exactly as its pad does.
  // A speed button is the clip's playback length said plainly. Tapping one
  // sets it outright rather than stepping towards it — that is the point of
  // there being twelve.
  _clipSettings->onSpeedChosen = [this] (int index) {
    if (index < 0 || index >= numSpeedButtons)
      return;

    auto const channel = _clipSettingsChannel;
    auto const slot = _clipSettingsSlot;
    if (auto &chosen = _patterns[channel][slot])
      chosen->setSpeedLog2 (speedButtonLog2[index]);

    if (auto &pattern = _patterns[channel][slot])
      pattern->setPlaybackLength (getPlaybackLength (channel, slot));

    updateClipSettingsDisplay ();
    scheduleSetSave ();
  };

  _clipSettings->onAccentHeld = [this] (bool held) {
    auto const channel = _clipSettingsChannel;
    auto const slot = _clipSettingsSlot;
    if (held)
      _engine.setChannelAction (channel, _slotAction[channel][slot].settings);
    _engine.setChannelAccentHeld (channel, held,
                                  held ? _patterns[channel][slot] : nullptr);
    updateControlReadout (juce::String ("CH") + juce::String (channel + 1)
                          + " ACTION");
  };

  _clipSettings->onShiftHeld = [this] (bool held) {
    _screenShiftHeld = held;
    updateFunctionKeyLEDs ();
    updateControlReadout (juce::String ("-- SHIFT ") + (held ? "ON" : "OFF"));
  };
  // Held, not tapped: Shift+Action previews for as long as it is down. In the
  // global strip it stands on both pages, so it can be held while the other
  // hand works the pads.
  _clipSettings->onShiftHeld = [this] (bool held) {
    _screenShiftHeld = held;
    updateControlReadout (juce::String ("-- SHIFT ") + (held ? "ON" : "OFF"));
  };

  _clipSettings->onControlDragged = [this] (int section, int sub,
                                           int increment) {
    handleClipSettingsValueChange (_clipSettingsChannel, section, sub,
                                   increment);
  };

  _clipSettings->onControlToggled = [this] (int section, int sub) {
    handleClipSettingsToggle (_clipSettingsChannel, section, sub);
  };

  // The elevation graphic sets where the middle of the trajectory sits. An
  // absolute height, not an increment: the graphic shows where things are,
  // so a finger on it means that height.
  _clipSettings->onElevationBaseSet = [this] (float base) {
    auto &pattern = _patterns[_clipSettingsChannel][_clipSettingsSlot];
    if (!pattern)
      return;

    pattern->setElevationBase (base);
    refreshPatternDisplayFromTicks (pattern);
    updateClipSettingsDisplay ();
    scheduleSetSave ();
  };

  _clipSettings->onControlReset = [this] (int section, int sub) {
    handleClipSettingsReset (_clipSettingsChannel, section, sub);
  };

  _clipSettings->onPageSelected
      = [this] (BarPage page) { showBarPage (page); };

  _controller = std::make_unique<ControllerComponent> ();
  _controller->onPadPressed = [this] (index_t channel, index_t pad) {
    handlePadPress (channel, pad);
  };
  _controller->onPadReleased = [this] (index_t channel, index_t pad) {
    handlePadRelease (channel, pad);
  };

  // The ACTION page: what the ACT key does to the clip the bar is showing.
  // Like the pads page, it decides nothing -- a turn arrives here as (control,
  // increment) and is applied to the same Pattern the bar's own knobs write.
  _action = std::make_unique<ActionComponent> ();
  _action->onControlDragged = [this] (int control, int increment) {
    applyActionControl (control, increment);
  };
  _action->onControlDoubleTapped = [this] (int control) {
    resetActionControl (control);
  };
  _action->onControlTapped = [this] (int control) {
    // Only the mode is a tap: the three knobs are turned, and a tap on one
    // would otherwise step it by nothing at all.
    if (control == ActionComponent::ActMode)
      applyActionControl (control, 1);
  };

  _action->onActionChosen = [this] (juce::String const &name) {
    setSlotAction (_clipSettingsChannel, _clipSettingsSlot,
                   name.isEmpty () ? juce::File{}
                                   : actionsDir ().getChildFile (name + ".scd"));
    refreshBrowser ();
  };

  // The keyboard is the system's own, and it types into whatever has the
  // focus -- so opening the editor is what shows it and losing the editor is
  // what takes it away.
  _action->onScriptEditingChanged = [this] (bool editing) {
    showKeyboard (editing);

    // Leaving the editor no longer applies anything -- Save does that, and
    // Cancel puts the text back. Walking away is neither, so what was typed
    // stays in the editor with the edge still marked.
  };

  // Written only on Save, and running the script is the same gesture: half a
  // line is not a script, and a file written from one is worse than no file.
  _action->onScriptSaved = [this] {
    writeSlotActionScript ();
    setSlotAction (_clipSettingsChannel, _clipSettingsSlot,
                   _slotAction[_clipSettingsChannel][_clipSettingsSlot].file);
  };

  // Cancel puts the file's own text back, which is what the slot still holds.
  _action->onScriptCancelled = [this] {
    _action->setScript (
        _slotAction[_clipSettingsChannel][_clipSettingsSlot].source);
  };

  // The browser: the eight clips of the device and the library beside them.
  // What a field or a row means is decided here rather than there, the same
  // way the pads page knows nothing about what a pad does.
  _browser = std::make_unique<BrowserComponent> ();
  // Save writes what is on show back over the file it came from; Save as
  // writes it to a new one. Two keys rather than one and a modifier: which of
  // the two you meant is the whole question, and a modifier makes it something
  // you find out afterwards.
  _browser->onSavePressed = [this] { saveChosen (); };
  _browser->onSaveAsPressed = [this] { saveAsChosen (); };
  _browser->onRenamePressed = [this] {
    // The same key finishes what it started: it says "Keep" while a row is
    // open, so a name can be settled without reaching for a keyboard that is
    // covering half the screen.
    if (_browser->isRenaming ())
      {
        _browser->commitRename ();
        return;
      }

    if (!chosenEntryHasAFile ())
      return;

    _deleteArmed = false;
    _browser->beginRename (
        _browser->entryName (_browser->getSelectedEntry ()));
    refreshBrowser ();
  };

  _browser->onRenamed
      = [this] (juce::String const &name) { renameChosenEntry (name); };

  // The keyboard is the system's own and types into whatever holds the focus,
  // so opening the row is what shows it -- the same arrangement the script
  // editor has.
  _browser->onRenameEditingChanged = [this] (bool editing) {
    showKeyboard (editing);
    refreshBrowser ();
  };

  _browser->onDeletePressed = [this] { deleteChosenEntry (); };

  // Steps through the three on a tap, like every other few-valued control in
  // the bar. The word on the key is the state it is in, not the one the next
  // press would bring -- a key that names what you would get rather than what
  // you have is a key you have to press to find out where you are.
  _browser->onFilterPressed = [this] {
    if (_browserList != BrowserList::Clips)
      return;

    _deleteArmed = false;
    _browser->cancelRename ();

    switch (_clipFilter)
      {
      case ClipFilter::All: _clipFilter = ClipFilter::User; break;
      case ClipFilter::User: _clipFilter = ClipFilter::System; break;
      case ClipFilter::System: _clipFilter = ClipFilter::All; break;
      }

    refreshBrowser ();
  };

  _browser->onClipsChosen = [this] {
    _browserList = BrowserList::Clips;
    _deleteArmed = false;
    _browser->cancelRename ();
    _browser->setSelectedEntry (-1);
    refreshBrowser ();
  };
  _browser->onActionsChosen = [this] {
    _browserList = BrowserList::Actions;
    _deleteArmed = false;
    _browser->cancelRename ();
    _browser->setSelectedEntry (-1);
    refreshBrowser ();
  };
  _browser->onSetsChosen = [this] {
    _browserList = BrowserList::Sessions;
    _deleteArmed = false;
    _browser->cancelRename ();
    _browser->setSelectedEntry (-1);
    refreshBrowser ();
  };
  _browser->onEntryChosen = [this] (int index) {
    // Anything else you do puts the delete key back to sleep. An armed key
    // you have forgotten about is worse than no key at all.
    _deleteArmed = false;
    _browser->cancelRename ();
    _browser->setSelectedEntry (index);

    if (_browserList == BrowserList::Sessions)
      loadSessionNamed (_browser->entryName (index));
    else if (_browserList == BrowserList::Actions)
      assignActionEntry (_browser->entryName (index));
    else
      assignBrowserEntry (libraryForBrowserRow (index));
  };

  addChildComponent (*_clipSettings);
  _clipSettings->setVisible (true);

  // A child of the bar, not a sibling of it. The bar fills its whole area
  // with the surface colour at 85% (panelOpacity), so a sibling drawn under
  // that came through at fifteen percent of itself — a page whose job is
  // showing which clip is running, showing it in the dark. A child is painted
  // after its parent by construction, and no z-order call can undo that.
  _clipSettings->addChildComponent (*_controller);
  _clipSettings->addChildComponent (*_action);
  _clipSettings->addChildComponent (*_browser);
  selectClip (0, 0); // sensible default before any button has been pressed

  // Clockmode is all that is left to restore. Pot Size and the two font sizes
  // are the skin's now, and the skin brings its own.
  auto const persisted = loadSettings (getPersistedSettingsFile ());
  applyClockMode (persisted.clockMode);
  _recMode = persisted.recMode;
  _engine.setRecMode (_recMode);


  // Fast enough for the write head to move while a take runs; the directory
  // check inside keeps its old two-second pace by counting ticks.
  startTimer (50);

  _engine.addPatternStatusListener (this);
  _tickCallbackHandle = _engine.getTempoClock ().scheduleEventHandlerAddition (
      [this] (auto measure) { tickCallback (measure); },
      TempoClock::Event::Tick, TempoClock::Execution::JuceMessageThread);

  auto constexpr testTempoEstimation = false;
  if (testTempoEstimation)
    {
      _tempoEstimatorTest = std::make_unique<TempoEstimatorTest> ();
      _ioAdapter->getTapTimeMicros ().addListener (_tempoEstimatorTest.get ());
    }

  // Setup OSC Receiver from config
  int oscRecvPort = 7771; // default
  juce::String oscRecvHost = "0.0.0.0";
  if (userConfig.hasProperty ("ui"))
    {
      auto const uiConfig = userConfig["ui"];
      if (uiConfig.hasProperty ("pauseRenderingInMenu"))
        _pauseRenderingInMenu
            = static_cast<bool> (uiConfig["pauseRenderingInMenu"]);
    }

  if (userConfig.hasProperty ("oscReceiver"))
    {
      auto oscRecvConfig = userConfig["oscReceiver"];
      if (oscRecvConfig.hasProperty ("port"))
        oscRecvPort = static_cast<int> (oscRecvConfig["port"]);
      if (oscRecvConfig.hasProperty ("host"))
        oscRecvHost = oscRecvConfig["host"].toString ();
    }
  if (_oscReceiver.connect (oscRecvPort))
    {
      std::cout << "OSC Receiver listening on " << oscRecvHost << ":" << oscRecvPort << std::endl;
      _oscReceiver.addListener (this);
    }
  else
    {
      std::cerr << "ERROR: Could not bind OSC Receiver to port " << oscRecvPort << std::endl;
    }

  // Setup OSC Receiver for VU meters (separate port)
  int oscVuPort = 7772; // default
  if (userConfig.hasProperty ("oscReceiver"))
    {
      auto oscRecvConfig = userConfig["oscReceiver"];
      if (oscRecvConfig.hasProperty ("vuPort"))
        oscVuPort = static_cast<int> (oscRecvConfig["vuPort"]);
    }
  if (_oscReceiverVU.connect (oscVuPort))
    {
      std::cout << "OSC VU Receiver listening on " << oscRecvHost << ":" << oscVuPort << std::endl;
      _oscReceiverVU.addListener (this);
    }
  else
    {
      std::cerr << "ERROR: Could not bind OSC VU Receiver to port " << oscVuPort << std::endl;
    }

  // Setup OSC Receiver for the IEM EnergyVisualizer (separate port again —
  // it sends 426 floats at 9 Hz and has no business sharing a socket with the
  // beat clock).
  int oscEnergyPort = 7777; // default
  if (userConfig.hasProperty ("oscReceiver"))
    {
      auto oscRecvConfig = userConfig["oscReceiver"];
      if (oscRecvConfig.hasProperty ("energyPort"))
        oscEnergyPort = static_cast<int> (oscRecvConfig["energyPort"]);
    }
  if (_oscReceiverEnergy.connect (oscEnergyPort))
    {
      std::cout << "OSC Energy Receiver listening on " << oscRecvHost << ":" << oscEnergyPort << std::endl;
      _oscReceiverEnergy.addListener (this);
    }
  else
    {
      std::cerr << "ERROR: Could not bind OSC Energy Receiver to port " << oscEnergyPort << std::endl;
    }

  // Setup OSC Sender from config (for beatclock)
  if (userConfig.hasProperty ("oscSender"))
    {
      auto oscSendConfig = userConfig["oscSender"];
      juce::String oscSendHost = oscSendConfig["host"].toString ();
      // Use beatclockPort if specified, otherwise fall back to port
      int oscSendPort = static_cast<int> (oscSendConfig["port"]);
      if (oscSendConfig.hasProperty ("beatclockPort"))
        oscSendPort = static_cast<int> (oscSendConfig["beatclockPort"]);
      if (_oscSender.connect (oscSendHost, oscSendPort))
        std::cout << "OSC Sender for beatclock connected to " << oscSendHost << ":" << oscSendPort << std::endl;
      else
        std::cerr << "ERROR: OSC Sender failed to connect to " << oscSendHost << ":" << oscSendPort << std::endl;
      
      // Direct tap sender (same host/port, bypasses async queue for zero latency)
      if (_tapSender.connect (oscSendHost, oscSendPort))
        std::cout << "OSC Tap Sender connected to " << oscSendHost << ":" << oscSendPort << std::endl;
      else
        std::cerr << "ERROR: OSC Tap Sender failed to connect" << std::endl;
    }
}

A3MotionUIComponent::~A3MotionUIComponent ()
{
  stopTimer ();
  _oscReceiverEnergy.removeListener (this);
  _oscReceiverEnergy.disconnect ();
  _oscReceiverVU.removeListener (this);
  _oscReceiverVU.disconnect ();
  _oscReceiver.removeListener (this);
  _oscReceiver.disconnect ();
  _oscSender.disconnect ();
  _tapSender.disconnect ();

  if (runsOnHardware ())
    {
      _ioAdapter->stopThread (-1);
    }

  _engine.removePatternStatusListener (this);
  setLookAndFeel (nullptr);
}

void
A3MotionUIComponent::createChannelsUI ()
{
  auto const numChannels = _engine.getNumChannels ();

  _channelUIStates.reserve (numChannels);
  _channelStrips.reserve (numChannels);

  for (auto channel = 0u; channel < numChannels; ++channel)
    {
      auto uiState = std::make_unique<ChannelUIState> ();

      // Straight from the theme, which has already parsed the skin file. This
      // used to read `channels` out of the skin a second time, by hand, with a
      // grey fallback of its own — two parsers for one array, and only one of
      // them knew what a channel's colour is when the skin omits it. The
      // second parse went; the skin it was loading for went with it.
      if (static_cast<int> (channel) < numThemeChannels)
        uiState->colour = toColour (theme ().channel[channel]);
      else
        {
          // Fallback: generate colours from HSV
          auto const hueNorm
              = static_cast<float> (channel) / numChannels;
          auto hue = hueNorm / 360.f * 256.f;
          uiState->colour = juce::Colour::fromHSV (hue, 0.6f, 0.8f, 1.f);
        }

      auto strip = std::make_unique<ChannelStrip> (*uiState);
      addChildComponent (*strip);
      strip->setVisible (true);

      _channelStrips.push_back (std::move (strip));
      _channelUIStates.push_back (std::move (uiState));
    }

  _previewHeldPad = std::vector<int> (numChannels, -1);
}

float
A3MotionUIComponent::getPatternLengthBeats (index_t channel, index_t slot) const
{
  // What the pattern itself is, which every pattern file already states as
  // data-beats — the shipped shapes carry 4, 8, 16 and 32. Derived from the
  // tick count the way PatternFile derives it when saving; both rely on
  // MotionEngine::recordingSamplesPerTick being 1, and would need to divide it
  // out together if that ever changes.
  auto const &pattern = _patterns[channel][slot];
  if (pattern && pattern->getNumTicks () > 0)
    return static_cast<float> (pattern->getNumTicks ())
           / static_cast<float> (TempoClock::getTicksPerBeat ());

  return defaultPatternLengthBeats;
}

float
A3MotionUIComponent::getLengthBeats (index_t channel, index_t slot) const
{
  // The pattern's own length, taken at this clip's rate. Speed used to *be*
  // the length and the pattern's own was ignored, so turning the knob
  // redefined how long a take had been after the fact.
  auto const &pattern = _patterns[channel][slot];
  return playbackLengthBeats (getPatternLengthBeats (channel, slot),
                              pattern ? pattern->getSpeedLog2 () : 0);
}

void
A3MotionUIComponent::applyMotionMode (index_t channel, index_t slot)
{
  auto &pattern = _patterns[channel][slot];
  if (!pattern)
    return;

  auto const &params = _clipUIParams[channel][slot];
  pattern->setPlayDirection (params.direction == 1 ? PlayDirection::Reverse
                                                   : PlayDirection::Forward);

  // The bar's order is the engine's order; a value out of range would be a
  // list that grew in one place and not the other.
  auto const actions = { EndAction::Loop, EndAction::Stop, EndAction::Pause,
                         EndAction::Bounce, EndAction::Random };
  if (params.endAction >= 0
      && params.endAction < static_cast<int> (actions.size ()))
    pattern->setEndAction (*(actions.begin () + params.endAction));
}

Measure
A3MotionUIComponent::getPlaybackLength (index_t channel, index_t slot) const
{
  auto const ticks = playbackLengthTicks (
      getLengthBeats (channel, slot),
      static_cast<index_t> (TempoClock::getTicksPerBeat ()));

  return Measure{ 0, 0, static_cast<int> (ticks) }.consolidate (
      _engine.getBeatsPerBar ());
}

void
A3MotionUIComponent::createMainUI ()
{
  // Seeded before the bar reads it: the value was only ever written on a tap
  // or a mode change, so until somebody tapped, the internal reading had no
  // tempo to show at all.
  _valueBPM = static_cast<double> (_engine.getTempoBPM ());

  _statusBar = std::make_unique<StatusBar> (_valueBPM);
  _statusBar->onKeyboardIconTapped = [this] { toggleKeyboard (); };
  addChildComponent (*_statusBar);
  _statusBar->setVisible (true);
  _statusBarCallbackHandle
      = _engine.getTempoClock ().scheduleEventHandlerAddition (
          [this] (auto measure) {
            _statusBar->beatCallback (measure);

            // The TAP key breathes with the beat, on the screen and on the
            // panel, from the one place that knows a beat went by. It was
            // taken out once for being too loud; it is a faint wash now, not
            // the flash a press makes.
            if (_clipSettings)
              _clipSettings->pulseTapOnBeat ();
            pulseTapLED ();
          },
          TempoClock::Event::Beat, TempoClock::Execution::JuceMessageThread);

  _motionComponent
      = std::make_unique<MotionComponent> (_engine, _channelUIStates);
  addChildComponent (*_motionComponent);
  _motionComponent->setVisible (true);

  // Hidden: no longer part of the visible layout (see resized()), but keeps
  // receiving its normal update calls underneath.
  _filterDisplay = std::make_unique<FilterDisplay> ();
  addChildComponent (*_filterDisplay);
  _filterDisplay->setVisible (false);
  for (auto ch = 0u; ch < _channelUIStates.size () && ch < FilterDisplay::numChannels; ++ch)
    _filterDisplay->setChannelColour (static_cast<int> (ch), _channelUIStates[ch]->colour);

  // Hidden: see comment on _filterDisplay above.
  _loopLengthDisplay = std::make_unique<LoopLengthDisplay> ();
  addChildComponent (*_loopLengthDisplay);
  _loopLengthDisplay->setVisible (false);
  _loopLengthDisplay->setReferenceBeats (
      _engine.getBeatsPerBar ());
  for (auto ch = 0u; ch < _channelUIStates.size () && ch < LoopLengthDisplay::numChannels; ++ch)
    {
      _loopLengthDisplay->setChannelColour (static_cast<int> (ch), _channelUIStates[ch]->colour);
      _loopLengthDisplay->setLoopLengthBeats (static_cast<int> (ch),
                                              getLengthBeats (ch, 0));
    }

  // Hidden: see comment on _filterDisplay above.
  _elevationDisplay = std::make_unique<ElevationDisplay> ();
  addChildComponent (*_elevationDisplay);
  _elevationDisplay->setVisible (false);
  for (auto ch = 0u; ch < _channelUIStates.size () && ch < ElevationDisplay::numChannels; ++ch)
    {
      _elevationDisplay->setChannelColour (static_cast<int> (ch), _channelUIStates[ch]->colour);
      // Coverage is per-clip now (see ClipSettingsComponent), not
      // per-channel — this hidden legacy display just keeps its own
      // built-in default.
    }
}

constexpr bool
A3MotionUIComponent::runsOnHardware ()
{
#if HARDWARE_INTERFACE_ENABLED
  return true;
#else
  return false;
#endif
}

void
A3MotionUIComponent::createHardwareInterface ()
{
#if HARDWARE_INTERFACE_ENABLED
#ifdef HARDWARE_INTERFACE_V2
  _ioAdapter = std::make_unique<InputOutputAdapterV2> ();
#elif defined(HARDWARE_INTERFACE_V3)
  _ioAdapter = std::make_unique<InputOutputAdapterV3> ();
#else
#error hardware interface enabled but no implementation selected!
#endif
  _ioAdapter->getButton (Button::ClockMode).addListener (this);
  _ioAdapter->getButton (Button::Menu).addListener (this);
  _ioAdapter->getButton (Button::Record).addListener (this);
  _ioAdapter->getButton (Button::Tap).addListener (this);
  _ioAdapter->getButton (Button::Shift).addListener (this);
  _ioAdapter->getButton (Button::RecMode).addListener (this);
  _ioAdapter->getTapTimeMicros ().addListener (this);
  for (auto channel = 0u; channel < _ioAdapter->getNumChannels (); ++channel)
    {
      for (auto pad = 0u; pad < _ioAdapter->getNumPadsPerChannel (); ++pad)
        {
          _ioAdapter->getPad (channel, pad).addListener (this);
        }

      _ioAdapter->getPot (channel, 0).addListener (this);
      _ioAdapter->getPot (channel, 1).addListener (this);
      _ioAdapter->getEncoderIncrement (channel).addListener (this);
      _ioAdapter->getEncoderPress (channel).addListener (this);
      _ioAdapter->getEncoderIncrement (channel, 1).addListener (this);
      _ioAdapter->getEncoderPress (channel, 1).addListener (this);
    }

  // The panel's four physical potentiometers, one per channel — the third
  // per-channel value ("3d"). Registered outside the per-channel loop
  // because they are global on the hardware, not per channel: V3 reads four
  // of them in one frame.
  for (index_t pot = 0; pot < _ioAdapter->getNumGlobalPots (); ++pot)
    _ioAdapter->getGlobalPot (pot).addListener (this);

  _ioAdapter->startThread ();
  blankLEDs ();
#endif
}

void
A3MotionUIComponent::initializePatterns ()
{
  auto const numChannels = runsOnHardware ()
                               ? _ioAdapter->getNumChannels ()
                               : _engine.getNumChannels ();
  _patterns.resize (numChannels);
  _clipUIParams.resize (numChannels);
  _slotClipFile.resize (numChannels);

  for (auto &channelPatterns : _patterns)
    channelPatterns.resize (numClipSlots);
  for (auto &channelParams : _clipUIParams)
    channelParams.resize (numClipSlots);
  // Sized alongside _patterns, always: fillSlotFromLibrary() indexes both, so
  // one shorter than the other is a slot that cannot say where it came from.
  for (auto &channelClips : _slotClipFile)
    channelClips.resize (numClipSlots);

  _slotAction.resize (numChannels);
  for (auto &channelActions : _slotAction)
    channelActions.resize (numClipSlots);

  // Load patterns from the library, one per clip slot; channels share the
  // same library slot but each gets its own Pattern instance. Default
  // shape is "Square" (name lookup rather than a raw library index, so it
  // doesn't depend on alphabetical file ordering).
  auto const numLibEntries = _patternLibrary->getNumEntries (); // includes Empty at 0
  auto const defaultLibIndex = _patternLibrary->indexForName ("Square");
  for (auto channel = 0u; channel < numChannels; ++channel)
    {
      for (auto slot = 0u; slot < numClipSlots; ++slot)
        {
          auto const libIndex = defaultLibIndex + static_cast<int> (slot);
          if (libIndex > 0 && libIndex < numLibEntries)
            {
              fillSlotFromLibrary (channel, slot, libIndex);
            }
        }
    }
}

void
A3MotionUIComponent::blankLEDs ()
{
  updateFunctionKeyLEDs ();

  for (auto channel = 0u; channel < _ioAdapter->getNumChannels (); ++channel)
    {
      for (auto pad = 0u; pad < _ioAdapter->getNumPadsPerChannel (); ++pad)
        {
          // An LED that is off is not coloured black, it is unlit —
          // transparentBlack is how this codebase says "no colour", and the
          // hardware path reads the rgb, which is zero either way.
          _ioAdapter->getPadLED (channel, pad)
              = juce::VariantConverter<juce::Colour>::toVar (
                  juce::Colours::transparentBlack);
        }
    }
}

void
A3MotionUIComponent::paint (juce::Graphics &g)
{
  juce::ignoreUnused (g);
}

void
A3MotionUIComponent::resized ()
{
  juce::Component::resized ();

  auto bounds = getLocalBounds ();

  // Status bar at the top
  // The bar is as tall as the header size needs; the sphere gets the rest.
  auto const statusBarHeight = _statusBar->preferredHeight ();
  auto boundsStatus = bounds.removeFromTop (statusBarHeight);
  _statusBar->setBounds (boundsStatus);

  // LoopLength/Elevation/PadRow/Filter option bars are hidden (see
  // createMainUI()/createPadRowDisplays()) — they no longer get screen
  // space, but keep receiving their normal update calls under the hood.

  // Hide channel strips - no longer needed after removing width/order displays
  for (auto &strip : _channelStrips)
    strip->setVisible (false);

  // Clip Settings panel: permanent bottom quarter of the screen. Carved out
  // of MotionComponent's actual bounds (not just overlaid) because
  // MotionComponent renders via its own directly-attached OpenGLContext
  // (see MotionComponent.cc), which always composites above normal JUCE
  // components regardless of z-order/toFront()/rendering-paused state, so
  // nothing can visibly overlap it.
  // The bar asks for what its content needs at the current font and pot
  // sizes, and gets it up to a share of the screen. A fixed quarter was what
  // made Font Size inert down there: the height fixed the control boxes, and
  // the boxes capped the text.
  auto const clipSettingsHeight
      = _clipSettings != nullptr
            ? clipSettingsHeightWithin (
                  _clipSettings->preferredHeight (bounds.getWidth ()),
                  bounds.getHeight ())
            : bounds.getHeight () / 4;
  auto boundsClipSettings = bounds.removeFromBottom (clipSettingsHeight);
  if (_clipSettings)
    _clipSettings->setBounds (boundsClipSettings);

  // The bar's clip part, in the bar's own coordinates — it is a child of the
  // bar. The header row and the global strip beside it belong to the bar on
  // both pages, and this paints nothing in the header, so what is drawn there
  // stays visible and its tabs stay reachable.
  if (_controller && _clipSettings)
    _controller->setBounds (_clipSettings->clipContentBounds ());
  if (_action)
    {
      _action->setBounds (_clipSettings->clipContentBounds ());
      // After the bounds, not before: the page subtracts its own origin from
      // this to bring the strip's rows into its own coordinates, so it has to
      // know where it is standing first.
      _action->setGridReference (_clipSettings->globalGridRowsBounds ());
    }
  if (_browser && _clipSettings)
    _browser->setBounds (_clipSettings->clipContentBounds ());

  // The menu covers the sphere and nothing else. It used to take the clip
  // settings' space as well — the bar gave up its bounds and the menu had the
  // whole screen — which meant the one thing several of its settings change
  // was invisible while they were being changed. Font and pot sizes are the
  // bar's own look; you have to see it to set it.
  //
  // The bar can stay because the menu is a child of MotionComponent and the
  // GL context composites above everything else: whatever is not that child
  // would hide behind the sphere's image, so the menu must not reach past it.

  _motionComponent->setBounds (bounds);

  if (_globalSettings)
    _globalSettings->setBounds (_motionComponent->getLocalBounds ());
  if (_skinEditor)
    _skinEditor->setBounds (_motionComponent->getLocalBounds ());
  if (_colourPicker)
    {
      // Only the lower part of the screen: the sphere above it is what the
      // colour is being chosen for, and it has to stay in sight.
      auto picker = _motionComponent->getLocalBounds ();
      _colourPicker->setBounds (
          picker.removeFromBottom (picker.getHeight () * 2 / 5)
              .reduced (picker.getWidth () / 20, 0));
    }
}

float
A3MotionUIComponent::getMinimumWidth () const
{
  return _channelStrips.size () * LayoutHints::Channels::widthMin;
}

float
A3MotionUIComponent::getMinimumHeight () const
{
  auto minimumHeight = LayoutHints::MotionComponent::heightMin;
  return minimumHeight;
}

// TODO: factor this into separate listeners so not all sources have to
// be tested exhaustively.
void
A3MotionUIComponent::valueChanged (juce::Value &value)
{
  if (value.refersToSameSourceAs (_ioAdapter->getButton (Button::ClockMode)))
    {
      // Cycles, like the strip's clock key it stands beside — the panel and
      // the screen are two places to reach one function, so they had better
      // do the same thing when reached.
      if (value.getValue ())
        {
          applyClockMode ((_clockMode + 1) % 3);
          _clipSettings->setClockMode (_clockMode);
          updateControlReadout ("-- CLOCKMODE");
        }
    }
  else if (value.refersToSameSourceAs (_ioAdapter->getButton (Button::RecMode)))
    {
      if (value.getValue ())
        {
          auto const count = static_cast<int> (recMenuModes.size ());
          applyRecMode ((recMenuIndex (_recMode) + 1) % count);
          updateControlReadout ("-- RECMODE");
        }
    }
  else if (value.refersToSameSourceAs (_ioAdapter->getButton (Button::Menu)))
    {
      bool const pressed = static_cast<bool> (value.getValue ());
      if (pressed)
        {
          toggleGlobalSettings ();
        }
      // release: ignored (toggle on press)
    }
  else if (value.refersToSameSourceAs (_ioAdapter->getButton (Button::Record)))
    {
      updateControlReadout (juce::String ("-- RECORD ")
                            + (value.getValue () ? "ON" : "OFF"));

      // Pressed while a take is running, this ends it. The finger no longer
      // bounds a recording — that is what makes a jump recordable — so
      // something else has to, and Record is the button that started it.
      if (static_cast<bool> (value.getValue ()) && _engine.isRecording ())
        {
          endRecording ();
        }
      else if (static_cast<bool> (value.getValue ()))
        {
          // The panel's key shows the take's settings, the same as the bar's
          // key and the REC tab do. It arms nothing on its own -- recording
          // starts when a slot's Play|Pause pad is pressed while this is held
          // -- but what you are about to set up is on the other face, so that
          // is the face it turns to.
          toggleRecordPage ();
        }

      // Recording itself is armed when a slot's Play|Pause pad is pressed
      // while this button is held — see handlePadPress().
    }
  else if (value.refersToSameSourceAs (_ioAdapter->getButton (Button::Shift)))
    {
      // Pure modifier: level-checked via isButtonPressed(Button::Shift) when
      // a slot's Action pad is pressed (preview-and-fire gesture).
      //
      // The panel's key and the strip's are one state, so the strip is told
      // too: it lights the same in either case, and it is what the LEDs are
      // written from.
      bool const held = static_cast<bool> (value.getValue ());
      if (_clipSettings)
        _clipSettings->setShiftHeld (held);
      updateFunctionKeyLEDs ();
      updateControlReadout (juce::String ("-- SHIFT ")
                            + (held ? "ON" : "OFF"));
    }
  else if (value.refersToSameSourceAs (_ioAdapter->getButton (Button::Tap)))
    {
      if (value.getValue ())
        {
          // Button pressed - send /tap OSC immediately via DIRECT sender
          // Bypasses async queue for zero latency - time-critical!
          _clipSettings->flashTap ();
          updateFunctionKeyLEDs ();
          updateControlReadout ("-- TAP");

          auto tapMsg = juce::OSCMessage (_oscAddresses.tap);
          tapMsg.addInt32 (1);
          _tapSender.send (tapMsg);  // Direct, synchronous send - no queue!
        }
      else
        {
        }
    }
  else if (value.refersToSameSourceAs (_ioAdapter->getTapTimeMicros ()))
    {
      if (_clockMode == 0)
        handleTapAt (juce::int64 (value.getValue ()));
    }
  else
    {
      for (index_t pot = 0; pot < _ioAdapter->getNumGlobalPots (); ++pot)
        {
          if (!value.refersToSameSourceAs (_ioAdapter->getGlobalPot (pot)))
            continue;

          jassert (value.getValue ().isDouble ());
          auto const normalized = static_cast<float> (value.getValue ());

          // One knob per channel. What Core makes of the value is its
          // business; here it is a number between 0 and 1.
          _engine.setChannelPot3 (pot, normalized);
          scheduleSetSave ();
          updateControlReadout ("CH" + juce::String (pot + 1) + " 3D "
                                + juce::String (normalized, 2));
          updateClipSettingsDisplay ();
          return;
        }

      for (auto channel = 0u; channel < _ioAdapter->getNumChannels ();
           ++channel)
        {
          // The encoders have one job each now, the same one whatever is
          // on screen: freq and Q for their channel. They used to scroll
          // the bar's sections and drive the menu — that is all touch now,
          // so nothing here depends on what happens to be open.
          if (value.refersToSameSourceAs (
                  _ioAdapter->getEncoderIncrement (channel)))
            {
              auto const increment = static_cast<int> (
                  _ioAdapter->getEncoderIncrement (channel).getValue ());
              if (increment != 0)
                {
                  handleChannelValueChange (channel, channelRowFreq,
                                            increment);
                  updateControlReadout (
                      "CH" + juce::String (channel + 1) + " FREQ "
                      + juce::String (_engine.getChannelPot1 (channel), 2));
                }
              return;
            }
          else if (value.refersToSameSourceAs (
                       _ioAdapter->getEncoderIncrement (channel, 1)))
            {
              auto const increment = static_cast<int> (
                  _ioAdapter->getEncoderIncrement (channel, 1).getValue ());
              if (increment != 0)
                {
                  handleChannelValueChange (channel, channelRowQ, increment);
                  updateControlReadout (
                      "CH" + juce::String (channel + 1) + " Q "
                      + juce::String (_engine.getChannelPot2 (channel), 2));
                }
              return;
            }
          else if (value.refersToSameSourceAs (
                       _ioAdapter->getEncoderPress (channel))
                   || value.refersToSameSourceAs (
                       _ioAdapter->getEncoderPress (channel, 1)))
            {
              // Nothing. Pressing used to arm a menu row or cycle a
              // section's sub-element; both are reached by touch now, and a
              // press that does something different depending on what is
              // open is exactly what this rework got rid of.
              return;
            }
          else if (value.refersToSameSourceAs (
                       _ioAdapter->getPot (channel, 0))
                   || value.refersToSameSourceAs (
                       _ioAdapter->getPot (channel, 1)))
            {
              // Not the panel's knobs: these are the pot-encoder's synthetic
              // values, and that encoder turns Q outright now. The physical
              // pots are getGlobalPot() — see the listener registration.
              return;
            }

          for (auto pad = 0u; pad < _ioAdapter->getNumPadsPerChannel (); ++pad)
            {
              if (value.refersToSameSourceAs (
                      _ioAdapter->getPad (channel, pad)))
                {
                  if (value.getValue ())
                    handlePadPress (channel, pad);
                  else
                    handlePadRelease (channel, pad);
                  return;
                }
            }
        }
    }
}

void
A3MotionUIComponent::handleTapAt (juce::int64 tapTimeMicros)
{
  auto const result = _engine.tap (tapTimeMicros);

  // FirstTap was falling through here entirely. The clock does reset
  // itself on it — that much is covered by
  // TempoClock.FirstTapResetsTheBeat — but the UI gave no sign of it,
  // and a stale BPM from the previous run stayed on the readout.
  switch (result)
    {
    case TempoClock::TapResult::TempoAvailable:
      {
        auto const bpm = _engine.getTempoBPM ();
        juce::Logger::writeToLog ("[TAP] BPM=" + juce::String (bpm));
        _valueBPM = bpm;
        break;
      }
    case TempoClock::TapResult::FirstTap:
      juce::Logger::writeToLog ("[TAP] first tap: beat reset to 1");
      updateControlReadout ("-- TAP 1");
      break;
    case TempoClock::TapResult::TempoNotAvailable:
      juce::Logger::writeToLog ("[TAP] counting, no tempo yet");
      break;
    }
}

void
A3MotionUIComponent::handleChannelValueChange (index_t channel, int row,
                                               int increment)
{
  // The same step the encoders take, so a finger and a knob move a value at
  // the same rate.
  auto const step = increment * 0.02f;

  // The rows read 3d, freq, Q from the top — see channelRow* in
  // ClipSettingsLayout.hh. The grid's order is the screen's, not the
  // engine's pot numbering.
  switch (row)
    {
    case channelRowThreeD:
      _engine.setChannelPot3 (
          channel,
          std::clamp (_engine.getChannelPot3 (channel) + step, 0.f, 1.f));
      break;
    case channelRowFreq:
      _engine.setChannelPot1 (
          channel,
          std::clamp (_engine.getChannelPot1 (channel) + step, 0.f, 1.f));
      break;
    case channelRowQ:
      _engine.setChannelPot2 (
          channel,
          std::clamp (_engine.getChannelPot2 (channel) + step, 0.f, 1.f));
      break;
    default:
      return;
    }

  updateClipSettingsDisplay ();
  scheduleSetSave ();
}

void
A3MotionUIComponent::closeAllOverlays ()
{
  updateControlReadout ("-- CLOSE");

  if (_colourPickerOpen)
    closeColourPicker ();
  if (_skinEditorOpen && _skinEditor->isNaming ())
    _skinEditor->finishNaming ();
  if (_skinEditorOpen)
    closeSkinEditor ();
  if (_globalSettingsOpen)
    closeGlobalSettings ();

  updateOverlayButtons ();
}

void
A3MotionUIComponent::updateOverlayButtons ()
{
  if (!_overlayButtons || !_motionComponent)
    return;

  auto const anyOpen
      = _globalSettingsOpen || _skinEditorOpen || _colourPickerOpen;
  _overlayButtons->setVisible (anyOpen);

  // The strips sit beside whichever page is showing, so they follow its
  // panel rather than a fixed width.
  if (_overlayStrips)
    {
      _overlayStrips->setVisible (anyOpen && !_colourPickerOpen);
      if (anyOpen && !_colourPickerOpen)
        {
          _overlayStrips->setBounds (_motionComponent->getLocalBounds ());
          _overlayStrips->setPanel (_skinEditorOpen
                                        ? _skinEditor->panelBounds ()
                                        : _globalSettings->panelBounds ());
          _overlayStrips->toFront (false);
        }
    }

  if (!anyOpen)
    return;

  auto const height = OverlayButtons::preferredHeight ();
  auto const width = height * 2 + juce::jmax (2, height / 8);
  auto const margin = juce::jmax (4, height / 4);

  auto const area = _motionComponent->getLocalBounds ();
  _overlayButtons->setBounds (area.getRight () - width - margin,
                              area.getY () + margin, width, height);
  _overlayButtons->toFront (false);
}

void
A3MotionUIComponent::toggleGlobalSettings ()
{
  updateControlReadout ("-- MENU");

  // One level at a time: a name being typed, then the editor, then the menu
  // itself.
  if (_colourPickerOpen)
    closeColourPicker ();
  else if (_skinEditorOpen && _skinEditor->isNaming ())
    _skinEditor->finishNaming ();
  else if (_skinEditorOpen)
    closeSkinEditor ();
  else if (_globalSettingsOpen)
    closeGlobalSettings ();
  else
    openGlobalSettings ();
}

void
A3MotionUIComponent::toggleRecordingOnShownClip ()
{
  // Asked for, not merely running: startRecording() schedules the take for
  // the next downbeat, so isRecording() is still false right after it. A
  // second tap inside that window — up to a whole beat at 60 BPM — started
  // another take instead of ending the first, and the slot ended up with two.
  if (_engine.isRecording () || _recordingSlot.has_value ())
    {
      updateControlReadout ("-- REC OFF");
      endRecording ();
      return;
    }

  // The clip the bar is showing. On the hardware the pad names the slot;
  // there is no pad here, and the bar already says which slot it describes.
  updateControlReadout ("-- REC ON");
  startRecording (_clipSettingsChannel, _clipSettingsSlot);
}

void
A3MotionUIComponent::handleScreenTap ()
{
  updateControlReadout ("-- TAP");

  auto tapMsg = juce::OSCMessage (_oscAddresses.tap);
  tapMsg.addInt32 (1);
  _tapSender.send (tapMsg);

  // The hardware's tap carries a timestamp from the adapter; a finger has
  // to bring its own, or the tempo estimator never sees this tap at all.
  if (_clockMode == 0)
    handleTapAt (juce::Time::getHighResolutionTicks ()
                 * 1000000 / juce::Time::getHighResolutionTicksPerSecond ());
}

void
A3MotionUIComponent::startRecording (index_t channel, index_t slot)
{
  auto &pattern = _patterns[channel][slot];

  // Stop any existing pattern at this slot
  if (pattern)
    {
      auto status = pattern->getStatus ();
      if (status == Pattern::Status::Playing
          || status == Pattern::Status::Recording)
        {
          _engine.stopPattern (pattern, TempoClock::nextDownBeat (_now));
        }
      _motionComponent->unsetPreviewPattern (pattern);
    }

  // The length set for the next take, not whatever the slot happens to
  // hold. Recording is the only moment a pattern's length is decided.
  auto const configuredLengthBeats
      = std::exp2 (static_cast<float> (
            _clipUIParams[channel][slot].recordLengthLog2))
        * _engine.getBeatsPerBar ();

  // A take runs until Record is pressed again. OneShot — the default —
  // schedules its own stop one length in, which is why recording ended by
  // itself with nobody touching the button.
  _engine.setRecordingMode (MotionEngine::RecordingMode::Loop);

  // Remembered so an empty take can be undone: the slot's pattern is
  // replaced right below, and a stray double press must not cost whatever
  // was in there.
  _recordingSlot = std::make_pair (channel, slot);
  _patternBeforeRecording = pattern;

  // Drawn faintly under the take so you can see what you are writing over.
  // MotionComponent decides whether to show it -- Write replaces the whole
  // pass, and a ghost of the old one there says nothing.
  if (_motionComponent)
    _motionComponent->setRecordingUnderlay (_patternBeforeRecording);

  // Always create a fresh Pattern for recording (user pattern)
  pattern = std::make_shared<Pattern> ();
  pattern->setChannel (channel);

  auto recordLength = Measure{
    0, static_cast<int> (std::max (1.f, configuredLengthBeats)), 0
  };
  recordLength.consolidate (_engine.getBeatsPerBar ());

  // Store the recording length in the pattern so it can be updated if encoder changes
  pattern->setPlaybackLength (recordLength);

  _engine.recordPattern (pattern, TempoClock::nextDownBeat (_now),
                         recordLength);

  // Show what is being recorded. Starting a recording on one channel while
  // the bar still displayed another one left every setting that shapes the
  // take — speed above all, which is its length — pointing at the wrong
  // clip, and the encoders with it.
  selectClip (channel, slot);

  // The bar's Rec button says so too — it is the only sign a take is
  // running for anyone not looking at the hardware key's LED.
  if (_clipSettings)
    _clipSettings->setRecording (true);
    updateFunctionKeyLEDs ();
}

void
A3MotionUIComponent::handlePadPress (index_t channel, index_t pad)
{
  auto const function = padFunctionByPadIndex[pad];
  auto const slot = slotForPadIndex[pad];
  auto &pattern = _patterns[channel][slot];

  char const *functionName = "";
  switch (function)
    {
    case PadFunction::PlayPause: functionName = "PLAYPAUSE"; break;
    case PadFunction::Stop:      functionName = "STOP";      break;
    case PadFunction::Action:    functionName = "ACTION";    break;
    case PadFunction::Settings:  functionName = "SETTINGS";  break;
    }
  updateControlReadout ("CH" + juce::String (channel + 1) + " "
                        + functionName);

  // The bar follows the hand. Pressing play or the accent on a clip is saying
  // "this one", so the settings you are looking at should be its — otherwise
  // you sit there reading one clip's values while another one plays.
  //
  // On a press, not on every start: a clip that an end action or a chain
  // started did not come from a finger, and moving the selection out from
  // under somebody who is mid-adjustment is the thing that made this a
  // question rather than an obvious yes (see the issue).
  if (function == PadFunction::PlayPause || function == PadFunction::Action)
    selectClip (channel, slot);

  if (isButtonPressed (Button::Record) && function == PadFunction::PlayPause)
    {
      startRecording (channel, slot);
      return;
    }

  switch (function)
    {
    case PadFunction::PlayPause:
      {
        if (!pattern)
          break;

        // Both ends on the next beat. The bar is the take's unit, but a bar
        // is up to a metre's worth of beats away and a clip that starts that
        // long after the finger reads as a button that did not work; the beat
        // is close enough to feel immediate and still lands in time.
        auto const on = TempoClock::nextBeat (_now, _engine.getBeatsPerBar ());

        auto const status = pattern->getStatus ();
        if (status == Pattern::Status::Idle)
          {
            pattern->setPlaybackLength (getPlaybackLength (channel, slot));
            _engine.playPattern (pattern, on);
          }
        else if (status == Pattern::Status::Playing
                 || status == Pattern::Status::ScheduledForPlaying)
          {
            _engine.stopPattern (pattern, on);
          }
        break;
      }
    case PadFunction::Stop:
      {
        if (!pattern)
          break;

        // Now, not on a beat. Stop is the way out of a thing that is going
        // wrong, and a way out that waits for the music is not one.
        auto const status = pattern->getStatus ();
        if (status == Pattern::Status::Playing
            || status == Pattern::Status::Recording
            || status == Pattern::Status::ScheduledForPlaying)
          {
            _engine.stopPattern (pattern, _now);
          }
        break;
      }
    case PadFunction::Action:
      {
        // Shift+Action: preview-and-fire — play in preview mode (OSC
        // silenced) while the encoder can browse the library; releasing
        // Action exits (see handlePadRelease()).
        // The accent first and unconditionally: it is an accent, not a start.
        // Behind the check below it fired only on a clip that happened to be
        // standing still, so hitting ACT on something already running — which
        // is most of when you would reach for it — did nothing at all.
        //
        // And what it throws the clip to, before the press rather than with
        // it: the engine takes the clip's settings down at the moment the
        // accent starts, and it can only do that if it already knows there is
        // something to put in their place.
        _engine.setChannelAction (channel, _slotAction[channel][slot].settings);
        _engine.setChannelAccentHeld (channel, true, pattern);

        if (!pattern || pattern->getStatus () != Pattern::Status::Idle)
          break;

        if (isButtonPressed (Button::Shift))
          {
            pattern->setPlaybackLength (getPlaybackLength (channel, slot));
            _engine.setPreviewMode (channel, true);
            _previewHeldPad[channel] = static_cast<int> (slot);
            _engine.playPattern (pattern, _now);
            setPreviewWithDisplayData (pattern);
            break;
          }

        // Without Shift: the instant start, beside PlayPause's quantised one.
        // The pair is the point — quantised is what you want almost always,
        // and this is for the moment that will not wait for the beat.
        //
        // And the accent with it: the 3d rises while this is held and falls
        // when it is let go, from the shape this clip carries. One gesture —
        // the clip is thrown and the sound opens up on the same finger.
        pattern->setPlaybackLength (getPlaybackLength (channel, slot));
        _engine.playPattern (pattern, _now);

        // In Hold the clip belongs to the finger for as long as it is down.
        // Remembered here rather than worked out on release: the mode can be
        // changed mid-press, and a gesture that ends under a different rule
        // than it began under is one that sometimes leaves a clip running
        // with nothing holding it.
        if (pattern->getActMode () == ActMode::Hold)
          _actHeldSlot[channel] = static_cast<int> (slot);
        break;
      }
    case PadFunction::Settings:
      {
        selectClip (channel, slot);
        break;
      }
    }
}

void
A3MotionUIComponent::showBarPage (BarPage page)
{
  _barPage = page;
  _clipSettings->setPage (page);
  _controller->setVisible (page == BarPage::Controller);
  if (_action)
    {
      _action->setVisible (page == BarPage::Action);
      if (page == BarPage::Action)
        updateActionPage ();
      else
        // Leaving the page ends the edit, which is also what puts the
        // keyboard away and makes the script live again.
        _action->stopEditingScript ();
    }
  if (_browser)
    {
      _browser->setVisible (page == BarPage::Browser);
      if (page == BarPage::Browser)
        refreshBrowser ();
    }
}

void
A3MotionUIComponent::fillSlotFromLibrary (index_t channel, index_t slot,
                                          int libIndex)
{
  if (channel >= _patterns.size () || slot >= _patterns[channel].size ())
    return;

  auto pattern = libIndex > 0 ? _patternLibrary->loadPattern (libIndex)
                              : nullptr;

  if (pattern)
    pattern->setChannel (channel);

  _patterns[channel][slot] = std::move (pattern);

  // The clip travels with the pattern, always through here, so a slot can
  // always answer both "what am I holding" and "where did it come from".
  _slotClipFile[channel][slot]
      = libIndex > 0 ? _patternLibrary->getEntry (libIndex).clipFile
                     : juce::File{};

  // The clip's direction and end action have to reach the strip, or the next
  // applyMotionMode() writes the strip's stale ones back over them and the
  // two settings a clip carries are the two it cannot keep.
  syncClipUIParamsFromPattern (channel, slot);

  // And the line to draw it by. This is the one place a slot is filled from
  // the library, so it is the one place that has to say so -- two of the three
  // callers used to do it themselves and the third (restoring a session) did
  // not, which left every slot it filled playing an invisible trajectory.
  registerPatternDisplayData (_patterns[channel][slot]);
}

bool
A3MotionUIComponent::slotHasDrifted (index_t channel, index_t slot) const
{
  if (channel >= _patterns.size () || slot >= _patterns[channel].size ())
    return false;

  auto const &pattern = _patterns[channel][slot];
  if (!pattern)
    return false;

  return clipHasDrifted (*pattern, _slotClipFile[channel][slot]);
}

void
A3MotionUIComponent::saveSlotClip (index_t channel, index_t slot)
{
  if (channel >= _patterns.size () || slot >= _patterns[channel].size ())
    return;

  auto const &pattern = _patterns[channel][slot];
  auto const &clipFile = _slotClipFile[channel][slot];

  if (!pattern || !clipFile.existsAsFile ())
    {
      updateControlReadout ("-- NOTHING TO SAVE");
      return;
    }

  auto const clip = ClipFile::load (clipFile);
  if (!clip)
    {
      updateControlReadout ("-- CLIP UNREADABLE");
      return;
    }

  // Nothing to write. Without this, Save on an untouched factory clip made a
  // copy of it anyway -- press it twice out of habit and the library grows a
  // clip you never asked for and cannot tell from the original.
  if (!clipHasDrifted (*pattern, clipFile))
    {
      updateControlReadout ("-- NOTHING CHANGED");
      return;
    }

  auto const index = _patternLibrary->indexForName (pattern->getName ());
  auto const factory = index > 0 && _patternLibrary->isFactory (index);

  if (!factory)
    {
      if (saveClipSettings (*pattern, clipFile))
        updateControlReadout ("-- SAVED");
      else
        updateControlReadout ("-- SAVE FAILED");

      refreshBrowser ();
      updateClipSettingsDisplay ();
      return;
    }

  // A factory clip is the instrument's, not the performer's, so saving one
  // makes a copy and points this slot at it. Silently rather than with a
  // refusal: you asked for your changes to be kept, and they are. It is the
  // same thing Save as does outright.
  saveSlotClipAsCopy ();
}

juce::String
A3MotionUIComponent::saveSlotClipAsCopy ()
{
  auto const channel = _clipSettingsChannel;
  auto const slot = _clipSettingsSlot;

  if (channel >= _patterns.size () || slot >= _patterns[channel].size ())
    return {};

  auto const &pattern = _patterns[channel][slot];
  if (!pattern)
    {
      updateControlReadout ("-- NOTHING TO SAVE");
      return {};
    }

  // Whatever the slot came from, if it came from anything: a slot holding a
  // shape with no clip beside it still has a name and a set of values, and
  // those are what a copy is made of.
  auto const from = ClipFile::load (_slotClipFile[channel][slot]);

  Clip copy;
  if (from.has_value ())
    copy = *from;

  // A settings preset, whatever it was copied from: what is being kept is how
  // the slot is played, and the shape it is played on is already in the
  // library under its own name. A copy that named a shape would be listed
  // nowhere at all -- the settings scan skips a clip that names one, on the
  // grounds that the shape lists it, and a shape finds its clip by file name.
  copy.svg.clear ();
  copy.aka.clear ();
  copy.name = freeClipName (_patternLibrary->getClipDir (),
                            juce::String (pattern->getName ()))
                  .toStdString ();
  copy.settings = clipSettingsFrom (*pattern);

  auto const target = _patternLibrary->getClipDir ().getChildFile (
      juce::String (copy.name) + ".json");

  if (!ClipFile::save (copy, target))
    {
      updateControlReadout ("-- SAVE FAILED");
      return {};
    }

  _slotClipFile[channel][slot] = target;
  updateControlReadout ("-- SAVED "
                        + juce::String (copy.name).toUpperCase ());

  _patternLibrary->refresh ();
  refreshBrowser ();
  updateClipSettingsDisplay ();
  return juce::String (copy.name);
}

juce::File
A3MotionUIComponent::sessionsDir () const
{
  return _patternLibrary->getRootDir ().getChildFile ("sessions");
}

juce::File
A3MotionUIComponent::actionsDir () const
{
  return _patternLibrary->getRootDir ().getChildFile ("actions");
}

juce::String
A3MotionUIComponent::saveCurrentSession ()
{
  auto set = buildSession ();

  // A name that is not taken yet. Naming one by hand comes with the naming
  // row; until then a set is "Set", "Set 2", "Set 3" -- countable, sayable,
  // and findable in a list, which is what a name is for.
  auto const dir = sessionsDir ();
  auto name = juce::String ("Set");
  for (int n = 2; dir.getChildFile (name + ".json").existsAsFile (); ++n)
    name = "Set " + juce::String (n);

  set.name = name.toStdString ();

  if (saveSession (dir.getChildFile (name + ".json"), set))
    {
      // The set that is loaded is now this one. Without this, Save stayed
      // dark after a Save as: the device had a file to write back to and no
      // idea that it did.
      _sessionName = name;
      updateControlReadout ("-- SAVED " + name.toUpperCase ());
    }
  else
    {
      updateControlReadout ("-- SAVE FAILED");
      name = {};
    }

  refreshBrowser ();
  return name;
}

void
A3MotionUIComponent::loadSessionNamed (juce::String const &name)
{
  auto const file = sessionsDir ().getChildFile (name + ".json");
  if (!file.existsAsFile ())
    {
      updateControlReadout ("-- NO SUCH SET");
      return;
    }

  // What is running now, written down before it is replaced. Not asked about
  // -- written. The previous arrangement is then never gone, even if nobody
  // thought to save it.
  writeSet ();

  // Everything stops. All eight slots are about to hold something else, and a
  // clip still running while its slot holds a different one is exactly the
  // state that dropping a single clip already avoids.
  for (index_t channel = 0; channel < _patterns.size (); ++channel)
    for (index_t slot = 0; slot < _patterns[channel].size (); ++slot)
      if (auto const &pattern = _patterns[channel][slot])
        {
          auto const status = pattern->getStatus ();
          if (status == Pattern::Status::Playing
              || status == Pattern::Status::Recording
              || status == Pattern::Status::ScheduledForPlaying)
            _engine.stopPattern (pattern, _now);
        }

  _sessionName = name;
  applySet (file);

  updateControlReadout ("-- LOADED " + name.toUpperCase ());
  refreshBrowser ();
  refreshAllPadRowLabels ();
  updateClipSettingsDisplay ();
}

void
A3MotionUIComponent::refreshBrowser ()
{
  if (!_browser)
    return;

  // Entry 0 is "no pattern" in the library's own numbering, and a row saying
  // nothing is a row that empties the field it is dropped on -- which is worth
  // having, so it is listed rather than skipped.
  juce::StringArray names;
  std::vector<bool> settingsRows;

  if (_browserList == BrowserList::Sessions)
    {
      for (auto const &file : sessionsDir ().findChildFiles (
               juce::File::findFiles, false, "*.json"))
        names.add (file.getFileNameWithoutExtension ());
      names.sort (true);
    }
  else if (_browserList == BrowserList::Actions)
    {
      // Entry 0 is "no action", the same way entry 0 of the library is "no
      // clip": a slot has to be able to go back to firing nothing.
      names.add ("");
      for (auto const &file : actionsDir ().findChildFiles (
               juce::File::findFiles, false, "*.scd"))
        names.add (file.getFileNameWithoutExtension ());
      names.sort (true);
    }
  else
    {
      _browserRowToLibrary.clear ();

      for (int i = 0; i < _patternLibrary->getNumEntries (); ++i)
        {
          auto const &entry = _patternLibrary->getEntry (i);

          // Row zero is the library's "Empty" and belongs to no category: it
          // is how a slot is given nothing, which is a thing you want however
          // the list is narrowed.
          auto const shown
              = i == 0 || _clipFilter == ClipFilter::All
                || (_clipFilter == ClipFilter::System
                    && entry.category == PatternLibrary::Category::System)
                || (_clipFilter == ClipFilter::User
                    && entry.category != PatternLibrary::Category::System);

          if (!shown)
            continue;

          names.add (juce::String (entry.name));
          settingsRows.push_back (entry.category
                                  == PatternLibrary::Category::Settings);
          _browserRowToLibrary.push_back (i);
        }
    }

  _browser->setEntries (names, settingsRows);
  _browser->setShowingList (_browserList);

  // The list points at what the chosen field is already holding. Without this
  // you have to remember what is in a slot in order to see it highlighted --
  // and the highlight is the only thing saying which of seventy rows you are
  // looking at.
  if (_browserList == BrowserList::Actions)
    {
      auto const ch = _clipSettingsChannel;
      auto const sl = _clipSettingsSlot;
      auto const &action = _slotAction[ch][sl].file;
      _browser->setSelectedEntry (
          action.existsAsFile ()
              ? names.indexOf (action.getFileNameWithoutExtension ())
              : 0);
    }
  else if (_browserList == BrowserList::Clips)
    {
      auto const ch = _clipSettingsChannel;
      auto const sl = _clipSettingsSlot;
      auto const &held = ch < _patterns.size () && sl < _patterns[ch].size ()
                             ? _patterns[ch][sl]
                             : nullptr;

      // Through the clip, not the shape's name: a settings preset leaves the
      // shape alone, so the name would point back at the shape's row however
      // many presets were applied on top of it -- and setSelectedEntry()
      // scrolls the chosen row into view, so the list would jump away from
      // the presets after every pick. A shape with no clip beside it still
      // has only its name to go on.
      auto const fromClip
          = _patternLibrary->indexForClipFile (_slotClipFile[ch][sl]);
      auto const entry
          = fromClip > 0
                ? fromClip
                : (held ? _patternLibrary->indexForName (held->getName ()) : 0);

      // Back through the map: with the list narrowed, the entry the slot
      // holds may not be on it at all, and a row number taken from the
      // library would then point at whatever happens to be there.
      _browser->setSelectedEntry (browserRowForLibrary (entry));
    }

  // What can actually be done. A key lights only when pressing it would do
  // something -- one that does nothing teaches you to stop trusting the
  // others.
  //
  // What the five keys say, and which of them can be pressed. All five say the
  // same words on every tab -- what they act on is the list you are looking
  // at, and the lit tab above it has already said which list that is. "Save
  // Action" spent a word saying it again, and keys that reword themselves
  // between tabs are keys you read instead of aim at.
  auto const chosen = chosenEntryHasAFile ();
  auto const rename = _browser->isRenaming () ? "Keep" : "Rename";

  // The filter narrows the library, and only the library: the actions and the
  // sets land shipped and hand-written in one folder each with nothing marking
  // which is which, so there is no split to offer there. The key goes dark
  // rather than showing a word that would do nothing.
  auto const filtering = _browserList == BrowserList::Clips;
  auto const filter = _clipFilter == ClipFilter::All      ? "All"
                      : _clipFilter == ClipFilter::User ? "User"
                                                        : "System";

  // Save wants somewhere to write back to; Save as only wants something to
  // write. A slot with nothing in it has neither.
  auto const inPlace = canSaveInPlace ();
  auto const holds = _clipSettingsChannel < _patterns.size ()
                     && _clipSettingsSlot
                            < _patterns[_clipSettingsChannel].size ()
                     && _patterns[_clipSettingsChannel][_clipSettingsSlot]
                            != nullptr;
  // The delete key says what the next press will do. Armed it wears the word
  // rather than a colour, because a key that only changed colour would be a
  // key you have to have been watching.
  auto const remove = _deleteArmed ? "Sure?" : "Delete";

  // A set can always be put away -- there is always an arrangement to keep --
  // where a clip and an action are made out of what a slot holds, and an
  // empty slot holds nothing to write.
  auto const canCopy = _browserList == BrowserList::Sessions ? true : holds;

  _browser->setActions ({ filter, rename, "Save", "Save new", remove },
                        { filtering, chosen, inPlace, canCopy, chosen });
}

void
A3MotionUIComponent::assignBrowserEntry (int index)
{
  auto const channel = _clipSettingsChannel;
  auto const slot = _clipSettingsSlot;

  if (channel >= _engine.getNumChannels () || slot >= numPadSlots)
    return;
  if (index < 0 || index >= _patternLibrary->getNumEntries ())
    return;

  // A settings preset says how a slot is played, not what it plays: whatever
  // trajectory is in the slot stays, and only the values change. It gets its
  // own way in rather than a branch further down, because almost nothing
  // below applies to it -- there is no shape to stop, load or draw.
  if (index > 0
      && _patternLibrary->getEntry (index).category
             == PatternLibrary::Category::Settings)
    {
      applySettingsPreset (channel, slot, index);
      return;
    }

  auto &pattern = _patterns[channel][slot];

  // Whatever was there stops first. Dropping a clip onto a slot that is
  // playing would otherwise leave the engine running a pattern the slot no
  // longer holds.
  if (pattern)
    {
      auto const status = pattern->getStatus ();
      if (status == Pattern::Status::Playing
          || status == Pattern::Status::Recording
          || status == Pattern::Status::ScheduledForPlaying)
        _engine.stopPattern (pattern, _now);
      _motionComponent->unsetPreviewPattern (pattern);
      _motionComponent->removePatternDisplayData (pattern);
    }

  // And the slot it landed in is the one the device is on: the clip you just
  // chose is the clip you are looking at.
  selectClip (channel, slot);

  fillSlotFromLibrary (channel, slot, index);

  // Registered by fillSlotFromLibrary() now -- and by way of
  // registerPatternDisplayData(), which knows about shapes made of dots.
  // Going straight to the ticks here drew those with no dots at all.
  if (_patterns[channel][slot])
    applyMotionMode (channel, slot);

  // And it plays, so you hear what you just chose. Building a set is
  // listening to clips one after another; having to reach for the transport
  // between each two would make the browser a filing cabinet rather than
  // something you audition with.
  //
  // On the next beat, the same as PLAY everywhere else on the device. A set is
  // usually built against a clock that is already running, and a clip that
  // dropped in out of time would have to be restarted to be judged.
  if (auto const &playing = _patterns[channel][slot])
    {
      playing->setPlaybackLength (getPlaybackLength (channel, slot));
      _engine.playPattern (
          playing, TempoClock::nextBeat (_now, _engine.getBeatsPerBar ()));
    }

  refreshBrowser ();
  refreshAllPadRowLabels ();
  if (channel == _clipSettingsChannel && slot == _clipSettingsSlot)
    updateClipSettingsDisplay ();
}

void
A3MotionUIComponent::applySettingsPreset (index_t channel, index_t slot,
                                          int index)
{
  auto const &pattern = _patterns[channel][slot];

  // The values belong to a movement, and an empty slot has none. Choosing a
  // shape is the way in; nothing is changed here so that the field stays
  // plainly empty rather than holding settings nothing can play.
  if (!pattern)
    return;

  auto const clip = ClipFile::load (_patternLibrary->getEntry (index).clipFile);
  if (!clip.has_value ())
    return;

  applyClipSettings (*pattern, clip->settings);
  _slotClipFile[channel][slot] = _patternLibrary->getEntry (index).clipFile;

  // The bar follows what was just changed, the same as choosing a shape does.
  selectClip (channel, slot);

  // Read back OUT of the pattern, not pushed into it: applyMotionMode() would
  // write the strip's own direction and end action over the ones the preset
  // just brought.
  syncClipUIParamsFromPattern (channel, slot);

  // It keeps running if it was running. You tap a preset to hear it on the
  // clip that is playing; restarting would drop you back at the top of a
  // movement you were listening into. A stopped slot starts, so choosing is
  // still hearing.
  auto const status = pattern->getStatus ();
  if (status != Pattern::Status::Playing
      && status != Pattern::Status::ScheduledForPlaying
      && status != Pattern::Status::Recording)
    {
      pattern->setPlaybackLength (getPlaybackLength (channel, slot));
      _engine.playPattern (
          pattern, TempoClock::nextBeat (_now, _engine.getBeatsPerBar ()));
    }

  refreshBrowser ();
  refreshAllPadRowLabels ();
  if (channel == _clipSettingsChannel && slot == _clipSettingsSlot)
    updateClipSettingsDisplay ();
}

void
A3MotionUIComponent::syncClipUIParamsFromPattern (index_t channel,
                                                  index_t slot)
{
  auto const &pattern = _patterns[channel][slot];
  if (!pattern)
    return;

  auto &params = _clipUIParams[channel][slot];
  params.direction
      = pattern->getPlayDirection () == PlayDirection::Reverse ? 1 : 0;

  // Same order as applyMotionMode(): the bar's order is the engine's order.
  auto const actions = { EndAction::Loop, EndAction::Stop, EndAction::Pause,
                         EndAction::Bounce, EndAction::Random };
  auto const found
      = std::find (actions.begin (), actions.end (), pattern->getEndAction ());
  if (found != actions.end ())
    params.endAction = static_cast<int> (found - actions.begin ());
}

void
A3MotionUIComponent::assignActionEntry (juce::String const &name)
{
  auto const channel = _clipSettingsChannel;
  auto const slot = _clipSettingsSlot;

  if (channel >= _engine.getNumChannels () || slot >= numPadSlots)
    return;

  // The empty row clears it: a slot has to be able to go back to firing
  // nothing, the same way it can go back to holding no clip.
  setSlotAction (channel, slot,
                 name.isEmpty ()
                     ? juce::File{}
                     : actionsDir ().getChildFile (name + ".scd"));

  selectClip (channel, slot);
  refreshBrowser ();
}

void
A3MotionUIComponent::writeSlotActionScript ()
{
  if (!_action)
    return;

  auto const channel = _clipSettingsChannel;
  auto const slot = _clipSettingsSlot;
  auto &action = _slotAction[channel][slot];

  if (!action.file.existsAsFile ())
    return;

  action.source = _action->script ();

  // Written on every keystroke rather than on the way out. A device in a
  // booth loses power without warning, the file is a few hundred bytes, and
  // an editor whose work only survives if you remember to leave it properly
  // is one nobody trusts. The mark on the field says it has been touched
  // since it last *ran*, which is the thing worth knowing.
  if (!action.file.replaceWithText (action.source))
    {
      std::cerr << "could not write action " << action.file.getFullPathName ()
                << std::endl;
      return;
    }

  // Not re-run here: a script is applied when it is chosen, and re-applying
  // it on every character would have half-typed lines setting values.
}

void
A3MotionUIComponent::setSlotAction (index_t channel, index_t slot,
                                    juce::File const &file)
{
  if (channel >= _slotAction.size () || slot >= _slotAction[channel].size ())
    return;

  auto &action = _slotAction[channel][slot];
  action.file = file;
  action.settings.reset ();
  action.source = {};
  action.errors = {};

  if (!file.existsAsFile ())
    {
      updateActionPage ();
      return;
    }

  action.source = file.loadFileAsString ();

  auto const &pattern = _patterns[channel][slot];
  auto const current = pattern ? clipSettingsFrom (*pattern) : ClipSettings{};

  // A seed that is new every time a script is chosen, so a script with dice
  // in it throws them again on being picked -- picking it is the gesture that
  // says "give me another one of these".
  auto const result
      = runActionScript (action.source, current,
                         juce::Time::getHighResolutionTicks ());

  action.settings = result.settings;
  action.errors = result.errors;

  // What the ACTION page shows is the slot's, and choosing a script is how
  // those get set: the nine envelope values and the mode go onto the clip
  // right away, so the page reads what the script says. The movement and the
  // shape stay in the action and are put on only while ACT is down -- see
  // actionOver().
  if (pattern)
    {
      pattern->setEnvelopeAttack (result.settings.envelopeAttack);
      pattern->setEnvelopeDecay (result.settings.envelopeDecay);
      pattern->setEnvelopeMax (result.settings.envelopeMax);
      pattern->setFreqAttack (result.settings.freqAttack);
      pattern->setFreqDecay (result.settings.freqDecay);
      pattern->setFreqMax (result.settings.freqMax);
      pattern->setQAttack (result.settings.qAttack);
      pattern->setQDecay (result.settings.qDecay);
      pattern->setQMax (result.settings.qMax);
      pattern->setActMode (result.settings.actMode);
    }

  if (!result.errors.isEmpty ())
    updateControlReadout (result.errors[0]);

  updateActionPage ();
  updateClipSettingsDisplay ();
}

juce::String
A3MotionUIComponent::saveSlotAsAction ()
{
  auto const channel = _clipSettingsChannel;
  auto const slot = _clipSettingsSlot;

  if (channel >= _patterns.size () || slot >= _patterns[channel].size ())
    return {};

  auto const &pattern = _patterns[channel][slot];
  if (!pattern)
    {
      updateControlReadout ("-- NOTHING TO SAVE");
      return {};
    }

  // An action is the clip as it stands, written out as a script: dial it the
  // way you want ACT to make it sound, and keep that. No second vocabulary to
  // learn, and what is written is what can be read back and edited.
  actionsDir ().createDirectory ();

  auto const name = freeClipName (actionsDir (), "Action", ".scd");
  auto const file = actionsDir ().getChildFile (name + ".scd");

  if (!file.replaceWithText (actionScriptFor (clipSettingsFrom (*pattern))))
    {
      std::cerr << "could not write action " << file.getFullPathName ()
                << std::endl;
      updateControlReadout ("-- SAVE FAILED");
      return {};
    }

  setSlotAction (channel, slot, file);
  updateControlReadout ("-- SAVED " + name.toUpperCase ());
  refreshBrowser ();
  return name;
}

juce::File
A3MotionUIComponent::chosenActionFile () const
{
  if (_browserList != BrowserList::Actions || !_browser)
    return {};

  auto const index = _browser->getSelectedEntry ();
  if (index <= 0)
    return {}; // row zero is "no action", and has no file behind it

  auto const name = _browser->entryName (index);
  if (name.isEmpty ())
    return {};

  return actionsDir ().getChildFile (name + ".scd");
}

int
A3MotionUIComponent::browserRowForLibrary (int entry) const
{
  for (size_t row = 0; row < _browserRowToLibrary.size (); ++row)
    if (_browserRowToLibrary[row] == entry)
      return static_cast<int> (row);

  // Not on the list as it is narrowed. Nothing is highlighted rather than
  // something else being: a highlight on the wrong row is worse than none.
  return -1;
}

int
A3MotionUIComponent::libraryForBrowserRow (int row) const
{
  if (row < 0 || row >= static_cast<int> (_browserRowToLibrary.size ()))
    return -1;

  return _browserRowToLibrary[static_cast<size_t> (row)];
}

int
A3MotionUIComponent::stepThroughLibrary (int from, int increment,
                                         bool settings) const
{
  // Two lists in one library, walked one at a time. Which one an entry is on
  // is its category: a settings preset says how a slot is played, everything
  // else is a figure to play it on.
  std::vector<int> kind;
  for (int i = 0; i < _patternLibrary->getNumEntries (); ++i)
    {
      auto const isPreset = _patternLibrary->getEntry (i).category
                            == PatternLibrary::Category::Settings;
      if (isPreset == settings)
        kind.push_back (i);
    }

  if (kind.empty () || increment == 0)
    return -1;

  auto const at = std::find (kind.begin (), kind.end (), from);
  if (at == kind.end ())
    // Not on this list at all -- a slot with no preset on it, say. The first
    // step lands on an end of it rather than nowhere.
    return increment > 0 ? kind.front () : kind.back ();

  auto const size = static_cast<int> (kind.size ());
  auto position = static_cast<int> (at - kind.begin ()) + increment;
  position = ((position % size) + size) % size;

  return kind[static_cast<size_t> (position)];
}

juce::File
A3MotionUIComponent::chosenSetFile () const
{
  if (_browserList != BrowserList::Sessions || !_browser)
    return {};

  auto const index = _browser->getSelectedEntry ();
  if (index < 0)
    return {};

  auto const name = _browser->entryName (index);
  if (name.isEmpty ())
    return {};

  return sessionsDir ().getChildFile (name + ".json");
}

int
A3MotionUIComponent::chosenLibraryIndex () const
{
  if (_browserList != BrowserList::Clips || !_browser)
    return -1;

  // Through the map: a row is a row of what is listed, and what is listed is
  // the library narrowed by the filter.
  auto const index = libraryForBrowserRow (_browser->getSelectedEntry ());

  // Entry zero is the library's "Empty": a shape nobody has chosen, with no
  // file of its own to name or throw away.
  if (index <= 0 || index >= _patternLibrary->getNumEntries ())
    return -1;

  return index;
}

bool
A3MotionUIComponent::chosenEntryHasAFile () const
{
  switch (_browserList)
    {
    case BrowserList::Actions:
      return chosenActionFile ().existsAsFile ();
    case BrowserList::Sessions:
      return chosenSetFile ().existsAsFile ();
    case BrowserList::Clips:
      {
        auto const index = chosenLibraryIndex ();
        if (index < 0)
          return false;

        // A shape is its SVG; a settings preset is its clip and has no shape.
        // Either is something to name; an entry with neither is a row the
        // library made up, and there is nothing to do to it.
        auto const &entry = _patternLibrary->getEntry (index);
        return entry.file.existsAsFile () || entry.clipFile.existsAsFile ();
      }
    }

  return false;
}

int
A3MotionUIComponent::setsNaming (juce::String const &patternName) const
{
  if (patternName.isEmpty ())
    return 0;

  auto count = 0;
  for (auto const &file : sessionsDir ().findChildFiles (juce::File::findFiles,
                                                         false, "*.json"))
    {
      auto const set
          = loadSession (file, static_cast<int> (numChannelColumns),
                         static_cast<int> (numPadSlots));
      auto found = false;
      for (auto const &channel : set.channels)
        for (auto const &slot : channel.slots)
          if (juce::String (slot.patternName) == patternName)
            found = true;

      if (found)
        ++count;
    }

  return count;
}

int
A3MotionUIComponent::renameInSets (juce::String const &from,
                                   juce::String const &to)
{
  if (from.isEmpty () || to.isEmpty () || from == to)
    return 0;

  auto rewritten = 0;
  for (auto const &file : sessionsDir ().findChildFiles (juce::File::findFiles,
                                                         false, "*.json"))
    {
      auto set = loadSession (file, static_cast<int> (numChannelColumns),
                              static_cast<int> (numPadSlots));

      auto touched = false;
      for (auto &channel : set.channels)
        for (auto &slot : channel.slots)
          if (juce::String (slot.patternName) == from)
            {
              slot.patternName = to.toStdString ();
              touched = true;
            }

      if (!touched)
        continue;

      if (saveSession (file, set))
        ++rewritten;
      else
        std::cerr << "could not rewrite set " << file.getFullPathName ()
                  << std::endl;
    }

  return rewritten;
}

juce::String
A3MotionUIComponent::chosenEntryCost () const
{
  if (_browserList != BrowserList::Clips)
    return {};

  auto const index = chosenLibraryIndex ();
  if (index < 0)
    return {};

  auto const in
      = setsNaming (juce::String (_patternLibrary->getEntry (index).name));

  return in > 0 ? " IN " + juce::String (in) + (in == 1 ? " SET" : " SETS")
                : juce::String{};
}

void
A3MotionUIComponent::renameChosenEntry (juce::String const &name)
{
  switch (_browserList)
    {
    case BrowserList::Actions: renameChosenAction (name); break;
    case BrowserList::Sessions: renameChosenSet (name); break;
    case BrowserList::Clips: renameChosenClip (name); break;
    }
}

void
A3MotionUIComponent::deleteChosenEntry ()
{
  // Asked twice, whatever the folder. A file thrown away in front of a room
  // does not come back, and the second press is the only thing standing
  // between a fat finger and somebody's work. Not a dialogue: there is nothing
  // here that could put one up without covering the list it is asking about.
  if (!chosenEntryHasAFile ())
    return;

  if (!_deleteArmed)
    {
      _deleteArmed = true;
      updateControlReadout ("-- DELETE?" + chosenEntryCost ());
      refreshBrowser ();
      return;
    }

  _deleteArmed = false;

  switch (_browserList)
    {
    case BrowserList::Actions: deleteChosenAction (); break;
    case BrowserList::Sessions: deleteChosenSet (); break;
    case BrowserList::Clips: deleteChosenClip (); break;
    }
}

void
A3MotionUIComponent::renameChosenAction (juce::String const &name)
{
  auto const from = chosenActionFile ();
  if (!from.existsAsFile ())
    return;

  auto const to = actionsDir ().getChildFile (name + ".scd");
  if (to == from)
    return;

  // Nothing is overwritten. Two actions with one name is the state where the
  // next rename is a file quietly gone, and the way back is to type another
  // name -- which the row is still open for.
  if (to.exists ())
    {
      updateControlReadout ("-- NAME TAKEN");
      return;
    }

  if (!from.moveFileTo (to))
    {
      updateControlReadout ("-- COULD NOT RENAME");
      return;
    }

  // Every slot firing it comes across. A slot pointing at a file that is no
  // longer there fires nothing, silently, and the ACTION page would show an
  // empty field where a script was a moment ago.
  for (index_t channel = 0; channel < _slotAction.size (); ++channel)
    for (index_t slot = 0; slot < _slotAction[channel].size (); ++slot)
      if (_slotAction[channel][slot].file == from)
        setSlotAction (channel, slot, to);

  updateControlReadout ("-- RENAMED " + name.toUpperCase ());
  refreshBrowser ();
}

void
A3MotionUIComponent::deleteChosenAction ()
{
  auto const file = chosenActionFile ();
  if (!file.existsAsFile ())
    return;

  if (!file.deleteFile ())
    {
      updateControlReadout ("-- COULD NOT DELETE");
      refreshBrowser ();
      return;
    }

  // Every slot that fired it stops firing anything, rather than being left
  // pointing at a name with nothing behind it.
  for (index_t channel = 0; channel < _slotAction.size (); ++channel)
    for (index_t slot = 0; slot < _slotAction[channel].size (); ++slot)
      if (_slotAction[channel][slot].file == file)
        setSlotAction (channel, slot, juce::File{});

  updateControlReadout (
      "-- DELETED " + file.getFileNameWithoutExtension ().toUpperCase ());

  _browser->setSelectedEntry (0);
  refreshBrowser ();
}

void
A3MotionUIComponent::renameChosenSet (juce::String const &name)
{
  auto const from = chosenSetFile ();
  if (!from.existsAsFile ())
    return;

  auto const to = sessionsDir ().getChildFile (name + ".json");
  if (to == from)
    return;

  if (to.exists ())
    {
      updateControlReadout ("-- NAME TAKEN");
      return;
    }

  // Written out under the new name rather than moved: a set carries its own
  // name inside it, and the browser lists it by that. A file renamed without
  // it would show its old name in the list it was renamed in.
  auto set = loadSession (from, static_cast<int> (numChannelColumns),
                          static_cast<int> (numPadSlots));
  auto const was = juce::String (set.name);
  set.name = name.toStdString ();

  if (!saveSession (to, set))
    {
      updateControlReadout ("-- COULD NOT RENAME");
      return;
    }

  from.deleteFile ();

  // The set on the device is still the set on the device; only what it is
  // called has changed.
  if (_sessionName == was)
    _sessionName = name;

  updateControlReadout ("-- RENAMED " + name.toUpperCase ());
  refreshBrowser ();
}

void
A3MotionUIComponent::deleteChosenSet ()
{
  auto const file = chosenSetFile ();
  if (!file.existsAsFile ())
    return;

  if (!file.deleteFile ())
    {
      updateControlReadout ("-- COULD NOT DELETE");
      refreshBrowser ();
      return;
    }

  // What is loaded stays loaded. Throwing away the file a set was written to
  // is not the same as undoing the arrangement in front of you, and clearing
  // the device because a file went would be a delete key that stopped the
  // music.
  updateControlReadout (
      "-- DELETED " + file.getFileNameWithoutExtension ().toUpperCase ());

  _browser->setSelectedEntry (-1);
  refreshBrowser ();
}

void
A3MotionUIComponent::renameChosenClip (juce::String const &name)
{
  auto const index = chosenLibraryIndex ();
  if (index < 0)
    return;

  auto const entry = _patternLibrary->getEntry (index);
  auto const was = juce::String (entry.name);
  if (was == name)
    return;

  if (_patternLibrary->indexForName (name.toStdString ()) > 0)
    {
      updateControlReadout ("-- NAME TAKEN");
      return;
    }

  // A shape's file name carries its beat count -- 16_Wave.svg -- and the clip
  // beside it is found by the rest of it. Both move together or the clip stops
  // reaching the shape and turns up in the browser as a preset of its own.
  if (entry.file.existsAsFile ())
    {
      auto const prefix
          = entry.file.getFileNameWithoutExtension ().upToFirstOccurrenceOf (
              "_", true, false);
      auto const to = entry.file.getSiblingFile (prefix + name + ".svg");

      if (to.exists () || !PatternFile::setName (entry.file, name)
          || !entry.file.moveFileTo (to))
        {
          updateControlReadout ("-- COULD NOT RENAME");
          return;
        }
    }

  if (entry.clipFile.existsAsFile ())
    {
      auto clip = ClipFile::load (entry.clipFile);
      if (clip.has_value ())
        {
          clip->name = name.toStdString ();

          auto const to
              = _patternLibrary->getClipDir ().getChildFile (name + ".json");
          if (ClipFile::save (*clip, to))
            {
              if (to != entry.clipFile)
                entry.clipFile.deleteFile ();

              // Every slot that came from the old file still points at it.
              for (index_t channel = 0; channel < _slotClipFile.size ();
                   ++channel)
                for (index_t slot = 0;
                     slot < _slotClipFile[channel].size (); ++slot)
                  if (_slotClipFile[channel][slot] == entry.clipFile)
                    _slotClipFile[channel][slot] = to;
            }
        }
    }

  // Every set that named it comes with it. A set names its shapes rather than
  // carrying them, so one left behind holds a name nothing resolves -- and
  // that is not an error there, it is a slot that loads empty.
  auto const sets = renameInSets (was, name);

  // The patterns already in slots carry the old name in memory.
  for (auto &channel : _patterns)
    for (auto &pattern : channel)
      if (pattern && juce::String (pattern->getName ()) == was)
        pattern->setName (name.toStdString ());

  _patternLibrary->refresh ();
  refreshAllPadRowLabels ();
  updateClipSettingsDisplay ();

  updateControlReadout ("-- RENAMED " + name.toUpperCase ()
                        + (sets > 0 ? " +" + juce::String (sets) + " SETS"
                                    : juce::String{}));
  refreshBrowser ();
}

void
A3MotionUIComponent::deleteChosenClip ()
{
  auto const index = chosenLibraryIndex ();
  if (index < 0)
    return;

  auto const entry = _patternLibrary->getEntry (index);
  auto const was = juce::String (entry.name);

  auto gone = false;
  if (entry.file.existsAsFile ())
    gone = entry.file.deleteFile () || gone;
  if (entry.clipFile.existsAsFile ())
    gone = entry.clipFile.deleteFile () || gone;

  if (!gone)
    {
      updateControlReadout ("-- COULD NOT DELETE");
      refreshBrowser ();
      return;
    }

  // The sets that named it are left alone. Rewriting somebody's arrangement
  // because a shape went would be a delete key that edits files it was not
  // pointed at; a name a set cannot resolve loads as an empty slot, which is
  // what the set now honestly holds. How many that is was said before the
  // second press -- see chosenEntryCost().
  //
  // What is playing goes on playing: the pattern is in memory, and stopping
  // the room because a file went is not what the key was pressed for.
  for (index_t channel = 0; channel < _slotClipFile.size (); ++channel)
    for (index_t slot = 0; slot < _slotClipFile[channel].size (); ++slot)
      if (_slotClipFile[channel][slot] == entry.clipFile
          && entry.clipFile != juce::File{})
        _slotClipFile[channel][slot] = juce::File{};

  _patternLibrary->refresh ();
  refreshAllPadRowLabels ();
  updateClipSettingsDisplay ();

  updateControlReadout ("-- DELETED " + was.toUpperCase ());

  _browser->setSelectedEntry (0);
  refreshBrowser ();
}

void
A3MotionUIComponent::saveSlotActionInPlace ()
{
  auto const channel = _clipSettingsChannel;
  auto const slot = _clipSettingsSlot;

  auto const &file = _slotAction[channel][slot].file;
  auto const &pattern = _patterns[channel][slot];

  if (!file.existsAsFile () || !pattern)
    {
      updateControlReadout ("-- NOTHING TO SAVE");
      return;
    }

  // The clip as it stands, over the action this slot already fires. Save as
  // is what makes a second one; this is what lets an action be corrected
  // without collecting "Action 4" beside "Action 3".
  if (!file.replaceWithText (actionScriptFor (clipSettingsFrom (*pattern))))
    {
      updateControlReadout ("-- SAVE FAILED");
      return;
    }

  setSlotAction (channel, slot, file);
  updateControlReadout ("-- SAVED "
                        + file.getFileNameWithoutExtension ().toUpperCase ());
  refreshBrowser ();
}

void
A3MotionUIComponent::saveSessionInPlace ()
{
  auto const file = sessionsDir ().getChildFile (_sessionName + ".json");
  if (_sessionName.isEmpty () || !file.existsAsFile ())
    {
      updateControlReadout ("-- NOTHING TO SAVE");
      return;
    }

  auto set = buildSession ();
  set.name = _sessionName.toStdString ();

  if (!saveSession (file, set))
    {
      updateControlReadout ("-- SAVE FAILED");
      return;
    }

  updateControlReadout ("-- SAVED " + _sessionName.toUpperCase ());
  refreshBrowser ();
}

bool
A3MotionUIComponent::canSaveInPlace () const
{
  auto const channel = _clipSettingsChannel;
  auto const slot = _clipSettingsSlot;
  auto const holds = channel < _patterns.size ()
                     && slot < _patterns[channel].size ()
                     && _patterns[channel][slot] != nullptr;

  switch (_browserList)
    {
    case BrowserList::Clips:
      // Only when there is something to write. Save on an untouched clip made
      // a copy of it anyway once -- press it twice out of habit and the
      // library grows a clip you cannot tell from the original.
      return slotHasDrifted (channel, slot);
    case BrowserList::Actions:
      return holds && _slotAction[channel][slot].file.existsAsFile ();
    case BrowserList::Sessions:
      return _sessionName.isNotEmpty ()
             && sessionsDir ()
                    .getChildFile (_sessionName + ".json")
                    .existsAsFile ();
    }

  return false;
}

void
A3MotionUIComponent::saveChosen ()
{
  switch (_browserList)
    {
    case BrowserList::Clips:
      saveSlotClip (_clipSettingsChannel, _clipSettingsSlot);
      break;
    case BrowserList::Actions: saveSlotActionInPlace (); break;
    case BrowserList::Sessions: saveSessionInPlace (); break;
    }
}

void
A3MotionUIComponent::saveAsChosen ()
{
  juce::String name;
  switch (_browserList)
    {
    case BrowserList::Clips: name = saveSlotClipAsCopy (); break;
    case BrowserList::Actions: name = saveSlotAsAction (); break;
    case BrowserList::Sessions: name = saveCurrentSession (); break;
    }

  if (name.isEmpty ())
    return;

  // The name it got is a counted one -- "Action 4" -- which is findable and
  // says nothing. So the new row opens for typing straight away, keyboard and
  // all: naming a thing is part of making it, and a second key press to get
  // there is a key press somebody skips and then cannot find what they saved.
  for (int row = 0; row < _browser->getNumEntries (); ++row)
    if (_browser->entryName (row) == name)
      {
        _deleteArmed = false;
        _browser->setSelectedEntry (row);
        _browser->beginRename (name);
        refreshBrowser ();
        return;
      }
}

void
A3MotionUIComponent::updateActionPage ()
{
  if (!_action)
    return;

  auto const channel = _clipSettingsChannel;
  auto const slot = _clipSettingsSlot;
  auto const &pattern = _patterns[channel][slot];

  _action->setTarget (static_cast<int> (channel), static_cast<int> (slot),
                      _channelUIStates[channel]->colour);

  // The defaults when the slot is empty, so the page reads as a page rather
  // than as a blank: there is nothing to fire, but what firing would do is
  // still worth seeing.
  ClipSettings const defaults;
  _action->setEnvelope (
      pattern ? pattern->getEnvelopeAttack () : defaults.envelopeAttack,
      pattern ? pattern->getEnvelopeDecay () : defaults.envelopeDecay,
      pattern ? pattern->getEnvelopeMax () : defaults.envelopeMax);
  _action->setActMode (
      (pattern ? pattern->getActMode () : defaults.actMode) == ActMode::Hold
          ? 1
          : 0);

  _action->setFreqEnvelope (
      pattern ? pattern->getFreqAttack () : defaults.freqAttack,
      pattern ? pattern->getFreqDecay () : defaults.freqDecay,
      pattern ? pattern->getFreqMax () : defaults.freqMax);
  _action->setQEnvelope (pattern ? pattern->getQAttack () : defaults.qAttack,
                         pattern ? pattern->getQDecay () : defaults.qDecay,
                         pattern ? pattern->getQMax () : defaults.qMax);

  auto const &slotAction = _slotAction[channel][slot];
  auto const &action = slotAction.file;

  // What the field's list offers: the empty row first, so a slot can go back
  // to firing nothing the same way it can go back to holding no clip.
  juce::StringArray choices;
  choices.add ("");
  for (auto const &file : actionsDir ().findChildFiles (juce::File::findFiles,
                                                        false, "*.scd"))
    choices.add (file.getFileNameWithoutExtension ());
  choices.sort (true);
  _action->setActionChoices (choices);

  _action->setScript (slotAction.source);
  _action->setScriptErrors (slotAction.errors);

  _action->setActionName (action.existsAsFile ()
                              ? action.getFileNameWithoutExtension ()
                              : juce::String{});
}

void
A3MotionUIComponent::applyActionControl (int control, int increment)
{
  auto const channel = _clipSettingsChannel;
  auto const slot = _clipSettingsSlot;
  auto &pattern = _patterns[channel][slot];
  if (!pattern)
    return;

  switch (control)
    {
    case ActionComponent::Attack:
      pattern->setEnvelopeAttack (std::clamp (
          pattern->getEnvelopeAttack () + increment, 0, envelopeMaxStep));
      break;
    case ActionComponent::Decay:
      pattern->setEnvelopeDecay (std::clamp (
          pattern->getEnvelopeDecay () + increment, 0, envelopeMaxStep));
      break;
    case ActionComponent::EnvelopeMax:
      pattern->setEnvelopeMax (pattern->getEnvelopeMax () + increment * 0.05f);
      break;
    case ActionComponent::FreqAttack:
      pattern->setFreqAttack (pattern->getFreqAttack () + increment);
      break;
    case ActionComponent::FreqDecay:
      pattern->setFreqDecay (pattern->getFreqDecay () + increment);
      break;
    case ActionComponent::FreqMax:
      pattern->setFreqMax (pattern->getFreqMax () + increment * 0.05f);
      break;
    case ActionComponent::QAttack:
      pattern->setQAttack (pattern->getQAttack () + increment);
      break;
    case ActionComponent::QDecay:
      pattern->setQDecay (pattern->getQDecay () + increment);
      break;
    case ActionComponent::QMax:
      pattern->setQMax (pattern->getQMax () + increment * 0.05f);
      break;
    case ActionComponent::ActMode:
      // Modulo rather than a toggle: a tap in an open list arrives as the
      // difference to the entry tapped, and a toggle only happens to land
      // right while the list has two entries in it.
      {
        auto const now = pattern->getActMode () == ActMode::Hold ? 1 : 0;
        auto const next = (now + increment % value::numActModes
                           + value::numActModes)
                          % value::numActModes;
        pattern->setActMode (next == 1 ? ActMode::Hold : ActMode::OneShot);
      }
      break;
    default:
      return;
    }

  updateActionPage ();
  updateControlReadout (actionReadoutFor (control, *pattern));
}

void
A3MotionUIComponent::resetActionControl (int control)
{
  auto const channel = _clipSettingsChannel;
  auto const slot = _clipSettingsSlot;
  auto &pattern = _patterns[channel][slot];
  if (!pattern)
    return;

  // The mode is a list and has no middle to come back to.
  switch (control)
    {
    case ActionComponent::Attack:
      pattern->setEnvelopeAttack (envelopeMaxStep / 2);
      break;
    case ActionComponent::Decay:
      pattern->setEnvelopeDecay (envelopeMaxStep / 2);
      break;
    case ActionComponent::EnvelopeMax:
      pattern->setEnvelopeMax (0.5f);
      break;
    case ActionComponent::FreqAttack:
      pattern->setFreqAttack (envelopeMaxStep / 2);
      break;
    case ActionComponent::FreqDecay:
      pattern->setFreqDecay (envelopeMaxStep / 2);
      break;
    case ActionComponent::QAttack:
      pattern->setQAttack (envelopeMaxStep / 2);
      break;
    case ActionComponent::QDecay:
      pattern->setQDecay (envelopeMaxStep / 2);
      break;
    case ActionComponent::FreqMax:
      // Off, not half way: a filter sweep nobody asked for is a filter sweep
      // in the middle of a set.
      pattern->setFreqMax (0.f);
      break;
    case ActionComponent::QMax:
      pattern->setQMax (0.f);
      break;
    default:
      return;
    }

  updateActionPage ();
  updateControlReadout (actionReadoutFor (control, *pattern));
}

namespace
{
/** A TempoLfo step written the way a hand counts it: off, or the number of
 *  bars one cycle takes with the direction on the front. The step itself is
 *  an index into a table of powers of two and means nothing to read. */
juce::String
sweepReadout (int step)
{
  if (step == 0)
    return "off";

  auto const bars = lfoBarsPerCycle (std::abs (step));
  auto const number = bars >= 1.f
                          ? juce::String (juce::roundToInt (bars))
                          : "1/" + juce::String (juce::roundToInt (1.f / bars));

  return (step < 0 ? "-" : "") + number;
}
}

juce::String
A3MotionUIComponent::actionReadoutFor (int control, Pattern const &pattern)
{
  switch (control)
    {
    case ActionComponent::Attack:
      return "atk " + value::envelopeBarsName (pattern.getEnvelopeAttack ());
    case ActionComponent::Decay:
      return "dec " + value::envelopeBarsName (pattern.getEnvelopeDecay ());
    case ActionComponent::EnvelopeMax:
      return "max "
             + juce::String (juce::roundToInt (pattern.getEnvelopeMax ()
                                               * 100.f))
             + "%";
    case ActionComponent::FreqAttack:
      return "freq atk " + value::envelopeBarsName (pattern.getFreqAttack ());
    case ActionComponent::FreqDecay:
      return "freq dec " + value::envelopeBarsName (pattern.getFreqDecay ());
    case ActionComponent::FreqMax:
      return "freq max "
             + juce::String (juce::roundToInt (pattern.getFreqMax () * 100.f))
             + "%";
    case ActionComponent::QAttack:
      return "q atk " + value::envelopeBarsName (pattern.getQAttack ());
    case ActionComponent::QDecay:
      return "q dec " + value::envelopeBarsName (pattern.getQDecay ());
    case ActionComponent::QMax:
      return "q max "
             + juce::String (juce::roundToInt (pattern.getQMax () * 100.f))
             + "%";
    default:
      return juce::String ("act ")
             + value::actModeNames[pattern.getActMode () == ActMode::Hold ? 1
                                                                          : 0];
    }
}

void
A3MotionUIComponent::toggleRecordPage ()
{
  // Record turns the card over and turns it back, wherever it is pressed --
  // the panel's key, the bar's key, the tab. A key that only ever goes one way
  // leaves you tapping a different control to undo what it did.
  showBarPage (_barPage == BarPage::Record ? BarPage::Clip : BarPage::Record);
}

void
A3MotionUIComponent::handlePadRelease (index_t channel, index_t pad)
{
  auto const slot = slotForPadIndex[pad];

  // Action released -> exit the Shift+Action preview gesture. OSC fires from
  // the current position, the pattern keeps playing.
  //
  // Its own function rather than a branch inside the panel's listener,
  // because the screen's pads have to leave the gesture the same way they
  // entered it. A press that could be released only by the hardware would
  // leave a channel previewing with nothing on screen to stop it.
  if (padFunctionByPadIndex[pad] != PadFunction::Action)
    return;

  // The accent falls, whichever way the pad was used: with Shift it was a
  // preview and without it an instant start, but either way the finger has
  // left and the envelope has to hear that or it holds forever.
  _engine.setChannelAccentHeld (channel, false, nullptr);

  // And in Hold the clip falls with it, now rather than on a beat. Hold is a
  // stab: the whole point is that it lasts exactly as long as the finger, so
  // quantising the end would be quantising away the gesture.
  if (_actHeldSlot[channel] == static_cast<int> (slot))
    {
      _actHeldSlot[channel] = -1;
      if (auto const &held = _patterns[channel][slot])
        {
          auto const status = held->getStatus ();
          if (status == Pattern::Status::Playing
              || status == Pattern::Status::ScheduledForPlaying)
            _engine.stopPattern (held, _now);
        }
    }

  if (_previewHeldPad[channel] != static_cast<int> (slot))
    return;

  _engine.setPreviewMode (channel, false);
  _previewHeldPad[channel] = -1;

  if (_patterns[channel][slot])
    _motionComponent->unsetPreviewPattern (_patterns[channel][slot]);
}

void
A3MotionUIComponent::handleMessage (juce::Message const &message)
{
  using Status = MotionEngine::PatternStatusMessage::Status;
  auto const &messagePatternStatus
      = static_cast<MotionEngine::PatternStatusMessage const &> (message);

  auto const channel = messagePatternStatus.pattern->getChannel ();
  switch (messagePatternStatus.status)
    {
    case Status::Playing:
      {
        break;
      }
    case Status::Recording:
      {
        setPreviewWithDisplayData (messagePatternStatus.pattern);
        _channelStrips[channel]->setTextColour (toColour (theme ().danger));
        break;
      }
    case Status::Stopped:
      {
        _motionComponent->unsetPreviewPattern (messagePatternStatus.pattern);
        _channelStrips[channel]->setTextColour (
            toColour (theme ().textPrimary));

        // If this was a recording that just finished, save as user pattern.
        // We use wasRecording() because the status chain is:
        //   Recording → ScheduledForIdle → Idle
        // so getLastStatus() returns ScheduledForIdle, not Recording.
        // The take has definitely stopped now, so it is safe to start it: the
        // motion was looping anyway, and ending a take stops the writing
        // rather than the movement.
        if (_playWhenRecordingStops == messagePatternStatus.pattern)
          {
            _playWhenRecordingStops.reset ();
            _engine.playPattern (messagePatternStatus.pattern, _now);
          }

        if (messagePatternStatus.pattern->wasRecording ())
          {
            // Find which clip slot this pattern belongs to
            bool found = false;
            for (size_t slot = 0; slot < _patterns[channel].size (); ++slot)
              {
                if (_patterns[channel][slot] == messagePatternStatus.pattern)
                  {
                    std::cout << "  -> found in clip slot " << slot << std::endl;
                    saveRecordedPattern (messagePatternStatus.pattern,
                                         channel,
                                         static_cast<index_t> (slot));
                    found = true;
                    break;
                  }
              }
            if (!found)
              std::cout << "  -> pattern NOT found in any clip slot!" << std::endl;
          }
        break;
      }
    }
}

bool
A3MotionUIComponent::isButtonPressed (Button button)
{
  // The panel's key or the screen's, whichever is down. One state, so that
  // everything asking "is Shift held" gets the same answer no matter which
  // of the two the hand is on — and so that a build with no panel can still
  // reach the gestures that need a modifier.
  if (button == Button::Shift && _screenShiftHeld)
    return true;

  return _ioAdapter->getButton (button).getValue ();
}

void
A3MotionUIComponent::tickCallback (Measure measure)
{
  _now = measure;

  // This thread's own copy of the beat address, taken over here and read
  // nowhere else.
  applyPendingBeatAddress ();

  // Send beat via OSC on every beat (only in INT mode to avoid feedback with external clock)
  // AsyncOSCSender enqueues to lock-free FIFO, safe to call from any thread
  if (_clockMode == 0 && measure.tick () == 0)
    {
      auto beatClockMsg = juce::OSCMessage (_beatAddress);
      beatClockMsg.addInt32 (measure.beat () + 1);  // 1-indexed beat
      beatClockMsg.addInt32 (measure.bar () + 1);   // 1-indexed bar
      beatClockMsg.addInt32 (static_cast<int> (std::round (_engine.getTempoBPM ())));
      _oscSender.send (beatClockMsg);
    }

  if (runsOnHardware ())
    {
      if (measure.beat () == 0 && measure.tick () == 0)
        {
          _stepsLED = 0;
        }

      using T =
          typename std::remove_reference<decltype (measure.tick ())>::type;
      jassert (ticksPerStepPadLEDs <= std::numeric_limits<T>::max ());
      auto const divisor = static_cast<T> (ticksPerStepPadLEDs);
      if (measure.tick () % divisor == 0)
        {
          padLEDCallback (_stepsLED++);

          // All three rows follow their envelopes while they run. On the
          // same tick as the pad LEDs because it is the same kind of thing —
          // what is shown catching up with what the engine is doing — and
          // often enough to read as movement without repainting the bar every
          // tick. setChannelValues() repaints only when something moved, so a
          // still grid costs nothing here.
          refreshChannelValues ();
        }

      if (!_ioAdapter->getButton (Button::Record).getValue ())
        {
        }
    }

  // Throttle repaint to ~30 Hz (every 4th tick at typical tick rate)
  // The channel strips are hidden but progress values still update
  bool shouldRepaint = (measure.tick () % 4 == 0);

  auto recordingPattern = _engine.getRecordingPattern ();
  if (recordingPattern)
    {
      auto const channel = recordingPattern->getChannel ();
      _motionComponent->setBackgroundColour (
          _channelUIStates[channel]->colour.withAlpha (0.2f));

      auto const progress
          = recordingPattern->getLastUpdatedTick ()
            / static_cast<float> (recordingPattern->getNumTicks ());
      _channelUIStates[channel]->progress = progress;
      if (shouldRepaint)
        _channelStrips[channel]->repaint ();
    }
  else
    {
      // No background rather than a black one — the sphere shows through.
      _motionComponent->setBackgroundColour (juce::Colours::transparentBlack);
    }

  // Update loop length display with global playhead (INT mode only).
  // Display = 1 bar reference.  Playhead = position within the bar.
  // In EXT mode, LoopLengthDisplay interpolates from setExternalBeat().
  if (_clockMode == 0)
    {
      auto const beatsPerBar = _engine.getBeatsPerBar ();
      auto const ticksPerBeat
          = static_cast<float> (TempoClock::getTicksPerBeat ());
      auto const totalTicksPerBar
          = static_cast<float> (beatsPerBar) * ticksPerBeat;

      auto const ticksInBar
          = static_cast<float> (measure.beat ()) * ticksPerBeat
            + static_cast<float> (measure.tick ());
      auto const barPosition = ticksInBar / totalTicksPerBar;

      for (auto channel = 0u; channel < _engine.getNumChannels (); ++channel)
        {
          _loopLengthDisplay->setPlayheadPosition (
              static_cast<int> (channel), barPosition);
        }
    }

  for (auto channel = 0u; channel < _engine.getNumChannels (); ++channel)
    {
      auto playingPattern = _engine.getPlayingPattern (channel);
      if (playingPattern)
        {
          auto const playPosition = playingPattern->getPlayPosition ();
          _channelUIStates[channel]->progress = playPosition;
          if (shouldRepaint)
            _channelStrips[channel]->repaint ();
        }
      else if (!recordingPattern || recordingPattern->getChannel () != channel)
        {
          if (!juce::exactlyEqual (_channelUIStates[channel]->progress, 1.f))
            {
              _channelUIStates[channel]->progress = 1.f;
              _channelStrips[channel]->repaint ();
            }
        }
    }
}

juce::File
A3MotionUIComponent::setFilePath () const
{
  if (userConfig.hasProperty ("setFile"))
    return juce::File (userConfig["setFile"].toString ());

  // Beside the takes by default, so the two travel together without anyone
  // having to configure that they do.
  //
  // "current" rather than "set": it is the session the device comes back to,
  // and it is an ordinary session like the named ones beside it -- the only
  // difference is that nobody chose its name.
  return _patternLibrary->getRootDir ().getChildFile ("current.json");
}

void
A3MotionUIComponent::applySet ()
{
  applySet (setFilePath ());
}

void
A3MotionUIComponent::applySet (juce::File const &file)
{
  auto const numChannels = static_cast<int> (_patterns.size ());
  auto const set = loadSession (file, numChannels,
                            static_cast<int> (numClipSlots));

  for (int ch = 0; ch < numChannels; ++ch)
    {
      auto const index = static_cast<index_t> (ch);
      auto const &channel = set.channels[static_cast<size_t> (ch)];

      // Where the channel was parked. Empty on a first run, which is zero,
      // which is where they start anyway.
      _engine.setChannelPot1 (index, channel.freq);
      _engine.setChannelPot2 (index, channel.q);
      _engine.setChannelPot3 (index, channel.threeD);

      for (index_t slot = 0; slot < numClipSlots; ++slot)
        {
          auto const &saved = channel.slots[static_cast<size_t> (slot)];
          _clipUIParams[index][slot].recordLengthLog2
              = saved.recordLengthLog2;

          // The action first, so a slot that fires one gets it whether or not
          // it also holds a clip.
          if (!saved.action.empty ())
            setSlotAction (index, slot,
                           actionsDir ().getChildFile (
                               juce::String (saved.action) + ".scd"));

          if (saved.patternName.empty ())
            continue;

          // Named, not indexed: a library's order depends on what is in the
          // folder, and a set that meant "the third file" would mean
          // something else on the next stick.
          auto const libIndex = _patternLibrary->indexForName (
              saved.patternName);
          if (libIndex <= 0)
            continue;

          fillSlotFromLibrary (index, slot, libIndex);

          // Which clip file the slot came from. fillSlotFromLibrary() sets it
          // from the shape's own clip, which is right for a slot filled from
          // the library and wrong for one a settings preset was dropped on --
          // the preset is where its values came from, and Save has to write
          // back there.
          if (!saved.clipFile.empty ())
            {
              auto const clip = _patternLibrary->getClipDir ().getChildFile (
                  juce::String (saved.clipFile) + ".json");
              if (clip.existsAsFile ())
                _slotClipFile[index][slot] = clip;
            }

          // And what the slot was turned to. A set written before this has
          // none, and then the clip's own settings are what the slot keeps --
          // applying the defaults there would reset every clip in it.
          if (saved.overrides.has_value ())
            if (auto const &pattern = _patterns[index][slot])
              {
                applyClipSettings (*pattern, *saved.overrides);
                syncClipUIParamsFromPattern (index, slot);
              }

          // And what was running runs again -- from the top, on the next
          // downbeat. Not from where it was: coming back mid-figure would put
          // the set down somewhere other than the beginning of its own
          // movement, and where it happened to be when Save was pressed is
          // not a thing anybody chose. The downbeat rather than the next beat
          // because this is several clips starting together, and together is
          // the whole point of a set -- a pad press is one clip and gets the
          // nearer quantisation.
          if (saved.playing)
            if (auto const &pattern = _patterns[index][slot])
              {
                pattern->setPlaybackLength (getPlaybackLength (index, slot));
                _engine.playPattern (pattern,
                                     TempoClock::nextDownBeat (_now));
              }
        }
    }
}

void
A3MotionUIComponent::scheduleSetSave ()
{
  // Debounced. A drag across the grid is dozens of changes and one
  // arrangement, and this ends in a file write.
  auto const generation = ++_setSaveGeneration;

  juce::Timer::callAfterDelay (
      1500,
      [safeThis = juce::Component::SafePointer<A3MotionUIComponent> (this),
       generation] {
        if (safeThis == nullptr)
          return;
        if (safeThis->_setSaveGeneration != generation)
          return; // something changed since; that one will write

        safeThis->writeSet ();
      });
}

void
A3MotionUIComponent::writeSet ()
{
  saveSession (setFilePath (), buildSession ());
}

Session
A3MotionUIComponent::buildSession ()
{
  Session set;
  set.channels.resize (_patterns.size ());

  for (size_t ch = 0; ch < _patterns.size (); ++ch)
    {
      auto const index = static_cast<index_t> (ch);
      auto &channel = set.channels[ch];

      channel.freq = _engine.getChannelPot1 (index);
      channel.q = _engine.getChannelPot2 (index);
      // What was set, not what the accent is doing to it right now: a set
      // that saved mid-accent would come back with the accent baked in.
      channel.threeD = _engine.getChannelPot3 (index);

      channel.slots.resize (numClipSlots);
      for (index_t slot = 0; slot < numClipSlots; ++slot)
        {
          auto &saved = channel.slots[static_cast<size_t> (slot)];
          saved.recordLengthLog2
              = _clipUIParams[index][slot].recordLengthLog2;

          // The action goes with the slot whether or not there is a clip in
          // it: a slot can be given one and filled afterwards.
          saved.action = _slotAction[index][slot]
                             .file.getFileNameWithoutExtension ()
                             .toStdString ();

          if (auto const &pattern = _patterns[index][slot])
            {
              saved.patternName = pattern->getName ();
              saved.clipFile = _slotClipFile[index][slot]
                                   .getFileNameWithoutExtension ()
                                   .toStdString ();

              // Running counts as running: a clip waiting for the next beat
              // is one somebody has already started, and a set saved in that
              // half-second should not come back silent.
              auto const status = pattern->getStatus ();
              saved.playing
                  = status == Pattern::Status::Playing
                    || status == Pattern::Status::ScheduledForPlaying;

              // Everything, not only what differs from the clip's own file.
              // The difference was worked out with clipHasDrifted(), which
              // answers false for a slot with no clip file at all -- so every
              // slot filled straight from a shape wrote nothing, and came
              // back as the bare shape with its settings gone.
              saved.overrides = clipSettingsFrom (*pattern);
            }
        }
    }

  return set;
}

void
A3MotionUIComponent::refreshChannelValues ()
{
  if (!_clipSettings)
    return;

  // Every channel's three, not only the shown one's: the grid belongs to the
  // channels rather than to the clip on display. Each goes in with its
  // *effective* value beside it — the setting with its envelope laid over it —
  // because a modulation that moves nothing on screen is one you have to take
  // on trust. Only 3d used to; freq and Q were sent moving and drawn still.
  for (int ch = 0; ch < numChannelColumns; ++ch)
    {
      auto const index = static_cast<index_t> (ch);
      _clipSettings->setChannelValues (
          ch, _engine.getChannelPot1 (index),
          _engine.getChannelPot1Effective (index),
          _engine.getChannelPot2 (index),
          _engine.getChannelPot2Effective (index),
          _engine.getChannelPot3 (index),
          _engine.getChannelPot3Effective (index));
    }
}

void
A3MotionUIComponent::updateFunctionKeyLEDs ()
{
  if (!_ioAdapter || !_clipSettings)
    return;

  // The panel's half of what theme/FunctionKeyColours.hh decides; the global
  // strip paints the other half from the same look. A key that is coloured on
  // the screen is coloured under the hand, and neither is worked out twice.
  auto const look = _clipSettings->functionKeyLook ();

  for (auto const key : functionKeyOrder)
    _ioAdapter->setButtonLED (key, functionKeyColour (key, look));
}

void
A3MotionUIComponent::pulseTapLED ()
{
  // Louder than the screen's, on purpose. On a lit screen the beat is a faint
  // wash — any more and it is a blink you watch instead of one you catch. An
  // LED in a dark booth is the other way round: at anything less than its own
  // colour, full on, it is not a metronome, it is a flicker.
  //
  // The colour itself still comes from the one rule; only how long it stays
  // is decided here.
  updateFunctionKeyLEDs ();

  juce::Timer::callAfterDelay (
      tapBeatLedMillis,
      [safeThis = juce::Component::SafePointer<A3MotionUIComponent> (this)] {
        if (safeThis != nullptr)
          safeThis->updateFunctionKeyLEDs ();
      });
}

void
A3MotionUIComponent::padLEDCallback (int step)
{
  for (auto channel = 0u; channel < _ioAdapter->getNumChannels (); ++channel)
    {
      auto const channelColour = _channelUIStates[channel]->colour;
      for (auto pad = 0u; pad < _ioAdapter->getNumPadsPerChannel (); ++pad)
        {
          // All 4 buttons of a clip slot share that slot's Pattern, so
          // their LEDs stay in sync.
          auto const slot = slotForPadIndex[pad];
          auto const status = _patterns[channel][slot]
                                   ? _patterns[channel][slot]->getStatus ()
                                   : Pattern::Status::Empty;
          auto const statusLast
              = _patterns[channel][slot]
                    ? _patterns[channel][slot]->getLastStatus ()
                    : Pattern::Status::Empty;

          // Play|Pause is the one pad that must be readable at a glance:
          // green while actually playing, channel colour otherwise (idle/
          // empty/recording), so play vs. paused/stopped is unambiguous.
          bool const isPlayingOnPlayPause
              = padFunctionByPadIndex[pad] == PadFunction::PlayPause
                && (status == Pattern::Status::Playing
                    || status == Pattern::Status::ScheduledForPlaying);
          auto const base
              = isPlayingOnPlayPause ? toColour (theme ().accent)
                                     : channelColour;

          auto const colour = channelColourForPadStatus (
              base, status, statusLast, step);
          _ioAdapter->getPadLED (channel, pad)
              = juce::VariantConverter<juce::Colour>::toVar (colour);

          // The same colour to the screen. One place works out what empty,
          // idle, armed and running look like; two places show it.
          if (_controller)
            _controller->setPadColour (channel, pad, colour,
                                       isPlayingOnPlayPause);
        }
    }
}

juce::Colour
A3MotionUIComponent::channelColourForPadStatus (juce::Colour base,
                                                Pattern::Status status,
                                                Pattern::Status statusLast,
                                                int step)
{
  switch (status)
    {
    case Pattern::Status::Empty:
      return base.darker (0.85f);
    case Pattern::Status::Idle:
      return base.darker (0.3f);
    case Pattern::Status::ScheduledForRecording:
      return step % 2 == 0 ? base : base.darker (0.6f);
    case Pattern::Status::Recording:
      return base;
    case Pattern::Status::ScheduledForPlaying:
      return step % 2 == 0 ? base : base.darker (0.6f);
    case Pattern::Status::Playing:
      return base;
    case Pattern::Status::ScheduledForIdle:
      jassert (statusLast != Pattern::Status::ScheduledForRecording
               && statusLast != Pattern::Status::Idle);
      return scheduledForIdleLEDColour (base, step, statusLast);
    }
  return base.darker (0.85f);
}

juce::Colour
A3MotionUIComponent::scheduledForIdleLEDColour (juce::Colour base, int step,
                                                Pattern::Status statusLast)
{
  // one-shot recording: don't blink when scheduled for idle
  if (_engine.getRecordingMode () == MotionEngine::RecordingMode::OneShot
      && statusLast == Pattern::Status::Recording)
    {
      return base;
    }

  if (step % 2 == 0)
    {
      return base.darker (0.85f);
    }
  else
    {
      return base.darker (0.6f);
    }
}

void
A3MotionUIComponent::createPadRowDisplays ()
{
  for (auto slot = 0u; slot < numClipSlots; ++slot)
    {
      auto display = std::make_unique<PadRowDisplay> (static_cast<int> (slot));
      for (auto ch = 0u; ch < _channelUIStates.size ()
                         && ch < PadRowDisplay::numChannels;
           ++ch)
        {
          display->setChannelColour (static_cast<int> (ch),
                                     _channelUIStates[ch]->colour);
        }
      addChildComponent (*display);
      // Hidden: no longer part of the visible layout (see resized()), but
      // keeps receiving its normal update calls underneath.
      display->setVisible (false);
      _padRowDisplays.push_back (std::move (display));
    }

  // Set initial trajectory icons now that all displays exist
  for (auto slot = 0u; slot < numClipSlots; ++slot)
    {
      for (auto ch = 0u; ch < _engine.getNumChannels (); ++ch)
        {
          if (slot < _patterns[ch].size () && _patterns[ch][slot])
            {
              updatePadRowLabel (ch, slot);
              registerPatternDisplayData (_patterns[ch][slot]);
            }
        }
    }

  // Set initial row highlight on LoopLength row
  for (auto ch = 0u; ch < _engine.getNumChannels (); ++ch)
    {
      _loopLengthDisplay->setRowHighlighted (static_cast<int> (ch), true);
    }
}

void
A3MotionUIComponent::updatePadRowLabel (index_t channel, index_t slot)
{
  if (slot >= numClipSlots)
    return;

  if (slot >= _padRowDisplays.size ())
    return;

  if (_patterns[channel][slot])
    {
      auto const &name = _patterns[channel][slot]->getName ();
      auto libIndex = _patternLibrary->indexForName (name);

      if (libIndex > 0)
        {
          // Use SVG path from the library entry for the icon
          auto const &entry = _patternLibrary->getEntry (libIndex);
          auto iconPath = svgDToPath (entry.svgPathData);
          if (!iconPath.isEmpty () || entry.hasJumpDots)
            {
              _padRowDisplays[slot]->setIconPath (
                  static_cast<int> (channel),
                  iconPath,
                  entry.jumpDots);
            }
          else
            {
              _padRowDisplays[slot]->setTickData (
                  static_cast<int> (channel), entry.ticks);
            }
          // Show pattern length in beats
          _padRowDisplays[slot]->setLengthBeats (
              static_cast<int> (channel), entry.lengthBeats);
          // Category prefix: "S" for system, "U" for user
          _padRowDisplays[slot]->setCategoryPrefix (
              static_cast<int> (channel),
              entry.category == PatternLibrary::Category::System ? "S" : "U");
        }
      else
        {
          // Pattern not in library (e.g. newly recorded, not yet saved)
          // Generate icon from the pattern's own tick data
          auto ticks = _patterns[channel][slot]->getTicks ();
          _padRowDisplays[slot]->setTickData (static_cast<int> (channel),
                                              ticks.positions);
          // Compute beats from tick count
          auto numTicks = _patterns[channel][slot]->getNumTicks ();
          auto ticksPerBeat = TempoClock::getTicksPerBeat ();
          int beats = ticksPerBeat > 0
                          ? static_cast<int> (numTicks / ticksPerBeat)
                          : 0;
          _padRowDisplays[slot]->setLengthBeats (
              static_cast<int> (channel), beats);
          _padRowDisplays[slot]->setCategoryPrefix (
              static_cast<int> (channel), "U");
        }
    }
  else
    {
      // Empty: clear tick data and set Empty type
      _padRowDisplays[slot]->setTickData (static_cast<int> (channel), {});
      _padRowDisplays[slot]->setTrajectoryType (
          static_cast<int> (channel), PadRowDisplay::TrajectoryType::Empty);
      _padRowDisplays[slot]->setLengthBeats (
          static_cast<int> (channel), 0);
      // Use the category from the library slot (slot+1) for the prefix,
      // even when the current channel has no pattern loaded
      auto const libSlotIndex = static_cast<int> (slot) + 1;
      if (libSlotIndex < _patternLibrary->getNumEntries ())
        {
          auto const &slotEntry = _patternLibrary->getEntry (libSlotIndex);
          _padRowDisplays[slot]->setCategoryPrefix (
              static_cast<int> (channel),
              slotEntry.category == PatternLibrary::Category::System ? "S" : "U");
        }
      else
        {
          _padRowDisplays[slot]->setCategoryPrefix (
              static_cast<int> (channel), "");
        }
    }
}

void
A3MotionUIComponent::setPreviewWithDisplayData (
    std::shared_ptr<Pattern> const &pattern)
{
  if (!pattern)
    return;

  auto const &name = pattern->getName ();
  auto libIndex = _patternLibrary->indexForName (name);

  if (libIndex > 0)
    {
      auto const &entry = _patternLibrary->getEntry (libIndex);
      auto displayPath = svgDToPath (entry.svgPathData);
      _motionComponent->setPreviewPattern (pattern, displayPath,
                                           entry.jumpDots);
    }
  else
    {
      // No library entry (e.g. live recording) — no display path
      _motionComponent->setPreviewPattern (pattern);
    }
}

void
A3MotionUIComponent::refreshPatternDisplayFromTicks (
    std::shared_ptr<Pattern> const &pattern)
{
  if (!pattern || !_motionComponent)
    return;

  // Cut at teleports as well as at gaps, the same way the take's own trail is
  // drawn, so a jump the clip still has is not bridged by a line.
  // The one place that follows the clip's own plan: this is the line the blob
  // runs on, so it has to break exactly where the movement jumps.
  juce::Path path;
  for (auto const &segment : trajectorySegments (
           pattern->getTicks ().positions, pattern->getBridgePlan ()))
    {
      path.startNewSubPath (segment.front ().x (), segment.front ().y ());
      for (size_t i = 1; i < segment.size (); ++i)
        path.lineTo (segment[i].x (), segment[i].y ());
    }

  _motionComponent->setPatternDisplayData (pattern, path, {});
}

void
A3MotionUIComponent::registerPatternDisplayData (
    std::shared_ptr<Pattern> const &pattern)
{
  // Called from fillSlotFromLibrary(), which also runs while the interface is
  // still being built: at startup the slots are filled before there is
  // anything to draw them on, and the initial pass registers them again once
  // there is.
  if (!pattern || !_motionComponent)
    return;

  auto const &name = pattern->getName ();
  auto libIndex = _patternLibrary->indexForName (name);

  // A shape made of dots has no line to draw, and its dots are only in the
  // file: keep taking those from the library.
  if (libIndex > 0)
    {
      auto const &entry = _patternLibrary->getEntry (libIndex);
      if (entry.hasJumpDots && svgDToPath (entry.svgPathData).isEmpty ())
        {
          _motionComponent->setPatternDisplayData (pattern, {},
                                                   entry.jumpDots);
          return;
        }
    }

  // Everything else is drawn from the ticks, because that is what plays.
  //
  // Taking the line from the file was right while the file was a picture of
  // the pattern. It stopped being one when the take started going to disk as
  // it was played, with the closing move a setting laid over it: the blob
  // followed the ending the fade gives it and the line showed a take whose
  // ends do not meet.
  refreshPatternDisplayFromTicks (pattern);
}

int
A3MotionUIComponent::trajectoryNameToIndex (std::string const &name) const
{
  return _patternLibrary->indexForName (name);
}

std::shared_ptr<Pattern>
A3MotionUIComponent::createPatternForIndex (int index, index_t channel)
{
  auto p = _patternLibrary->loadPattern (index);
  if (p)
    p->setChannel (channel);
  return p;
}

void
A3MotionUIComponent::endRecording ()
{
  if (_clipSettings)
    _clipSettings->setRecording (false);
    updateFunctionKeyLEDs ();

    // And back to the clip's own face: the take is made, so what there is to
    // look at is what it plays.
    if (_barPage == BarPage::Record)
      showBarPage (BarPage::Clip);

  auto pattern = _engine.getRecordingPattern ();
  if (!pattern || !_recordingSlot.has_value ())
    {
      // Nothing has started yet: the take was scheduled and is being called
      // off before its downbeat. Put the slot back the way it was and clear
      // the request, or the next press would find one still standing.
      if (_recordingSlot.has_value ())
        {
          auto const channel = _recordingSlot->first;
          auto const slot = _recordingSlot->second;
          _patterns[channel][slot] = _patternBeforeRecording;
          if (_motionComponent)
            _motionComponent->setRecordingUnderlay (nullptr);
          _recordingSlot.reset ();
          _patternBeforeRecording = nullptr;
          updateClipSettingsDisplay ();
        }
      return;
    }

  auto const channel = _recordingSlot->first;
  auto const slot = _recordingSlot->second;

  auto const written = pattern->writtenTicks ();
  auto const anyWritten
      = std::any_of (written.begin (), written.end (),
                     [] (bool isWritten) { return isWritten; });

  // Where the write head stands is where the take stops, and that edge -- the
  // last tick of the freshest pass against the previous pass still sitting
  // after it -- is what breaks visibly. Asked before stopping, because the
  // engine forgets it the moment it does.
  auto const progress = _engine.getRecordingProgress ();
  auto const stopTick
      = progress >= 0.f
            ? std::optional<index_t>{ static_cast<index_t> (
                  progress * static_cast<float> (pattern->getNumTicks ())) }
            : std::nullopt;

  if (anyWritten)
    {
      // Before stopping, because the save happens on the Stopped message and
      // has to carry the filled stretches with it. No fade length goes in any
      // more: every stretch is held, and whether the take's own join is drawn
      // through is read from the clip at playback.
      closeRecordingSeams (*pattern, stopTick);
    }

  _engine.stopPattern (pattern, _now);

  if (anyWritten)
    {
      // Started from the Stopped message rather than here. Stopping is
      // asynchronous, so playing straight after it left the pattern in a state
      // the Play pad does not know — and the first press on it did nothing.
      pattern->setPlaybackLength (getPlaybackLength (channel, slot));
      _playWhenRecordingStops = pattern;
    }
  else
    {
      // Nothing was ever played into it. Put back what the slot held rather
      // than leaving a clip made of nothing.
      _patterns[channel][slot] = _patternBeforeRecording;
      updateControlReadout ("recording discarded - nothing played");
    }

  _recordingSlot.reset ();
  _patternBeforeRecording.reset ();
  if (_motionComponent)
    _motionComponent->setRecordingUnderlay (nullptr);
  selectClip (channel, slot);
}

void
A3MotionUIComponent::saveRecordedPattern (
    std::shared_ptr<Pattern> const &pattern, index_t channel, index_t slot)
{
  if (!pattern || pattern->getNumTicks () == 0)
    return;

  // Name the recording with a timestamp
  auto now = juce::Time::getCurrentTime ();
  auto name = "Rec_" + now.formatted ("%H%M%S").toStdString ();
  pattern->setName (name);

  // Save to user directory
  auto newIndex = _patternLibrary->saveUserPattern (pattern);
  std::cout << "saveRecordedPattern: channel=" << channel
            << " slot=" << slot
            << " ticks=" << pattern->getNumTicks ()
            << " name=" << name
            << " newIndex=" << newIndex << std::endl;
  if (newIndex > 0)
    {
      std::cout << "Recording saved as user pattern '" << name
                << "' at library index " << newIndex << std::endl;

      // The slot now holds a take with a name, which is what a set records.
      scheduleSetSave ();

      // The take stays in the slot. It used to be swapped for a fresh load of
      // the file it had just been written to, for the sake of the display data
      // that came with it -- and the line is drawn from the ticks now, so
      // there is nothing left to fetch.
      //
      // The round trip cost more than it gave: loading resamples the shape by
      // arc length, so every tick moves. The tick where the take stopped means
      // nothing afterwards, so the closing move landed somewhere else, and the
      // stretch between two subpaths came back as a hole -- 45 ticks of one on
      // the take that showed it.
      registerPatternDisplayData (pattern);

      // Update fingerprint so the timer doesn't re-trigger for this save
      _lastLibraryFingerprint = _patternLibrary->getDirectoryFingerprint ();

      // Refresh the pad cell display to show the new recording
      updatePadRowLabel (channel, slot);

      // And the clip settings bar, if it happens to be showing this slot.
      // It reads _patterns[channel][slot], which was just replaced — without
      // this it went on showing the pattern that was there before, until the
      // next encoder turn happened to refresh it for another reason.
      if (channel == _clipSettingsChannel && slot == _clipSettingsSlot)
        updateClipSettingsDisplay ();
    }
}

void
A3MotionUIComponent::refreshAllPadRowLabels ()
{
  auto const numChannels = _engine.getNumChannels ();
  for (index_t ch = 0; ch < numChannels; ++ch)
    {
      for (index_t slot = 0; slot < numClipSlots; ++slot)
        {
          updatePadRowLabel (ch, slot);
        }
    }
}

void
A3MotionUIComponent::timerCallback ()
{
  // While a take runs its write head moves, and while a clip plays its
  // playhead does. Both fill the tick indicator, so both have to be followed
  // -- watching only the recording is what left a playing clip with a
  // transport key that never turned green and an indicator that stayed empty.
  // The rest of the time nothing here changes on its own.
  auto const &shown = _patterns[_clipSettingsChannel][_clipSettingsSlot];

  // One tick past the accent as well as during it. An action puts its
  // settings on the clip and takes them off again, and the taking off is the
  // last thing that happens -- a screen that stopped following one tick
  // earlier would show the action's values for as long as the page stayed
  // open.
  auto const accent = _engine.isChannelAccentActive (_clipSettingsChannel);
  if (_engine.isRecording () || accent || _accentWasActive
      || (shown && shown->getStatus () == Pattern::Status::Playing))
    updateClipSettingsDisplay ();
  _accentWasActive = accent;

  // Every fortieth tick, which is the two seconds this used to run at.
  if (++_timerTick % 40 != 0)
    return;

  // Periodically check if pattern directories have changed
  auto fp = _patternLibrary->getDirectoryFingerprint ();
  if (fp != _lastLibraryFingerprint)
    {
      _lastLibraryFingerprint = fp;
      std::cout << "PatternLibrary: directory change detected, refreshing..."
                << std::endl;
      _patternLibrary->refresh ();
      refreshAllPadRowLabels ();
      // The browser holds a copy of the list. Refreshing the library behind an
      // open browser and leaving its rows alone is how a file that is plainly
      // in the folder stays missing from the only list that shows it.
      if (_barPage == BarPage::Browser)
        refreshBrowser ();
    }
}

void
A3MotionUIComponent::oscBundleReceived (const juce::OSCBundle &bundle)
{
  for (auto &element : bundle)
    {
      if (element.isMessage ())
        oscMessageReceived (element.getMessage ());
      else if (element.isBundle ())
        oscBundleReceived (element.getBundle ());
    }
}

void
A3MotionUIComponent::oscMessageReceived (const juce::OSCMessage &message)
{
  _oscMessageHandler->handleMessage (message, _clockMode);
}

void
A3MotionUIComponent::onChannelVU (int channel, float peak, float rms)
{
  if (static_cast<size_t> (channel) < _channelUIStates.size ())
    {
      _channelUIStates[static_cast<size_t> (channel)]->vuPeak = peak;
      _channelUIStates[static_cast<size_t> (channel)]->vuLevel = rms;
    }
}

void
A3MotionUIComponent::onSubwooferVU (float peak, float rms)
{
  _motionComponent->setSphereGlow (peak, rms);
}

void
A3MotionUIComponent::onEnergyGrid (float const *values, int count)
{
  _motionComponent->setEnergyGrid (values, count);
}

void
A3MotionUIComponent::onSpeakerVU (int speakerIndex, float peak, float rms)
{
  _motionComponent->setSpeakerLight (speakerIndex, peak, rms);
}

void
A3MotionUIComponent::onExternalBeatClock (int beat, int bar, float bpm)
{
  _statusBar->setExternalBPM (bpm);
  _statusBar->setBeatClock (beat, bar);
}

void
A3MotionUIComponent::onExternalBeatSync (int beat, int beatsPerBar)
{
  _loopLengthDisplay->setExternalBeat (beat, beatsPerBar);
}

// ── Global Settings helpers ──────────────────────────────────────────────────────

void
A3MotionUIComponent::openGlobalSettings ()
{
  // The key says where you are. Told here rather than by whoever
  // opened it: there are three ways in (the panel's key, the strip's,
  // the encoder) and a key that only lit for some of them would be
  // worse than one that never lit at all.
  if (_clipSettings)
    _clipSettings->setMenuOpen (true);
  if (runsOnHardware ())
    updateFunctionKeyLEDs ();

  if (_globalSettingsOpen)
    return;

  _globalSettingsOpen = true;
  _globalSettingsValueFieldSelected = false;
  _globalSettingsOptionIndex = 0;

  rebuildGlobalSettingsOptions ();

  // Reuse the Clip Settings panel's safe zone (real screen space carved out
  // of MotionComponent's bounds in resized(), not overlapping its OpenGL
  // context) so the menu has room to show its option rows properly.
  if (_clipSettings)
    _globalSettings->setBounds (_clipSettings->getBounds ());

  _globalSettings->setVisible (true);
  _globalSettings->toFront (true);
  resized (); // the menu takes the whole window, the sphere gives up its bounds

  // Pausing here was a concession to the RPi4's GPU. The rig runs on an Intel
  // NUC now and the sphere is meant to carry on behind the menu, but the option
  // stays for a machine that needs it again.
  if (_motionComponent && _pauseRenderingInMenu)
    _motionComponent->setRenderingPaused (true);

  updateOverlayButtons ();
}

void
A3MotionUIComponent::rebuildGlobalSettingsOptions ()
{
  // Built fresh rather than patched: the list of skins changes underneath it
  // whenever the editor saves, renames or deletes one.
  std::vector<GlobalSettingsComponent::Option> options;
  _menuRowOrder.clear ();

  // Each row is added with what it does, so the two cannot drift apart.
  auto const add = [&] (MenuRow row, GlobalSettingsComponent::Option option) {
    _menuRowOrder.push_back (row);
    options.push_back (std::move (option));
  };

  // Clockmode is not here any more: it is a button in the clip settings bar,
  // where it is visible and switchable without opening anything. A setting
  // that lives in two places is a setting you have to remember the location
  // of.

  // Built from what is actually in config/skins, so a skin added on the
  // device shows up without a rebuild.
  _skinNames = availableSkins (getConfigFile ().getParentDirectory ());
  auto const active = activeSkinName (getConfigFile ());
  _skinIndex = juce::jmax (0, _skinNames.indexOf (active));

  std::vector<GlobalSettingsComponent::ValueItem> skinValues;
  for (auto const &name : _skinNames)
    skinValues.push_back ({ name });

  add (MenuRow::Skin, { "Skin", std::move (skinValues), _skinIndex });
  add (MenuRow::SkinEditor, { "Skin Editor", { { "open" } }, 0, true });
  add (MenuRow::Network, { "Network", { { "open" } }, 0, true });
  add (MenuRow::ButtonLeds, { "Button LEDs", { { "open" } }, 0, true });
  add (MenuRow::PatternFolder, { "Pattern Folder", { { "open" } }, 0, true });
  add (MenuRow::SphereInMenu,
       { "Sphere in Menu", { { "off" }, { "on" } },
         _pauseRenderingInMenu ? 0 : 1 });
  _globalSettings->setOptions (std::move (options));
  _globalSettings->setOptionIndex (_globalSettingsOptionIndex);
  _globalSettings->setValueFieldSelected (false);
}

void
A3MotionUIComponent::closeGlobalSettings ()
{
  // The key says where you are. Told here rather than by whoever
  // opened it: there are three ways in (the panel's key, the strip's,
  // the encoder) and a key that only lit for some of them would be
  // worse than one that never lit at all.
  if (_clipSettings)
    _clipSettings->setMenuOpen (false);
  if (runsOnHardware ())
    updateFunctionKeyLEDs ();

  if (!_globalSettingsOpen)
    return;

  // The editor is a page of this menu, so closing the menu leaves it first —
  // that is also what saves the edited skin.
  closeSkinEditor ();

  _globalSettingsOpen = false;
  _globalSettingsValueFieldSelected = false;
  _globalSettings->setVisible (false);
  _globalSettings->setValueFieldSelected (false);
  resized (); // sphere and clip settings get their bounds back
  if (_motionComponent)
    _motionComponent->setRenderingPaused (false);

  updateOverlayButtons ();
}

std::optional<A3MotionUIComponent::MenuRow>
A3MotionUIComponent::browsedMenuRow () const
{
  if (_globalSettingsOptionIndex < 0
      || _globalSettingsOptionIndex >= (int)_menuRowOrder.size ())
    return {};

  return _menuRowOrder[(size_t)_globalSettingsOptionIndex];
}

void
A3MotionUIComponent::confirmGlobalSettingsOption ()
{
  if (!_globalSettingsOpen || !_globalSettingsValueFieldSelected)
    return;

  int const chosen = _globalSettings->getSelectedValueIndex ();

  auto const row = browsedMenuRow ();
  if (!row.has_value ())
    return;

  switch (row.value ())
    {
    case MenuRow::Skin: applySkin (chosen); break;
    case MenuRow::SkinEditor: openSkinEditor (); break;
    case MenuRow::Network:
      openConfigPage ("Network",
                      { "oscSender", "oscReceiver", "oscAddresses" });
      break;
    case MenuRow::ButtonLeds:
      openConfigPage ("Button LEDs", { "buttonLeds" });
      break;
    case MenuRow::PatternFolder:
      openConfigPage ("Pattern Folder", { "patternDir" });
      break;
    case MenuRow::SphereInMenu: applyPauseRendering (chosen == 0); break;
    }

  _globalSettings->setActiveValueIndex (_globalSettingsOptionIndex, chosen);

  _globalSettingsValueFieldSelected = false;
  _globalSettings->setValueFieldSelected (false);
}

void
A3MotionUIComponent::applyOscAddresses (juce::var const &config)
{
  _oscAddresses = loadOscAddresses (config);

  // The engine's backend sends on its own thread and picks these up there;
  // the message handler receives on this one and can take them directly.
  _engine.setOscAddresses (_oscAddresses);

  if (_oscMessageHandler)
    _oscMessageHandler->setAddresses (_oscAddresses);

  // The beat is sent from the tempo-clock thread, so it cannot read the
  // struct this function just replaced.
  {
    std::lock_guard<std::mutex> lock{ _beatAddressMutex };
    _pendingBeatAddress = _oscAddresses.beatOut;
  }
  _beatAddressPending.store (true, std::memory_order_release);
}

void
A3MotionUIComponent::applyPendingBeatAddress ()
{
  if (!_beatAddressPending.load (std::memory_order_acquire))
    return;

  std::lock_guard<std::mutex> lock{ _beatAddressMutex };
  _beatAddress = _pendingBeatAddress;
  _beatAddressPending.store (false, std::memory_order_relaxed);
}

void
A3MotionUIComponent::applyRecMode (int index)
{
  if (index < 0 || index >= static_cast<int> (recMenuModes.size ()))
    return;

  _recMode = recMenuModes[static_cast<size_t> (index)];
  _engine.setRecMode (_recMode);

  // Shown here rather than by whoever called, exactly as applyClockMode() has
  // always done. The screen's key refreshed itself and the panel's did not, so
  // pressing the panel key cycled the mode and left both the screen and the
  // key's own LED saying the old one -- which is indistinguishable from a key
  // that does nothing, and was reported as exactly that. On the panel the LED
  // is the only answer there is, so leaving it stale is the whole bug.
  if (_clipSettings)
    _clipSettings->setRecMode (_recMode);

  if (runsOnHardware ())
    updateFunctionKeyLEDs ();

  saveSettings (getPersistedSettingsFile (),
                AppSettings{ _clockMode, _recMode });
}

void
A3MotionUIComponent::applyClockMode (int mode)
{
  if (mode == _clockMode)
    return;

  _clockMode = mode;

  if (_clockMode != 0)
    {
      if (std::abs (_internalBPM) < 0.0001f)
        _internalBPM = _engine.getTempoBPM ();
      _engine.resetTempo ();
    }
  else
    {
      if (_internalBPM > 0.f)
        {
          _engine.setTempoBPM (_internalBPM);
          _valueBPM = static_cast<double> (_internalBPM);
        }
    }

  _statusBar->setClockMode (_clockMode);
  if (_clipSettings)
    _clipSettings->setClockMode (_clockMode);
  _loopLengthDisplay->setClockMode (_clockMode);

  // The clock key carries the mode's colour under the hand as well as on the
  // screen — it is the same rule, so it has to be told at the same moment.
  if (runsOnHardware ())
    updateFunctionKeyLEDs ();

  auto clockModeMsg = juce::OSCMessage (_oscAddresses.clockMode);
  clockModeMsg.addInt32 (_clockMode);
  _oscSender.send (clockModeMsg);

  saveSettings (getPersistedSettingsFile (),
               AppSettings{ _clockMode, _recMode });
}




void
A3MotionUIComponent::resetEditedSkinToDefault ()
{
  auto const source = skinFile (getConfigFile ().getParentDirectory (),
                                protectedSkinName);
  auto const restored = juce::JSON::parse (source.loadFileAsString ());
  if (!restored.isObject ())
    return;

  // Under the name it already had: the skin is put right, not replaced. What
  // is on screen changes at once; the file follows when the editor is left,
  // like every other edit here.
  _skinEditor->setSkin (restored, _skinEditor->getSkinName ());
  applyEditedSkin ();
}

void
A3MotionUIComponent::applySkinNamed (juce::String const &name)
{
  _skinNames = availableSkins (getConfigFile ().getParentDirectory ());
  auto const index = _skinNames.indexOf (name);
  if (index < 0)
    return;

  applySkin (index);
}

void
A3MotionUIComponent::openSkinEditor ()
{
  if (_skinEditorOpen)
    return;

  auto const file = skinFile (getConfigFile ().getParentDirectory (),
                              _skinNames[juce::jlimit (
                                  0, juce::jmax (0, _skinNames.size () - 1),
                                  _skinIndex)]);

  // Through the rename, like every other read of a skin: a file still carrying
  // the old spellings would otherwise show them here while the app runs on the
  // new ones, and the first save would write a mixture.
  _skinEditor->setSkin (migrateSkinNames (juce::JSON::parse (file.loadFileAsString ())),
                        file.getFileNameWithoutExtension ());
  _skinEditorOpen = true;
  _globalSettings->setVisible (false);
  _skinEditor->setVisible (true);
  _skinEditor->toFront (true);

  updateOverlayButtons ();
}

void
A3MotionUIComponent::openConfigPage (juce::String const &title,
                                     juce::StringArray const &keys)
{
  // A slice of config.json rather than the whole file: a page with one thing
  // on it is a page somebody can read. The slice is written back key by key,
  // so the rest of the file is untouched by a visit here.
  auto const config = juce::JSON::parse (getConfigFile ().loadFileAsString ());

  auto *slice = new juce::DynamicObject ();
  for (auto const &key : keys)
    {
      auto const identifier = juce::Identifier (key);
      if (config.hasProperty (identifier))
        slice->setProperty (identifier, config[identifier]);
    }

  _configPageKeys = keys;
  _skinEditor->setDocument (juce::var (slice), title, false,
                            SkinEditorComponent::Numbers::Typed);
  _skinEditorOpen = true;
  _globalSettings->setVisible (false);
  _skinEditor->setVisible (true);
  _skinEditor->toFront (true);
}

void
A3MotionUIComponent::applyEditedConfigPage ()
{
  // The hardware reads the running configuration, not the file, so a colour
  // being picked has to go in there straight away — otherwise the LEDs only
  // catch up when the page is left and the file watcher comes round, which
  // reads as a picker that does nothing.
  auto const edited = _skinEditor->getSkin ();
  auto const keys = _configPageKeys;

  juce::MessageManager::callAsync ([edited, keys] {
    userConfig = withKeysReplaced (userConfig, edited, keys);
  });
}

void
A3MotionUIComponent::saveConfigPage ()
{
  if (_configPageKeys.isEmpty ())
    return;

  auto config = juce::JSON::parse (getConfigFile ().loadFileAsString ());
  auto *object = config.getDynamicObject ();
  if (object == nullptr)
    return;

  config = withKeysReplaced (config, _skinEditor->getSkin (), _configPageKeys);

  getConfigFile ().replaceWithText (
      juce::JSON::toString (config, false) + "\n", false, false, "\n");
  _configPageKeys.clear ();

  // Ports and hosts are read when a socket opens, so they take effect at the
  // next start rather than here. Saying so beats a setting that looks live
  // and is not.
  updateControlReadout ("network saved - restart to apply");
}

void
A3MotionUIComponent::applyPauseRendering (bool paused)
{
  _pauseRenderingInMenu = paused;

  auto config = juce::JSON::parse (getConfigFile ().loadFileAsString ());
  if (auto *object = config.getDynamicObject ())
    {
      auto *ui = config["ui"].getDynamicObject ();
      if (ui != nullptr)
        {
          ui->setProperty ("pauseRenderingInMenu", paused);
          object->setProperty ("ui", config["ui"]);
          getConfigFile ().replaceWithText (
              juce::JSON::toString (config, false) + "\n", false, false,
              "\n");
        }
    }

  if (_motionComponent)
    _motionComponent->setRenderingPaused (paused);
}

void
A3MotionUIComponent::applyTheme ()
{
  // A channel's colour was read once, at construction, and kept in
  // ChannelUIState — so editing it in the skin changed the file and the
  // theme and nothing on the screen. Every blob, pad and frame is drawn
  // from this copy, which is why it has to be refreshed here.
  auto const numChannels = _engine.getNumChannels ();
  for (index_t channel = 0;
       channel < numChannels && channel < (index_t)numThemeChannels; ++channel)
    if (_channelUIStates[channel] != nullptr)
      _channelUIStates[channel]->colour = toColour (theme ().channel[channel]);

  // The clip settings bar was handed its channel's colour by value too.
  if (_clipSettings && _channelUIStates[_clipSettingsChannel] != nullptr)
    _clipSettings->setTarget (static_cast<int> (_clipSettingsChannel),
                              static_cast<int> (_clipSettingsSlot),
                              _channelUIStates[_clipSettingsChannel]->colour);

  // How tall the bar is comes out of the skin now — clipSettingsHeightScale,
  // and the font and pot sizes it already followed. That is a layout change
  // and not merely a repaint, and it is this component that hands the bar
  // its bounds, so telling the bar alone would change nothing.
  resized ();
}

void
A3MotionUIComponent::openColourPicker (juce::String const &path)
{
  auto const document = _skinEditor->getSkin ();
  auto const channel = [&document, &path] (char const *name) {
    return (juce::uint8)juce::jlimit (
        0, 255, (int)skinValue (document, path + "." + name));
  };

  _colourPath = path;
  _colourPicker->setColour (
      juce::Colour (channel ("r"), channel ("g"), channel ("b")), path);
  _colourPickerOpen = true;
  _skinEditor->setVisible (false);
  _colourPicker->setVisible (true);
  _colourPicker->toFront (true);

  updateOverlayButtons ();
}

void
A3MotionUIComponent::closeColourPicker ()
{
  if (!_colourPickerOpen)
    return;

  _colourPickerOpen = false;
  _colourPath = {};
  _colourPicker->setVisible (false);
  _skinEditor->setVisible (true);
  _skinEditor->toFront (true);

  updateOverlayButtons ();
}

void
A3MotionUIComponent::applyPickedColour ()
{
  if (_colourPath.isEmpty ())
    return;

  // Back into the document as r/g/b: the file keeps saying what it always
  // said, and HSL is only how a person reaches the number.
  auto document = _skinEditor->getSkin ();
  auto const colour = _colourPicker->getColour ();

  setSkinValue (document, _colourPath + ".r", colour.getRed (), true);
  setSkinValue (document, _colourPath + ".g", colour.getGreen (), true);
  setSkinValue (document, _colourPath + ".b", colour.getBlue (), true);

  applyEditedSkin ();
}

void
A3MotionUIComponent::showKeyboard (bool shown)
{
  shown ? onScreenKeyboard::show () : onScreenKeyboard::hide ();
  refreshKeyboardIcon ();
}

void
A3MotionUIComponent::toggleKeyboard ()
{
  // Always available, whatever is on screen: it is the system's keyboard and
  // it types into whatever has the focus.
  auto const wasShown = onScreenKeyboard::isShown ();
  showKeyboard (!wasShown);

  // Showing it over a row that can be typed says what it is for. A row that
  // is only turned stays that way; the keyboard is then simply up.
  if (!wasShown && _skinEditorOpen && !_skinEditor->isNaming ())
    _skinEditor->beginTypingBrowsedRow ();
}

void
A3MotionUIComponent::refreshKeyboardIcon ()
{
  if (!_statusBar)
    return;

  using State = StatusBar::KeyboardState;

  auto const state = onScreenKeyboard::isShown () ? State::Shown
                                                 : State::Available;

  _statusBar->setKeyboardState (state);
}

void
A3MotionUIComponent::saveSkinAsNew ()
{
  auto const configDir = getConfigFile ().getParentDirectory ();
  auto const name = nextFreeSkinName (configDir, _skinEditor->getSkinName ());

  // The edited state is what gets copied — "save as new" on a skin that has
  // been turned about is meant to keep what is on the screen, not what was
  // last written.
  skinFile (configDir, name)
      .replaceWithText (juce::JSON::toString (_skinEditor->getSkin (), false)
                            + "\n",
                        false, false, "\n");

  writeActiveSkin (getConfigFile (), name);
  reopenEditorOn (name);
}

void
A3MotionUIComponent::renameEditedSkin (juce::String const &name)
{
  auto const configDir = getConfigFile ().getParentDirectory ();

  // Written first: a rename moves the file, and the edits would be left in
  // the old one.
  saveEditedSkin ();

  if (renameSkin (configDir, _skinEditor->getSkinName (), name))
    reopenEditorOn (name);
}

void
A3MotionUIComponent::deleteEditedSkin ()
{
  auto const configDir = getConfigFile ().getParentDirectory ();

  if (!deleteSkin (configDir, _skinEditor->getSkinName ()))
    return; // the last one stays — see deleteSkin

  reopenEditorOn (activeSkinName (getConfigFile ()));
}

void
A3MotionUIComponent::reopenEditorOn (juce::String const &name)
{
  auto const file = skinFile (getConfigFile ().getParentDirectory (), name);

  _skinEditor->setSkin (migrateSkinNames (juce::JSON::parse (file.loadFileAsString ())),
                        file.getFileNameWithoutExtension ());

  // The menu underneath is showing a list of skins that just changed.
  rebuildGlobalSettingsOptions ();
  applyEditedSkin ();
}

void
A3MotionUIComponent::closeSkinEditor ()
{
  if (!_skinEditorOpen)
    return;

  // Written on the way out rather than on every detent: turning an encoder
  // produces a value per tick, and a file save per tick would spend the
  // session writing to disk and waking the file watcher.
  if (_configPageKeys.isEmpty ())
    saveEditedSkin ();
  else
    saveConfigPage ();

  closeColourPicker ();
  showKeyboard (false);
  _skinEditorOpen = false;
  _skinEditor->setVisible (false);
  _globalSettings->setVisible (true);
  _globalSettings->toFront (true);

  updateOverlayButtons ();
}

void
A3MotionUIComponent::applyEditedSkin ()
{
  if (!_configPageKeys.isEmpty ())
    {
      applyEditedConfigPage ();
      return;
    }

  // Straight to the theme, so the change is visible on the sphere behind the
  // editor while the encoder is still turning. The file follows on close.
  auto const edited = _skinEditor->getSkin ();

  // Everything the sphere reads from a skin, handed over the way the file
  // watcher hands it over — corona, glow, speaker light, the energy net, blob
  // and sphere size, the recording underlay. Only sphereScale used to come
  // through here, so every other one of those values sat unchanged while its
  // encoder turned and only appeared once the editor was closed. A value you
  // cannot see while you set it is a value you are setting blind.
  if (_motionComponent != nullptr)
    _motionComponent->applyVisualConfig (edited);

  juce::Component::SafePointer<A3MotionUIComponent> safeThis{ this };
  auto const loaded = loadTheme (edited);
  juce::MessageManager::callAsync ([safeThis, loaded] {
    if (safeThis != nullptr)
      applyThemeEverywhere (loaded, *safeThis);
  });
}

void
A3MotionUIComponent::saveEditedSkin ()
{
  auto const edited = _skinEditor->getSkinName ();
  auto const target = skinNameToWriteTo (edited);

  auto const file
      = skinFile (getConfigFile ().getParentDirectory (), target);

  // Rewritten whole, unlike config.json: a skin file is this editor's own
  // output, and its shape is generated rather than hand-arranged.
  file.replaceWithText (
      juce::JSON::toString (_skinEditor->getSkin (), false) + "\n", false,
      false, "\n");

  // The edits branched off the default, so the skin they landed in is the one
  // that should now be in force -- otherwise they would be written and then
  // immediately not shown.
  if (target != edited)
    applySkinNamed (target);
}




void
A3MotionUIComponent::previewSkin (int index)
{
  if (index < 0 || index >= _skinNames.size ())
    return;

  // Shown while the encoder is still turning, so a skin is chosen by
  // looking at it rather than by reading its name. Nothing is written —
  // the press is what makes it the one that is running.
  auto const file = skinFile (getConfigFile ().getParentDirectory (),
                              _skinNames[index]);
  auto const loaded = loadTheme (migrateSkinNames (juce::JSON::parse (file.loadFileAsString ())));

  juce::Component::SafePointer<A3MotionUIComponent> safeThis{ this };
  juce::MessageManager::callAsync ([safeThis, loaded] {
    if (safeThis != nullptr)
      applyThemeEverywhere (loaded, *safeThis);
  });
}

void
A3MotionUIComponent::applySkin (int index)
{
  if (index < 0 || index >= _skinNames.size ())
    return;

  _skinIndex = index;

  // Written to config.json rather than kept in a variable: which skin is
  // running is operation, and config.json is where operation lives. A hand
  // edit and this menu then say the same thing in the same place.
  writeActiveSkin (getConfigFile (), _skinNames[index]);

  // The watcher that normally picks that up runs inside the GL render loop,
  // and the sphere is not rendering while the menu covers it — so the half of
  // the reload that is pure message-thread work happens here. The sphere's own
  // tuning follows from the watcher as soon as it draws again, which is when
  // the menu closes and it is visible in the first place.
  auto const file = skinFile (getConfigFile ().getParentDirectory (),
                              _skinNames[index]);
  // Queued rather than applied on the spot, so that a reload the render
  // thread dispatched a moment ago runs first and this one has the last
  // word. Applying directly let a callback that was already in flight put
  // the previous skin back.
  auto const loaded = loadTheme (migrateSkinNames (juce::JSON::parse (file.loadFileAsString ())));
  juce::Component::SafePointer<A3MotionUIComponent> safeThis{ this };
  juce::MessageManager::callAsync ([safeThis, loaded] {
    if (safeThis != nullptr)
      applyThemeEverywhere (loaded, *safeThis);
  });
}

void
A3MotionUIComponent::refreshFonts ()
{
  // The status bar's height and the keyboard's follow the font sizes, so
  // this is a layout change and not only a repaint — and the layout has to come first: the
  // bar sizes its own text against the height it holds, so giving it the new
  // height is what lets the text follow.
  resized ();

  if (auto *root = getTopLevelComponent ())
    root->repaint ();

  saveSettings (getPersistedSettingsFile (),
               AppSettings{ _clockMode, _recMode });
}

juce::File
A3MotionUIComponent::getConfigFile () const
{
  return juce::File::getCurrentWorkingDirectory ().getChildFile (
      "config/config.json");
}

juce::File
A3MotionUIComponent::getPersistedSettingsFile () const
{
  return juce::File::getCurrentWorkingDirectory ()
      .getChildFile ("config/ui_state.json");
}

void
A3MotionUIComponent::selectClip (index_t channel, index_t slot)
{
  // The face shows what the bar shows. Kept here rather than only where a
  // toggle is touched, because a clip is also selected by a pad, by the
  // browser and by the transport -- and a face saying 1 over a bar showing 2
  // would be worse than no face at all.
  if (channel < _channelSlot.size ())
    _channelSlot[channel] = slot;

  _clipSettingsChannel = channel;
  _clipSettingsSlot = slot;
  _clipSettingsMenuIndex = 0;
  _clipSettingsSubIndex = 0;
  _clipSettings->setTarget (static_cast<int> (channel),
                                   static_cast<int> (slot),
                                   _channelUIStates[channel]->colour);
  updateClipSettingsDisplay ();

  // The ACTION page shows the same slot the bar does; choosing a clip has to
  // move both or the page describes a clip nobody is looking at.
  updateActionPage ();

  // So does the library: its highlighted row is the one thing saying which of
  // seventy rows the chosen slot is holding, and a face tapped on FILES has
  // just changed which slot that is. Only while it is on screen -- refreshing
  // it walks the pattern folder, and a pad press should not go to disk.
  if (_barPage == BarPage::Browser)
    refreshBrowser ();
}

void
A3MotionUIComponent::handleClipSettingsScroll (index_t channel, int increment)
{
  if (channel != _clipSettingsChannel || increment == 0)
    return;

  static constexpr int numMenuItems = ClipSettingsComponent::numParameters;
  selectClipSettingsSection (
      (_clipSettingsMenuIndex + increment % numMenuItems + numMenuItems)
      % numMenuItems);
}

void
A3MotionUIComponent::selectClipSettingsSection (int index)
{
  // Sets rather than cycles: a finger names the section it wants outright,
  // where the encoder has to turn past the ones in between.
  if (index < 0 || index >= ClipSettingsComponent::numParameters)
    return;

  _clipSettingsMenuIndex = index;
  _clipSettingsSubIndex = 0;
  updateClipSettingsDisplay ();
}

void
A3MotionUIComponent::selectClipSettingsSubElement (int index)
{
  if (index < 0 || index >= numSubElementsForSection (_clipSettingsMenuIndex))
    return;

  _clipSettingsSubIndex = index;
  updateClipSettingsDisplay ();
}

int
A3MotionUIComponent::numSubElementsForSection (int menuIndex) const
{
  // These must agree with numControlsInSection() -- a tap addresses a control
  // by the same sub-index an encoder would. They had drifted: elevation said
  // six and motion four long after either was true.
  if (menuIndex == ClipSettingsComponent::elevationIndex)
    return 3; // clip-top, clip-bottom, sway
  if (menuIndex == ClipSettingsComponent::motionIndex)
    return 10; // rot, fade, bias, dir, end, two squeezes, spin, swell, reach
  if (menuIndex == ClipSettingsComponent::trajectoryIndex)
    return 2; // the picture, and the clip field under it
  return 1;
}

void
A3MotionUIComponent::handleClipSettingsSubElementCycle (index_t channel)
{
  if (channel != _clipSettingsChannel)
    return;

  auto const numSub = numSubElementsForSection (_clipSettingsMenuIndex);
  selectClipSettingsSubElement ((_clipSettingsSubIndex + 1) % numSub);
}

void
A3MotionUIComponent::handleClipSettingsReset (index_t channel, int section,
                                              int sub)
{
  if (channel != _clipSettingsChannel)
    return;

  // Twelve o'clock, everywhere: the middle of whatever range the control has.
  // Not yet a per-control table of remembered defaults -- for a knob you have
  // just pushed somewhere unhelpful mid-set, "back to the middle" is the thing
  // you actually want, and it is the same answer for all of them.
  auto const slot = _clipSettingsSlot;
  auto &pattern = _patterns[channel][slot];

  switch (section)
    {
    case 0: // Shape: rotate on the front, fade on the back
      if (sub != 1)
        return;
      if (_barPage == BarPage::Record)
        {
          // The fade is a reading of the movement now, not a change to it:
          // there is nothing to write into the ticks and nothing to redraw.
          pattern->setFadeReach (ClipSettings{}.fadeReach);
        }
      else if (pattern)
        {
          // No turn at all is this knob's middle: the ring's twelve o'clock
          // and the shape as it was drawn are the same thing.
          pattern->setRotate (0.f);
        }
      break;

    case 1: // Elevation
      if (!pattern)
        return;
      if (sub == 0)
        pattern->setClipTop (0.f);
      else if (sub == 1)
        pattern->setClipBottom (0.f);
      else if (sub == 2)
        // Off. The middle of a bipolar sweep is no sweep at all.
        pattern->setElevationLfo (0);
      else
        return;
      break;

    case 2: // Motion — rot (0), fade (1), bias (2), the two squeezes at five
            // and six, spin, swell and reach at seven, eight and nine.
            // Appended rather than inserted where they sit, so nothing else
            // here had to move.
      if (!pattern)
        return;
      // Only the knobs have a middle to go back to; direction and end action
      // are lists, and a list has no default a double tap could mean.
      switch (sub)
        {
        case 0: pattern->setRotate (0.f); break;
        case 1: pattern->setFadeReach (ClipSettings{}.fadeReach); break;
        case 2:
          pattern->setBridgeBias (0);
          refreshPatternDisplayFromTicks (pattern);
          break;
        // The figure as it was recorded. Worth a double tap of its own: a
        // squeeze is easy to push somewhere unrecognisable, and finding the
        // exact middle of a knob by hand mid-set is not a thing anyone does.
        case 5: pattern->setSqueezeX (0.f); break;
        case 6: pattern->setSqueezeY (0.f); break;
        // Off. The middle of a bipolar sweep is no sweep at all, which is
        // also what a hand is reaching for when it double taps one.
        case 7: pattern->setSpin (0); break;
        case 8: pattern->setReachLfo (0); break;
        case 9: pattern->setReach (ClipSettings{}.reach); break;
        default: return;
        }
      break;

    default:
      return;
    }

  updateControlReadout ("-- DEFAULT");
  updateClipSettingsDisplay ();
}

void
A3MotionUIComponent::handleClipSettingsValueChange (index_t channel,
                                                    int section, int sub,
                                                    int increment)
{
  if (channel != _clipSettingsChannel || increment == 0)
    return;

  // Which control to change is a parameter, not the current selection. Read
  // off the selection instead, two fingers on two controls both changed
  // whatever was selected last — one value moving twice as fast rather than
  // two values moving.
  auto const slot = _clipSettingsSlot;
  auto &params = _clipUIParams[channel][slot];

  switch (section)
    {
    case 0: // Shape — the picture (0) and the clip field under it (1). Two
            // controls because they are two questions: which figure the sound
            // traces, and which set of values it is played with. One scroller
            // over both walked shapes and presets in one list, so scrolling
            // the picture could quietly apply somebody's preset.
      {
        auto &pattern = _patterns[channel][slot];

        if (sub == 1)
          {
            // The clip field: the settings presets, applied onto whatever
            // shape is in the slot. applySettingsPreset() is the same way in
            // the browser uses, so a preset means one thing however it is
            // reached.
            auto const held
                = _patternLibrary->indexForClipFile (_slotClipFile[channel][slot]);
            auto const next = stepThroughLibrary (held, increment, true);
            if (next > 0)
              applySettingsPreset (channel, slot, next);
            break;
          }

        if (sub != 0)
          break;

        int currentIndex = 0;
        if (pattern)
          currentIndex = trajectoryNameToIndex (pattern->getName ());

        // Shapes only. The settings the slot is playing with are not the
        // shape's, and swapping the figure must not throw them away -- that
        // is what the field under the picture is for.
        auto const newIndex = stepThroughLibrary (currentIndex, increment,
                                                  false);
        if (newIndex < 0)
          break;

        // Kept across the swap and put back on the new figure. Without this,
        // reaching for another shape reset everything that had been dialled
        // into the slot -- and the one thing the picture must not change is
        // how the slot is played.
        auto const held
            = pattern ? clipSettingsFrom (*pattern) : ClipSettings{};

        bool const wasPlaying
            = pattern
              && (pattern->getStatus () == Pattern::Status::Playing
                  || pattern->getStatus ()
                         == Pattern::Status::ScheduledForPlaying);

        if (pattern)
          {
            auto status = pattern->getStatus ();
            if (status == Pattern::Status::Playing
                || status == Pattern::Status::Recording)
              _engine.stopPattern (pattern, _now);
            _motionComponent->unsetPreviewPattern (pattern);
            _motionComponent->removePatternDisplayData (pattern);
          }

        if (newIndex == 0)
          {
            pattern = nullptr;
          }
        else
          {
            pattern = createPatternForIndex (newIndex, channel);
            if (pattern)
              applyClipSettings (*pattern, held);
            registerPatternDisplayData (pattern);

            if (wasPlaying && pattern)
              {
                pattern->setPlaybackLength (
                    getPlaybackLength (channel, slot));
                _engine.playPattern (pattern, _now);
              }
          }

        updatePadRowLabel (channel, slot);
        break;
      }
    case 1: // Elevation — clip-top (0), clip-bottom (1), sway (2). Where the
            // middle of the trajectory sits is set in the graphic above them,
            // not by a knob, and sway is how fast that line travels. reach
            // went to Motion, beside the swell that sweeps it.
      {
        auto &pattern = _patterns[channel][slot];
        if (!pattern)
          break;

        switch (sub)
          {
          case 0:
            pattern->setClipTop (pattern->getClipTop () + increment * 0.05f);
            break;
          case 1:
            pattern->setClipBottom (pattern->getClipBottom ()
                                    + increment * 0.05f);
            break;
          default:
            // How fast the line the graphic draws travels, and towards which
            // pole. It moved here from Motion to stand under the thing it
            // moves; reach went the other way, to stand beside its own sweep.
            pattern->setElevationLfo (std::clamp (
                pattern->getElevationLfo () + increment, -lfoMaxStep,
                lfoMaxStep));
            updateControlReadout (
                "sway " + sweepReadout (pattern->getElevationLfo ()));
            break;
          }
        break;
      }
    case 2: // Motion — rot (0), fade (1), bias (2), direction (3),
            // end-action (4), sqzX (5), sqzY (6), spin (7), swell (8),
            // reach (9). What the movement *is* in the plane, each standing
            // value beside the movement that works on it. Renumbering a
            // section means moving the layout,
            // tapAdvancesValue, this handler, the reset handler and the
            // painter together -- see CLAUDE.md -- which is exactly why the
            // squeezes were appended at the end rather than given the seats
            // they occupy on screen.
      {
        auto &pattern = _patterns[channel][slot];

        switch (sub)
          {
          case 0:
            // The standing angle the spin adds to -- both are summed at the
            // one place that turns anything.
            if (pattern)
              pattern->setRotate (pattern->getRotate () + increment * 0.02f);
            break;

          case 1:
            // How far a gap may be for the fade to draw through it. A reading
            // of the movement, not a change to it: nothing is written into
            // the ticks, so it can be turned down as freely as up, and the
            // drawn line follows because it is cut from the same plan.
            if (pattern)
              {
                pattern->setFadeReach (
                    pattern->getFadeReach ()
                    + 0.02f * static_cast<float> (increment));
                refreshPatternDisplayFromTicks (pattern);
              }
            break;

          case 2:
            // Where a drawn-through gap leads. Whole steps, like spin and
            // swell: nine positions, and a finger should feel each one rather
            // than slide past them.
            if (pattern)
              {
                pattern->setBridgeBias (pattern->getBridgeBias () + increment);
                refreshPatternDisplayFromTicks (pattern);
              }
            break;

          case 3:
            params.direction = (params.direction + increment % 2 + 2) % 2;
            applyMotionMode (channel, slot);
            break;

          case 5:
          case 6:
            // The two squeezes. A tenth per step, which is a twentieth of
            // their ring: these run -1..1 where reach and the clips run 0..1,
            // so the same number here would move half as far under the same
            // finger. Twenty steps end to end, like the elevation knobs.
            if (pattern)
              {
                auto const amount = 0.1f * static_cast<float> (increment);
                if (sub == 5)
                  pattern->setSqueezeX (pattern->getSqueezeX () + amount);
                else
                  pattern->setSqueezeY (pattern->getSqueezeY () + amount);
              }
            break;

          case 7:
          case 8:
            // Two of the clip's three slow sweeps -- the third, sway, is in
            // Elevation under the line it travels. Whole steps: a TempoLfo
            // step is a signed power of two in bars per cycle, so a finger
            // should feel each one rather than slide past them.
            //
            // On the Pattern rather than in _clipUIParams because the engine
            // reads them every tick and they have to survive being saved.
            if (pattern)
              {
                auto const stepped = [increment] (int step) {
                  return std::clamp (step + increment, -lfoMaxStep,
                                     lfoMaxStep);
                };

                if (sub == 7)
                  pattern->setSpin (stepped (pattern->getSpin ()));
                else
                  pattern->setReachLfo (stepped (pattern->getReachLfo ()));

                // A step is an index into a table of powers of two and means
                // nothing to read, so the readout says the cycle it stands
                // for -- the one thing a ring alone does not say.
                updateControlReadout (
                    juce::String (sub == 7 ? "spin " : "swell ")
                    + sweepReadout (sub == 7 ? pattern->getSpin ()
                                             : pattern->getReachLfo ()));
              }
            break;

          case 9:
            // How far the trajectory's outer edge lands from the base the
            // elevation graphic sets. It stands beside the swell that sweeps
            // it, the way rot stands beside its spin.
            if (pattern)
              pattern->setReach (pattern->getReach () + increment * 0.05f);
            break;

          default:
            params.endAction
                = (params.endAction + increment % value::numEndActions
                   + value::numEndActions)
                  % value::numEndActions;
            applyMotionMode (channel, slot);
            break;
          }
      }
      break;
    case ClipSettingsComponent::globalIndex:
      {
        // Nothing in _clipUIParams changes here: the strip holds one setting
        // that every channel shares, which is why it sits outside their
        // sections rather than inside each of them.
        auto const count = static_cast<int> (recMenuModes.size ());
        applyRecMode ((recMenuIndex (_recMode) + increment % count + count)
                      % count);
      }
      break;
    }

  updateClipSettingsDisplay ();
  scheduleSetSave ();
}

void
A3MotionUIComponent::handleClipSettingsToggle (index_t channel, int section,
                                               int sub)
{
  if (channel != _clipSettingsChannel)
    return;

  // Nothing toggles any more. pole and flat were the last two, and the
  // elevation base the graphic sets says what they said -- see
  // tapTogglesValue(), which now answers no to everything.
  juce::ignoreUnused (section, sub);
}

void
A3MotionUIComponent::updateClipSettingsDisplay ()
{
  auto const channel = _clipSettingsChannel;
  auto const slot = _clipSettingsSlot;
  auto const &params = _clipUIParams[channel][slot];
  auto const &pattern = _patterns[channel][slot];

  // Trajectory Shape: pictogram + name, built the same way as the (hidden)
  // PadRowDisplay rows — prefer the library's SVG icon, fall back to the
  // pattern's own tick data if it's not (yet) saved to the library.
  if (pattern)
    {
      auto const &name = pattern->getName ();
      auto const libIndex = _patternLibrary->indexForName (name);
      if (libIndex > 0)
        {
          auto const &entry = _patternLibrary->getEntry (libIndex);
          auto iconPath = svgDToPath (entry.svgPathData);
          if (!iconPath.isEmpty () || entry.hasJumpDots)
            _clipSettings->setTrajectoryIcon (
                trajectoryIconFromPath (iconPath, entry.jumpDots));
          else
            _clipSettings->setTrajectoryIcon (
                trajectoryIconFromTicks (entry.ticks));
        }
      else
        {
          _clipSettings->setTrajectoryIcon (
              trajectoryIconFromTicks (pattern->getTicks ().positions));
        }
      _clipSettings->setTrajectoryName (juce::String (name));
    }
  else
    {
      _clipSettings->setTrajectoryIcon (TrajectoryIconData{});
      _clipSettings->setTrajectoryName ("Empty");
    }

  // What the slot is played with, and whether it has been turned since. The
  // clip's file name rather than the shape's: they are two different things
  // and the field beside the picture is the one that changes this one.
  _clipSettings->setClipName (
      _slotClipFile[channel][slot].getFileNameWithoutExtension (),
      slotHasDrifted (channel, slot));

  // Both slots, not only the one on show: the keys sit side by side, and a
  // mark on one is only readable next to the absence of one on the other.
  for (index_t s = 0; s < numPadSlots; ++s)
    _clipSettings->setSlotDrifted (s, slotHasDrifted (channel, s));

  _clipSettings->setRecMode (_recMode);
  _clipSettings->setElevationSubIndex (_clipSettingsSubIndex);

  // The four faces: each channel's own colour and its own slot, and which of
  // them the bar is describing.
  {
    std::array<juce::Colour, numChannelColumns> colours;
    std::array<int, numChannelColumns> slots;
    for (size_t ch = 0; ch < numChannelColumns; ++ch)
      {
        // From the theme where a channel has no state of its own: a literal
        // here is a colour the skin cannot reach, which is what
        // NoColourLiterals exists to stop.
        colours[ch] = ch < _channelUIStates.size ()
                          ? _channelUIStates[ch]->colour
                          : toColour (theme ().textMuted);
        slots[ch] = static_cast<int> (_channelSlot[ch]);
      }

    _clipSettings->setChannelFaces (colours, slots,
                                    static_cast<int> (_clipSettingsChannel));
  }
  // The coverage the hand set, and where the swell is holding it now.
  _clipSettings->setElevationReach (
      pattern ? pattern->getReach () : 0.5f,
      pattern && pattern->getReachLfo () != 0
          ? lfoSweep (pattern->getReach (), pattern->getReachLfo (),
                      pattern->getReachLfoPhase ())
          : -1.f);
  // The line the hand set, and where the sway is holding it now -- the same
  // pair the reach above is given, and drawn the same way.
  _clipSettings->setElevationBase (
      pattern ? pattern->getElevationBase () : 0.f,
      pattern && pattern->getElevationLfo () != 0
          ? lfoSweep (pattern->getElevationBase (),
                      pattern->getElevationLfo (),
                      pattern->getElevationLfoPhase ())
          : -1.f);
  _clipSettings->setElevationClipTop (pattern ? pattern->getClipTop ()
                                              : 0.0f);
  _clipSettings->setElevationClipBottom (
      pattern ? pattern->getClipBottom () : 0.0f);

  // Motion/Filter: all sub-controls visible in parallel, like Elevation —
  // _clipSettingsSubIndex only picks which one is highlighted, and only
  // means anything while that section is actually selected.
  //
  // Speed is passed as an already-normalized knob fraction + a formatted
  // musical label (e.g. "1/4", "2") rather than the raw speedLog2 value,
  // so ClipSettingsComponent doesn't need to know speedLog2Min/Max.
  // Inverted against the raw range: far left (frac 0) = speedLog2Max
  // ("16", slowest), far right (frac 1) = speedLog2Min ("1/128", fastest).
  auto const clipSpeedLog2 = pattern ? pattern->getSpeedLog2 () : 0;
  auto const speedRange
      = static_cast<float> (speedLog2Max - speedLog2Min);
  auto const speedFrac
      = speedRange > 0.f
            ? (speedLog2Max - clipSpeedLog2) / speedRange
            : 0.f;
  auto const speedLabel
      = clipSpeedLog2 >= 0
            ? juce::String (static_cast<int> (std::exp2 (clipSpeedLog2)))
            : "1/"
                  + juce::String (
                      static_cast<int> (std::exp2 (-clipSpeedLog2)));
  _clipSettings->setMotionSpeed (speedFrac, speedLabel);
  // Read back off the pattern rather than from this table. The pattern is
  // where the engine looks and what the file carries, so a clip that came from
  // disk brings its own settings -- and the bar has to show those, not the
  // ones the last clip happened to leave in the table.
  if (pattern)
    {
      auto &editable = _clipUIParams[channel][slot];
      editable.direction
          = pattern->getPlayDirection () == PlayDirection::Reverse ? 1 : 0;

      switch (pattern->getEndAction ())
        {
        case EndAction::Loop: editable.endAction = 0; break;
        case EndAction::Stop: editable.endAction = 1; break;
        case EndAction::Pause: editable.endAction = 2; break;
        case EndAction::Bounce: editable.endAction = 3; break;
        case EndAction::Random: editable.endAction = 4; break;
        }
    }

  _clipSettings->setMotionDirection (_clipUIParams[channel][slot].direction);
  _clipSettings->setMotionEndAction (_clipUIParams[channel][slot].endAction);
  _clipSettings->setMotionFadeReach (
      pattern ? pattern->getFadeReach () : ClipSettings{}.fadeReach);
  _clipSettings->setMotionBridgeBias (
      pattern ? pattern->getBridgeBias () : ClipSettings{}.bridgeBias);
  _clipSettings->setMotionSqueeze (
      pattern ? pattern->getSqueezeX () : ClipSettings{}.squeezeX,
      pattern ? pattern->getSqueezeY () : ClipSettings{}.squeezeY);
  _clipSettings->setSweeps (
      pattern ? pattern->getSpin () : ClipSettings{}.spin,
      pattern ? pattern->getReachLfo () : ClipSettings{}.reachLfo,
      pattern ? pattern->getElevationLfo () : ClipSettings{}.elevationLfo);
  _clipSettings->setMotionEnvelope (
      pattern ? pattern->getEnvelopeAttack () : 0,
      pattern ? pattern->getEnvelopeDecay () : 0);
  _clipSettings->setMotionEnvelopeMax (pattern ? pattern->getEnvelopeMax ()
                                               : 1.f);
  _clipSettings->setMotionActMode (
      pattern && pattern->getActMode () == ActMode::Hold ? 1 : 0);
  _clipSettings->setShapeSpeed (clipSpeedLog2);
  // The rotation the hand set, and where the spin has carried it: the knob
  // shows both, the way the channel grid shows the accent over 3d.
  //
  // The second number comes from turnsOf(), which is what the engine and the
  // renderer turn by. Summed here by hand it kept counting a stopped spin's
  // leftover phase, so the arc stayed off the pointer after the spin was
  // turned off and there was no way to bring it back.
  auto const rotate = pattern ? pattern->getRotate () : 0.f;
  _clipSettings->setShapeRotate (rotate, pattern ? turnsOf (*pattern) : 0.f);

  // Worded like Speed is, because it is the same kind of number: bars as a
  // power of two, "2" for two bars, "1/4" for a quarter of one.
  _clipSettings->setRecordLength (
      params.recordLengthLog2 >= 0
          ? juce::String (static_cast<int> (std::exp2 (params.recordLengthLog2)))
          : "1/"
                + juce::String (static_cast<int> (
                    std::exp2 (-params.recordLengthLog2))));
  // How far the clip has got, as a line under the tick indicator — over the
  // sphere, where the eye already is. Recording fills in the channel's own
  // colour, playing in the skin's green: the same indicator answers "how far
  // in am I" for both, and the colour answers which of the two it is without
  // a second glance anywhere else.
  {
    auto const &pattern = _patterns[channel][slot];
    auto const recording = _engine.getRecordingPattern ();
    auto const isRecordingThis = recording != nullptr && recording == pattern
                                 && _engine.isRecording ();
    auto const isPlayingThis
        = pattern != nullptr
          && pattern->getStatus () == Pattern::Status::Playing;

    if (_statusBar)
      {
        if (isRecordingThis)
          _statusBar->setRecordingProgress (_engine.getRecordingProgress (),
                                            _channelUIStates[channel]->colour);
        else if (isPlayingThis)
          _statusBar->setRecordingProgress (pattern->getPlayPosition (),
                                            toColour (theme ().accent));
        else
          _statusBar->setRecordingProgress (-1.f,
                                            _channelUIStates[channel]->colour);
      }

    if (_clipSettings)
      _clipSettings->setTransportState (isPlayingThis, isRecordingThis);
  }

  _clipSettings->setTrajectorySubIndex (
      _clipSettingsMenuIndex == ClipSettingsComponent::trajectoryIndex
          ? _clipSettingsSubIndex
          : 0);
  _clipSettings->setMotionSubIndex (
      _clipSettingsMenuIndex == ClipSettingsComponent::motionIndex
          ? _clipSettingsSubIndex
          : 0);

  refreshChannelValues ();

  _clipSettings->setSelectedParameterIndex (_clipSettingsMenuIndex);
}

void
A3MotionUIComponent::updateControlReadout (juce::String const &text)
{
  // To the status bar, not to the clip bar: what was last turned is a reading
  // like the tempo and the beat, and the band it used to stand in over the
  // global strip is the transport's now.
  if (_statusBar)
    _statusBar->setControlReadout (text);

  // ... and to the library while it is open, which is the one page whose
  // controls are a thousand pixels from that bar. Pressing Save and reading
  // the answer at the top of the screen is reading it somewhere you are not
  // looking; the same words appear over the foot of the list as well.
  if (_browser && _browser->isVisible ())
    _browser->showMessage (text);
}

}
