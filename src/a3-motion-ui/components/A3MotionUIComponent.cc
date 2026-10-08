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

#include "A3MotionUIComponent.hh"

#include <a3-motion-engine/tempo/BeatTrace.hh>

#include <a3-motion-ui/components/RecordingLength.hh>

#include <a3-motion-engine/Envelope.hh>
#include <a3-motion-engine/OscSendGuard.hh>
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
#include <a3-motion-engine/ScriptLine.hh>
#include <a3-motion-ui/components/ActionKnobs.hh>
#include <a3-motion-ui/components/ActionMotionKnobs.hh>
#include <a3-motion-engine/PatternLibrary.hh>
#include <a3-motion-engine/OscEndpoints.hh>
#include <a3-motion-engine/UserConfig.hh>
#include <a3-motion-ui/PatternDir.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-engine/elevation/HeightMap.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>

#include <a3-motion-ui/Config.hh>
#include <a3-motion-ui/Helpers.hh>
#include <a3-motion-ui/components/ChannelStrip.hh>
#include <a3-motion-ui/components/ChannelUIState.hh>
#include <a3-motion-ui/components/ControllerLayout.hh>
#include <a3-motion-ui/components/LoopLengthDisplay.hh>
#include <a3-motion-ui/components/ElevationDisplay.hh>
#include <a3-motion-ui/components/ElevationSideView.hh>
#include <a3-motion-ui/components/MotionComponent.hh>
#include <a3-motion-ui/components/PadRowDisplay.hh>
#include <a3-motion-ui/components/PatternDisplay.hh>
#include <a3-motion-ui/components/ChannelValueReset.hh>
#include <a3-motion-ui/components/RecArming.hh>
#include <a3-motion-ui/components/LibraryKeys.hh>
#include <a3-motion-engine/PatternRunning.hh>
#include <a3-motion-engine/SpaceTurn.hh>
#include <a3-motion-engine/RecordingName.hh>
#include <a3-motion-engine/SplitFolder.hh>
#include <a3-motion-engine/TextFile.hh>
#include <a3-motion-ui/components/RecordingIndicator.hh>
#include <a3-motion-ui/theme/PadStatusColours.hh>
#include <a3-motion-ui/theme/CleanSkin.hh>
#include <a3-motion-ui/components/SceneLaunch.hh>
#include <a3-motion-ui/components/GlobalSettingsComponent.hh>
#include <a3-motion-ui/components/ClipSettingsComponent.hh>
#include <a3-motion-ui/components/StatusBar.hh>
#include <a3-motion-ui/io/PanelButtonCells.hh>
#include <a3-motion-ui/components/PanelKeyboard.hh>
#include <a3-motion-ui/components/WorkspaceList.hh>
#include <a3-motion-ui/io/Workspaces.hh>

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

/** How many points of a take the elevation circle is drawn from. It is a
 *  couple of centimetres across: past this the extra ticks land on pixels
 *  that are already lit, and every one of them is work done on the timer. */
constexpr std::size_t elevationFigureSamples = 96;

/** How often this component's own timer runs.
 *
 *  Fast enough for a take's write head to move while it is being recorded,
 *  which is what this rate was chosen for.
 *
 *  **It is also the rate the channel faces' meters are redrawn at**, since
 *  repainting them is the first thing timerCallback() does. That
 *  is deliberately *not* vuMeterRefreshHz: the mixer's meters live on pages
 *  that come and go and have a timer each, and this bar never goes away, so
 *  its meters ride the timer that is already running rather than starting a
 *  second one behind everything else on screen. The two rates being close but
 *  unequal is therefore a fact about where each of them lives, not an
 *  oversight -- see vuMeterRefreshHz, which says the same thing from the
 *  other side. */
constexpr int uiTimerHz = 20;

/** How many of those ticks make the two seconds the directory check runs at.
 *
 *  Derived rather than written as 40, which is what it used to be: the count
 *  and the rate are one decision, and a rate changed without it would move a
 *  filesystem scan without anybody meaning to. */
constexpr int libraryCheckTicks = uiTimerHz * 2;

/** How long this device keeps its own position to itself at start-up, while
 *  it waits for A3 Core to answer /state/recall.
 *
 *  Core is a UDP hop away -- on the rig, the same machine -- so the answer is
 *  back in well under a millisecond when Core is up. This is not a guess at
 *  the round trip; it is how long to wait before concluding that no answer is
 *  coming, and then going ahead with this device's own values. Nothing is
 *  playing yet at this point, so the wait is inaudible; what it costs is that
 *  a rig started with Core down hears its first position a third of a second
 *  later than it used to. */
constexpr double recallGraceMillis = 300.;

}


A3MotionUIComponent::A3MotionUIComponent (unsigned int const numChannels)
    : _heightMap (std::make_unique<HeightMapSphere> ()),
      _engine (numChannels, *_heightMap)
{
  setLookAndFeel (&_lookAndFeel);

  // Every touch on the device, whichever component takes it -- see
  // mouseDown(). JUCE calls global listeners after the touched component, so
  // DISCARD's own press has already been handled when this hears it.
  juce::Desktop::getInstance ().addGlobalMouseListener (this);

  // First thing, before anything can tick: until Core has had its chance to
  // say where the sound actually is, this device says nothing about it. The
  // engine would otherwise announce all four channels the moment it runs,
  // Core would forward that straight to the IEM plugins, and the sound would
  // jump to this device's idea of it -- which the recall would then confirm
  // rather than prevent. See MotionEngine::holdOutputUntil and
  // issues/a3-motion-ui-recall-kommt-zu-spaet.md.
  //
  // Armed here and again in askCoreForItsState(). This one covers
  // construction itself -- the tick handler is registered part-way through
  // it, so a tick can fire while the rest is still being built.
  //
  // Measured on the rig, 2026-09-12: this arming **alone** was enough, the
  // order came out right without the second one. It is armed twice anyway,
  // and the reason is not belt and braces. Measured from here, the grace is
  // a bet that everything between this line and the question -- the hardware
  // interface, the pattern library's 39 system and 2 user patterns, the OSC
  // setup -- fits inside 300 ms. Nobody maintains that property, and the day
  // it stops holding, this fails silently and reads as "the recall never
  // worked". Measured from the question, the grace is the thing it claims to
  // be: how long to wait for an answer.
  _engine.holdOutputUntil (juce::Time::getMillisecondCounterHiRes ()
                                   + recallGraceMillis);

  _oscMessageHandler = std::make_unique<OscMessageHandler> (_engine, *this);
  applyOscAddresses ();

  if (runsOnHardware ())
    {
      createHardwareInterface ();
    }

  // Initialize pattern library (creates system/ and user/ dirs if needed)
  // Path is configurable via "patternDir" in config.json; a relative one is
  // taken from the working directory, like config.json itself.
  auto patternsDir = patternDirectory (
      userConfig, juce::File::getCurrentWorkingDirectory ());
  // Before the library exists, not after: PatternLibrary scans in its own
  // constructor, so a migration that ran afterwards would leave the first
  // start of the day looking at shapes whose clips it had not seen. It showed
  // up in the log as "patterns loaded" printing before "ClipMigration".
  //
  // Non-destructive and idempotent: it runs on every start and does nothing
  // once every take has a clip.
  createBrowserLists ();

  migrateCombinedPatterns (patternsDir);
  migrateSetToCurrent (patternsDir);
  // Before anything writes a set back with one slot (2026-09-27): every
  // two-slot set is copied aside once, nothing deleted.
  migrateTwoSlotSets (patternsDir);

  // The actions and the sets are split into what the instrument ships with
  // and what the performer made, the way the shapes already were. Whatever a
  // device is still holding flat is the performer's -- the repository puts
  // its own into system/ -- so it is moved once and then left alone.
  splitLooseFilesIn (patternsDir.getChildFile ("actions"), ".scd");
  splitLooseFilesIn (patternsDir.getChildFile ("sessions"), ".json");
  splitLooseFilesIn (patternsDir.getChildFile ("clips"), ".json");

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
      // Six rows need no scrolling; the list of a row's values may.
      _globalSettings->scrollPicker (delta);
  };

  _motionComponent->addChildComponent (*_overlayStrips);

  _mixer = std::make_unique<MixerComponent> (_mixerState, _vuLevels);
  _mixer->setAlwaysOnTop (true);
  _motionComponent->addChildComponent (*_mixer);

  // The same step the encoders and the bar's channel grid take, so a finger
  // moves a mixer value at the rate it moves every other value on this
  // device.
  auto const mixerStep = [] (int steps) { return steps * 0.02f; };

  _mixerStrip
      = std::make_unique<MixerStripComponent> (_mixerState, _vuLevels);

  // Both views of the same seven controls, so both land in one pair of
  // handlers rather than in two that agree today. Whichever was touched, both
  // are repainted, and that is load-bearing rather than belt and braces: the
  // overlay takes MotionComponent's bounds and the settings bar is carved out
  // of the window before those are computed, so the bar -- MIX tab and all --
  // stays visible and touchable underneath it. Turn a channel's gain in the
  // overlay and the strip behind it is showing the same value.
  auto const repaintMixers = [this] {
    _mixer->repaint ();
    _mixerStrip->repaint ();
  };

  auto const channelDragged
      = [this, mixerStep, repaintMixers] (int channel, MixerControl control,
                                          int steps) {
          // A two-valued control keeps the drag's direction -- up is on, down
          // is off -- because a drag has one where a tap does not. The same
          // split tapTogglesValue makes in the bar.
          auto const next
              = mixerControlIsAToggle (control)
                    ? (steps > 0 ? 1.f : 0.f)
                    : _mixerState.channelValue (channel, control)
                          + mixerStep (steps);
          _mixerState.setChannelFromTouch (channel, control, next);
          repaintMixers ();
        };
  auto const channelTapped
      = [this, repaintMixers] (int channel, MixerControl control) {
          _mixerState.setChannelFromTouch (
              channel, control,
              _mixerState.channelToggle (channel, control) ? 0.f : 1.f);
          repaintMixers ();
        };

  // Two taps put a control back where it belongs. Only SEND has an answer;
  // the rest of the strip stays where the hand left it, which is why this
  // asks the table rather than resetting whatever was tapped. See
  // mixerControlRestPosition.
  auto const channelDoubleTapped
      = [this, repaintMixers] (int channel, MixerControl control) {
          auto const rest = mixerControlRestPosition (control);
          if (!rest.has_value ())
            return;

          _mixerState.setChannelFromTouch (channel, control, *rest);
          repaintMixers ();
        };

  _mixer->onChannelDragged = channelDragged;
  _mixer->onChannelTapped = channelTapped;
  _mixer->onChannelDoubleTapped = channelDoubleTapped;
  _mixerStrip->onChannelDragged = channelDragged;
  _mixerStrip->onChannelTapped = channelTapped;
  _mixerStrip->onChannelDoubleTapped = channelDoubleTapped;

  // Two taps on a meter: that channel at full volume.
  auto const meterDoubleTapped = [this, repaintMixers] (int channel) {
    _mixerState.setChannelFromTouch (channel, MixerControl::Volume, 1.f);
    repaintMixers ();
  };
  _mixer->onMeterDoubleTapped = meterDoubleTapped;
  _mixerStrip->onMeterDoubleTapped = meterDoubleTapped;

  // A drag on a meter: VOL where the finger has taken it, one to one. Only
  // the meters are redrawn, on both pages: the handle stays inside its own
  // meter, and repainting the whole overlay for every pixel of a drag is what
  // made the long faders feel like they were catching.
  auto const meterDraggedTo = [this] (int channel, float value) {
    _mixerState.setChannelFromTouch (channel, MixerControl::Volume, value);
    _mixer->syncControls ();
    _mixerStrip->syncControls ();
  };
  _mixer->onMeterDraggedTo = meterDraggedTo;
  _mixerStrip->onMeterDraggedTo = meterDraggedTo;
  _mixer->onMasterDragged = [this, mixerStep] (MasterControl control,
                                               int steps) {
    _mixerState.setMasterFromTouch (
        control, _mixerState.masterValue (control) + mixerStep (steps));
    _mixer->repaint ();
  };
  // A knob was turned: the slider owns the value, the state is told where it
  // landed. No steps to add up any more.
  _mixerStrip->onChannelValueChanged
      = [this] (int channel, MixerControl control, float value) {
          _mixerState.setChannelFromTouch (channel, control, value);
          _mixer->syncControls ();
        };
  _mixer->onChannelValueChanged
      = [this] (int channel, MixerControl control, float value) {
          _mixerState.setChannelFromTouch (channel, control, value);
          _mixerStrip->syncControls ();
        };
  // 3D, FREQ and Q stand in both pages but belong to the engine: the same
  // two handlers the grid had, set outright rather than stepped.
  auto const channelPotChanged = [this] (int channel, ChannelPot pot,
                                         float value) {
    setChannelPotValue (static_cast<index_t> (channel), pot, value);
  };
  auto const channelPotDoubleTapped = [this] (int channel, ChannelPot pot) {
    resetChannelPot (static_cast<index_t> (channel), pot);
  };
  _mixer->onChannelPotChanged = channelPotChanged;
  _mixerStrip->onChannelPotChanged = channelPotChanged;
  _mixer->onChannelPotDoubleTapped = channelPotDoubleTapped;
  _mixerStrip->onChannelPotDoubleTapped = channelPotDoubleTapped;

  _mixer->onMasterValueChanged = [this] (MasterControl control, float value) {
    _mixerState.setMasterFromTouch (control, value);
  };
  _mixer->onFilterValueChanged = [this] (FilterControl control, float value) {
    _mixerState.setFilterFromTouch (control, value);
  };

  _mixer->onMasterMeterDraggedTo = [this] (float value) {
    _mixerState.setMasterFromTouch (MasterControl::Volume, value);
    _mixer->syncControls ();
  };
  _mixer->onFilterDragged = [this, mixerStep] (FilterControl control,
                                               int steps) {
    auto const next = control == FilterControl::Mode
                          ? (steps > 0 ? 1.f : 0.f)
                          : _mixerState.filterValue (control)
                                + mixerStep (steps);
    _mixerState.setFilterFromTouch (control, next);
    _mixer->repaint ();
  };
  _mixer->onFilterTapped = [this] (FilterControl control) {
    _mixerState.setFilterFromTouch (
        control, _mixerState.filterIsHighPass () ? 0.f : 1.f);
    _mixer->repaint ();
  };

  // Where a touched value goes. The state holds values and knows nothing
  // about a protocol; the addresses are config, and the tables are indexed by
  // the ui's own control order -- controlSlot() is the one function that says
  // where a control sits in it, and it is the same one MixerState indexes
  // with.
  _mixerState.channelAddress = [this] (int channel, MixerControl control) {
    return withChannelIndex (
        _oscAddresses.mixerChannel[static_cast<std::size_t> (
            controlSlot (control))],
        channel);
  };
  _mixerState.masterAddress = [this] (MasterControl control) {
    return _oscAddresses
        .mixerMaster[static_cast<std::size_t> (controlSlot (control))];
  };
  _mixerState.filterAddress = [this] (FilterControl control) {
    return _oscAddresses
        .mixerFilter[static_cast<std::size_t> (controlSlot (control))];
  };
  _mixerState.onSend = [this] (juce::String const &address, float value) {
    auto message = juce::OSCMessage (address);
    message.addFloat32 (value);
    _mixerSender.send (message);
  };

  _globalSettings = std::make_unique<GlobalSettingsComponent> ();
  _globalSettings->setAlwaysOnTop (true);

  // A tap selects, a double tap or Enter opens: a page for a row that leads
  // somewhere, the list of its values for a row that holds one. A value is
  // changed in that list and nowhere else -- a drag used to arm and turn it,
  // one misplaced finger away from a scroll.
  _globalSettings->onRowTapped = [this] (int option) {
    _globalSettingsOptionIndex = option;
    _globalSettingsValueFieldSelected = false;
  };

  _globalSettings->onRowOpened = [this] (int option) {
    _globalSettingsOptionIndex = option;
    _globalSettings->setOptionIndex (option);
    _globalSettingsValueFieldSelected = true;

    if (_globalSettings->opensSubmenu (option))
      {
        _globalSettings->setValueFieldSelected (true);
        confirmGlobalSettingsOption ();
        return;
      }

    _globalSettings->openPicker ();
    updateOverlayButtons (); // the panel changed size; the strips follow
  };

  // Seeing the skin while choosing it is the point of choosing it here.
  _globalSettings->onPickerBrowsed = [this] (int value) {
    if (browsedMenuRow () == std::optional<MenuRow>{ MenuRow::Skin })
      previewSkin (value);
  };

  _globalSettings->onPickerChosen = [this] (int) {
    confirmGlobalSettingsOption ();
    updateOverlayButtons ();
  };

  _globalSettings->onPickerCancelled = [this] {
    _globalSettingsValueFieldSelected = false;
    // A skin looked at and not chosen goes back to the one that is running.
    if (browsedMenuRow () == std::optional<MenuRow>{ MenuRow::Skin })
      previewSkin (_skinIndex);
    updateOverlayButtons ();
  };

  _motionComponent->addChildComponent (*_globalSettings);

  _workspaceList = std::make_unique<WorkspaceList> ();
  _workspaceList->setAlwaysOnTop (true);
  _workspaceList->onChosen = [] (int number) { workspaces::goTo (number); };
  _motionComponent->addChildComponent (*_workspaceList);

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

  // The keyboard is the app's own, in the bar -- see BarKeyboardComponent.
  // It types into whatever holds the focus, which the mask takes.
  _skinEditor->onNamingChanged = [this] (bool naming) { showKeyboard (naming); };
  _skinEditor->onColourPicked = [this] (auto const &path, auto colour) {
    openColourPicker (path, colour);
  };

  // What the Skin Editor row opens: a narrow panel at the left edge, so the
  // sphere it changes stays in view (SkinPanelComponent.hh). The list above
  // stays for what the panel leaves out, one level further in.
  _skinPanel = std::make_unique<SkinPanelComponent> ();
  _skinPanel->setAlwaysOnTop (true);
  _motionComponent->addChildComponent (*_skinPanel);
  _skinPanel->onValueChanged = [this] { applyEditedSkin (); };
  _skinPanel->onColourPicked = [this] (auto const &path, auto colour) {
    openColourPicker (path, colour);
  };
  _skinPanel->onOpenFullList = [this] { openFullSkinList (); };

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
  _clipSettings->onControlHeld = [this] (int section, int sub, bool held) {
    auto const knob = knobAt (section, sub);
    if (!knob)
      return;

    if (held)
      {
        if (auto const &shown
            = _patterns[_clipSettingsChannel][_clipSettingsSlot])
          shown->takeOverKnob (*knob);
        _knobHold.press (*knob);
      }
    else
      _knobHold.release (*knob);
    pushKnobHolds ();
  };
  _clipSettings->onRecModePressed = [this] {
    auto const count = static_cast<int> (recMenuModes.size ());
    applyRecMode ((recMenuIndex (_recMode) + 1) % count);
    updateClipSettingsDisplay ();
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
    chooseChannelFace (channel, true);
  };
  _clipSettings->onChannelFaceChosen = [this] (index_t channel) {
    chooseChannelFace (channel, false);
  };
  _clipSettings->onChannelPotChanged = channelPotChanged;
  _clipSettings->onChannelPotDoubleTapped = channelPotDoubleTapped;
  _clipSettings->channelLevel = [this] (int channel) {
    return _vuLevels.channel (channel, vuNowMs ());
  };

  // A drag gives the key under the finger another speed, and the clip is
  // played at it straight away -- every other drag in this bar changes what
  // you hear while you drag, and one that only rearranged the keys would be
  // the exception you have to remember. The key keeps what it was dragged
  // to, which is how a speed the four do not name is reached and then found
  // again the next time.
  _clipSettings->onSpeedDragged = [this] (int index, int increment) {
    dragSpeedKey (index, increment);
  };

  _clipSettings->onTransportTapped = [this] (TransportKey key) {
    // Anything else touched drops an armed DISCARD, as Delete does in FILES.
    // REC goes on to toggleRecordingOnShownClip(), which saves or records.
    _pendingTakes.disarm ();
    refreshTakeState ();
    switch (key)
      {
      case TransportKey::Record:
        toggleRecordingOnShownClip ();
        return;
      case TransportKey::Stop:
        // Armed, ■ is a way out that writes nothing.
        if (stopKeyDisarms (recArmedOnShownSlot ()))
          {
            _recArmedSlot.reset ();
            updateControlReadout ("-- REC OFF");
            refreshRecArmed ();
            return;
          }
        // The panel has no Stop pad since 2026-09-27; the screen keeps its ■.
        stopChannel (_clipSettingsChannel);
        return;
      case TransportKey::PlayPause:
        // Armed, ▶ starts the take set up on the REC page.
        if (playKeyAction (recArmedOnShownSlot ()) == PlayKeyAction::StartTake)
          {
            startArmedTake ();
            return;
          }
        handlePadPress (_clipSettingsChannel,
                        padIndexFor (PadFunction::PlayPause));
        return;
      case TransportKey::Action:
        return;
      }
  };
  _clipSettings->onTransportActionHeld = [this] (bool held) {
    // DISCARD wears ACT's place on an unsaved take. The release goes where
    // the press went: a take that ends under a held accent turns the key into
    // DISCARD, and the accent's release must still reach the accent.
    if (held)
      _actPressWasDiscard = _pendingTakes.offersKeys (
          _clipSettingsChannel, _clipSettingsSlot, takeIsUnderway ());

    if (_actPressWasDiscard)
      {
        if (held)
          pressDiscardOnShownTake ();
        else
          _actPressWasDiscard = false;
        return;
      }
    // The chosen button's pad -- the one the ACTION page shows (2026-09-27).
    auto const pad = padIndexForAction (
        _chosenActionButton[_clipSettingsChannel]);
    if (held)
      handlePadPress (_clipSettingsChannel, pad);
    else
      handlePadRelease (_clipSettingsChannel, pad);
  };

  // The bar's ACT plays the shown clip's accent, exactly as its pad does.
  // A speed key is the clip's playback length said plainly. Tapping one sets
  // it outright rather than stepping towards it — that is what the keys are
  // for, and it is untouched by the keys becoming assignable.
  _clipSettings->onSpeedChosen = [this] (int index) {
    chooseSpeedKey (index);
  };

  _clipSettings->onAccentHeld = [this] (bool held) {
    auto const channel = _clipSettingsChannel;
    auto const slot = _clipSettingsSlot;
    if (held)
      _engine.setChannelAction (
          channel, firedActionOf (channel, _chosenActionButton[channel]));
    _engine.setChannelAccentHeld (channel, held,
                                  held ? _patterns[channel][slot] : nullptr);
    updateControlReadout (juce::String ("CH") + juce::String (channel + 1)
                          + " ACTION");
  };

  _clipSettings->onControlDragged = [this] (int section, int sub,
                                           int increment) {
    handleClipSettingsValueChange (_clipSettingsChannel, section, sub,
                                   increment);
  };

  // A knob on the bar says where it stands; the increments below stay for the
  // fields and for the encoders, which count steps.
  _clipSettings->onControlSet = [this] (int section, int sub, double value) {
    setClipSettingsValue (_clipSettingsChannel, section, sub, value);
  };

  _clipSettings->onControlToggled = [this] (int section, int sub) {
    handleClipSettingsToggle (_clipSettingsChannel, section, sub);
  };

  _clipSettings->onControlReset = [this] (int section, int sub) {
    handleClipSettingsReset (_clipSettingsChannel, section, sub);
  };

  // MAINMIX, FILES and PADS lie over the sphere; a second tap on the lit one
  // takes it away again. Any page tab takes it away and shows its page.
  _clipSettings->onPageSelected = [this] (BarPage page) {
    showOverSphere (SphereOverlay::None);
    showBarPage (page);
  };
  // The elevation picture switches camera mode: while it is on, a finger on
  // the sphere turns the view instead of taking a blob.
  _clipSettings->onElevationPictureTapped = [this] {
    _cameraMode = !_cameraMode;
    if (_motionComponent)
      _motionComponent->setCameraMode (_cameraMode);
    _clipSettings->setCameraMode (_cameraMode);
    updateControlReadout (_cameraMode ? "-- CAMERA ON" : "-- CAMERA OFF");
  };
  // The other key swaps one for the other: they share the sphere's rectangle.
  _clipSettings->onSphereOverlayTapped = [this] (SphereOverlay tapped) {
    auto const next = overlayAfterTap (_overSphere, tapped);
    updateControlReadout (overlayReadout (next == SphereOverlay::None
                                              ? _overSphere
                                              : next,
                                          next != SphereOverlay::None));
    showOverSphere (next);
  };
  _clipSettings->setOverSphere (_overSphere);

  _controller = std::make_unique<ControllerComponent> ();
  _controller->onPadPressed = [this] (index_t channel, index_t pad) {
    handlePadPress (channel, pad);
    showPushedAction (channel, pad, PadSource::PadsPage);
  };
  _controller->onPadReleased = [this] (index_t channel, index_t pad) {
    handlePadRelease (channel, pad);
  };
  _controller->onScenePressed = [this] (index_t slot, std::size_t row) {
    handleScenePress (slot, row);
  };
  _controller->onSceneReleased = [this] (index_t slot, std::size_t row) {
    handleSceneRelease (slot, row);
  };
  _controller->onKeyPressed = [this] (FunctionKey key) {
    setFunctionKey (key, KeySource::Screen, true);
  };
  _controller->onKeyReleased = [this] (FunctionKey key) {
    setFunctionKey (key, KeySource::Screen, false);
  };

  // The ACTION page: what the ACT key does to the clip the bar is showing.
  // Like the pads page, it decides nothing -- a turn arrives here as (control,
  // increment) and is applied to the same Pattern the bar's own knobs write.
  _action = std::make_unique<ActionComponent> ();
  _action->onControlDragged = [this] (int control, int increment) {
    applyActionControl (control, increment);
  };
  // A knob says where it stands; the page counts nothing. Two taps are the
  // slider's own (setDoubleClickReturnValue), and they arrive here the same
  // way as a turn.
  _action->onControlSet = [this] (int control, double value) {
    setActionControl (control, value);
  };
  _action->onControlDoubleTapped = [this] (int control) {
    resetActionControl (control);
  };
  _action->onMotionSet = [this] (MotionParam param, float value) {
    setShownButtonMotion (param, value);
  };
  _action->onMotionUnset = [this] (MotionParam param) {
    setShownButtonMotion (param, std::nullopt);
  };
  _action->onControlTapped = [this] (int control) {
    // Only the mode is a tap: the three knobs are turned, and a tap on one
    // would otherwise step it by nothing at all.
    if (control == ActionComponent::ActMode)
      applyActionControl (control, 1);
  };

  // The six fields choose (2026-09-28): the list, the keys, the card and
  // the screen's ACT act on the chosen button. Firing is the pads' job.
  _action->onButtonChosen = [this] (int button) { chooseActionButton (button); };
  _action->onAfterStepped = [this] (int increment) {
    if (auto const *shown = shownActionButton ())
      setShownButtonAfter (stepAfter (shown->after, increment));
  };
  _action->onAfterCleared = [this] { setShownButtonAfter (std::nullopt); };

  _action->onActionChosen = [this] (juce::String const &name) {
    setButtonAction (_clipSettingsChannel,
                     _chosenActionButton[_clipSettingsChannel],
                   name.isEmpty ()
                       ? juce::File{}
                       : namedFileIn (actionsDir (), name, ".scd"));
    refreshBrowser ();
  };

  // EDIT: the shown clip's action, opened beside the list in FILES. A Save
  // as from there points this clip at the copy (takeEditOrigin), as the
  // editor on this page did before it moved (2026-09-27). The panel is
  // brought to the chosen row by refreshBrowser -- see syncFilePanel.
  _action->onEditPressed = [this] {
    // The origin's second index is the button EDIT came from.
    _editOrigin = SlotRef{ _clipSettingsChannel,
                           static_cast<index_t> (
                               _chosenActionButton[_clipSettingsChannel]) };
    showOverSphere (SphereOverlay::Files);
    _browserList = BrowserList::Actions;
    _browser->setShowingList (BrowserList::Actions);
    _deleteArmed = false;
    _browser->cancelRename ();
    refreshBrowser (BrowserSelection::PointAtTheSlot);
  };

  // The browser: the library the shown slot is filled from. What a row means
  // is decided here rather than there, the same way the pads page knows
  // nothing about what a pad does.
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
    if (fileTextHoldsTheList ())
      return;

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

  _browser->onDeletePressed = [this] {
    if (fileTextHoldsTheList ())
      return;
    deleteChosenEntry ();
  };
  // Load, on every tab (2026-09-27): the chosen row onto the shown slot --
  // its clip, its figure, its action -- or the set. One press, not two; a key
  // you reach for and read before pressing is already the deliberate act the
  // tap is not.
  _browser->onLoadPressed = [this] {
    auto const row = _browser->getSelectedEntry ();
    if (row < 0 || !currentList ().hasFileAt (row))
      return;
    _deleteArmed = false;
    currentList ().assign (row);
  };

  // Steps through the three on a tap, like every other few-valued control in
  // the bar. The word on the key is the state it is in, not the one the next
  // press would bring -- a key that names what you would get rather than what
  // you have is a key you have to press to find out where you are.
  _browser->onFilterPressed = [this] {
    // Asks the rule the key is lit by, rather than carrying a second copy of
    // it. Twice now the two have been changed one at a time and the key has
    // spent a round lit and inert, or dark and working -- which teaches you
    // that dark means nothing in particular. One condition cannot disagree
    // with itself.
    if (!currentLibraryKeys ().filter)
      return;

    _deleteArmed = false;
    _browser->cancelRename ();

    switch (_clipFilter)
      {
      case ClipFilter::All: _clipFilter = ClipFilter::User; break;
      case ClipFilter::User: _clipFilter = ClipFilter::System; break;
      case ClipFilter::System: _clipFilter = ClipFilter::All; break;
      }

    // Der Filter engt die Liste ein. Bewusst wie bisher: welche Zeile
    // danach gemeint ist, ist eine eigene Frage -- die gehaltene Nummer zeigt
    // in einer kuerzeren Liste auf einen anderen Clip.
    refreshBrowser (BrowserSelection::PointAtTheSlot);
  };

  _browser->onClipsChosen = [this] {
    // Another tab is another file: unsaved text holds it here.
    if (fileTextHoldsTheList ())
      return;
    _browser->scriptPanel ().stopEditing ();
    _browserList = BrowserList::Clips;
    _deleteArmed = false;
    _browser->cancelRename ();
    _browser->setSelectedEntry (-1);
    // Reiterwechsel: eine andere Liste, also eine andere Frage.
    refreshBrowser (BrowserSelection::PointAtTheSlot);
  };
  _browser->onShapesChosen = [this] {
    // Another tab is another file: unsaved text holds it here.
    if (fileTextHoldsTheList ())
      return;
    _browser->scriptPanel ().stopEditing ();
    _browserList = BrowserList::Shapes;
    _deleteArmed = false;
    _browser->cancelRename ();
    _browser->setSelectedEntry (-1);
    // Reiterwechsel.
    refreshBrowser (BrowserSelection::PointAtTheSlot);
  };
  _browser->onActionsChosen = [this] {
    // Another tab is another file: unsaved text holds it here.
    if (fileTextHoldsTheList ())
      return;
    _browserList = BrowserList::Actions;
    _deleteArmed = false;
    _browser->cancelRename ();
    _browser->setSelectedEntry (-1);
    // Reiterwechsel. The script beside the list follows through
    // syncFilePanel, which keeps unsaved text rather than dropping it.
    refreshBrowser (BrowserSelection::PointAtTheSlot);
  };

  // The chosen action's script beside the list (2026-09-27): the keys only
  // say they were pressed, what they mean is decided here.
  auto &filesScript = _browser->scriptPanel ();
  filesScript.onEditingChanged = [this] (bool editing) {
    showKeyboard (editing);
  };
  filesScript.onSave = [this] { saveFileText (); };
  filesScript.onSaveAs = [this] { saveFileTextAs (); };
  filesScript.onCancel = [this] { showFileText (_panelFile); };
  // FROM: the current state as this tab's file text, written by the same
  // writer Save used to call straight into the file (currentStateText).
  filesScript.onFromClip = [this] {
    auto const text = currentList ().currentStateText ();
    if (text.isNotEmpty ())
      _browser->scriptPanel ().offerScript (text);
  };
  _browser->onSetsChosen = [this] {
    // Another tab is another file: unsaved text holds it here.
    if (fileTextHoldsTheList ())
      return;
    _browser->scriptPanel ().stopEditing ();
    _browserList = BrowserList::Sessions;
    _deleteArmed = false;
    _browser->cancelRename ();
    _browser->setSelectedEntry (-1);
    // Reiterwechsel.
    refreshBrowser (BrowserSelection::PointAtTheSlot);
  };
  _browser->onEntryChosen = [this] (int index) {
    if (fileTextHoldsTheList ())
      return;
    // Anything else you do puts the delete key back to sleep. An armed key
    // you have forgotten about is worse than no key at all.
    _deleteArmed = false;
    _browser->cancelRename ();
    _browser->setSelectedEntry (index);

    // A tap chooses and shows, on every tab (maintainer, 2026-09-27: "tap only
    // shows, Load loads"): the file stands beside the list to read, and
    // nothing lands on a slot by brushing the list. The Load key does that --
    // a set was the first to learn it, since it replaces all eight slots.
    refreshBrowser ();
  };

  addChildComponent (*_clipSettings);
  _clipSettings->setVisible (true);

  // A child of the bar, not a sibling of it. The bar fills its whole area
  // with the surface colour at 85% (panelOpacity), so a sibling drawn under
  // that came through at fifteen percent of itself — a page whose job is
  // showing which clip is running, showing it in the dark. A child is painted
  // after its parent by construction, and no z-order call can undo that.
  _clipSettings->addChildComponent (*_action);
  // Over the sphere since 2026-09-27, where the big mixer stands: a child of
  // MotionComponent like it, so it composites above the GL context, and
  // always on top of the sphere's other furniture for the same reason.
  _browser->setAlwaysOnTop (true);
  _motionComponent->addChildComponent (*_browser);
  // PADS followed FILES over the sphere the same day, for the same reasons.
  _controller->setAlwaysOnTop (true);
  _motionComponent->addChildComponent (*_controller);
  _clipSettings->addChildComponent (*_mixerStrip);

  // The keyboard, over the clip content like ACTION and CHMIX, and above
  // them. Its keys go out as ordinary key presses through the window's peer
  // -- the same way a plugged-in keyboard's do -- to whatever holds the focus.
  _barKeyboard = std::make_unique<BarKeyboardComponent> ();
  _barKeyboard->setAlwaysOnTop (true);
  _barKeyboard->onKey = [this] (juce::KeyPress const &key) { typeKey (key); };
  _barKeyboard->onHide = [this] { showKeyboard (false); };
  _barKeyboard->onPlacementStale = [this] { placeKeyboard (); };
  _barKeyboard->isShiftHeld
      = [this] { return isButtonPressed (Button::Shift); };
  _clipSettings->addChildComponent (*_barKeyboard);
  selectClip (0, 0); // sensible default before any button has been pressed

  // The device's own habits, restored: the clock mode, the rec mode, and the
  // four speeds the bar's keys carry. Pot Size and the two font sizes are the
  // skin's now, and the skin brings its own.
  auto const persisted = loadSettings (getPersistedSettingsFile ());

  // Read out of `persisted` before applyClockMode() runs, because it writes
  // the settings back out: anything still sitting at its default when it does
  // is written over what the file said -- silently, since the UI goes on
  // showing the value that was read and only the next start reveals it. The
  // rec mode has stood in that position since it was added and survived only
  // because the mode it wrote back happened to be the default one. The clock
  // mode itself is not pre-assigned: applyClockMode() returns early on a mode
  // it is already in, and would then apply none of it.
  _recMode = persisted.recMode;
  _speedButtonLog2 = persisted.speedButtonLog2;
  _developerMode = persisted.developerMode;
  _skinBeforeClean = persisted.skinBeforeClean;
  _view = persisted.fpvView ? AppView::Fpv : AppView::Full;
  refreshCleanKey ();

  // The view the room was last looked at from, and saved again whenever a
  // camera gesture settles. Wired after it is applied, so putting it back is
  // not itself a change that writes the file.
  if (_motionComponent)
    {
      _motionComponent->setCamera (
          { persisted.cameraPitch, persisted.cameraTurn });
      _motionComponent->setCameraZoom (persisted.cameraZoom);
      _motionComponent->onCameraChanged = [this] { persistSettings (); };
    }

  // Where each encoder was left on MOTION and REC.
  _encoderClicksMotion = encoderClicksFromMask (persisted.encoderClicksMotion);
  _encoderClicksRecord = encoderClicksFromMask (persisted.encoderClicksRecord);
  showEncoderMarks ();

  applyClockMode (persisted.clockMode);
  _engine.setRecMode (_recMode);
  _clipSettings->setSpeedButtons (_speedButtonLog2);
  // The view last left in. After everything above: setView writes the
  // settings back out.
  setView (_view);


  // See uiTimerHz: fast enough for the write head to move while a take runs,
  // and the rate the status bar's meters are redrawn at. The directory check
  // inside keeps its two-second pace by counting ticks.
  startTimerHz (uiTimerHz);

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

  // Where Motion listens and sends: the one truth, a3-osc.json.
  auto const endpoints = oscEndpointsFrom (installedOscTruth ());
  if (!installedOscTruth ().isValid ())
    std::cerr << "ERROR: " << installedOscTruth ().error ()
              << " -- OSC stays closed" << std::endl;

  auto const oscRecvPort = endpoints.receivePort;
  if (userConfig.hasProperty ("ui"))
    {
      auto const uiConfig = userConfig["ui"];
      if (uiConfig.hasProperty ("pauseRenderingInMenu"))
        _pauseRenderingInMenu
            = static_cast<bool> (uiConfig["pauseRenderingInMenu"]);
    }

  if (_oscReceiver.connect (oscRecvPort))
    {
      std::cout << "OSC Receiver listening on port " << oscRecvPort << std::endl;
      _oscReceiver.addListener (this);

      _beatArrival.setAddress (_oscAddresses.beatIn);
      _oscReceiver.addListener (&_beatArrival);

      if (BeatTrace::device ().isEnabled ())
        {
          std::cout << "Beat trace on: " << std::getenv ("A3_BEAT_TRACE")
                    << std::endl;
          _beatTraceHandle
              = _engine.getTempoClock ().scheduleEventHandlerAddition (
                  [] (auto measure) {
                    BeatTrace::device ().record ("engine", measure.beat (),
                                                 measure.bar (), 0.f);
                  },
                  TempoClock::Event::Beat, TempoClock::Execution::TimerThread);
        }
    }
  else
    {
      std::cerr << "ERROR: Could not bind OSC Receiver to port " << oscRecvPort << std::endl;
    }

  // Setup OSC Receiver for VU meters (separate port)
  auto const oscVuPort = endpoints.vuPort;
  if (_oscReceiverVU.connect (oscVuPort))
    {
      std::cout << "OSC VU Receiver listening on port " << oscVuPort << std::endl;
      _oscReceiverVU.addListener (this);
    }
  else
    {
      std::cerr << "ERROR: Could not bind OSC VU Receiver to port " << oscVuPort << std::endl;
    }

  // Setup OSC Receiver for the IEM EnergyVisualizer (separate port again —
  // it sends 426 floats at 9 Hz and has no business sharing a socket with the
  // beat clock).
  auto const oscEnergyPort = endpoints.energyPort;
  if (_oscReceiverEnergy.connect (oscEnergyPort))
    {
      std::cout << "OSC Energy Receiver listening on port " << oscEnergyPort << std::endl;
      _oscReceiverEnergy.addListener (this);
    }
  else
    {
      std::cerr << "ERROR: Could not bind OSC Energy Receiver to port " << oscEnergyPort << std::endl;
    }

  // The OSC senders. Two destinations, not one: the beat clock and the tap
  // belong to the beat-analyzer, everything the mixer turns belongs to
  // A3 Core -- which is where MotionEngine's SpatBackendA3 already sends the
  // spatial position. oscEndpointsFrom() is the one place that says which
  // is which.
  auto const &clock = endpoints.beatclock;
  if (_oscSender.connect (clock.host, clock.port))
    std::cout << "OSC Sender for beatclock connected to " << clock.host << ":" << clock.port << std::endl;
  else
    std::cerr << "ERROR: OSC Sender failed to connect to " << clock.host << ":" << clock.port << std::endl;

  // Direct tap sender (same host/port, bypasses async queue for zero latency)
  if (connectOscSender (_tapSender, clock.host, clock.port))
    std::cout << "OSC Tap Sender connected to " << clock.host << ":" << clock.port << std::endl;
  else
    std::cerr << "ERROR: OSC Tap Sender failed to connect" << std::endl;

  auto const &core = endpoints.core;
  if (!connectOscSender (_helloSender, core.host, core.port))
    std::cerr << "ERROR: OSC hello sender failed to connect to " << core.host << ":" << core.port << std::endl;
  if (_mixerSender.connect (core.host, core.port))
    {
      std::cout << "OSC Sender for mixer connected to " << core.host << ":" << core.port << std::endl;
      askCoreForItsState ();
    }
  else
    std::cerr << "ERROR: OSC Sender for mixer failed to connect to " << core.host << ":" << core.port << std::endl;
}

A3MotionUIComponent::~A3MotionUIComponent ()
{
  flushScriptWrites ();
  juce::Desktop::getInstance ().removeGlobalMouseListener (this);
  stopTimer ();
  _oscReceiverEnergy.removeListener (this);
  _oscReceiverEnergy.disconnect ();
  _oscReceiverVU.removeListener (this);
  _oscReceiverVU.disconnect ();
  _oscReceiver.removeListener (this);
  _oscReceiver.disconnect ();
  _oscSender.disconnect ();
  _tapSender.disconnect ();
  _mixerSender.disconnect ();

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
  // The bar's order is the engine's order -- the captions are listed in it
  // (ClipSettingsCaptions.hh) -- so the index is the enum. A value out of
  // range would be a list that grew in one place and not the other.
  if (params.direction >= 0 && params.direction < value::numDirections)
    pattern->setPlayDirection (static_cast<PlayDirection> (params.direction));
  if (params.endAction >= 0 && params.endAction < value::numEndActions)
    pattern->setEndAction (static_cast<EndAction> (params.endAction));
}

void
A3MotionUIComponent::applySpeedLog2ToShownClip (int speedLog2)
{
  auto const channel = _clipSettingsChannel;
  auto const slot = _clipSettingsSlot;
  auto &pattern = _patterns[channel][slot];
  if (!pattern)
    return;

  pattern->setSpeedLog2 (speedLog2);
  // How long one traversal takes is derived from the speed, so the engine
  // goes on playing the old length until it is told the new one.
  pattern->setPlaybackLength (getPlaybackLength (channel, slot));

  updateClipSettingsDisplay ();
  scheduleSetSave ();
}

void
A3MotionUIComponent::persistSettings () const
{
  auto settings = AppSettings{ _clockMode, _recMode, _speedButtonLog2,
                               _developerMode, _skinBeforeClean };
  if (_motionComponent)
    {
      auto const camera = _motionComponent->getCamera ();
      settings.cameraPitch = camera.pitch;
      settings.cameraTurn = camera.turn;
      settings.cameraZoom = _motionComponent->getCameraZoom ();
    }
  settings.encoderClicksMotion = encoderClicksMask (_encoderClicksMotion);
  settings.encoderClicksRecord = encoderClicksMask (_encoderClicksRecord);
  settings.fpvView = _view == AppView::Fpv;
  saveSettings (getPersistedSettingsFile (), settings);
}

Measure
A3MotionUIComponent::playbackLengthOf (Pattern const &pattern) const
{
  // What getPlaybackLength() works out for a pattern in a slot, for one that
  // is in none yet.
  auto const patternBeats
      = pattern.getNumTicks () > 0
            ? static_cast<float> (pattern.getNumTicks ())
                  / static_cast<float> (TempoClock::getTicksPerBeat ())
            : defaultPatternLengthBeats;
  auto const ticks = playbackLengthTicks (
      playbackLengthBeats (patternBeats, pattern.getSpeedLog2 ()),
      static_cast<index_t> (TempoClock::getTicksPerBeat ()));

  return Measure{ 0, 0, static_cast<int> (ticks) }.consolidate (
      _engine.getBeatsPerBar ());
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
A3MotionUIComponent::toggleWorkspaceList ()
{
  if (_workspaceList->isVisible ())
    {
      _workspaceList->setVisible (false);
      return;
    }

  _workspaceList->show (workspaces::list ());
}

void
A3MotionUIComponent::createMainUI ()
{
  // Seeded before the bar reads it: the value was only ever written on a tap
  // or a mode change, so until somebody tapped, the internal reading had no
  // tempo to show at all.
  _valueBPM = static_cast<double> (_engine.getTempoBPM ());

  _statusBar = std::make_unique<StatusBar> (_valueBPM);
  // The keyboard lives in the bar, which FPV hides: it opens in FULL only.
  _statusBar->onKeyboardIconTapped = [this] {
    if (_view == AppView::Fpv)
      setView (AppView::Full);
    toggleKeyboard ();
  };
  _statusBar->onCleanIconTapped = [this] { toggleClean (); };
  _statusBar->onClockKeyTapped = [this] { stepClockMode (); };
  _statusBar->onMenuKeyTapped = [this] { toggleGlobalSettings (); };
  _statusBar->onDeckKeyTapped = [] { workspaces::goTo (workspaces::stemDeck); };
  _statusBar->onWorkspacesKeyTapped = [this] { toggleWorkspaceList (); };
  _statusBar->onTickTapped = [this] { handleScreenTap (); };
  addChildComponent (*_statusBar);
  _statusBar->setVisible (true);
  _statusBarCallbackHandle
      = _engine.getTempoClock ().scheduleEventHandlerAddition (
          [this] (auto measure) {
            _statusBar->beatCallback (measure);

            // The TAP key breathes with the beat, on the screen and on the
            // panel, from the one place that knows a beat went by. It was
            // taken out once for being too loud; it is a faint wash now, not
            // the flash a press makes. The play key's wait turns over on the
            // same beat -- see pulseOnBeat().
            if (_clipSettings)
              _clipSettings->pulseOnBeat ();
            pulseTapLED ();

            // Counting the hand in while a take waits for its downbeat. From
            // here rather than from StatusBar::beatCallback, which returns
            // early in every clock mode but internal.
            if (_statusBar)
              _statusBar->pulseCountInOnBeat ();
          },
          TempoClock::Event::Beat, TempoClock::Execution::JuceMessageThread);

  _motionComponent
      = std::make_unique<MotionComponent> (_engine, _channelUIStates);
  addChildComponent (*_motionComponent);
  _motionComponent->setVisible (true);

  // FPV's four strips below the sphere; shown by setView().
  _fpvStrips = std::make_unique<FpvStrips> ();
  _fpvStrips->channelLevel
      = [this] (int ch) { return _vuLevels.channel (ch, vuNowMs ()); };
  addChildComponent (*_fpvStrips);
  _statusBar->onViewKeyTapped = [this] { setView (toggled (_view)); };

  // Hidden: no longer part of the visible layout (see resized()), but these
  // keep receiving their normal update calls underneath.
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
  // While the keyboard is up, the panel's buttons are its keys.
  _ioAdapter->onPanelKey = [this] (PanelCell cell, bool down) {
    if (_barKeyboard)
      _barKeyboard->pressPanelCell (cell, down);
  };
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
  _armedFollows.resize (numChannels);

  for (auto &channelPatterns : _patterns)
    channelPatterns.resize (numClipSlots);
  for (auto &channelParams : _clipUIParams)
    channelParams.resize (numClipSlots);
  // Sized alongside _patterns, always: fillSlotFromLibrary() indexes both, so
  // one shorter than the other is a slot that cannot say where it came from.
  for (auto &channelClips : _slotClipFile)
    channelClips.resize (numClipSlots);
  _pendingTakes = PendingTakes (numChannels, numClipSlots);

  _channelActions.resize (numChannels);

  // Every slot starts holding Bloom. A slot that fires nothing has an ACT key
  // that does nothing, which is a key you have to be told about rather than
  // one you find; and Bloom is the one script that is obviously an effect the
  // first time it is held -- the clip opens out to the whole room and closes
  // again when you let go. A set that carries its own choice overwrites this
  // the moment it loads.
  {
    auto const bloom = namedFileIn (actionsDir (), "Bloom", ".scd");
    if (bloom.existsAsFile ())
      for (auto channel = 0u; channel < numChannels; ++channel)
        setButtonAction (channel, 0, bloom); // A1
  }

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

  if (_view == AppView::Fpv)
    {
      resizedFpv (bounds);
      return;
    }
  _fpvStrips->setBounds ({});

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
  if (_action)
    {
      _action->setBounds (_clipSettings->clipContentBounds ());
    }
  if (_mixerStrip && _clipSettings)
    _mixerStrip->setBounds (_clipSettings->clipContentBounds ());
  placeKeyboard ();

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
  placeOverSphere ();
}

void
A3MotionUIComponent::resizedFpv (juce::Rectangle<int> bounds)
{
  auto const layout
      = fpvLayout (bounds, juce::roundToInt (theme ().paddingSmall));
  _motionComponent->setBounds (layout.sphere);
  _fpvStrips->setBounds (
      layout.strips[0].whole.getUnion (layout.strips[3].whole));
  placeOverSphere ();
}

void
A3MotionUIComponent::placeOverSphere ()
{
  if (_globalSettings)
    _globalSettings->setBounds (_motionComponent->getLocalBounds ());
  if (_workspaceList)
    _workspaceList->setBounds (_motionComponent->getLocalBounds ());
  if (_mixer)
    _mixer->setBounds (_motionComponent->getLocalBounds ());
  if (_browser)
    _browser->setBounds (_motionComponent->getLocalBounds ());
  if (_controller)
    _controller->setBounds (_motionComponent->getLocalBounds ());
  if (_skinEditor)
    _skinEditor->setBounds (_motionComponent->getLocalBounds ());
  if (_skinPanel)
    {
      _skinPanel->setBounds (
          skinPanelBounds (_motionComponent->getLocalBounds ()));
      if (_skinPanelOpen && !_skinEditorOpen)
        _motionComponent->setSphereLeftInset (_skinPanel->getRight ());
    }
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
  return _channelStrips.size () * minimumChannelWidth;
}

float
A3MotionUIComponent::getMinimumHeight () const
{
  auto minimumHeight = minimumMotionHeight;
  return minimumHeight;
}

// TODO: factor this into separate listeners so not all sources have to
// be tested exhaustively.
void
A3MotionUIComponent::valueChanged (juce::Value &value)
{
  // The panel's six keys first: each goes into the one place that says
  // what a function key does, the same place the PADS page's keys reach.
  for (auto const key : functionKeyOrder)
    if (value.refersToSameSourceAs (_ioAdapter->getButton (key)))
      {
        setFunctionKey (key, KeySource::Panel,
                        static_cast<bool> (value.getValue ()));
        return;
      }

  if (value.refersToSameSourceAs (_ioAdapter->getTapTimeMicros ()))
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
          // The encoders turn the field they stand under on CLIP, MOTION,
          // REC and CHMIX, four by two as the fields are (2026-09-27); with
          // Shift, and on the other pages, their channel's FREQ and Q as
          // before. What each one turns is encoderTarget()'s to say.
          for (int row = 0; row < 2; ++row)
            {
              auto &turned = _ioAdapter->getEncoderIncrement (
                  channel, static_cast<index_t> (row));
              if (value.refersToSameSourceAs (turned))
                {
                  auto const increment
                      = static_cast<int> (turned.getValue ());
                  if (increment != 0)
                    {
                      // An encoder step is an input like a touch; the analog
                      // pots are not -- their noise would drop an armed
                      // DISCARD before the second tap could land (#32).
                      disarmOnOtherInput ();
                      handleEncoderTurn (static_cast<int> (channel), row,
                                         increment);
                    }
                  return;
                }

              auto &pressed = _ioAdapter->getEncoderPress (
                  channel, static_cast<index_t> (row));
              if (value.refersToSameSourceAs (pressed))
                {
                  if (static_cast<bool> (pressed.getValue ()))
                    handleEncoderPress (static_cast<int> (channel), row);
                  return;
                }
            }

          if (value.refersToSameSourceAs (
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
                    {
                      handlePadPress (channel, pad);
                      showPushedAction (channel, pad, PadSource::Panel);
                    }
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
A3MotionUIComponent::handleChannelValueChange (index_t channel, ChannelPot pot,
                                               int increment)
{
  // The same step the encoders take, so a finger and a knob move a value at
  // the same rate.
  auto const step = increment * 0.02f;
  setChannelPotValue (channel, pot, channelPotValue (channel, pot) + step);
}

float
A3MotionUIComponent::channelPotValue (index_t channel, ChannelPot pot)
{
  // The engine numbers them by the panel: pot 1 is freq, pot 2 is Q, pot 3
  // is the 3d pot.
  switch (pot)
    {
    case ChannelPot::ThreeD: return _engine.getChannelPot3 (channel);
    case ChannelPot::Freq: return _engine.getChannelPot1 (channel);
    case ChannelPot::Q: return _engine.getChannelPot2 (channel);
    }
  return 0.f;
}

void
A3MotionUIComponent::closeAllOverlays ()
{
  updateControlReadout ("-- CLOSE");

  if (_colourPickerOpen)
    closeColourPicker ();
  // Out of everything at once, and out of a mask without keeping it -- the
  // same as Back and Escape. Keeping is Enter's.
  if (_skinEditorOpen && _skinEditor->isNaming ())
    _skinEditor->cancelNaming ();
  if (_skinEditorOpen)
    closeSkinEditor ();
  if (_skinPanelOpen)
    closeSkinPanel ();
  if (_globalSettingsOpen)
    closeGlobalSettings ();
  showOverSphere (SphereOverlay::None);

  updateOverlayButtons ();
}

void
A3MotionUIComponent::setView (AppView view)
{
  jassert (_clipSettings && _fpvStrips && _motionComponent && _statusBar);
  _view = view;
  auto const fpv = view == AppView::Fpv;
  // An invisible keyboard would still own the panel's buttons.
  if (fpv)
    {
      closeAllOverlays ();
      showKeyboard (false);
    }
  _clipSettings->setVisible (!fpv);
  _fpvStrips->setVisible (fpv);
  _motionComponent->setFpv (fpv);
  _statusBar->setView (view);
  if (fpv)
    refreshFpvStrips ();
  resized ();
  updateControlReadout (fpv ? "-- FPV" : "-- FULL");
  persistSettings ();
}

void
A3MotionUIComponent::refreshFpvStrips ()
{
  std::array<FpvChannel, 4> channels{};
  for (index_t ch = 0; ch < channels.size () && ch < _channelUIStates.size ();
       ++ch)
    {
      auto &c = channels[ch];
      c.colour = _channelUIStates[ch]->colour;
      if (ch < _slotClipFile.size () && !_slotClipFile[ch].empty ())
        c.clipName = _slotClipFile[ch][0].getFileNameWithoutExtension ();
      if (ch < _patterns.size () && !_patterns[ch].empty () && _patterns[ch][0])
        c.playing = _patterns[ch][0]->getStatus () == Pattern::Status::Playing;
      // In channelPotOrder: pot 3 is 3D, pot 1 FREQ, pot 2 Q
      // (see channelPotValue).
      c.pots = { _engine.getChannelPot3Effective (ch),
                 _engine.getChannelPot1Effective (ch),
                 _engine.getChannelPot2Effective (ch) };
    }
  _fpvStrips->setChannels (channels);
}

void
A3MotionUIComponent::updateOverlayButtons ()
{
  if (!_overlayButtons || !_motionComponent)
    return;

  auto const keysShown = overlayKeysAreShown (
      _globalSettingsOpen, _skinEditorOpen, _colourPickerOpen, _overSphere);
  _overlayButtons->setVisible (keysShown);

  // The strips walk a list and change the highlighted row's value, so they
  // belong to the overlay in front and only while that one has a list. Asked
  // in OverlaySideStrips.hh, where a test can reach the question — the answer
  // decides who receives a fifth of the window on each side, and while the
  // mixer stood in front of an open menu the menu was still receiving it.
  auto const openOverlayHasAList = sideStripsHaveAList (
      menuListIsInFront (_globalSettingsOpen, _skinPanelOpen), _skinEditorOpen,
      _colourPickerOpen, _overSphere);

  // The strips sit beside whichever page is showing, so they follow its
  // panel rather than a fixed width.
  if (_overlayStrips)
    {
      _overlayStrips->setVisible (openOverlayHasAList);
      if (openOverlayHasAList)
        {
          _overlayStrips->setBounds (_motionComponent->getLocalBounds ());
          _overlayStrips->setPanel (_skinEditorOpen
                                        ? _skinEditor->panelBounds ()
                                        : _globalSettings->panelBounds ());
          // Beside the list the finger moves it as far as on it.
          _overlayStrips->setPixelsPerStep (_skinEditorOpen
                                                ? _skinEditor->rowPitch ()
                                                : _globalSettings->rowPitch ());
          _overlayStrips->toFront (false);
        }
    }

  if (!keysShown)
    return;

  auto const height = OverlayButtons::preferredHeight ();
  auto const width = height * 2 + juce::jmax (2, height / 8);
  auto const margin = OverlayButtons::preferredMargin ();

  auto const area = _motionComponent->getLocalBounds ();
  _overlayButtons->setBounds (area.getRight () - width - margin,
                              area.getY () + margin, width, height);
  _overlayButtons->toFront (false);
}

void
A3MotionUIComponent::toggleGlobalSettings ()
{
  if (_view == AppView::Fpv)
    setView (AppView::Full);
  updateControlReadout ("-- MENU");

  // One level at a time: what is over the sphere, then a name being typed,
  // then the editor, then the menu itself. The mixer and the browser are first
  // because they are opened from outside this chain — MAINMIX and FILES in the
  // global strip are reachable whatever else is up — so either is the
  // innermost room whenever it is open. A name being typed in the browser is
  // one level further in, and goes first without being kept, as Escape does.
  if (_overSphere == SphereOverlay::Files && _browser->isRenaming ())
    {
      _browser->cancelRename ();
      return;
    }
  if (_overSphere != SphereOverlay::None)
    {
      showOverSphere (SphereOverlay::None);
      return;
    }

  // Back leaves a mask without keeping what was in it, the way Escape does:
  // keeping is Enter's, or a tap on the value in a list.
  if (_colourPickerOpen)
    closeColourPicker ();
  else if (_skinEditorOpen && _skinEditor->isNaming ())
    _skinEditor->cancelNaming ();
  else if (_skinEditorOpen)
    closeSkinEditor ();
  else if (_skinPanelOpen)
    closeSkinPanel ();
  else if (_globalSettingsOpen && _globalSettings->isPickerOpen ())
    _globalSettings->cancelPicker ();
  else if (_globalSettingsOpen)
    closeGlobalSettings ();
  else
    openGlobalSettings ();
}

void
A3MotionUIComponent::showOverSphere (SphereOverlay overlay)
{
  if (_view == AppView::Fpv && overlay != SphereOverlay::None)
    setView (AppView::Full);
  if (!_mixer || !_browser || !_controller || !_motionComponent)
    return;

  auto const wasFiles = _overSphere == SphereOverlay::Files;
  _overSphere = overlay;
  auto const mixer = overlay == SphereOverlay::MainMix;
  auto const files = overlay == SphereOverlay::Files;
  auto const pads = overlay == SphereOverlay::Pads;

  // Leaving the browser leaves its masks: a name being typed goes without
  // being kept -- keeping is Enter's or Keep's -- which also puts the keyboard
  // away, and an armed Delete is put back to sleep. An armed key you cannot
  // see is worse than no key at all.
  if (wasFiles && !files)
    {
      // The script beside the list stops taking keys, and what was typed
      // stays marked as unsaved; EDIT's origin lasted this one visit.
      _browser->scriptPanel ().stopEditing ();
      _editOrigin.reset ();
      _browser->cancelRename ();
      _deleteArmed = false;
    }

  // Over the sphere, on the bounds MotionComponent actually has: the settings
  // bar is carved out of those, so an overlay taking them covers the sphere
  // and nothing else.
  _mixer->setBounds (_motionComponent->getLocalBounds ());
  _mixer->setVisible (mixer);
  if (mixer)
    _mixer->toFront (false);

  _controller->setBounds (_motionComponent->getLocalBounds ());
  _controller->setVisible (pads);
  if (pads)
    _controller->toFront (false);

  _browser->setBounds (_motionComponent->getLocalBounds ());
  _browser->setVisible (files);
  if (files)
    {
      _browser->toFront (false);
      // Opening: the list has no chosen row yet that means anything, so it
      // points at what the shown slot is holding.
      if (!wasFiles)
        refreshBrowser (BrowserSelection::PointAtTheSlot);
    }

  // Guarded because the overlays are built with the rest of the sphere's
  // furniture, well before the bar exists — and closeAllOverlays() is
  // reachable from anywhere.
  if (_clipSettings)
    _clipSettings->setOverSphere (overlay);

  updateOverlayButtons ();
}

bool
A3MotionUIComponent::recArmedOnShownSlot () const
{
  return _recArmedSlot
         == std::optional<std::pair<index_t, index_t> > (
             std::pair{ _clipSettingsChannel, _clipSettingsSlot });
}

void
A3MotionUIComponent::toggleRecordingOnShownClip ()
{
  // What ● does is RecArming's to say (2026-09-26): SAVE while an unsaved
  // take waits, end a take that runs, otherwise arm or disarm REC PAUSE --
  // the take is set up on the REC page and ▶ starts it.
  //
  // "Underway" is asked for, not merely running: startRecording() schedules
  // the take for the next downbeat, so isRecording() is still false right
  // after it, and a second tap inside that window must end the take rather
  // than arm another.
  auto const underway = takeIsUnderway ();
  auto const action = recKeyAction (
      _pendingTakes.offersKeys (_clipSettingsChannel, _clipSettingsSlot,
                                underway),
      underway, recArmedOnShownSlot ());

  _pendingTakes.disarm ();
  refreshTakeState ();

  switch (action)
    {
    case RecKeyAction::Save:
      saveShownTake ();
      return;

    case RecKeyAction::EndTake:
      updateControlReadout ("-- REC OFF");
      endRecording ();
      return;

    case RecKeyAction::Disarm:
      _recArmedSlot.reset ();
      updateControlReadout ("-- REC OFF");
      refreshRecArmed ();
      return;

    case RecKeyAction::Arm:
      // The clip the bar is showing, and the page the take is set up on. The
      // clip on that slot keeps playing: arming changes nothing audible.
      _recArmedSlot = std::pair{ _clipSettingsChannel, _clipSettingsSlot };
      updateControlReadout ("-- REC ARMED");
      showBarPage (BarPage::Record);
      refreshRecArmed ();
      return;
    }
}

void
A3MotionUIComponent::startArmedTake ()
{
  _recArmedSlot.reset ();
  refreshRecArmed ();
  updateControlReadout ("-- REC ON");
  startRecording (_clipSettingsChannel, _clipSettingsSlot);
}

void
A3MotionUIComponent::refreshRecArmed ()
{
  // Armed is only ever the shown slot: showing another drops it, or ▶ there
  // would start a take on a slot nobody is looking at.
  if (_recArmedSlot && !recArmedOnShownSlot ())
    _recArmedSlot.reset ();

  if (_clipSettings)
    _clipSettings->setRecArmed (recArmedOnShownSlot ());
}

void
A3MotionUIComponent::stepClockMode ()
{
  // The same three the menu offers, in the same order.
  applyClockMode ((_clockMode + 1) % 3);
  if (_clipSettings)
    _clipSettings->setClockMode (_clockMode);
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

  // The length the shown clip is set to play at -- the lit speed key, which
  // names a length rather than a rate. The slot's last one only when there is
  // no clip. See RecordingLength.hh.
  auto const beatsPerBar = _engine.getBeatsPerBar ();
  auto const clipLengthBeats
      = pattern ? juce::roundToInt (playbackLengthBeats (
            getPatternLengthBeats (channel, slot), pattern->getSpeedLog2 ()))
                : 0;

  auto const configuredLengthBeats = static_cast<float> (recordingLengthBeats (
      clipLengthBeats, _clipUIParams[channel][slot].recordLengthLog2,
      beatsPerBar));

  // A take runs until Record is pressed again. OneShot — the default —
  // schedules its own stop one length in, which is why recording ended by
  // itself with nobody touching the button.
  _engine.setRecordingMode (MotionEngine::RecordingMode::Loop);

  // Remembered so an empty take can be undone: the slot's pattern is
  // replaced right below, and a stray double press must not cost whatever
  // was in there.
  _recordingSlot = std::make_pair (channel, slot);
  _patternBeforeRecording = pattern;
  _clipFileBeforeRecording = _slotClipFile[channel][slot];
  // The take is no clip file's yet. Pointing at the old one would let FILES'
  // Save write the take's values over it.
  _slotClipFile[channel][slot] = juce::File{};

  // Drawn faintly under the take so you can see what you are writing over.
  // MotionComponent decides whether to show it -- Write replaces the whole
  // pass, and a ghost of the old one there says nothing.
  if (_motionComponent)
    _motionComponent->setRecordingUnderlay (_patternBeforeRecording);

  // A fresh Pattern for the take, starting from the clip the slot held: its
  // settings now, so the bar goes on showing them, and its path and lanes at
  // the downbeat (seedTake). TOUCH over it is an overdub -- turning pots over
  // the old figure keeps the figure.
  pattern = std::make_shared<Pattern> ();
  pattern->setChannel (channel);
  if (_patternBeforeRecording)
    applyClipSettings (*pattern, clipSettingsFrom (*_patternBeforeRecording));

  auto recordLength = Measure{
    0, static_cast<int> (std::max (1.f, configuredLengthBeats)), 0
  };
  recordLength.consolidate (_engine.getBeatsPerBar ());

  // Store the recording length in the pattern so it can be updated if encoder changes
  pattern->setPlaybackLength (recordLength);

  _engine.recordPattern (pattern, TempoClock::nextDownBeat (_now),
                         recordLength, _patternBeforeRecording);

  // A take underway turns SAVE and DISCARD back into REC and ACT.
  refreshTakeState ();

  // Show what is being recorded. Starting a recording on one channel while
  // the bar still displayed another one left every setting that shapes the
  // take — speed above all, which is its length — pointing at the wrong
  // clip, and the encoders with it.
  selectClip (channel, slot);

  // The bar's REC light is not set here. It follows the engine in the UI
  // tick, because a take also ends by itself and nothing is pressed then.

  updateFunctionKeyLEDs ();
}

void
A3MotionUIComponent::handlePadPress (index_t channel, index_t pad)
{
  // A pad, on the screen or on the device, is another input: DISCARD only
  // confirms if nothing came between its two taps (#32).
  disarmOnOtherInput ();

  // One clip per channel since 2026-09-27: every pad is about slot 0.
  index_t const slot = 0;
  auto const function = padFunctionByPadIndex[pad];
  auto const button = actionButtonForPad[pad];
  auto &pattern = _patterns[channel][slot];

  auto const name
      = function == PadFunction::PlayPause ? juce::String ("PLAYPAUSE")
        : function == PadFunction::Page    ? juce::String ("PAGE")
                                           : "A" + juce::String (button + 1);
  updateControlReadout ("CH" + juce::String (channel + 1) + " " + name);

  // The bar follows the hand. Pressing play or an action on a clip is saying
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
      // A take is steered and saved in FULL, as MENU does.
      if (_view == AppView::Fpv)
        setView (AppView::Full);
      startRecording (channel, slot);
      return;
    }

  switch (function)
    {
    case PadFunction::PlayPause:
      {
        // Play|Pause is a hand taking the channel back: a chain ends here.
        if (channel < _actionChains.size ())
          _actionChains[channel].broken ();
        if (!pattern)
          break;

        // **On the next downbeat**, and with Shift on the spot -- which,
        // since the panel lost its Stop pads (2026-09-27), is also how a
        // running clip is stopped now.
        //
        // This used to be the next beat, on the reasoning that a bar is up to
        // a metre's worth of beats away and a clip starting that late reads as
        // a button that did not work. What that reasoning was missing is that
        // a figure which does not begin on the one runs the whole pass against
        // the music -- and it was written before the key blinked while it
        // waited, which is what makes the wait legible rather than dead.
        auto const on = isButtonPressed (Button::Shift)
                            ? _now
                            : TempoClock::nextDownBeat (_now);

        auto const status = pattern->getStatus ();
        if (status == Pattern::Status::Idle)
          {
            pattern->setPlaybackLength (getPlaybackLength (channel, slot));
            _engine.playPattern (pattern, on);
          }
        else if (status == Pattern::Status::Playing)
          {
            // On the next downbeat, like a start, and with Shift on the spot.
            // A pause that answered a lap later read as a key that does not
            // work (maintainer, 2026-09-25). The key blinks while it waits.
            _engine.stopPattern (pattern, on);
          }
        else if (status == Pattern::Status::ScheduledForPlaying)
          {
            // Not started yet, so there is no lap to finish: this is calling
            // off the start that is waiting for the downbeat. Taken back
            // rather than stopped -- a stop scheduled on top of a start is
            // still a start, see cancelScheduledPlay().
            _engine.cancelScheduledPlay (pattern);
          }
        break;
      }
    case PadFunction::Page:
      {
        // Another channel's PAGE brings that channel up on the page you are
        // on; the shown channel's steps through its pages, back with Shift.
        // Whatever lies over the sphere goes first -- PAGE is about the clip.
        if (_overSphere != SphereOverlay::None)
          showOverSphere (SphereOverlay::None);
        if (channel != _clipSettingsChannel)
          {
            selectClip (channel, slot);
            break;
          }
        showBarPage (nextClipPage (_barPage, isButtonPressed (Button::Shift)));
        break;
      }
    case PadFunction::Action:
      {
        // A button with nothing assigned does nothing (2026-09-27): six
        // plain accents that look assigned would be six ways to be misled.
        // A Cue (library v2): the button's clip goes onto the channel and
        // starts on the next downbeat -- Shift at once -- and stays there.
        // No accent: a Cue changes what plays, not how it plays. Decided
        // before anything else, so a Cue whose clip is gone never falls
        // through to an ordinary accent.
        {
          auto const &cueButton
              = _channelActions[channel][static_cast<size_t> (button)];
          auto const press = cuePressFor (
              cueButton.isCue, cueButton.cueClip.existsAsFile (),
              _recordingSlot.has_value () && _recordingSlot->first == channel,
              _pendingTakes.isPending (channel, slot));
          switch (press)
            {
            case CuePress::NotACue:
              break;
            case CuePress::Recording:
              return;
            case CuePress::TakeWaiting:
              updateControlReadout ("-- SAVE THE TAKE FIRST");
              return;
            case CuePress::NoClip:
              updateControlReadout ("-- NO SUCH CLIP");
              return;
            case CuePress::Load:
              {
                if (!loadClipIntoChannel (channel, cueButton.cueClip, false))
                  return;
                updateControlReadout (
                    "CH" + juce::String (channel + 1) + " " + name + " CUE "
                    + cueButton.cueClip.getFileNameWithoutExtension ()
                          .toUpperCase ());
                if (auto const &loaded = _patterns[channel][slot])
                  {
                    // The script's own lines ride on the clip it cues: ACTION
                    // shows them for this button and writes them into its
                    // script, and a press that dropped them did nothing at
                    // all on a channel already playing that clip.
                    applyClipSettings (
                        *loaded,
                        cuedClipSettings (cueButton.source,
                                          clipSettingsFrom (*loaded),
                                          cueButton.seed));
                    syncClipUIParamsFromPattern (channel, slot);
                    auto const status = loaded->getStatus ();
                    if (status != Pattern::Status::Playing
                        && status != Pattern::Status::ScheduledForPlaying)
                      {
                        loaded->setPlaybackLength (
                            getPlaybackLength (channel, slot));
                        _engine.playPattern (
                            loaded, isButtonPressed (Button::Shift)
                                        ? _now
                                        : TempoClock::nextDownBeat (_now));
                      }
                  }
                refreshBrowser ();
                updateClipSettingsDisplay ();
                scheduleSetSave ();
                return;
              }
            }
        }

        auto const fired = firedActionOf (channel, button);
        if (!fired)
          break;

        if (channel < _actionSlot.size ())
          _actionSlot[channel] = button;
        if (channel < _actionChains.size ())
          _actionChains[channel].pressed (button,
                                          _engine.accentEndCount (channel));

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
        _engine.setChannelAction (channel, fired);
        _engine.setChannelAccentHeld (channel, true, pattern);

        if (!pattern || pattern->getStatus () != Pattern::Status::Idle)
          break;

        if (isButtonPressed (Button::Shift))
          {
            pattern->setPlaybackLength (getPlaybackLength (channel, slot));
            _engine.setPreviewMode (channel, true);
            _previewHeldPad[channel] = button;
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
        // Remembered here rather than worked out on release, and read from
        // the button -- how it is played is the button's (2026-09-27).
        if (fired->actMode == ActMode::Hold)
          _actHeldSlot[channel] = button;
        break;
      }
    }
}

void
A3MotionUIComponent::stopChannel (index_t channel)
{
  index_t const slot = 0;

  // Stop is a way out, of a chain of actions too.
  if (channel < _actionChains.size ())
    _actionChains[channel].broken ();

  // Stop on the channel a take is going into ends the take, the same way REC
  // does. Stopped alone, the engine finished it with nobody to mark it
  // unsaved, and the next REC put the old clip back over it.
  if (_recordingSlot.has_value ()
      && *_recordingSlot == std::make_pair (channel, slot))
    {
      endRecording ();
      return;
    }

  auto const &pattern = _patterns[channel][slot];
  if (!pattern)
    return;

  // Now, not on a beat. Stop is the way out of a thing that is going wrong,
  // and a way out that waits for the music is not one.
  auto const status = pattern->getStatus ();
  if (status == Pattern::Status::Playing
      || status == Pattern::Status::Recording
      || status == Pattern::Status::ScheduledForPlaying)
    _engine.stopPattern (pattern, _now);
}

void
A3MotionUIComponent::handleScenePress (index_t, std::size_t pad)
{
  if (pad >= numSceneRows)
    return;

  auto const function = padFunctionByPadIndex[pad];
  for (index_t channel = 0; channel < _patterns.size (); ++channel)
    {
      auto const &pattern = _patterns[channel][0];
      if (!pattern)
        continue;
      // Play starts only what stands still; see sceneStartsClip(). An action
      // fires on every channel that has it, running or not, as its pad does.
      if (function == PadFunction::PlayPause
          && !sceneStartsClip (pattern->getStatus ()))
        continue;
      handlePadPress (channel, static_cast<index_t> (pad));
    }
}

void
A3MotionUIComponent::handleSceneRelease (index_t, std::size_t pad)
{
  if (pad >= numSceneRows)
    return;

  for (index_t channel = 0; channel < _patterns.size (); ++channel)
    handlePadRelease (channel, static_cast<index_t> (pad));
}

void
A3MotionUIComponent::showBarPage (BarPage page)
{
  _pendingTakes.disarm ();
  refreshTakeState ();
  _barPage = page;
  _clipSettings->setPage (page);
  showEncoderMarks ();
  if (_action)
    {
      _action->setVisible (page == BarPage::Action);
      if (page == BarPage::Action)
        updateActionPage ();
    }
  if (_mixerStrip)
    {
      _mixerStrip->setVisible (page == BarPage::Mixer);
      if (page == BarPage::Mixer)
        // The strip is the shown clip's channel, so it is set here as well as
        // in selectClip(): arriving on the page has to show the channel you
        // are on, not the one that was on show when the page was last left.
        _mixerStrip->setChannel (static_cast<int> (_clipSettingsChannel));
    }
}

void
A3MotionUIComponent::fillSlotFromLibrary (index_t channel, index_t slot,
                                          int libIndex)
{
  if (channel >= _patterns.size () || slot >= _patterns[channel].size ())
    return;

  // Whatever this puts in the slot replaces an unsaved take in it. A settings
  // preset without a shape does not come through here (see applyClip()), so
  // it leaves the take standing and only changes its values.
  dropPendingTake (channel, slot);

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
  refreshPatternDisplay (_patterns[channel][slot]);
}

/** Whether the clip a slot came from is one of the instrument's.
 *
 *  Of the file, through the library entry that *is* that file -- not through
 *  the figure's name, which finds a figure. A clip entry's category is Clip
 *  whichever half it sits in, so isShipped is the field that carries this and
 *  isFactory() is not.
 */
bool
A3MotionUIComponent::slotClipIsShipped (index_t channel, index_t slot) const
{
  if (channel >= _slotClipFile.size () || slot >= _slotClipFile[channel].size ())
    return false;

  auto const index = _patternLibrary->indexForClipFile (_slotClipFile[channel][slot]);
  return index > 0 && _patternLibrary->getEntry (index).isShipped;
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
  if (_pendingTakes.isPending (channel, slot))
    {
      updateControlReadout ("-- SAVE THE TAKE FIRST");
      return;
    }

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

  // Whether this file may be written over, asked of the file.
  //
  // This looked the *figure's* name up in the library and copied when that
  // entry was one of the instrument's. Figures and clips share the library and
  // indexForName() takes the first match, so a clip of the performer's own
  // standing on a shipped figure -- Heart, Epicycloid, most of them -- counted
  // as shipped and was copied instead of written back. Measured on 2026-09-21:
  // 18 of the 33 clips in the user folder were such copies.
  //
  // isFactory() could not have answered it either way: it asks whether an
  // entry's category is System, and a clip's category is Clip. For a clip it
  // is false whatever folder the clip sits in, so the only way the old line
  // ever came out true was by finding a figure.
  if (shippedFileMayBeOverwritten (clipFile.existsAsFile (),
                            slotClipIsShipped (channel, slot),
                            shippedClips ()))
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
  if (_pendingTakes.isPending (_clipSettingsChannel, _clipSettingsSlot))
    {
      updateControlReadout ("-- SAVE THE TAKE FIRST");
      return {};
    }

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

  // A name the whole library is free of, not merely the clips folder.
  // freeClipName() looks at the folder alone, so a copy of a slot playing
  // "Helix" was itself called "Helix" -- a preset wearing a shape's name,
  // which indexForName() cannot tell apart and which made the row this was
  // about to open for renaming the *shape's* row.
  auto base = juce::String (pattern->getName ());
  for (int n = 2; _patternLibrary->indexForName (base.toStdString ()) > 0; ++n)
    base = juce::String (pattern->getName ()) + " " + juce::String (n);

  copy.name
      = freeNameIn (_patternLibrary->getClipDir (), base, ".json")
            .toStdString ();
  copy.settings = clipSettingsFrom (*pattern);
  copy.endClip = pattern->getEndClip ();
  copy.lanes = pattern->getLanes ();

  auto const target = newFileIn (_patternLibrary->getClipDir (),
                                 juce::String (copy.name), ".json");

  if (!ClipFile::save (copy, target))
    {
      updateControlReadout ("-- SAVE FAILED");
      return {};
    }

  setSlotClipFile (channel, slot, target);
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
  //
  // In user/, counted against both halves: written into the folder's top
  // level, a new set was missing from the list, and its name was checked
  // against nothing -- so the Save after it wrote over an older set of the
  // same name.
  auto const file = freeFileIn (sessionsDir (), "Set", ".json");
  auto name = file.getFileNameWithoutExtension ();
  file.getParentDirectory ().createDirectory ();

  set.name = name.toStdString ();

  if (saveSession (file, set))
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
  auto const file = namedFileIn (sessionsDir (), name, ".json");
  if (!file.existsAsFile ())
    {
      updateControlReadout ("-- NO SUCH SET");
      return;
    }

  // What is running now, written down before it is replaced. Not asked about
  // -- written. The previous arrangement is then never gone, even if nobody
  // thought to save it.
  writeSet ();

  // Every unsaved take goes with the arrangement it belonged to, and what
  // its slot held before comes back -- a slot the set leaves empty would
  // otherwise keep the take, unmarked. After writeSet(), which has to see
  // what those slots held before.
  for (index_t channel = 0; channel < _patterns.size (); ++channel)
    for (index_t slot = 0; slot < _patterns[channel].size (); ++slot)
      if (_pendingTakes.isPending (channel, slot))
        {
          if (auto const &take = _patterns[channel][slot])
            _engine.stopPattern (take, _now);
          auto const before = _pendingTakes.resolve (channel, slot);
          _patterns[channel][slot] = before.pattern;
          _slotClipFile[channel][slot] = before.clipFile;
        }
  refreshTakeState ();

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
  // The keys it brought are the device's now too, so a restart comes back
  // with them. Here rather than in applySet(), which also runs at start-up
  // before the settings have been read -- writing them out from there would
  // put every other setting back to its default.
  persistSettings ();
  // And the set itself becomes the current one on disk. Nothing else a Load
  // does schedules the write, so a restart straight after loading came back
  // with the set from before (2026-09-28).
  scheduleSetSave ();

  updateControlReadout ("-- LOADED " + name.toUpperCase ());
  refreshBrowser ();
  refreshAllPadRowLabels ();
  updateClipSettingsDisplay ();
}

void
A3MotionUIComponent::refreshBrowser (BrowserSelection selection)
{
  if (!_browser)
    return;

  // Entry 0 is "no pattern" in the library's own numbering, and a row saying
  // nothing is a row that empties the field it is dropped on -- which is worth
  // having, so it is listed rather than skipped.
  // One list asked once. It used to be four branches here and a row-to-entry
  // map built beside them, which is two things that had to agree and twice
  // did not.
  juce::StringArray names;

  for (auto const &row : currentList ().rows (_clipFilter))
    names.add (row.name);

  _browser->setEntries (names);
  _browser->setShowingList (_browserList);

  // Which channel the page is being used for. Everything else on the device
  // says it in colour; a browser that lit up in one house colour left the
  // performer to remember which deck they were loading.
  _browser->setChannelColour (
      _clipSettingsChannel < _channelUIStates.size ()
          ? _channelUIStates[_clipSettingsChannel]->colour
          : toColour (theme ().accent));

  // The list points at what the chosen field is already holding. Without this
  // you have to remember what is in a slot in order to see it highlighted --
  // and the highlight is the only thing saying which of seventy rows you are
  // looking at.
  // Held where it is, and only held inside the list that is left: the rows
  // have just been rebuilt, so a row number from before can point past the
  // end. The keys below are computed from it either way, which is the half
  // that was missing when this was set from outside instead.
  if (selection == BrowserSelection::Keep)
    {
      _browser->setSelectedEntry (selectionAfterRemoving (
          _browser->getSelectedEntry (), _browser->getNumEntries ()));
    }
  else if (_browserList == BrowserList::Actions)
    {
      auto const ch = _clipSettingsChannel;
      auto const sl = _clipSettingsSlot;
      auto const &action
          = _channelActions[ch][static_cast<size_t> (_chosenActionButton[ch])]
                .file;
      _browser->setSelectedEntry (
          action.existsAsFile ()
              ? names.indexOf (action.getFileNameWithoutExtension ())
              : 0);
    }
  else if (_browserList == BrowserList::Clips
           || _browserList == BrowserList::Shapes)
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
      // On the clips tab, the clip the slot's values came from; on the
      // shapes tab, the figure it is playing. Two lists, two questions, and
      // the highlight answers whichever one is being asked.
      auto const entry
          = _browserList == BrowserList::Clips
                ? _patternLibrary->indexForClipFile (_slotClipFile[ch][sl])
                : (held ? _patternLibrary->indexForName (held->getName ())
                        : 0);

      // Back through the map: with the list narrowed, the entry the slot
      // holds may not be on it at all, and a row number taken from the
      // library would then point at whatever happens to be there.
      _browser->setSelectedEntry (browserRowForLibrary (entry));
    }

  // The dot goes on after the row is known, because it goes on that row: the
  // clip the slot's values came from, when they have been turned since. Asked
  // for on 2026-09-19, so that FILES answers the same question the clip field
  // on the CLIP page already answers, in the same mark and the same colour.
  //
  // The slot's own clip's row, not the chosen one: since a tap only shows
  // (2026-09-27) the two can differ, and a dot on the row being read would
  // say that clip had drifted.
  _browser->setDriftedRow (driftedRowIn (
      _browserList,
      browserRowForLibrary (_patternLibrary->indexForClipFile (
          _slotClipFile[_clipSettingsChannel][_clipSettingsSlot])),
      slotHasDrifted (_clipSettingsChannel, _clipSettingsSlot)));

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

  // The word on the key is the state it is in, not the one the next press
  // would bring -- a key naming what you would get rather than what you have
  // is a key you press to find out where you are.
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
  auto const keys = currentLibraryKeys ();

  // The list keeps only its own keys: Save and Save as are the script
  // panel's beside it on every tab (2026-09-27); Load stays SETS'.
  _browser->setActions ({ "Load", filter, rename, "", "", remove },
                        { keys.load, keys.filter, keys.rename, false, false,
                          keys.remove });

  // Last, once the chosen row is settled: the script beside the list
  // follows it, or holds it while it has unsaved text.
  syncFilePanel ();
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
             == PatternLibrary::Category::Clip)
    {
      applyClip (channel, slot, index);
      return;
    }

  putFigureInSlot (channel, slot, index, true, true);
}

void
A3MotionUIComponent::putFigureInSlot (index_t channel, index_t slot, int index,
                                      bool select, bool play)
{
  auto &pattern = _patterns[channel][slot];

  // A figure, from the shapes tab. What the slot is played with stays where
  // the hand put it -- the same rule the picture on the CLIP page follows,
  // because it is the same gesture reached from the other side. Choosing a
  // whole clip is what replaces the values, and that has its own tab.
  auto const held = pattern ? clipSettingsFrom (*pattern) : ClipSettings{};
  auto const heldLanes = pattern ? pattern->getLanes () : KnobLanes{};
  auto const hadOne = pattern != nullptr;

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
  // chose is the clip you are looking at. Not when a saved figure is put back
  // into the slots holding it -- that is not a choice of clip.
  if (select)
    selectClip (channel, slot);

  auto const wasFrom = _slotClipFile[channel][slot];
  fillSlotFromLibrary (channel, slot, index);

  if (hadOne)
    if (auto const &filled = _patterns[channel][slot])
      {
        applyClipSettings (*filled, held);
        applyLanes (*filled, heldLanes);
        // And the clip those values came from is still where they came from:
        // the figure changed, not what it is played with.
        setSlotClipFile (channel, slot, wasFrom);
        syncClipUIParamsFromPattern (channel, slot);
      }

  // Registered by fillSlotFromLibrary() now, through refreshPatternDisplay(),
  // which knows about shapes made of dots.
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
  if (auto const &playing = _patterns[channel][slot]; playing && play)
    {
      playing->setPlaybackLength (getPlaybackLength (channel, slot));
      _engine.playPattern (
          playing, TempoClock::nextBeat (_now, _engine.getBeatsPerBar ()));
    }

  refreshBrowser ();
  refreshAllPadRowLabels ();
  if (channel == _clipSettingsChannel && slot == _clipSettingsSlot)
    updateClipSettingsDisplay ();

  // What was dropped is what the slot holds now, and the set has to say so
  // -- it did not, until something else happened to save it.
  scheduleSetSave ();
}

bool
A3MotionUIComponent::loadClipIntoChannel (index_t channel,
                                          juce::File const &clipFile,
                                          bool stopTheOldOneNow)
{
  // One clip per channel: the slot is always the first.
  index_t const slot = 0;

  auto const clip = ClipFile::load (clipFile);
  if (!clip.has_value ())
    return false;

  // The figure it names, if it names one. A clip is the whole playable thing
  // -- a figure and every value it is played with -- so choosing one fills
  // the slot with both. A clip written before that says no figure, and then
  // the slot keeps the one it has and only the values land, which is what
  // every clip used to do.
  auto const shape = clip->svg.empty ()
                         ? 0
                         : _patternLibrary->indexForName (clip->svg);

  if (shape > 0 && (!_patterns[channel][slot]
                    || _patterns[channel][slot]->getName () != clip->svg))
    {
      // Whatever was there stops first, the same way choosing a shape on the
      // CLIP page does: a pattern the slot no longer holds must not be left
      // running in the engine.
      if (auto const &was = _patterns[channel][slot])
        {
          auto const status = was->getStatus ();
          // Not for a Cue: the engine hands over on the new clip's start,
          // and stopping now would leave the room still until the downbeat.
          if (stopTheOldOneNow
              && (status == Pattern::Status::Playing
                  || status == Pattern::Status::Recording))
            _engine.stopPattern (was, _now);
          _motionComponent->unsetPreviewPattern (was);
          _motionComponent->removePatternDisplayData (was);
        }

      fillSlotFromLibrary (channel, slot, shape);
    }

  auto const &pattern = _patterns[channel][slot];

  // The values belong to a movement, and an empty slot has none. A clip with
  // no figure dropped on an empty slot leaves it plainly empty rather than
  // holding settings nothing can play.
  if (!pattern)
    return false;

  // Every value out of the clip. Sections could be held against this until
  // 2026-09-27; the locks are gone.
  applyClipValues (*pattern, *clip);

  // Set after filling: fillSlotFromLibrary() points the slot at the shape's
  // own clip, and a shape has none any more -- the clip names the shape, not
  // the other way round.
  setSlotClipFile (channel, slot, clipFile);

  // The bar follows what was just changed, the same as choosing a shape does.
  selectClip (channel, slot);

  // Read back OUT of the pattern, not pushed into it: applyMotionMode() would
  // write the strip's own direction and end action over the ones the preset
  // just brought.
  syncClipUIParamsFromPattern (channel, slot);
  return true;
}

void
A3MotionUIComponent::applyClip (index_t channel, index_t slot, int index)
{
  if (!loadClipIntoChannel (channel,
                            _patternLibrary->getEntry (index).clipFile))
    return;

  auto const &pattern = _patterns[channel][slot];

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
A3MotionUIComponent::putPatternInChannel (index_t channel,
                                          std::shared_ptr<Pattern> pattern,
                                          juce::File const &clipFile)
{
  if (channel >= _patterns.size () || !pattern)
    return;

  // One clip per channel since 2026-09-27: the channel's clip is its slot 0.
  auto const slot = index_t{ 0 };

  if (auto const &was = _patterns[channel][slot]; was && was != pattern)
    {
      _motionComponent->unsetPreviewPattern (was);
      _motionComponent->removePatternDisplayData (was);
    }

  // What comes in replaces an unsaved take, as filling from the library does.
  dropPendingTake (channel, slot);

  pattern->setChannel (channel);
  _patterns[channel][slot] = std::move (pattern);
  _slotClipFile[channel][slot] = clipFile;

  syncClipUIParamsFromPattern (channel, slot);
  refreshPatternDisplay (_patterns[channel][slot]);

  refreshBrowser ();
  refreshAllPadRowLabels ();
  if (channel == _clipSettingsChannel && slot == _clipSettingsSlot)
    updateClipSettingsDisplay ();

  scheduleSetSave ();
}

std::string
A3MotionUIComponent::followWantedFor (index_t channel) const
{
  if (channel >= _patterns.size ())
    return {};

  auto const &pattern = _patterns[channel][0];
  if (!pattern || pattern->getEndAction () != EndAction::Clip)
    return {};

  // A take waiting for SAVE or DISCARD is not handed over to anything: the
  // follow would take its place, and the take with it.
  if (_pendingTakes.isPending (channel, 0))
    return {};

  return pattern->getEndClip ();
}

void
A3MotionUIComponent::armFollowClips ()
{
  for (index_t channel = 0;
       channel < _patterns.size () && channel < _armedFollows.size ();
       ++channel)
    {
      auto const &pattern = _patterns[channel][0];
      auto const wanted = followWantedFor (channel);
      auto &armed = _armedFollows[channel];
      if (armed.from == pattern && armed.name == wanted)
        continue;

      armed.from = pattern;
      armed.name = wanted;
      armed.file = wanted.empty ()
                       ? juce::File{}
                       : _patternLibrary->clipFileNamed (juce::String (wanted));
      armed.follow = wanted.empty () ? nullptr
                                     : _patternLibrary->loadClip (armed.file);
      if (armed.follow)
        {
          armed.follow->setChannel (channel);
          armed.follow->setPlaybackLength (playbackLengthOf (*armed.follow));
        }

      // An empty follow takes back what was armed: the end is then a stop.
      _engine.armFollowPattern (channel, pattern, armed.follow);

      if (channel == _clipSettingsChannel)
        updateClipSettingsDisplay ();
    }
}

void
A3MotionUIComponent::takeOverFollow (index_t channel,
                                     std::shared_ptr<Pattern> const &pattern)
{
  if (channel >= _armedFollows.size () || !pattern)
    return;

  auto &armed = _armedFollows[channel];
  if (armed.follow != pattern || _patterns[channel][0] == pattern)
    return;

  auto const file = armed.file;
  // Disarmed here, so the next frame arms the follow's own follow: a chain
  // is each clip handing over to the next, one at a time.
  armed = {};

  // Not selected: a clip a chain started did not come from a finger, and the
  // bar stays on whatever the hand last chose -- see "The bar follows the
  // hand" in ARCHITECTURE.md.
  putPatternInChannel (channel, pattern, file);
}

juce::String
A3MotionUIComponent::followShownFor (index_t channel) const
{
  if (channel >= _armedFollows.size ())
    return {};

  auto const &armed = _armedFollows[channel];
  if (!armed.follow || armed.from != _patterns[channel][0])
    return {};
  return juce::String (armed.name);
}

void
A3MotionUIComponent::stepFollowClip (index_t channel, int increment)
{
  auto const &pattern = _patterns[channel][0];
  if (!pattern)
    return;

  // From the follow if it is in the library, from the channel's own clip if
  // there is none yet: the first step lands beside what is playing.
  auto from = 0;
  if (!pattern->getEndClip ().empty ())
    from = _patternLibrary->indexForClipFile (
        _patternLibrary->clipFileNamed (juce::String (pattern->getEndClip ())));
  if (from <= 0)
    from = _patternLibrary->indexForClipFile (_slotClipFile[channel][0]);

  auto const next = stepThroughLibrary (from, increment, true);
  if (next <= 0)
    return;

  auto const name
      = _patternLibrary->getEntry (next).clipFile.getFileNameWithoutExtension ();
  pattern->setEndClip (name.toStdString ());
  updateControlReadout ("-- THEN " + name.toUpperCase ());

  armFollowClips ();
  updateClipSettingsDisplay ();
  scheduleSetSave ();
}

void
A3MotionUIComponent::syncClipUIParamsFromPattern (index_t channel,
                                                  index_t slot)
{
  auto const &pattern = _patterns[channel][slot];
  if (!pattern)
    return;

  // Same order as applyMotionMode(): the bar's order is the engine's order.
  auto &params = _clipUIParams[channel][slot];
  params.direction = static_cast<int> (pattern->getPlayDirection ());
  params.endAction = static_cast<int> (pattern->getEndAction ());
}

void
A3MotionUIComponent::setButtonAction (index_t channel, int button,
                                      juce::File const &file)
{
  if (channel >= _channelActions.size () || button < 0
      || button >= numActionButtons)
    return;
  // Empty clears the button; anything else has to be an action -- the
  // manual beside the scripts is not one (#57).
  if (file != juce::File{} && !isActionScript (file))
    return;

  auto &action = _channelActions[channel][static_cast<size_t> (button)];
  action.file = file;
  action.settings.reset ();
  action.source = {};
  action.errors = {};
  action.feel = ActionFeel{};
  action.after.reset ();
  action.cueClip = juce::File{};
  action.isCue = false;

  if (!file.existsAsFile ())
    {
      updateActionPage ();
      return;
    }

  action.source = file.loadFileAsString ();

  // A seed that is new every time a script is chosen, so a script with dice
  // in it throws them again on being picked -- picking it is the gesture that
  // says "give me another one of these".
  action.seed = juce::Time::getHighResolutionTicks ();
  runButtonScript (channel, action);

  if (!action.errors.isEmpty ())
    updateControlReadout (action.errors[0]);

  updateActionPage ();
  updateClipSettingsDisplay ();
}

void
A3MotionUIComponent::runButtonScript (index_t channel, ActionButton &action)
{
  // Against the shown clip, so a line the script leaves commented out means
  // "as the clip is" -- for the feel too.
  auto const &pattern = _patterns[channel][0];
  auto const current = pattern ? clipSettingsFrom (*pattern) : ClipSettings{};
  auto const result = runActionScript (action.source, current, action.seed);

  action.settings = result.settings;
  action.errors = result.errors;
  // How it is played, and what follows it, are the script's (2026-09-29):
  // ACTION writes both there, so the button keeps no values of its own.
  action.feel = actionFeelFrom (result.settings);
  action.after = result.then;

  auto const cue = cueClipFor (result.clip, _patternLibrary->getClipDir ());
  action.cueClip = cue.file;
  action.isCue = result.clip.has_value ();
  if (cue.error.isNotEmpty ())
    action.errors.add (cue.error);
}

A3MotionUIComponent::ActionButton *
A3MotionUIComponent::shownActionButton ()
{
  auto const channel = _clipSettingsChannel;
  auto const button = _chosenActionButton[channel];
  if (channel >= _channelActions.size () || button >= numActionButtons)
    return nullptr;
  return &_channelActions[channel][static_cast<size_t> (button)];
}

std::optional<ClipSettings>
A3MotionUIComponent::firedActionOf (index_t channel, int button)
{
  if (channel >= _channelActions.size () || channel >= _accentBase.size ()
      || button < 0 || button >= numActionButtons)
    return std::nullopt;
  auto const &action = _channelActions[channel][static_cast<size_t> (button)];
  if (!action.settings)
    return std::nullopt;

  auto &base = _accentBase[channel];
  if (!base || !_engine.isChannelAccentActive (channel))
    {
      auto const &pattern = _patterns[channel][0];
      base = pattern ? clipSettingsFrom (*pattern) : ClipSettings{};
    }
  return resolveActionAt (action.source, *base, action.seed, action.feel);
}

juce::File
A3MotionUIComponent::chosenRowFile () const
{
  if (!_browser)
    return {};
  return currentList ().fileAt (_browser->getSelectedEntry ());
}

void
A3MotionUIComponent::showChosenFileText ()
{
  showFileText (chosenRowFile ());
}

void
A3MotionUIComponent::showFileText (juce::File const &file)
{
  flushScriptWrites ();
  auto &panel = _browser->scriptPanel ();
  _panelFile = file;
  panel.setLanguage (languageFor (file));
  panel.setFromLabel (fromKeyLabelFor (_browserList));
  dressFilePanel ();
  panel.stopEditing ();

  auto const exists = file.existsAsFile ();
  auto const text = exists ? file.loadFileAsString () : juce::String{};
  panel.setScript (text);
  panel.markSaved ();
  panel.setErrors (exists ? fileErrorsOf (text, file) : juce::StringArray{});
}

void
A3MotionUIComponent::dressFilePanel ()
{
  auto &panel = _browser->scriptPanel ();
  auto const &pattern = _patterns[_clipSettingsChannel][_clipSettingsSlot];

  // What the panel is told about the file and the shown clip, asked again
  // whenever either may have changed: a lock or a colour remembered from the
  // last clip is a lock or a colour on the wrong one.
  panel.setChannelColour (_channelUIStates[_clipSettingsChannel]->colour);
  panel.setHasFile (_panelFile.existsAsFile ());
  // One rule for every kind of file: nothing shipped is written over while
  // developer mode is off.
  panel.setProtected (!shippedFileMayBeOverwritten (
      _panelFile.existsAsFile (),
      isSystemFileIn (currentList ().folder (), _panelFile), shippedClips ()));
  // FROM needs something to take: a clip on the shown slot, or -- on SETS --
  // the arrangement, which is always there.
  panel.setSlotHolds (_browserList == BrowserList::Sessions
                      || pattern != nullptr);
}

void
A3MotionUIComponent::syncFilePanel ()
{
  if (!_browser)
    return;

  // The row can move under the panel -- another clip shown, a row deleted,
  // FILES opened again -- and the panel's text must never be saved into a
  // file it did not come from (final review, 2026-09-27).
  switch (panelSyncFor (chosenRowFile (), _panelFile,
                        _browser->scriptPanel ().hasUnsavedChanges ()))
    {
    case PanelSync::Keep:
      dressFilePanel ();
      break;
    case PanelSync::Reload:
      showFileText (chosenRowFile ());
      break;
    case PanelSync::HoldRow:
      {
        auto const name = _panelFile.getFileNameWithoutExtension ();
        for (int row = 0; row < _browser->getNumEntries (); ++row)
          if (_browser->entryName (row) == name)
            _browser->setSelectedEntry (row);
        dressFilePanel ();
        updateControlReadout ("-- SAVE OR CANCEL");
        _browser->scriptPanel ().flashKeys ();
        break;
      }
    }
}

bool
A3MotionUIComponent::fileTextHoldsTheList ()
{
  if (!_browser
      || !listWaitsFor (_browser->scriptPanel ().hasUnsavedChanges ()))
    return false;

  updateControlReadout ("-- SAVE OR CANCEL");
  _browser->scriptPanel ().flashKeys ();
  return true;
}

bool
A3MotionUIComponent::fileTextIsFitToWrite (juce::StringArray const &errors,
                                           juce::File const &file)
{
  _browser->scriptPanel ().setErrors (errors);
  if (!errorsBlockSaving (errors, file))
    return true;

  // A set or an SVG that does not parse would not load again: it is not
  // written, and the reason stands in the strip and the readout.
  updateControlReadout ("-- NOT SAVED: " + errors[0]);
  return false;
}

void
A3MotionUIComponent::saveFileText ()
{
  // A turn on ACTION may still be waiting to be written (2026-09-29): out
  // first, or it lands after this and undoes it -- over a Save, or on a
  // name that was renamed or deleted, bringing the old file back.
  flushScriptWrites ();
  // The file the text came from, never the row that happens to be chosen:
  // the two can differ, and writing one file's text into another is the one
  // thing this key must never do.
  auto const file = _panelFile;
  auto &panel = _browser->scriptPanel ();

  // The second lock: the key is dark on a shipped file, but a save that
  // depends on a key having been dark happens the first time something else
  // lights it.
  if (!file.existsAsFile ()
      || !shippedFileMayBeOverwritten (
          true, isSystemFileIn (currentList ().folder (), file),
          shippedClips ()))
    return;

  auto const text = panel.script ();
  if (!fileTextIsFitToWrite (fileErrorsOf (text, file), file))
    return;

  if (!writeTextFile (file, text))
    {
      updateControlReadout ("-- SAVE FAILED");
      return;
    }
  panel.markSaved ();

  // What uses it takes it up now: every clip firing an action, every slot
  // holding a clip or a figure (LibraryList::afterSaving).
  currentList ().afterSaving (file);

  updateControlReadout ("-- SAVED "
                        + file.getFileNameWithoutExtension ().toUpperCase ());
  refreshBrowser ();
}

void
A3MotionUIComponent::saveFileTextAs ()
{
  // A turn on ACTION may still be waiting to be written (2026-09-29): out
  // first, or it lands after this and undoes it -- over a Save, or on a
  // name that was renamed or deleted, bringing the old file back.
  flushScriptWrites ();
  auto &panel = _browser->scriptPanel ();
  auto const text = panel.script ();
  auto const &list = currentList ();

  // Named after the one it came from -- "Bloom 2" beside "Bloom" -- because
  // a copy is how one of the instrument's own gets corrected, and a copy
  // arriving as "Action 4" would have lost the only thing saying where it
  // came from. Counted against both halves, or a new file would take a
  // shipped name; written into the user half.
  auto const copy
      = freeFileIn (list.folder (), copyBaseFor (_panelFile, list.folder ()), list.extension ());
  if (!fileTextIsFitToWrite (fileErrorsOf (text, copy), copy))
    return;

  list.folder ().getChildFile ("user").createDirectory ();
  if (!writeTextFile (copy, text))
    {
      updateControlReadout ("-- SAVE FAILED");
      return;
    }
  panel.markSaved ();

  // The copy is what the panel holds from here on; the list learns of it,
  // and on ACTIONS the clip EDIT came from fires it (takeEditOrigin).
  _panelFile = copy;
  currentList ().afterCopying (copy);

  auto const name = copy.getFileNameWithoutExtension ();
  refreshBrowser ();
  for (int row = 0; row < _browser->getNumEntries (); ++row)
    if (_browser->entryName (row) == name)
      {
        _browser->setSelectedEntry (row);
        break;
      }
  refreshBrowser ();
  updateControlReadout ("-- SAVED " + name.toUpperCase ());
}

std::vector<std::vector<juce::File>>
A3MotionUIComponent::slotActionFiles () const
{
  std::vector<std::vector<juce::File>> files;
  for (auto const &channel : _channelActions)
    {
      files.emplace_back ();
      for (auto const &slot : channel)
        files.back ().push_back (slot.file);
    }
  return files;
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

  return namedFileIn (actionsDir (), name, ".scd");
}

int
A3MotionUIComponent::browserRowForLibrary (int entry) const
{
  // Asked of the list that built the mapping, in the same pass that built the
  // rows. It used to be a vector on the component, filled in one place and
  // read in three, which is how a row number could outlive the list it was a
  // row of.
  if (entry < 0)
    return -1;

  for (int row = 0; row < _browser->getNumEntries (); ++row)
    if (currentList ().libraryEntryAt (row) == entry)
      return static_cast<int> (row);

  // Not on the list as it is narrowed. Nothing is highlighted rather than
  // something else being: a highlight on the wrong row is worse than none.
  return -1;
}

int
A3MotionUIComponent::libraryForBrowserRow (int row) const
{
  return currentList ().libraryEntryAt (row);
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
                            == PatternLibrary::Category::Clip;
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

  return namedFileIn (sessionsDir (), name, ".json");
}

int
A3MotionUIComponent::chosenLibraryIndex () const
{
  // Both library tabs, not just one. Everything that acts on a chosen row --
  // rename, delete, save, the system-shape check -- comes through here, so a
  // tab left out of this one line is a tab where all of it silently does
  // nothing. That is what kept the SVG tab's keys inert after the six
  // switches had already been taught about it: they were right, and every
  // one of them ran into this.
  auto const isLibrary = _browserList == BrowserList::Clips
                         || _browserList == BrowserList::Shapes;
  if (!isLibrary || !_browser)
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

/** What the five library keys may do, right now.
 *
 *  One rule for all four tabs, and one place that gathers what it needs. It
 *  used to be six switches and three loose conditions, and a tab could be
 *  added to some of them and not others -- which is how the SVG tab spent as
 *  long as it has existed with five dark keys, and how the filter then spent
 *  a round lit and inert. Everything that wants to know asks here.
 */
LibraryKeyStates
A3MotionUIComponent::currentLibraryKeys () const
{
  auto const channel = _clipSettingsChannel;
  auto const slot = _clipSettingsSlot;
  // An unsaved take has nothing to write into the clips or the shapes yet:
  // SAVE in the transport row is where it is kept. The actions and the sets
  // read it without writing over anything it came from.
  auto const takeWaits
      = _pendingTakes.isPending (channel, slot)
        && (_browserList == BrowserList::Clips
            || _browserList == BrowserList::Shapes);
  auto const holds = channel < _patterns.size () && slot < _patterns[channel].size ()
                     && _patterns[channel][slot] != nullptr && !takeWaits;

  return libraryKeysFor (_browserList,
                         { chosenEntryHasAFile (), chosenEntryIsShipped (),
                           holds, canSaveInPlace () });
}

/** Whether the chosen row is one of the instrument's own.
 *
 *  Three lists can answer it and each knows in its own way: a shape carries
 *  the category the library scanned it with, and an action or a set is
 *  shipped if its file sits in the system half. Clips are neither -- they
 *  carry Category::Clip -- so they say no, which is also what stops Save
 *  being darkened on the one list where drift is the question instead.
 */
bool
A3MotionUIComponent::chosenEntryIsShipped () const
{
  return _browser != nullptr
         && currentList ().isShippedAt (_browser->getSelectedEntry ());
}

/** The shown slot's figure, written over the shape it is standing on.
 *
 *  Only ever a shape of the performer's: writing over one of the
 *  instrument's would change what every clip naming it plays, on a device
 *  where the shape is the thing that ships. libraryKeysFor() is what stops
 *  the key lighting; this is the second lock, because a save that depends on
 *  a key having been dark is a save that happens the first time something
 *  else lights it.
 */
void
A3MotionUIComponent::saveSlotShapeInPlace ()
{
  if (_pendingTakes.isPending (_clipSettingsChannel, _clipSettingsSlot))
    {
      updateControlReadout ("-- SAVE THE TAKE FIRST");
      return;
    }

  if (chosenEntryIsShipped ())
    {
      updateControlReadout ("-- SYSTEM SHAPE");
      return;
    }

  auto const index = chosenLibraryIndex ();
  if (index < 0)
    return;

  auto const &entry = _patternLibrary->getEntry (index);
  auto const &pattern = _patterns[_clipSettingsChannel][_clipSettingsSlot];
  if (!pattern || !entry.file.existsAsFile ())
    return;

  if (!PatternFile::save (pattern, entry.file))
    {
      updateControlReadout ("-- COULD NOT SAVE");
      return;
    }

  updateControlReadout ("-- SHAPE SAVED");
  _patternLibrary->refresh ();
}

/** The shown slot's figure, kept as a new shape of its own.
 *
 *  There was no way to do this before: a figure reached the library only by
 *  being recorded. Named the way a recording is, so a shape kept by hand and
 *  one played in sit together in the list. */
juce::String
A3MotionUIComponent::saveSlotShapeAsCopy ()
{
  if (_pendingTakes.isPending (_clipSettingsChannel, _clipSettingsSlot))
    {
      updateControlReadout ("-- SAVE THE TAKE FIRST");
      return {};
    }

  auto const &pattern = _patterns[_clipSettingsChannel][_clipSettingsSlot];
  if (!pattern)
    return {};

  auto const base = recordingBaseName (juce::Time::getCurrentTime ());
  auto const name
      = freeRecordingName (base, [this] (juce::String const &candidate) {
          return _patternLibrary->indexForName (candidate.toStdString ()) > 0;
        });

  // Built fresh from the ticks rather than copied: Pattern holds atomics for
  // the values the clock thread writes and so cannot be copied at all. That
  // is also the safer shape here -- the slot keeps its own identity, and what
  // goes into the library is the figure, which is all a shape is.
  auto const copy = std::make_shared<Pattern> ();
  auto const ticks = pattern->getNumTicks ();
  copy->resize (ticks);
  for (index_t tick = 0; tick < ticks; ++tick)
    copy->setTick (tick, pattern->getTick (tick));
  copy->markComplete ();
  copy->setName (name.toStdString ());
  // A copy of a take is a take: stored tick for tick, not normalised (#68).
  if (pattern->isTake ())
    copy->markAsTake ();

  _patternLibrary->saveUserPattern (copy);

  return name;
}

LibraryList &
A3MotionUIComponent::currentList () const
{
  return *_lists[static_cast<size_t> (_browserList)];
}

// ── The four lists, as four objects ─────────────────────────────────────
//
// Each knows only what makes it different: where its files are, what it calls
// them, and what "put this on a slot" means. Everything else is one piece of
// code now that does not know which tab it is on.
//
// They delegate -- renameChosenClip(), saveFileTextAs() and the rest stay
// where they were. What moved is the *branching*: eight places that each had
// to learn about a new list, and on 2026-09-08 each learned about one at a
// different time. See components/LibraryList.hh.

namespace
{
/** The rows a filter leaves, out of a folder split into shipped and made. */
std::vector<LibraryRow>
rowsOfSplitFolder (juce::File const &root, juce::String const &extension,
                   ClipFilter filter)
{
  std::vector<LibraryRow> rows;
  for (auto const &entry : listFilesIn (root, extension))
    {
      if ((filter == ClipFilter::System && !entry.isSystem)
          || (filter == ClipFilter::User && entry.isSystem))
        continue;

      rows.push_back ({ entry.name, entry.isSystem });
    }
  return rows;
}

bool
sitsInTheShippedHalf (juce::File const &file)
{
  return file.getParentDirectory ().getFileName () == "system";
}

/** What a writer puts in a file, as text: the writers stay the one place
 *  that knows each format, and FROM reads what they would write. */
juce::String
textWrittenBy (juce::String const &extension,
               std::function<bool (juce::File const &)> const &write)
{
  juce::TemporaryFile temp (extension);
  if (!write (temp.getFile ()))
    return {};
  return temp.getFile ().loadFileAsString ();
}
}

class A3MotionUIComponent::ActionsList : public LibraryList
{
public:
  explicit ActionsList (A3MotionUIComponent &owner) : _owner (owner) {}

  std::vector<LibraryRow>
  rows (ClipFilter filter) const override
  {
    // Row zero clears the slot: an action is a thing you want to be able to
    // call off. The two library lists lost theirs -- a figure is always
    // replaced by another figure -- and this one keeps it.
    std::vector<LibraryRow> rows{ { "Empty", false } };
    for (auto const &row :
         rowsOfSplitFolder (_owner.actionsDir (), ".scd", filter))
      rows.push_back (row);
    return rows;
  }

  bool
  hasFileAt (int) const override
  {
    return _owner.chosenActionFile ().existsAsFile ();
  }

  bool
  isShippedAt (int) const override
  {
    return sitsInTheShippedHalf (_owner.chosenActionFile ());
  }

  bool
  canSaveInPlace () const override
  {
    auto const ch = _owner._clipSettingsChannel;
    auto const sl = _owner._clipSettingsSlot;
    return ch < _owner._patterns.size () && sl < _owner._patterns[ch].size ()
           && _owner._patterns[ch][sl] != nullptr
           && _owner._channelActions[ch][static_cast<size_t> (
                  _owner._chosenActionButton[ch])]
                  .file.existsAsFile ();
  }

  // Load: the shown clip fires this action from now on -- the same as a tap
  // in ACTION's own list.
  void
  assign (int row) override
  {
    auto const file = fileAt (row);
    if (!file.existsAsFile ())
      return;
    _owner.setButtonAction (_owner._clipSettingsChannel,
                            _owner._chosenActionButton[_owner._clipSettingsChannel],
                            file);
    _owner.updateControlReadout (
        "-- ACT " + file.getFileNameWithoutExtension ().toUpperCase ());
    _owner.refreshBrowser ();
  }
  void
  rename (int, juce::String const &name) override
  {
    _owner.renameChosenAction (name);
  }
  void remove (int) override { _owner.deleteChosenAction (); }

  juce::File
  fileAt (int row) const override
  {
    // Row zero is "no action", with no file behind it.
    auto const name = _owner._browser->entryName (row);
    return row <= 0 || name.isEmpty ()
               ? juce::File{}
               : namedFileIn (_owner.actionsDir (), name, ".scd");
  }
  juce::File folder () const override { return _owner.actionsDir (); }
  juce::String extension () const override { return ".scd"; }

  juce::String
  currentStateText () const override
  {
    auto const &pattern
        = _owner._patterns[_owner._clipSettingsChannel][_owner._clipSettingsSlot];
    return pattern ? actionScriptFor (clipSettingsFrom (*pattern))
                   : juce::String{};
  }

  void
  afterSaving (juce::File const &file) override
  {
    // Heard at once on every clip that fires it, not only the shown one.
    for (auto const &at : slotsFiring (file, _owner.slotActionFiles ()))
      _owner.setButtonAction (at.channel, static_cast<int> (at.slot), file);
  }

  void
  afterCopying (juce::File const &copy) override
  {
    // The clip EDIT came from fires the copy -- once (takeEditOrigin).
    if (auto const at = takeEditOrigin (_owner._editOrigin))
      _owner.setButtonAction (at->channel, static_cast<int> (at->slot), copy);
  }

  // The script beside the list carries Save and Save as on this tab; the
  // strip's keys are dark here, but the interface asks for both.
  void saveInPlace () override { _owner.saveFileText (); }
  juce::String
  saveAsCopy () override
  {
    _owner.saveFileTextAs ();
    return {};
  }

private:
  A3MotionUIComponent &_owner;
};

class A3MotionUIComponent::SetsList : public LibraryList
{
public:
  explicit SetsList (A3MotionUIComponent &owner) : _owner (owner) {}

  std::vector<LibraryRow>
  rows (ClipFilter filter) const override
  {
    return rowsOfSplitFolder (_owner.sessionsDir (), ".json", filter);
  }

  bool
  hasFileAt (int) const override
  {
    return _owner.chosenSetFile ().existsAsFile ();
  }

  bool
  isShippedAt (int) const override
  {
    return sitsInTheShippedHalf (_owner.chosenSetFile ());
  }

  bool
  canSaveInPlace () const override
  {
    return _owner._sessionName.isNotEmpty ()
           && namedFileIn (_owner.sessionsDir (), _owner._sessionName, ".json")
                  .existsAsFile ();
  }

  void
  assign (int row) override
  {
    _owner.loadSessionNamed (_owner._browser->entryName (row));
  }
  void
  rename (int, juce::String const &name) override
  {
    _owner.renameChosenSet (name);
  }
  void remove (int) override { _owner.deleteChosenSet (); }

  juce::File
  fileAt (int row) const override
  {
    auto const name = _owner._browser->entryName (row);
    return name.isEmpty () ? juce::File{}
                           : namedFileIn (_owner.sessionsDir (), name, ".json");
  }
  juce::File folder () const override { return _owner.sessionsDir (); }
  juce::String extension () const override { return ".json"; }

  juce::String
  currentStateText () const override
  {
    auto set = _owner.buildSession ();
    set.name = _owner._sessionName.toStdString ();
    return textWrittenBy (".json", [&set] (juce::File const &file) {
      return saveSession (file, set);
    });
  }

  // A set is written, not loaded: loading replaces all eight slots and is
  // the Load key's, on purpose.
  void afterSaving (juce::File const &) override {}
  void afterCopying (juce::File const &) override {}

  void saveInPlace () override { _owner.saveSessionInPlace (); }
  juce::String saveAsCopy () override { return _owner.saveCurrentSession (); }

private:
  A3MotionUIComponent &_owner;
};

/** The two the PatternLibrary feeds: the figures, and the clips naming them.
 *
 *  They differ in one line -- which kind of entry they keep -- so they share
 *  a base and say only that.
 *
 *  The row-to-entry map lives here rather than on the component, built by the
 *  same pass that builds the rows, so the two cannot fall out of step. A row
 *  number read against the whole library while the list was narrowed used to
 *  point at whatever happened to be there.
 */
class A3MotionUIComponent::LibraryBackedList : public LibraryList
{
public:
  explicit LibraryBackedList (A3MotionUIComponent &owner) : _owner (owner) {}

  std::vector<LibraryRow>
  rows (ClipFilter filter) const override
  {
    _rowToEntry.clear ();
    std::vector<LibraryRow> rows;

    for (int i = 0; i < _owner._patternLibrary->getNumEntries (); ++i)
      {
        // Entry zero is the library's "Empty", and neither library list
        // offers it: a figure is always replaced by another figure, so a row
        // offering none is one nobody reaches for.
        if (i == 0)
          continue;

        auto const &entry = _owner._patternLibrary->getEntry (i);
        auto const isClip = entry.category == PatternLibrary::Category::Clip;
        if (isClip != wantsClips ())
          continue;

        auto const shipped = entry.isShipped;
        if ((filter == ClipFilter::System && !shipped)
            || (filter == ClipFilter::User && shipped))
          continue;

        rows.push_back ({ juce::String (entry.name), shipped });
        _rowToEntry.push_back (i);
      }

    return rows;
  }

  bool
  hasFileAt (int row) const override
  {
    auto const index = libraryEntryAt (row);
    if (index < 0)
      return false;

    // A shape is its SVG; a settings preset is its clip and has no shape.
    // Either is something to name; an entry with neither is a row the library
    // made up, and there is nothing to do to it.
    auto const &entry = _owner._patternLibrary->getEntry (index);
    return entry.file.existsAsFile () || entry.clipFile.existsAsFile ();
  }

  bool
  isShippedAt (int row) const override
  {
    auto const index = libraryEntryAt (row);
    return index >= 0 && _owner._patternLibrary->getEntry (index).isShipped;
  }

  void assign (int row) override { _owner.assignBrowserEntry (libraryEntryAt (row)); }
  void
  rename (int, juce::String const &name) override
  {
    _owner.renameChosenClip (name);
  }
  void remove (int) override { _owner.deleteChosenClip (); }

  juce::String
  costOfRemoving (int row) const override
  {
    return _owner.costOfRemovingLibraryEntry (libraryEntryAt (row));
  }

  int
  libraryEntryAt (int row) const override
  {
    if (row < 0 || row >= static_cast<int> (_rowToEntry.size ()))
      return -1;
    return _rowToEntry[static_cast<size_t> (row)];
  }

  juce::File
  fileAt (int row) const override
  {
    auto const index = libraryEntryAt (row);
    if (index < 0)
      return {};
    auto const &entry = _owner._patternLibrary->getEntry (index);
    return wantsClips () ? entry.clipFile : entry.file;
  }

  void
  afterCopying (juce::File const &) override
  {
    // The library finds its files by scanning; a new one is there once it
    // has looked again.
    _owner._patternLibrary->refresh ();
  }

protected:
  virtual bool wantsClips () const = 0;
  A3MotionUIComponent &_owner;

private:
  mutable std::vector<int> _rowToEntry;
};

class A3MotionUIComponent::ClipsList : public LibraryBackedList
{
public:
  using LibraryBackedList::LibraryBackedList;

  bool
  canSaveInPlace () const override
  {
    // SAVE in the transport row is the one way to keep an unsaved take.
    if (_owner._pendingTakes.isPending (_owner._clipSettingsChannel,
                                        _owner._clipSettingsSlot))
      return false;

    // Only when there is something to write, and only where it may be
    // written. Save on an untouched clip made a copy of it anyway once --
    // press it twice out of habit and the library grows a clip you cannot
    // tell from the original.
    //
    // The second half asks exactly what saveSlotClip() asks, of the same
    // file. It used to ask a different question -- the chosen row's entry --
    // and a key whose condition is not the one the press acts on is a key
    // that is dark while it works, or lit while it does something else.
    auto const ch = _owner._clipSettingsChannel;
    auto const sl = _owner._clipSettingsSlot;

    return _owner.slotHasDrifted (ch, sl)
           && shippedFileMayBeOverwritten (_owner._slotClipFile[ch][sl].existsAsFile (),
                                    _owner.slotClipIsShipped (ch, sl),
                                    _owner.shippedClips ());
  }

  void
  saveInPlace () override
  {
    _owner.saveSlotClip (_owner._clipSettingsChannel,
                         _owner._clipSettingsSlot);
  }

  juce::String saveAsCopy () override { return _owner.saveSlotClipAsCopy (); }

  juce::File
  folder () const override
  {
    return _owner._patternLibrary->getClipDir ();
  }
  juce::String extension () const override { return ".json"; }

  juce::String
  currentStateText () const override
  {
    auto const ch = _owner._clipSettingsChannel;
    auto const sl = _owner._clipSettingsSlot;
    auto const &pattern = _owner._patterns[ch][sl];
    if (!pattern)
      return {};

    // The clip the slot came from, with the slot's values written into it --
    // what Save on this tab used to write straight to the file.
    auto const from = _owner._slotClipFile[ch][sl];
    return textWrittenBy (".json", [&] (juce::File const &file) {
      auto const base = from.existsAsFile () ? from.copyFileTo (file)
                                             : ClipFile::save (Clip{}, file);
      return base && saveClipSettings (*pattern, file);
    });
  }

  void
  afterSaving (juce::File const &file) override
  {
    // Every slot playing this clip takes it up now (maintainer, 2026-09-27).
    _owner._patternLibrary->refresh ();
    auto const index = _owner._patternLibrary->indexForClipFile (file);
    if (index <= 0)
      return;
    for (index_t ch = 0; ch < _owner._slotClipFile.size (); ++ch)
      for (index_t sl = 0; sl < _owner._slotClipFile[ch].size (); ++sl)
        if (_owner._slotClipFile[ch][sl] == file)
          _owner.applyClip (ch, sl, index);
  }

protected:
  bool wantsClips () const override { return true; }
};

class A3MotionUIComponent::ShapesList : public LibraryBackedList
{
public:
  using LibraryBackedList::LibraryBackedList;

  // Writing over one of the instrument's own would change what every clip
  // naming it plays. libraryKeysFor() darkens the key; saveSlotShapeInPlace()
  // refuses again on its own, because a save that depends on a key having
  // been dark is a save that happens the first time something else lights it.
  bool
  canSaveInPlace () const override
  {
    // SAVE in the transport row is the one way to keep an unsaved take.
    if (_owner._pendingTakes.isPending (_owner._clipSettingsChannel,
                                        _owner._clipSettingsSlot))
      return false;
    auto const ch = _owner._clipSettingsChannel;
    auto const sl = _owner._clipSettingsSlot;
    return ch < _owner._patterns.size () && sl < _owner._patterns[ch].size ()
           && _owner._patterns[ch][sl] != nullptr;
  }

  void saveInPlace () override { _owner.saveSlotShapeInPlace (); }
  juce::String saveAsCopy () override { return _owner.saveSlotShapeAsCopy (); }

  juce::File
  folder () const override
  {
    return _owner._patternLibrary->getRootDir ();
  }
  juce::String extension () const override { return ".svg"; }

  juce::String
  currentStateText () const override
  {
    auto const &pattern
        = _owner._patterns[_owner._clipSettingsChannel][_owner._clipSettingsSlot];
    if (!pattern)
      return {};
    return textWrittenBy (".svg", [&pattern] (juce::File const &file) {
      return PatternFile::save (pattern, file);
    });
  }

  void
  afterSaving (juce::File const &file) override
  {
    // Every slot holding this figure takes it up now, keeping its values;
    // one that was playing plays on from the next beat (2026-09-27).
    _owner._patternLibrary->refresh ();
    auto index = -1;
    for (int i = 1; i < _owner._patternLibrary->getNumEntries (); ++i)
      if (_owner._patternLibrary->getEntry (i).file == file)
        index = i;
    if (index < 0)
      return;

    auto const name = _owner._patternLibrary->getEntry (index).name;
    for (index_t ch = 0; ch < _owner._patterns.size (); ++ch)
      for (index_t sl = 0; sl < _owner._patterns[ch].size (); ++sl)
        if (auto const &held = _owner._patterns[ch][sl];
            held && held->getName () == name)
          {
            auto const status = held->getStatus ();
            auto const wasPlaying = status == Pattern::Status::Playing
                                    || status == Pattern::Status::ScheduledForPlaying;
            _owner.putFigureInSlot (ch, sl, index, false, wasPlaying);
          }
  }

protected:
  bool wantsClips () const override { return false; }
};

/** The four, in the order BrowserList names them so the tab indexes straight
 *  into the array.
 *
 *  Defined below the classes rather than in the constructor because a
 *  unique_ptr to a derived type needs that type to be complete before it can
 *  become a unique_ptr to the base. */
void
A3MotionUIComponent::createBrowserLists ()
{
  _lists[static_cast<size_t> (BrowserList::Clips)]
      = std::make_unique<ClipsList> (*this);
  _lists[static_cast<size_t> (BrowserList::Shapes)]
      = std::make_unique<ShapesList> (*this);
  _lists[static_cast<size_t> (BrowserList::Actions)]
      = std::make_unique<ActionsList> (*this);
  _lists[static_cast<size_t> (BrowserList::Sessions)]
      = std::make_unique<SetsList> (*this);
}

bool
A3MotionUIComponent::chosenEntryHasAFile () const
{
  return _browser != nullptr
         && currentList ().hasFileAt (_browser->getSelectedEntry ());
}

int
A3MotionUIComponent::setsNaming (juce::String const &patternName) const
{
  if (patternName.isEmpty ())
    return 0;

  auto count = 0;
  // Both halves: a shipped set names takes too, and a rename that skipped
  // them would leave them pointing at a name that is gone.
  for (auto const &listed : listFilesIn (sessionsDir (), ".json"))
    {
      auto const &file = listed.file;
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
  // Both halves: a shipped set names takes too, and a rename that skipped
  // them would leave them pointing at a name that is gone.
  for (auto const &listed : listFilesIn (sessionsDir (), ".json"))
    {
      auto const &file = listed.file;
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

/** How many sets name the library entry at `index`, said as words for the
 *  arming press to show before the second one lands. It asked the tab which
 *  list it was on and then asked the browser which row; the list object knows
 *  both, so it hands the entry straight in. */
juce::String
A3MotionUIComponent::costOfRemovingLibraryEntry (int index) const
{
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
  // A turn on ACTION may still be waiting to be written (2026-09-29): out
  // first, or it lands after this and undoes it -- over a Save, or on a
  // name that was renamed or deleted, bringing the old file back.
  flushScriptWrites ();
  if (_browser)
    currentList ().rename (_browser->getSelectedEntry (), name);
}

void
A3MotionUIComponent::deleteChosenEntry ()
{
  // A turn on ACTION may still be waiting to be written (2026-09-29): out
  // first, or it lands after this and undoes it -- over a Save, or on a
  // name that was renamed or deleted, bringing the old file back.
  flushScriptWrites ();
  // Asked twice, whatever the folder. A file thrown away in front of a room
  // does not come back, and the second press is the only thing standing
  // between a fat finger and somebody's work. Not a dialogue: there is nothing
  // here that could put one up without covering the list it is asking about.
  if (!chosenEntryHasAFile ())
    return;

  if (!_deleteArmed)
    {
      _deleteArmed = true;
      updateControlReadout (
          "-- DELETE?"
          + currentList ().costOfRemoving (_browser->getSelectedEntry ()));

      // Holding the row. This refresh exists only to redraw the key as
      // "Sure?" -- and a plain one re-points the list at the shown slot's
      // clip, which after the previous delete is a clip that is gone. The
      // selection went with it and the second press found nothing to delete:
      // the last piece of "deleting works twice and then stops".
      refreshBrowser ();
      return;
    }

  _deleteArmed = false;

  // Where the hand was, kept across the removal. Each list's remove() refreshes
  // the browser on its way out, and refreshBrowser() then decides the selection
  // for itself -- for a clip, the one the shown slot holds, which is exactly
  // the file that has just gone. The selection landed on row 0, the library's
  // "Empty", with no file and every key dark.
  //
  // In a library of seventy-odd rows that loses your place, and the row you
  // reach for next is one of the thirty-nine shipped ones, where Delete is
  // correctly dark. It reads as the key having stopped working -- which is how
  // it was reported, and it is why a restart "fixed" it: a fresh browser puts
  // the selection back beside your own files.
  //
  // Afterwards, not before: the old line set it before the refresh, which then
  // overwrote it. See selectionAfterRemoving().
  auto const row = _browser->getSelectedEntry ();
  currentList ().remove (row);

  // Once more, holding the row: remove() refreshed on its way out and pointed
  // the list back at the slot's clip -- which, when you have just deleted what
  // the slot was holding, is nothing at all. Setting the row alone was not
  // enough, because the keys are decided inside the refresh: the highlight
  // moved and Delete stayed dark. Measured at the device on 2026-09-13.
  _browser->setSelectedEntry (
      selectionAfterRemoving (row, _browser->getNumEntries ()));
  refreshBrowser ();
}

void
A3MotionUIComponent::renameChosenAction (juce::String const &name)
{
  auto const from = chosenActionFile ();
  if (!from.existsAsFile ())
    return;

  // Renaming only ever reaches one of the performer's -- the key is dark
  // on a shipped one -- so the new name lands beside the old.
  auto const to = newFileIn (actionsDir (), name, ".scd");
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
  for (index_t channel = 0; channel < _channelActions.size (); ++channel)
    for (int button = 0; button < numActionButtons; ++button)
      if (_channelActions[channel][static_cast<size_t> (button)].file == from)
        setButtonAction (channel, button, to);

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
  for (index_t channel = 0; channel < _channelActions.size (); ++channel)
    for (int button = 0; button < numActionButtons; ++button)
      if (_channelActions[channel][static_cast<size_t> (button)].file == file)
        setButtonAction (channel, button, juce::File{});

  updateControlReadout (
      "-- DELETED " + file.getFileNameWithoutExtension ().toUpperCase ());

  // The selection is not set here: deleteChosenEntry() puts it back on the row
  // that took this one's place, after everything has refreshed.
  refreshBrowser ();
}

void
A3MotionUIComponent::renameChosenSet (juce::String const &name)
{
  auto const from = chosenSetFile ();
  if (!from.existsAsFile ())
    return;

  auto const to = newFileIn (sessionsDir (), name, ".json");
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
              = newFileIn (_patternLibrary->getClipDir (), name, ".json");
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
                    setSlotClipFile (channel, slot, to);
              // And so does what an unsaved take's slot held before it: a
              // set writes that, and DISCARD puts it back (#30).
              _pendingTakes.moveClipFile (entry.clipFile, to);
            }
        }
    }

  // Every set that named it comes with it. A set names its shapes rather than
  // carrying them, so one left behind holds a name nothing resolves -- and
  // that is not an error there, it is a slot that loads empty.
  auto const sets = renameInSets (was, name);

  // The patterns already in slots carry the old name in memory -- and so do
  // the ones an unsaved take is waiting to hand back (#30).
  for (auto &channel : _patterns)
    for (auto &pattern : channel)
      if (pattern && juce::String (pattern->getName ()) == was)
        pattern->setName (name.toStdString ());
  _pendingTakes.renamePattern (was, name);

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
        setSlotClipFile (channel, slot, juce::File{});

  _patternLibrary->refresh ();
  refreshAllPadRowLabels ();
  updateClipSettingsDisplay ();

  updateControlReadout ("-- DELETED " + was.toUpperCase ());

  // The selection is not set here either: deleteChosenEntry() puts it back on
  // the row that took this one's place, after everything has refreshed.
  refreshBrowser ();
}

void
A3MotionUIComponent::saveSessionInPlace ()
{
  auto const file = namedFileIn (sessionsDir (), _sessionName, ".json");
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
  return currentList ().canSaveInPlace ();
}

void
A3MotionUIComponent::saveChosen ()
{
  currentList ().saveInPlace ();
}

void
A3MotionUIComponent::saveAsChosen ()
{
  juce::String name;
  name = currentList ().saveAsCopy ();

  if (name.isEmpty ())
    return;

  // Which row that is, by what it is rather than by what it is called. Found
  // by name, a new clip landed on the *shape's* row whenever the two shared a
  // name -- and the rename that followed renamed the instrument's shape and
  // every set that pointed at it. A clip knows its file; the other two lists
  // are one folder each, where a name is an identity.
  auto row = -1;
  if (_browserList == BrowserList::Clips)
    row = browserRowForLibrary (_patternLibrary->indexForClipFile (
        _slotClipFile[_clipSettingsChannel][_clipSettingsSlot]));
  else
    for (int i = 0; i < _browser->getNumEntries () && row < 0; ++i)
      if (_browser->entryName (i) == name)
        row = i;

  if (row < 0)
    return;

  // The name it got is a counted one -- "Action 4" -- which is findable and
  // says nothing. So the new row opens for typing straight away, keyboard and
  // all: naming a thing is part of making it, and a second key press to get
  // there is a key press somebody skips and then cannot find what they saved.
  _deleteArmed = false;
  _browser->setSelectedEntry (row);
  _browser->beginRename (name);
  refreshBrowser ();
}

void
A3MotionUIComponent::updateActionPage ()
{
  if (!_action)
    return;

  auto const channel = _clipSettingsChannel;
  auto const slot = _clipSettingsSlot;

  _action->setTarget (static_cast<int> (channel), static_cast<int> (slot),
                      _channelUIStates[channel]->colour);

  // How the shown button is played -- its own since 2026-09-27, so six
  // buttons on one clip can each have their own feel.
  auto const *shown = shownActionButton ();
  auto const feel = shown ? shown->feel : ActionFeel{};
  _action->setEnvelope (feel.envelopeAttack, feel.envelopeDecay,
                        feel.envelopeMax);
  _action->setActMode (feel.actMode == ActMode::Hold ? 1 : 0);
  _action->setFreqEnvelope (feel.freqAttack, feel.freqDecay, feel.freqMax);
  _action->setQEnvelope (feel.qAttack, feel.qDecay, feel.qMax);

  auto const action = shown ? shown->file : juce::File{};

  // What the field's list offers: the empty row first, so a slot can go back
  // to firing nothing the same way it can go back to holding no clip.
  juce::StringArray choices;
  choices.add ("");
  for (auto const &entry : listFilesIn (actionsDir (), ".scd"))
    if (isActionScript (entry.file))
      choices.add (entry.name);
  _action->setActionChoices (choices);

  _action->setActionName (action.existsAsFile ()
                              ? action.getFileNameWithoutExtension ()
                              : juce::String{});

  std::array<juce::String, numActionButtons> names;
  for (size_t button = 0; button < names.size (); ++button)
    {
      auto const &file = _channelActions[channel][button].file;
      names[button] = file.existsAsFile () ? file.getFileNameWithoutExtension ()
                                           : juce::String{};
    }
  _action->setActionButtons (names, _chosenActionButton[channel]);

  std::array<bool, numActionButtons> holds{};
  for (size_t button = 0; button < holds.size (); ++button)
    holds[button] = _channelActions[channel][button].feel.actMode == ActMode::Hold;
  _action->setActionButtonModes (holds);
  _action->setAfter (shown ? shown->after : std::nullopt);

  // What the shown button puts on the clip, against the clip as it stands
  // before any accent -- the base a press is worked out against.
  auto const &pattern = _patterns[channel][0];
  auto const accentRuns = _accentBase[channel].has_value ()
                          && _engine.isChannelAccentActive (channel);
  auto const base = accentRuns ? *_accentBase[channel]
                    : pattern  ? clipSettingsFrom (*pattern)
                               : ClipSettings{};
  auto const playable = shown != nullptr && shown->settings.has_value ();
  _action->setMotionTile (
      playable ? motionShownFor (shown->source, base, shown->seed, MotionOverrides{})
               : std::array<MotionShown, numMotionParams>{},
      playable, getPatternLengthBeats (channel, 0));
}

void
A3MotionUIComponent::setShownButtonMotion (MotionParam param,
                                           std::optional<float> value)
{
  auto *button = shownActionButton ();
  if (button == nullptr || !button->settings)
    return;
  juce::String const name = motionParamScriptName (param);
  MotionOverrides one;
  if (value)
    one.set (param, *value);
  auto const written
      = value ? writeShownScriptSetting (name, withMotion (*button->settings, one))
              : editShownScript ([&name] (juce::String const &source) {
                  return unsetScriptLine (source, name);
                });
  if (written)
    updateControlReadout (juce::String (motionParamCaption (param))
                        + (value ? " " + juce::String (*value, 2)
                                 : juce::String (" from the clip")));
}

void
A3MotionUIComponent::setShownButtonAfter (std::optional<int> after)
{
  auto const written = editShownScript ([&after] (juce::String const &source) {
    return after ? setScriptLine (source, "then", juce::String (*after + 1))
                 : unsetScriptLine (source, "then");
  });
  if (written)
    updateControlReadout (
      "A" + juce::String (_chosenActionButton[_clipSettingsChannel] + 1)
      + " then " + afterName (after));
}

bool
A3MotionUIComponent::editShownScript (
    std::function<juce::String (juce::String const &)> const &edit)
{
  // ACTION writes into the script (2026-09-29), in place: every button on
  // every channel holding this file takes the new text, the editor in FILES
  // shows it, and the file is written once the hand stops.
  // A button with no script has nowhere to write, and says so rather than
  // showing a value nothing holds. A script whose file has gone meanwhile
  // (git, rm, an upgrade) is still the button's text: the edit applies and
  // the write puts the file back -- or says CANNOT WRITE.
  auto *shown = shownActionButton ();
  if (shown == nullptr || shown->file == juce::File{})
    {
      updateControlReadout (
          "-- A" + juce::String (_chosenActionButton[_clipSettingsChannel] + 1)
          + " HAS NO SCRIPT");
      return false;
    }

  auto const file = shown->file;
  auto const text = edit (shown->source);
  if (text == shown->source)
    return true;

  std::vector<std::array<juce::File, numActionButtons> > files;
  for (auto const &channel : _channelActions)
    {
      files.emplace_back ();
      for (size_t b = 0; b < channel.size (); ++b)
        files.back ()[b] = channel[b].file;
    }
  for (auto const &[channel, button] : buttonsHoldingFile (files, file))
    {
      auto &action = _channelActions[static_cast<size_t> (channel)]
                                    [static_cast<size_t> (button)];
      action.source = text;
      runButtonScript (static_cast<index_t> (channel), action);
    }

  if (_browser != nullptr && _panelFile == file)
    {
      auto &panel = _browser->scriptPanel ();
      panel.applyEdit (panel.hasUnsavedChanges () ? edit (panel.script ())
                                                  : text);
      panel.setErrors (shown->errors);
    }

  _scriptWrites.put (file, text);
  scheduleScriptWrite ();
  updateActionPage ();
  updateClipSettingsDisplay ();
  return true;
}

bool
A3MotionUIComponent::writeShownScriptSetting (juce::String const &name,
                                              ClipSettings const &settings)
{
  auto const written = writtenSettingFor (settings, name);
  return editShownScript ([&name, &written] (juce::String const &source) {
    return setScriptLine (source, name, written);
  });
}

void
A3MotionUIComponent::scheduleScriptWrite ()
{
  // The same debounce as the set's, shorter: a script is one file and the
  // editor beside it should not lag the knob by more than a breath.
  auto const generation = ++_scriptWriteGeneration;
  juce::Timer::callAfterDelay (
      300, [safeThis = juce::Component::SafePointer<A3MotionUIComponent> (this),
            generation] {
        if (safeThis == nullptr
            || safeThis->_scriptWriteGeneration != generation)
          return;
        safeThis->flushScriptWrites ();
      });
}

void
A3MotionUIComponent::flushScriptWrites ()
{
  auto const failed = writeAll (_scriptWrites.take ());
  if (!failed.isEmpty ())
    updateControlReadout ("-- CANNOT WRITE " + failed[0].toUpperCase ());
}

void
A3MotionUIComponent::followActionChains ()
{
  for (index_t channel = 0;
       channel < _channelActions.size () && channel < _actionChains.size ();
       ++channel)
    {
      AfterTable after;
      for (size_t b = 0; b < after.size (); ++b)
        after[b] = _channelActions[channel][b].after;

      if (auto const next = _actionChains[channel].accentEnded (
              _engine.accentEndCount (channel), after))
        fireChainedAction (channel, *next);
    }
}

void
A3MotionUIComponent::fireChainedAction (index_t channel, int button)
{
  auto fired = firedActionOf (channel, button);
  if (!fired)
    return;

  // Nobody holds a chained action, so a Hold button plays as a one-shot:
  // held by no finger, it would fall the instant it rose.
  fired->actMode = ActMode::OneShot;

  _actionChains[channel].pressed (button, _engine.accentEndCount (channel));
  if (channel < _actionSlot.size ())
    _actionSlot[channel] = button;

  // As if pressed and let go: the accent and what it throws the clip to,
  // and a clip the last action stopped starts again, as a press starts it.
  auto const &pattern = _patterns[channel][0];
  _engine.setChannelAction (channel, fired);
  _engine.setChannelAccentHeld (channel, true, pattern);
  _engine.setChannelAccentHeld (channel, false, nullptr);
  if (pattern && pattern->getStatus () == Pattern::Status::Idle)
    {
      pattern->setPlaybackLength (getPlaybackLength (channel, 0));
      _engine.playPattern (pattern, _now);
    }
}

void
A3MotionUIComponent::showPushedAction (index_t channel, index_t pad,
                                       PadSource source)
{
  if (padFunctionByPadIndex[pad] != PadFunction::Action)
    return;
  if (!actionPressShowsItsPage (source, isButtonPressed (Button::Shift)))
    return;

  // handlePadPress has already brought the channel up. A take being set up
  // or running keeps its REC page: the page follows the hand, but not away
  // from a recording.
  if (channel != _clipSettingsChannel)
    selectClip (channel, 0);
  // FPV stays on screen; the pressed action is still chosen, so FULL shows
  // it later and the encoders edit it.
  if (_view != AppView::Fpv && !_recArmedSlot && !takeIsUnderway ())
    {
      if (_overSphere != SphereOverlay::None)
        showOverSphere (SphereOverlay::None);
      if (_barPage != BarPage::Action)
        showBarPage (BarPage::Action);
    }
  chooseActionButton (actionButtonForPad[pad]);
}

void
A3MotionUIComponent::chooseActionButton (int button)
{
  auto const channel = _clipSettingsChannel;
  _chosenActionButton[channel]
      = juce::jlimit (0, static_cast<int> (numActionButtons) - 1, button);
  updateActionPage ();
  refreshBrowser ();
}

void
A3MotionUIComponent::applyActionControl (int control, int increment)
{
  // The shown button's feel, edited through a Pattern so its setters stay
  // the one place that knows each value's range (2026-09-27).
  auto *button = shownActionButton ();
  if (button == nullptr)
    return;
  auto const pattern = std::make_shared<Pattern> ();
  applyClipSettings (*pattern, withFeel (ClipSettings{}, button->feel));

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

  if (writeShownScriptSetting (actionControlScriptName (control),
                               clipSettingsFrom (*pattern)))
    updateControlReadout (actionReadoutFor (control, *pattern));
}

void
A3MotionUIComponent::setActionControl (int control, double value)
{
  // The shown button's feel, edited through a Pattern so its setters stay
  // the one place that knows each value's range (2026-09-27).
  auto *button = shownActionButton ();
  if (button == nullptr)
    return;
  auto const pattern = std::make_shared<Pattern> ();
  applyClipSettings (*pattern, withFeel (ClipSettings{}, button->feel));

  auto const step = static_cast<int> (std::lround (value));
  auto const level = static_cast<float> (value);

  switch (control)
    {
    case ActionComponent::Attack:      pattern->setEnvelopeAttack (step); break;
    case ActionComponent::Decay:       pattern->setEnvelopeDecay (step); break;
    case ActionComponent::EnvelopeMax: pattern->setEnvelopeMax (level); break;
    case ActionComponent::FreqAttack:  pattern->setFreqAttack (step); break;
    case ActionComponent::FreqDecay:   pattern->setFreqDecay (step); break;
    case ActionComponent::FreqMax:     pattern->setFreqMax (level); break;
    case ActionComponent::QAttack:     pattern->setQAttack (step); break;
    case ActionComponent::QDecay:      pattern->setQDecay (step); break;
    case ActionComponent::QMax:        pattern->setQMax (level); break;
    default:                           return;
    }

  if (writeShownScriptSetting (actionControlScriptName (control),
                               clipSettingsFrom (*pattern)))
    updateControlReadout (actionReadoutFor (control, *pattern));
}

void
A3MotionUIComponent::resetActionControl (int control)
{
  // The shown button's feel, edited through a Pattern so its setters stay
  // the one place that knows each value's range (2026-09-27).
  auto *button = shownActionButton ();
  if (button == nullptr)
    return;
  auto const pattern = std::make_shared<Pattern> ();
  applyClipSettings (*pattern, withFeel (ClipSettings{}, button->feel));

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

  if (writeShownScriptSetting (actionControlScriptName (control),
                               clipSettingsFrom (*pattern)))
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
A3MotionUIComponent::handlePadRelease (index_t channel, index_t pad)
{
  index_t const slot = 0;
  auto const button = actionButtonForPad[pad];

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
  if (_actHeldSlot[channel] == button)
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

  if (_previewHeldPad[channel] != button)
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
        // An end action Clip handed the channel over on the clock thread; the
        // channel holds the follow from now on.
        takeOverFollow (channel, messagePatternStatus.pattern);
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

        // A finished take is *not* saved here any more: it waits in its slot
        // for SAVE or DISCARD. See PendingTakes. What it needs now is a line
        // on the sphere, a pad that shows it and a bar that offers the keys.
        if (messagePatternStatus.pattern->wasRecording ())
          {
            auto sitsInASlot = false;
            for (index_t slot = 0; slot < _patterns[channel].size (); ++slot)
              if (_patterns[channel][slot] == messagePatternStatus.pattern)
                {
                  sitsInASlot = true;
                  updatePadRowLabel (channel, slot);
                }

            // saveRecordedPattern() used to do this; a take outside the
            // library is drawn from its ticks (patternDisplayFor()).
            //
            // Only while the take is still in a slot. A DISCARDed take stops
            // *after* it has been taken out, and registering it here put it
            // back into the display data it had just been removed from (#31).
            if (sitsInASlot)
              refreshPatternDisplay (messagePatternStatus.pattern);
            refreshTakeState ();
          }
        break;
      }
    }
}

void
A3MotionUIComponent::setFunctionKey (FunctionKey key, KeySource source,
                                     bool down)
{
  // The panel and the PADS page are two places to hold one key: it goes down
  // with the first and up with the last, and only then does it mean anything.
  if (auto const changed = _functionKeys.set (key, source, down))
    functionKeyChanged (key, *changed);
}

void
A3MotionUIComponent::functionKeyChanged (FunctionKey key, bool down)
{
  switch (key)
    {
    case FunctionKey::ClockMode:
      // Cycles, like the status bar's clock key -- the panel and the screen
      // are places to reach one function, so they do the same thing.
      if (down)
        {
          applyClockMode ((_clockMode + 1) % 3);
          _clipSettings->setClockMode (_clockMode);
          updateControlReadout ("-- CLOCKMODE");
        }
      return;

    case FunctionKey::RecMode:
      if (down)
        {
          auto const count = static_cast<int> (recMenuModes.size ());
          applyRecMode ((recMenuIndex (_recMode) + 1) % count);
          updateControlReadout ("-- RECMODE");
        }
      return;

    case FunctionKey::Menu:
      // Toggles on press; the release means nothing.
      if (down)
        toggleGlobalSettings ();
      return;

    case FunctionKey::Record:
      updateControlReadout (juce::String ("-- RECORD ")
                            + (down ? "ON" : "OFF"));

      // Pressed while a take is running, this ends it. The finger no longer
      // bounds a recording — that is what makes a jump recordable — so
      // something else has to, and Record is the button that started it.
      // Held down, it arms nothing on its own: recording starts when a
      // channel's Play|Pause pad is pressed while it is held -- see
      // handlePadPress().
      if (down && _engine.isRecording ())
        endRecording ();
      return;

    case FunctionKey::Shift:
      // Pure modifier: read through isButtonPressed(Button::Shift) when a pad
      // is pressed. The strip is told too, because it is what the LEDs and
      // the PADS page's key are painted from.
      if (_clipSettings)
        _clipSettings->setShiftHeld (down);
      updateFunctionKeyLEDs ();
      updateControlReadout (juce::String ("-- SHIFT ") + (down ? "ON" : "OFF"));
      return;

    case FunctionKey::Tap:
      if (down)
        {
          // Sent at once through the direct sender, not the async queue:
          // a tap is only worth its timing.
          _clipSettings->flashTap ();
          updateFunctionKeyLEDs ();
          updateControlReadout ("-- TAP");

          auto tapMsg = juce::OSCMessage (_oscAddresses.tap);
          tapMsg.addInt32 (1);
          _tapSender.send (tapMsg);
        }
      return;
    }
}

bool
A3MotionUIComponent::isButtonPressed (Button button)
{
  // The panel's key or the PADS page's: a gesture that needs the modifier
  // does not care which hand is on it. Also safe in a build with no panel,
  // where there is no adapter to ask.
  return _functionKeys.isDown (button);
}

void
A3MotionUIComponent::tickCallback (Measure measure)
{
  _now = measure;

  // This thread's own copy of the beat address, taken over here and read
  // nowhere else.
  applyPendingBeatAddress ();

  // A button's "then": decided here, on the message thread, from the
  // engine's count of accents that have ended.
  followActionChains ();

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
          // tick. A knob repaints only when its value or arc moved, so still
          // knobs cost nothing here.
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
          _channelUIStates[channel]->colour.withAlpha (theme ().alphaOutline));

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
  flushScriptWrites ();
  auto const numChannels = static_cast<int> (_patterns.size ());
  auto const set = loadSession (file, numChannels,
                            static_cast<int> (numClipSlots));

  // A set without keys is one written before they were part of it, and
  // leaves the device's alone.
  if (set.speedButtonLog2)
    {
      _speedButtonLog2 = *set.speedButtonLog2;
      if (_clipSettings)
        _clipSettings->setSpeedButtons (_speedButtonLog2);
    }

  for (int ch = 0; ch < numChannels; ++ch)
    {
      auto const index = static_cast<index_t> (ch);
      auto const &channel = set.channels[static_cast<size_t> (ch)];

      // The six buttons first, as the slots' actions were: a button the set
      // names gets it, one it leaves empty keeps what it had. What a button
      // does is in its script (2026-09-29); the set only names it.
      for (int b = 0; b < numActionButtons; ++b)
        {
          auto const &entry = channel.actions[static_cast<size_t> (b)];
          if (entry.script.empty ())
            continue;
          setButtonAction (index, b,
                           namedFileIn (actionsDir (),
                                        juce::String (entry.script), ".scd"));
        }

      // Where the channel was parked. Empty on a first run, which is zero,
      // which is where they start anyway.
      //
      // These three are also what A3 Core answers a recall with, so both
      // claim them. At start-up Core wins, because the recall lands after
      // this; when a set is loaded by hand the set wins, because nothing
      // asks Core afterwards. Decided that way on purpose -- see the long
      // comment in askCoreForItsState().
      _engine.setChannelPot1 (index, channel.freq);
      _engine.setChannelPot2 (index, channel.q);
      _engine.setChannelPot3 (index, channel.threeD);

      for (index_t slot = 0; slot < numClipSlots; ++slot)
        {
          auto const &saved = channel.slots[static_cast<size_t> (slot)];
          _clipUIParams[index][slot].recordLengthLog2
              = saved.recordLengthLog2;

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
              auto const clip
                  = namedFileIn (_patternLibrary->getClipDir (),
                                 juce::String (saved.clipFile), ".json");
              if (clip.existsAsFile ())
                _slotClipFile[index][slot] = clip;
            }

          // And what the slot was turned to.
          //
          // A set with none is a set that never turned this slot away from
          // its clip, so the clip's own settings are what it wants -- and
          // they have to be read, not assumed. fillSlotFromLibrary() above
          // gives the slot the *shape's* clip, which is a different file from
          // the one the set names: a slot that named Breath and carried no
          // overrides came up holding Lissajous 1-2's settings and flagged
          // itself as drifted from Breath, which is exactly what it was.
          if (auto const &pattern = _patterns[index][slot])
            {
              auto const clip = ClipFile::load (_slotClipFile[index][slot]);
              if (saved.overrides.has_value ())
                {
                  applyClipSettings (*pattern, *saved.overrides);
                  syncClipUIParamsFromPattern (index, slot);
                }
              else if (clip)
                {
                  applyClipSettings (*pattern, clip->settings);
                  syncClipUIParamsFromPattern (index, slot);
                }
              // The lanes are the clip's whatever the set turned: a set
              // carries settings, not takes.
              if (clip)
                applyLanes (*pattern, *clip);
              // The follow is the clip's unless the set chose another.
              pattern->setEndClip (!saved.endClip.empty () ? saved.endClip
                                   : clip             ? clip->endClip
                                                      : std::string ());
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
A3MotionUIComponent::setSlotClipFile (index_t channel, index_t slot,
                                      juce::File const &file)
{
  _slotClipFile[channel][slot] = file;
  scheduleSetSave ();
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

      // The six buttons, by script name: what each does is in its script.
      for (int b = 0; b < numActionButtons; ++b)
        {
          auto const &button = _channelActions[index][static_cast<size_t> (b)];
          channel.actions[static_cast<size_t> (b)].script
              = button.file.getFileNameWithoutExtension ().toStdString ();
        }

      channel.slots.resize (numClipSlots);
      for (index_t slot = 0; slot < numClipSlots; ++slot)
        {
          auto &saved = channel.slots[static_cast<size_t> (slot)];
          saved.recordLengthLog2
              = _clipUIParams[index][slot].recordLengthLog2;

          // The actions are the channel's six buttons now (below); a
          // slot names none.
          saved.action.clear ();

          // A set names only what is on disk: an unsaved slot stands for
          // what it held before its take. See PendingTakes::forSet().
          auto const content = _pendingTakes.forSet (
              index, slot,
              { _patterns[index][slot], _slotClipFile[index][slot] });
          auto const pending = _pendingTakes.isPending (index, slot);

          if (auto const &pattern = content.pattern)
            {
              saved.patternName = pattern->getName ();
              saved.clipFile = content.clipFile
                                   .getFileNameWithoutExtension ()
                                   .toStdString ();

              // Running counts as running: a clip waiting for the next beat
              // is one somebody has already started, and a set saved in that
              // half-second should not come back silent.
              auto const status = pattern->getStatus ();
              // Not for a slot that is pending: what it held before was
              // stopped when the take began.
              saved.playing
                  = !pending
                    && (status == Pattern::Status::Playing
                        || status == Pattern::Status::ScheduledForPlaying);

              // Everything, not only what differs from the clip's own file.
              // The difference was worked out with clipHasDrifted(), which
              // answers false for a slot with no clip file at all -- so every
              // slot filled straight from a shape wrote nothing, and came
              // back as the bare shape with its settings gone.
              saved.overrides = clipSettingsFrom (*pattern);
              saved.endClip = pattern->getEndClip ();
            }
        }
    }

  set.speedButtonLog2 = _speedButtonLog2;

  return set;
}

void
A3MotionUIComponent::setChannelPotValue (index_t channel, ChannelPot pot,
                                         float value)
{
  auto const clamped = std::clamp (value, 0.f, 1.f);

  // Numbered by the panel -- see channelPotValue().
  switch (pot)
    {
    case ChannelPot::ThreeD: _engine.setChannelPot3 (channel, clamped); break;
    case ChannelPot::Freq: _engine.setChannelPot1 (channel, clamped); break;
    case ChannelPot::Q: _engine.setChannelPot2 (channel, clamped); break;
    }

  // The knobs showing these three, and nothing else (#64): describing the
  // whole clip again on every step of a drag -- every playing clip's
  // elevation figure, the shape's SVG, the bar's setters -- for a value none
  // of it shows was the lag behind the finger.
  refreshChannelValues ();
  scheduleSetSave ();
}

void
A3MotionUIComponent::resetChannelPot (index_t channel, ChannelPot pot)
{
  // Panel or not: a drag here was never refused, so neither is the way back.
  // The 3d pot disagrees with the screen until it is next moved, as it does
  // after a drag.
  if (auto const rest = channelPotRestPosition (pot))
    setChannelPotValue (channel, pot, *rest);
}

void
A3MotionUIComponent::refreshChannelValues ()
{
  if (!_mixer || !_mixerStrip || !_clipSettings)
    return;

  // Every channel's three, not only the shown one's: the overlay shows all
  // four, and the bar's strip keeps them so a face tapped shows its channel's
  // at once. Each goes in with its *effective* value beside it -- the setting
  // with its envelope laid over it -- because a modulation that moves nothing
  // on screen is one you have to take on trust.
  for (int ch = 0; ch < numChannelColumns; ++ch)
    {
      auto const index = static_cast<index_t> (ch);

      // In channelPotOrder: 3D, FREQ, Q.
      auto const pots = ChannelPotValues{
        { _engine.getChannelPot3 (index), _engine.getChannelPot1 (index),
          _engine.getChannelPot2 (index) },
        { _engine.getChannelPot3Effective (index),
          _engine.getChannelPot1Effective (index),
          _engine.getChannelPot2Effective (index) }
      };
      _mixer->setChannelPots (ch, pots);
      _mixerStrip->setChannelPots (ch, pots);
      _clipSettings->setChannelPots (ch, pots);
    }
}

void
A3MotionUIComponent::updateFunctionKeyLEDs ()
{
  if (!_clipSettings)
    return;

  // The panel's LEDs and the PADS page's keys, from one look: a key that is
  // coloured on the screen is coloured under the hand, and neither is worked
  // out twice.
  auto const look = _clipSettings->functionKeyLook ();
  if (_controller)
    _controller->setFunctionKeyLook (look);

  if (!_ioAdapter)
    return;

  // While the keyboard owns the panel, a key's LED says which key it types.
  // Both end columns share one LED per row, and in every row both ends are
  // the same kind of key (PanelKeyboard.hh), so the left one speaks for both.
  for (auto const key : functionKeyOrder)
    _ioAdapter->setButtonLED (
        key, keyboardShown ()
                 ? keyboardLedColour (
                     panelKeyAt (KeyboardPage::Letters,
                                 { functionKeyPosition (key), 0 }),
                     toColour (theme ().accent), keyboardLetterLed ())
                 : functionKeyColour (key, look));
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
      auto const accentActive = _engine.isChannelAccentActive (channel);
      for (auto pad = 0u; pad < _ioAdapter->getNumPadsPerChannel (); ++pad)
        {
          index_t const slot = 0;
          auto const function = padFunctionByPadIndex[pad];
          auto const button = actionButtonForPad[pad];
          auto const clipStatus = _patterns[channel][slot]
                                      ? _patterns[channel][slot]->getStatus ()
                                      : Pattern::Status::Empty;
          auto const clipStatusLast
              = _patterns[channel][slot]
                    ? _patterns[channel][slot]->getLastStatus ()
                    : Pattern::Status::Empty;

          // Play|Pause is the one pad that must be readable at a glance:
          // green while actually playing, channel colour otherwise (idle/
          // empty/recording), so play vs. paused/stopped is unambiguous.
          bool const clipPlaying
              = clipStatus == Pattern::Status::Playing
                || clipStatus == Pattern::Status::ScheduledForPlaying;
          // The Action pad whose button fired the channel's accent, for as
          // long as that accent runs -- the duration of the action, on the pad.
          bool const actionRunning
              = accentActive && button >= 0 && channel < _actionSlot.size ()
                && _actionSlot[channel] == button;
          bool const assigned
              = button < 0
                || _channelActions[channel][static_cast<size_t> (button)]
                       .file.existsAsFile ();
          bool const lit = function == PadFunction::Page
                               ? channel == _clipSettingsChannel
                               : actionRunning;
          auto const status
              = padShadeStatus (function, clipStatus, assigned, lit);
          auto const statusLast = function == PadFunction::PlayPause
                                      ? clipStatusLast
                                      : status;
          auto const base = padBaseColour (function, clipPlaying,
                                           actionRunning, channelColour);
          if (_action && channel == _clipSettingsChannel
              && function == PadFunction::Action)
            {
              if (actionRunning)
                _action->setRunningButton (button);
              else if (!accentActive)
                _action->setRunningButton (-1);
            }

          auto const colour = channelColourForPadStatus (
              base, status, statusLast, step);
          // Under the hand, the keyboard while it is up; the screen's PADS
          // page keeps showing the set.
          auto const led
              = keyboardShown ()
                    ? keyboardLedColour (
                        panelKeyAt (KeyboardPage::Letters,
                                    panelCellOfPad (static_cast<int> (channel),
                                                    static_cast<int> (pad))),
                        toColour (theme ().accent), keyboardLetterLed ())
                    : colour;
          _ioAdapter->getPadLED (channel, pad)
              = juce::VariantConverter<juce::Colour>::toVar (led);

          // The same colour to the screen. One place works out what empty,
          // idle, armed and running look like; two places show it.
          if (_controller)
            {
              _controller->setPadColour (channel, pad, colour);
              _controller->setPadPlaying (channel, pad, clipPlaying);
            }
        }
    }
}

juce::Colour
A3MotionUIComponent::channelColourForPadStatus (juce::Colour base,
                                                Pattern::Status status,
                                                Pattern::Status statusLast,
                                                int step)
{
  return padStatusColour (base, status, statusLast, step,
                          _engine.getRecordingMode ()
                              == MotionEngine::RecordingMode::OneShot);
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
              refreshPatternDisplay (_patterns[ch][slot]);
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

  _padRowDisplays[slot]->setUnsaved (static_cast<int> (channel),
                                     _pendingTakes.isPending (channel, slot));
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
A3MotionUIComponent::refreshPatternDisplay (
    std::shared_ptr<Pattern> const &pattern)
{
  // Called from fillSlotFromLibrary(), which also runs while the interface is
  // still being built: at startup the slots are filled before there is
  // anything to draw them on, and the initial pass registers them again once
  // there is.
  if (!pattern || !_motionComponent)
    return;

  // One answer for the first registration and for every refresh after a
  // value was turned -- see patternDisplayFor(). Two routes with half of it
  // each is how a shape of dots vanished on the first knob turned.
  auto shown = patternDisplayFor (*pattern, *_patternLibrary);
  _motionComponent->setPatternDisplayData (pattern, std::move (shown.path),
                                           std::move (shown.jumpDots));
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
  // The REC light is not cleared here either -- same reason, and the braces
  // that were missing are why the three statements under this `if` only
  // looked like they belonged to it.
  updateFunctionKeyLEDs ();

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
          _slotClipFile[channel][slot] = _clipFileBeforeRecording;
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

  // Something performed -- the finger or a knob -- and a path to play it on.
  // The path alone cannot say the first: a take over a clip starts out
  // holding that clip's.
  auto const written = pattern->writtenTicks ();
  auto const anyWritten
      = _engine.takeWroteSomething ()
        && std::any_of (written.begin (), written.end (),
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
      // Not on disk until somebody says so. A slot that already held an
      // unsaved take keeps the before it had -- see PendingTakes::begin().
      _pendingTakes.begin (channel, slot,
                           { _patternBeforeRecording,
                             _clipFileBeforeRecording });
    }
  else
    {
      // Nothing was ever played into it. Put back what the slot held rather
      // than leaving a clip made of nothing.
      _patterns[channel][slot] = _patternBeforeRecording;
      _slotClipFile[channel][slot] = _clipFileBeforeRecording;
      updateControlReadout ("recording discarded - nothing played");
    }

  _recordingSlot.reset ();
  _patternBeforeRecording.reset ();
  _clipFileBeforeRecording = juce::File{};
  if (_motionComponent)
    _motionComponent->setRecordingUnderlay (nullptr);
  selectClip (channel, slot);
}

void
A3MotionUIComponent::saveShownTake ()
{
  auto const channel = _clipSettingsChannel;
  auto const slot = _clipSettingsSlot;
  if (!_pendingTakes.isPending (channel, slot))
    return;

  auto const pattern = _patterns[channel][slot];
  if (!pattern)
    return;

  // Every setting on the take at this moment goes with it, including what was
  // dialled after it ended: saveRecordedPattern() reads them off the pattern.
  saveRecordedPattern (pattern, channel, slot);

  // It names the take and writes it; a take it could not write is still
  // unsaved, and says so.
  auto const index = _patternLibrary->indexForName (pattern->getName ());
  if (index <= 0)
    {
      updateControlReadout ("-- SAVE FAILED");
      return;
    }

  _pendingTakes.clear (channel, slot);
  // The clip saveUserPattern() wrote beside the shape. Not the library entry
  // found by name: that is the shape's, and a shape carries no clip file.
  auto const clip = namedFileIn (_patternLibrary->getClipDir (),
                                 juce::String (pattern->getName ()), ".json");
  _slotClipFile[channel][slot] = clip.existsAsFile () ? clip : juce::File{};
  updateControlReadout ("-- SAVED "
                        + juce::String (pattern->getName ()).toUpperCase ());
  updatePadRowLabel (channel, slot);
  refreshTakeState ();
  refreshBrowser ();
}

void
A3MotionUIComponent::pressDiscardOnShownTake ()
{
  auto const channel = _clipSettingsChannel;
  auto const slot = _clipSettingsSlot;

  if (!_pendingTakes.pressDiscard (channel, slot))
    {
      if (_pendingTakes.isDiscardArmed (channel, slot))
        updateControlReadout ("-- DISCARD? TAP AGAIN");
      refreshTakeState ();
      return;
    }

  // Stopped, and taken off the sphere the way a slot filled from the library
  // takes its old pattern off: left in the display data, every discarded take
  // stayed there for the rest of the evening (#31).
  if (auto const &take = _patterns[channel][slot])
    {
      _engine.stopPattern (take, _now);
      _motionComponent->unsetPreviewPattern (take);
      _motionComponent->removePatternDisplayData (take);
    }

  auto const before = _pendingTakes.resolve (channel, slot);
  _patterns[channel][slot] = before.pattern;
  _slotClipFile[channel][slot] = before.clipFile;
  // The bar's direction and end action are read from here, and were the
  // take's: the next edit would have written them onto what came back.
  if (before.pattern)
    syncClipUIParamsFromPattern (channel, slot);

  updateControlReadout ("-- DISCARDED");
  updatePadRowLabel (channel, slot);
  updateClipSettingsDisplay ();
  refreshTakeState ();
  scheduleSetSave ();
}

void
A3MotionUIComponent::disarmOnOtherInput ()
{
  if (!_pendingTakes.anyDiscardArmed ())
    return;
  _pendingTakes.disarm ();
  refreshTakeState ();
}

void
A3MotionUIComponent::mouseDown (juce::MouseEvent const &event)
{
  // An armed DISCARD drops on anything else that is touched, as the spec
  // asked (#32) -- one rule here instead of a call in every handler, which
  // had covered seven of them. ACT itself is the one exception: its press is
  // what confirms.
  if (_clipSettings != nullptr
      && _clipSettings->isOnTransportKey (event.originalComponent,
                                          TransportKey::Action))
    return;
  disarmOnOtherInput ();
}

void
A3MotionUIComponent::dropPendingTake (index_t channel, index_t slot)
{
  _pendingTakes.resolve (channel, slot);
  refreshTakeState ();
}

bool
A3MotionUIComponent::takeIsUnderway ()
{
  return _engine.isRecording () || _recordingSlot.has_value ();
}

void
A3MotionUIComponent::refreshTakeState ()
{
  if (!_clipSettings)
    return;

  // The same rule the two keys act on -- see PendingTakes::offersKeys().
  auto const offered = _pendingTakes.offersKeys (
      _clipSettingsChannel, _clipSettingsSlot, takeIsUnderway ());
  _clipSettings->setTakeState (
      offered,
      offered
          && _pendingTakes.isDiscardArmed (_clipSettingsChannel,
                                           _clipSettingsSlot));
}

void
A3MotionUIComponent::saveRecordedPattern (
    std::shared_ptr<Pattern> const &pattern, index_t channel, index_t slot)
{
  if (!pattern || pattern->getNumTicks () == 0)
    return;

  // The day it was made, plus a running number. The number is part of the
  // *name*, not only of the file: saveUserPattern already avoids overwriting
  // a file, but a set names its takes rather than pointing at a path, so two
  // takes of one day carrying one name is a set that cannot say which it
  // meant. See a3-motion-engine/RecordingName.hh.
  auto const base = recordingBaseName (juce::Time::getCurrentTime ());
  auto const name
      = freeRecordingName (base, [this] (juce::String const &candidate) {
          // Zero is "no such name" here, not minus one: the index is
          // 1-based because 0 is the library's own "Empty" row.
          return _patternLibrary->indexForName (candidate.toStdString ()) > 0;
        }).toStdString ();
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
      refreshPatternDisplay (pattern);

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
  sayHelloWhenDue ();

  if (_view == AppView::Fpv)
    refreshFpvStrips ();

  // At most one theme apply per tick, whatever arrived since the last one --
  // see applyEditedSkin(). Here rather than in a callAsync of its own because
  // a timer is what "once per frame" means; a queue is what it did before.
  if (_skinApplyPending)
    {
      _skinApplyPending = false;
      applyThemeEverywhere (loadTheme (_skinToApply), *this);
      _skinToApply = juce::var ();
    }

  // First and whatever else is happening: what the status bar's meters say is
  // that something is arriving at all, which is the question asked on the
  // pages that are not the mixer -- and it is a question the conditions below
  // cannot answer, since none of them knows anything about audio.
  //
  // So the bar's meters run at uiTimerHz rather than at vuMeterRefreshHz. A
  // bar that never leaves the screen has no visibility to start and stop a
  // timer on, and a second timer running for the life of the device is the
  // one thing vuMeterRefreshHz's own note argues against.
  if (_clipSettings)
    _clipSettings->repaintChannelMeters ();

  // While a take runs its write head moves, and while a clip plays its
  // playhead does. Both fill the tick indicator, so both have to be followed
  // -- watching only the recording is what left a playing clip with a
  // transport key that never turned green and an indicator that stayed empty.
  // The rest of the time nothing here changes on its own.
  // One tick past the accent as well as during it. An action puts its
  // settings on the clip and takes them off again, and the taking off is the
  // last thing that happens -- a screen that stopped following one tick
  // earlier would show the action's values for as long as the page stayed
  // open.
  auto const accent = _engine.isChannelAccentActive (_clipSettingsChannel);

  // Any channel, not just the shown one. The tick indicator now carries a
  // mark per channel, so a clip running on a channel nobody is looking at
  // still moves something on screen -- watching only the shown clip left
  // those marks standing still.
  auto anyMoving = false;
  for (index_t channel = 0; channel < _patterns.size () && !anyMoving;
       ++channel)
    for (index_t slot = 0; slot < _patterns[channel].size (); ++slot)
      {
        auto const &pattern = _patterns[channel][slot];
        if (pattern == nullptr)
          continue;

        // Waiting counts as moving. A clip pressed and not yet started -- or
        // stopped and not yet stopped -- is the window the play key blinks in,
        // and it is a window in which nothing else on this list is true.
        auto const status = pattern->getStatus ();
        if (status == Pattern::Status::Playing
            || status == Pattern::Status::ScheduledForPlaying
            || status == Pattern::Status::ScheduledForIdle)
          {
            anyMoving = true;
            break;
          }
      }

  auto const moving = _engine.isRecording () || accent || anyMoving
                      || _engine.getScheduledForRecordingPattern () != nullptr;

  // **One tick past, always.** Every transition *out* of motion happens at the
  // moment nothing is moving any more, and that is exactly the frame that has
  // to be drawn -- so a condition that only runs while something moves can
  // never draw it. Measured on 2026-09-13: pause a clip with nothing else
  // running and the key went on showing the triangle indefinitely, because the
  // refresh stopped in the same tick the clip did. The accent had this
  // already, as _accentWasActive; it was the one case somebody had hit.
  pushKnobHolds ();

  if (moving || _wasMoving)
    updateClipSettingsDisplay ();
  _wasMoving = moving;

  armFollowClips ();

  // Two seconds' worth of ticks, which is the pace this used to run at.
  if (++_timerTick % libraryCheckTicks != 0)
    return;

  // Periodically check if pattern directories have changed
  auto fp = _patternLibrary->getDirectoryFingerprint ();
  if (fp != _lastLibraryFingerprint)
    {
      _lastLibraryFingerprint = fp;
      // A follow is a snapshot of its clip file: one saved since is armed
      // again on the next frame.
      for (auto &armed : _armedFollows)
        armed.from = nullptr;
      std::cout << "PatternLibrary: directory change detected, refreshing..."
                << std::endl;
      _patternLibrary->refresh ();
      refreshAllPadRowLabels ();
      // The browser holds a copy of the list. Refreshing the library behind an
      // open browser and leaving its rows alone is how a file that is plainly
      // in the folder stays missing from the only list that shows it.
      //
      // Holding the selection, because this fires on *any* change to the
      // folder -- including one the performer just made. It used to re-point
      // the list at the shown slot's clip two seconds after every delete, and
      // take the highlight with it.
      if (_overSphere == SphereOverlay::Files)
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

  // And into the mixer's own store, beside the two atomics above rather than
  // instead of them. Those two are what the GL thread reads for the corona
  // around the blob and they remember nothing about when a value arrived; a
  // falling bar and a held mark need that, and it has to be one memory for
  // every page that draws this channel's meter.
  _vuLevels.setChannel (channel, { peak, rms }, vuNowMs ());
}

void
A3MotionUIComponent::onSubwooferVU (float peak, float rms)
{
  _motionComponent->setSphereGlow (peak, rms);
}

void
A3MotionUIComponent::onOutputVU (int meter, float peak, float rms)
{
  _vuLevels.setOutput (meter, { peak, rms }, vuNowMs ());
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
A3MotionUIComponent::BeatArrival::setAddress (juce::String const &newAddress)
{
  std::lock_guard<std::mutex> lock (_addressMutex);
  _address = newAddress;
}

void
A3MotionUIComponent::BeatArrival::oscMessageReceived (
    juce::OSCMessage const &message)
{
  auto const arrival = TempoClock::monotonicNanoseconds ();

  {
    std::lock_guard<std::mutex> lock (_addressMutex);
    if (message.getAddressPattern ().toString () != _address)
      return;
  }
  if (message.size () < 3)
    return;

  auto const number = [] (juce::OSCArgument const &arg) {
    if (arg.isInt32 ())
      return static_cast<float> (arg.getInt32 ());
    return arg.isFloat32 () ? arg.getFloat32 () : 0.f;
  };
  auto const beat = static_cast<int> (number (message[0]));

  if (BeatTrace::device ().isEnabled ())
    BeatTrace::device ().record ("rx", beat,
                                 static_cast<int> (number (message[1])),
                                 number (message[2]));

  // The sender counts beats from 1, the clock from 0.
  if (follow.load (std::memory_order_relaxed) && beat >= 1)
    engine.getTempoClock ().syncToBeat (beat - 1, arrival);
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

namespace
{

/** Whether a position from outside may move this channel.
 *
 *  A channel playing a trajectory has its own idea of where it is, four
 *  times a bar and more; letting an outside position in would be a jump in
 *  the middle of a movement, heard in the room. At start-up -- the case this
 *  whole path exists for -- nothing is playing yet, so the recall lands in
 *  full.
 *
 *  The cost, stated because it is invisible: a recall while a channel plays
 *  does nothing for that channel, and says nothing about it either.
 *
 *  Recording is deliberately not guarded here. The blob follows the finger
 *  through setRecording*Position on every tick, so a stray position would be
 *  overwritten within the frame rather than fought over.
 */
bool
mayBeMovedFromOutside (MotionEngine &engine, int channel)
{
  return engine.getPlayingPattern (static_cast<index_t> (channel)) == nullptr;
}

}

/** Moves one channel, keeping the two spherical values this message does not
 *  carry.
 *
 *  Azimuth and elevation arrive as two separate messages, so each one has to
 *  leave the other alone; the distance is preserved for the same reason. It
 *  is 1 on this device -- Channel's constructor builds fromSpherical(0, 0, 1)
 *  -- but reading it rather than writing 1 here means this keeps working if
 *  that ever stops being true.
 *
 *  Position::setAzimuth/setElevation would read better and deliberately do
 *  not exist -- see Geometry.hh. Setting one angle rebuilds the position out
 *  of the other two, and naming all three is honest about that.
 *
 *  A value that is not finite is dropped. It arrives over the network, and a
 *  NaN would reach the IEM plugins as a position.
 */
void
A3MotionUIComponent::moveChannelFromOutside (int channel, float azimuth,
                                             float elevation)
{
  if (!std::isfinite (azimuth) || !std::isfinite (elevation))
    return;

  if (!mayBeMovedFromOutside (_engine, channel))
    return;

  auto const index = static_cast<index_t> (channel);
  auto const now = _engine.getChannelPosition (index);

  _engine.setChannel3DPosition (
      index, Pos::fromSpherical (azimuth, elevation, now.distance ()));
}

/** Ask A3 Core to say its whole state again.
 *
 *  Sent from the constructor, at the first moment there is anywhere to send
 *  it: the receiver is bound a few lines above and the sender to Core has
 *  just connected. Core answers with its lamps and with the position of
 *  every channel it has heard one for -- the only place those positions
 *  exist, since Core writes them straight to the IEM plugins and nothing
 *  reports them back.
 *
 *  Without this the device comes up asserting its own idea of every channel
 *  and the room hears the difference at once:
 *  issues/a3-motion-ui-total-recall-at-startup.md.
 *
 *  The answer arrives through OSCReceiver::MessageLoopCallback, on the same
 *  message thread this constructor runs on, so it cannot land while
 *  construction is still going.
 *
 *  Sent once and never repeated. Over UDP a Core that is not up yet simply
 *  does not answer, and the device keeps its own values -- the documented
 *  fallback, and what happened before any of this existed. A retry would
 *  need a notion of "did an answer arrive", which nothing here has.
 */
void
A3MotionUIComponent::askCoreForItsState ()
{
  // WHO WINS, AND WHY IT IS NOT AN ACCIDENT.
  //
  // Core's answer carries freq, Q and 3d, and so does a set. Both claim the
  // same three values, and which one the channel ends up with is decided by
  // nothing but the order they run in. Decided by the maintainer 2026-09-12,
  // written down here because the code only expresses it by accident:
  //
  //   at start-up      applySet() runs first, this answer lands after it,
  //                    so CORE WINS -- the rig comes up where it actually
  //                    sounds, which is the whole point of asking.
  //   loading a set    the set is applied and nothing asks Core afterwards,
  //                    so THE SET WINS -- you asked for those values, and a
  //                    jump to them is the instruction, not a fault.
  //
  // That holds only because this is the one and only call site, and it is in
  // the constructor. Asking again later -- on a reconnect, on a timer, from
  // a menu -- would make Core win over a set that was just loaded by hand,
  // silently. If you add a second call, this is the rule you are changing.
  //
  // Re-armed from here, where the wait for an answer actually begins. See the
  // comment at the first arming for why this is not redundant even though the
  // first one measured as sufficient.
  _engine.holdOutputUntil (juce::Time::getMillisecondCounterHiRes ()
                                   + recallGraceMillis);

  auto message = juce::OSCMessage (_oscAddresses.stateRecall);
  message.addFloat32 (1.f);
  _mixerSender.send (message);
}

void
A3MotionUIComponent::sayHelloWhenDue ()
{
  // On a plain sender aimed at Core: the async mixer sender carries numbers
  // only. The digest is of the truth this process loaded, the one Core
  // compares with its fingerprint.
  auto const now = juce::Time::getMillisecondCounterHiRes ();
  if (!_helloSchedule.due (now))
    return;
  _helloSchedule.said (now);
  _helloSender.send (
      helloMessage (_oscAddresses, installedOscTruth ().digest ()));
}

void
A3MotionUIComponent::onChannelValue (
    int channel, OscMessageHandler::Listener::ChannelValue which, float value)
{
  using Value = OscMessageHandler::Listener::ChannelValue;

  auto const index = static_cast<index_t> (channel);

  switch (which)
    {
    case Value::Azimuth:
      moveChannelFromOutside (channel, value,
                              _engine.getChannelPosition (index).elevation ());
      return;

    case Value::Elevation:
      moveChannelFromOutside (channel,
                              _engine.getChannelPosition (index).azimuth (),
                              value);
      return;

    // The three that are not a position. No playing-channel guard on these:
    // a clip moves a channel through the room, it does not turn its filter
    // or its spread -- those are the hand's, and nothing here fights over
    // them. The start-up hold is what keeps them quiet until Core answers.
    case Value::Pot1:
      if (std::isfinite (value))
        _engine.setChannelPot1 (index, value);
      return;

    case Value::Pot2:
      if (std::isfinite (value))
        _engine.setChannelPot2 (index, value);
      return;

    case Value::ThreeD:
      if (std::isfinite (value))
        _engine.setChannelPot3 (index, value);
      return;
    }
}

void
A3MotionUIComponent::repaintMixerPages ()
{
  // Both pages read the same MixerState and either may be the one on screen,
  // so a value from the wire repaints both. The overlay's four strips and the
  // bar's one are the same mixer seen twice; repainting only the visible one
  // would mean the other carried a stale picture until something else
  // happened to touch it.
  if (_mixer)
    _mixer->repaint ();
  if (_mixerStrip)
    _mixerStrip->repaint ();
}

void
A3MotionUIComponent::onMixerChannelValue (int channel, int slot, float value)
{
  // What A3 Core relays back from REAPER for the channel strip. Five of the
  // eight arrive today -- gain, the three bands, volume -- and the strip used
  // to come up at its own defaults and stay there, which is how GAIN and VOL
  // came to read zero on a rig that was making sound.
  if (!std::isfinite (value))
    return;

  if (channel < 0 || channel >= static_cast<int> (_engine.getNumChannels ()))
    return;

  if (slot < 0 || slot >= numMixerControls)
    return;

  auto const control = mixerControlOrder[static_cast<std::size_t> (slot)];

  // setChannelFromPeer, never setChannelFromTouch. The difference is the
  // whole provision MixerState was given for this: a value from a finger is
  // set *and* sent, a value from the wire is set and not sent. Sending it
  // back would have Core report, Motion set, Motion send, Core report --
  // which is the loop a3_core_echo.py exists to suppress, rebuilt from this
  // side and out of its reach.
  _mixerState.setChannelFromPeer (channel, control, value);
  repaintMixerPages ();
}

void
A3MotionUIComponent::onMasterValue (int slot, float value)
{
  // The summing section. Nothing here belonged to a channel, so A3 Core had
  // no way back for any of it until 2026-09-12 and this page showed its own
  // defaults for as long as it existed.
  if (!std::isfinite (value) || slot < 0 || slot >= numMasterControls)
    return;

  _mixerState.setMasterFromPeer (
      masterControlOrder[static_cast<std::size_t> (slot)], value);
  repaintMixerPages ();
}

void
A3MotionUIComponent::onFilterValue (int slot, float value)
{
  // The one filter all four channels share. /fx/mode arrives as a number --
  // 1 is high pass -- which is the spelling this device sends on that same
  // address; the word the desk's LED reads travels /fx/led and never comes
  // here.
  if (!std::isfinite (value) || slot < 0 || slot >= numFilterControls)
    return;

  _mixerState.setFilterFromPeer (
      filterControlOrder[static_cast<std::size_t> (slot)], value);
  repaintMixerPages ();
}

// ── Global Settings helpers ──────────────────────────────────────────────────────

void
A3MotionUIComponent::openGlobalSettings ()
{
  if (_view == AppView::Fpv)
    setView (AppView::Full);
  // The key says where you are. Told here rather than by whoever
  // opened it: there are three ways in (the panel's key, the strip's,
  // the encoder) and a key that only lit for some of them would be
  // worse than one that never lit at all.
  if (_clipSettings)
    _clipSettings->setMenuOpen (true);
  if (_statusBar)
    _statusBar->setMenuOpen (true);
  if (runsOnHardware ())
    updateFunctionKeyLEDs ();

  if (_globalSettingsOpen)
    return;

  _globalSettingsOpen = true;
  _globalSettingsValueFieldSelected = false;
  _globalSettingsOptionIndex = 0;

  rebuildGlobalSettingsOptions ();

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
  add (MenuRow::ButtonLeds, { "Button LEDs", { { "open" } }, 0, true });
  add (MenuRow::PatternFolder, { "Pattern Folder", { { "open" } }, 0, true });
  add (MenuRow::SphereInMenu,
       { "Sphere in Menu", { { "off" }, { "on" } },
         _pauseRenderingInMenu ? 0 : 1 });
  add (MenuRow::DeveloperMode,
       { "Developer Mode", { { "off" }, { "on" } }, _developerMode ? 1 : 0 });
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
  if (_statusBar)
    _statusBar->setMenuOpen (false);
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
    case MenuRow::ButtonLeds:
      openConfigPage ("Button LEDs", { "buttonLeds" });
      break;
    case MenuRow::PatternFolder:
      openConfigPage ("Pattern Folder", { "patternDir" });
      break;
    case MenuRow::SphereInMenu: applyPauseRendering (chosen == 0); break;
    case MenuRow::DeveloperMode: applyDeveloperMode (chosen == 1); break;
    }

  _globalSettings->setActiveValueIndex (_globalSettingsOptionIndex, chosen);

  _globalSettingsValueFieldSelected = false;
  _globalSettings->setValueFieldSelected (false);
}

void
A3MotionUIComponent::applyOscAddresses ()
{
  _oscAddresses = oscAddressesFrom (installedOscTruth ());
  for (auto const &key : missingOscKeys (installedOscTruth ()))
    std::cerr << "ERROR: a3-osc.json has no address '" << key
              << "' -- sending /a3-osc-missing/" << key << " instead"
              << std::endl;

  // The engine's backend sends on its own thread and picks these up there;
  // the message handler receives on this one and can take them directly.
  _engine.setOscAddresses (_oscAddresses);

  if (_oscMessageHandler)
    _oscMessageHandler->setAddresses (_oscAddresses);
  _beatArrival.setAddress (_oscAddresses.beatIn);

  // The beat is sent from the tempo-clock thread, so it cannot read the
  // struct this function just replaced.
  {
    std::lock_guard<std::mutex> lock{ _beatAddressMutex };
    _pendingBeatAddress = _oscAddresses.beatOut;
  }
  _beatAddressPending.store (true, std::memory_order_release);

  // How the meters move is Core's to say, like the addresses: the same
  // numbers the desk and StemDeck read, so one burst falls alike on all three.
  _vuLevels.setBallistics (installedOscTruth ().meterBallistics ());
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

  persistSettings ();
}

void
A3MotionUIComponent::applyClockMode (int mode)
{
  if (mode == _clockMode)
    return;

  _clockMode = mode;
  _beatArrival.follow.store (_clockMode != 0);

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

  persistSettings ();
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
  if (_skinPanelOpen || _skinEditorOpen)
    return;

  auto const file = skinFile (getConfigFile ().getParentDirectory (),
                              _skinNames[juce::jlimit (
                                  0, juce::jmax (0, _skinNames.size () - 1),
                                  _skinIndex)]);

  // Through the rename, like every other read of a skin: a file still carrying
  // the old spellings would otherwise show them here while the app runs on the
  // new ones, and the first save would write a mixture.
  _skinPanel->setSkin (migrateSkinNames (juce::JSON::parse (file.loadFileAsString ())),
                       file.getFileNameWithoutExtension ());
  _skinAsOpened = copyOfSkin (_skinPanel->getSkin ());
  _skinPanelOpen = true;
  _globalSettings->setVisible (false);
  _skinPanel->setVisible (true);
  _skinPanel->toFront (true);

  // The sphere moves over into what the panel leaves (sphereRegion).
  _motionComponent->setSphereLeftInset (_skinPanel->getRight ());

  updateOverlayButtons ();
}

void
A3MotionUIComponent::openFullSkinList ()
{
  if (!_skinPanelOpen || _skinEditorOpen)
    return;

  // The same document, handed over: the list edits what the panel holds, and
  // hands back whatever it holds when it closes -- a reset or a rename there
  // replaces the document outright.
  _skinEditor->setSkin (_skinPanel->getSkin (), _skinPanel->getSkinName ());
  _skinEditorOpen = true;
  _skinPanel->setVisible (false);
  _skinEditor->setVisible (true);
  _skinEditor->toFront (true);

  // The list covers the sphere's component as it always did.
  _motionComponent->setSphereLeftInset (0);

  updateOverlayButtons ();
}

void
A3MotionUIComponent::closeSkinPanel ()
{
  if (!_skinPanelOpen)
    return;

  closeColourPicker ();

  // Written on the way out, like the list: a drag produces a value per
  // mouse sample, and a file save per sample would wake the watcher all the
  // way through it.
  saveEditedSkinIfChanged ();

  showKeyboard (false);
  _skinPanelOpen = false;
  _skinPanel->setVisible (false);
  _motionComponent->setSphereLeftInset (0);
  _globalSettings->setVisible (true);
  _globalSettings->toFront (true);

  updateOverlayButtons ();
}

juce::var
A3MotionUIComponent::editedSkin () const
{
  return _skinEditorOpen ? _skinEditor->getSkin () : _skinPanel->getSkin ();
}

juce::String
A3MotionUIComponent::editedSkinName () const
{
  return _skinEditorOpen ? _skinEditor->getSkinName ()
                         : _skinPanel->getSkinName ();
}

juce::Component *
A3MotionUIComponent::skinPageInFront () const
{
  if (_skinEditorOpen)
    return _skinEditor.get ();
  if (_skinPanelOpen)
    return _skinPanel.get ();
  return nullptr;
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
  _configPageTitle = title;
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

  writeTextFile (getConfigFile (),
                 juce::JSON::toString (config, false) + "\n");
  _configPageKeys.clear ();

  // Which page, by its own name. This said "network saved - restart to
  // apply" for every page until 2026-09-30, a leftover of the Network page;
  // the pages left (Button LEDs, Pattern Folder) apply without a restart.
  updateControlReadout (_configPageTitle.toLowerCase () + " saved");
}

ShippedClips
A3MotionUIComponent::shippedClips () const
{
  return _developerMode ? ShippedClips::Writable : ShippedClips::Protected;
}

void
A3MotionUIComponent::applyDeveloperMode (bool on)
{
  _developerMode = on;
  persistSettings ();
  // Both Save keys ask the same rule, so both have to be asked again: a key
  // that stays dark after the switch says developer mode did nothing.
  refreshBrowser ();
  updateActionPage ();
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
          writeTextFile (getConfigFile (),
                         juce::JSON::toString (config, false) + "\n");
        }
    }

  if (_motionComponent)
    _motionComponent->setRenderingPaused (paused);
}

void
A3MotionUIComponent::toggleClean ()
{
  auto const toggle = toggleCleanSkin (
      activeSkinName (getConfigFile ()), _skinBeforeClean,
      availableSkins (getConfigFile ().getParentDirectory ()));
  if (!toggle)
    return;

  _skinBeforeClean = toggle->remember;
  persistSettings ();
  applySkinNamed (toggle->apply);
  refreshCleanKey ();
}

void
A3MotionUIComponent::refreshCleanKey ()
{
  if (!_statusBar)
    return;

  auto const active = activeSkinName (getConfigFile ());
  auto const available
      = toggleCleanSkin (active, _skinBeforeClean,
                         availableSkins (getConfigFile ().getParentDirectory ()))
            .has_value ();
  _statusBar->setCleanState (available, active == cleanSkinName);
}

void
A3MotionUIComponent::applyTheme ()
{
  // Every way a skin comes into force passes here -- the menu, the editor,
  // the CLEAN key, a hand edit picked up by the watcher -- so the key cannot
  // be left saying clean after something else took over.
  refreshCleanKey ();

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
A3MotionUIComponent::openColourPicker (juce::String const &path,
                                       juce::Colour colour)
{
  // colour is already resolved against the theme default for a role the
  // skin file does not name -- see SkinEditorComponent::onColourPicked.
  // Re-reading the raw document here (getSkin()) would answer 0 for such a
  // path and open the picker on black.
  _colourPath = path;
  _colourPicker->setColour (colour, path);
  _colourPickerOpen = true;
  if (auto *page = skinPageInFront ())
    page->setVisible (false);
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
  if (auto *page = skinPageInFront ())
    {
      page->setVisible (true);
      page->toFront (true);
    }

  updateOverlayButtons ();
}

void
A3MotionUIComponent::applyPickedColour ()
{
  if (_colourPath.isEmpty ())
    return;

  // Back into the document as r/g/b: the file keeps saying what it always
  // said, and HSL is only how a person reaches the number.
  // A copy of the var, not of the document: it shares the object the page
  // holds, so the writes below land in what is being edited.
  auto document = editedSkin ();
  auto const colour = _colourPicker->getColour ();

  setSkinValue (document, _colourPath + ".r", colour.getRed (), true);
  setSkinValue (document, _colourPath + ".g", colour.getGreen (), true);
  setSkinValue (document, _colourPath + ".b", colour.getBlue (), true);

  applyEditedSkin ();
}

void
A3MotionUIComponent::showKeyboard (bool shown)
{
  if (!_barKeyboard)
    return;

  _barKeyboard->setVisible (shown);
  // Up, it owns the panel: every button types until HIDE or ESC, and the
  // keys' LEDs say so at once (the pads follow on their next frame).
  if (_ioAdapter)
    _ioAdapter->setKeyboardOwnsPanel (shown);
  updateFunctionKeyLEDs ();
  // Above ACTION and CHMIX, which share its area -- without taking the
  // focus from the field it types into.
  if (shown)
    _barKeyboard->toFront (false);

  refreshKeyboardIcon ();
}

void
A3MotionUIComponent::toggleKeyboard ()
{
  // Always available, whatever is on screen: it types into whatever has the
  // focus.
  auto const wasShown = _barKeyboard && _barKeyboard->isVisible ();
  showKeyboard (!wasShown);

  // Showing it over a row that can be typed says what it is for. A row that
  // is only turned stays that way; the keyboard is then simply up.
  if (!wasShown && _skinEditorOpen && !_skinEditor->isNaming ())
    _skinEditor->beginTypingBrowsedRow ();
}

void
A3MotionUIComponent::placeKeyboard ()
{
  if (!_barKeyboard || !_clipSettings)
    return;

  _barKeyboard->setBounds (_clipSettings->clipContentBounds ());
  _barKeyboard->setMetrics (_clipSettings->barMetrics ());
}

void
A3MotionUIComponent::typeKey (juce::KeyPress const &key)
{
  if (auto *peer = getPeer ())
    peer->handleKeyPress (key);
}

juce::Colour
A3MotionUIComponent::keyboardLetterLed () const
{
  return toColour (theme ().textPrimary)
      .withBrightness (keyboardLetterLedBrightness);
}

bool
A3MotionUIComponent::keyboardShown () const
{
  return _barKeyboard && _barKeyboard->isVisible ();
}

void
A3MotionUIComponent::refreshKeyboardIcon ()
{
  if (!_statusBar)
    return;

  using State = StatusBar::KeyboardState;

  auto const state = _barKeyboard && _barKeyboard->isVisible ()
                         ? State::Shown
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
  writeTextFile (skinFile (configDir, name),
                 juce::JSON::toString (_skinEditor->getSkin (), false) + "\n");

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
  _skinAsOpened = copyOfSkin (_skinEditor->getSkin ());

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
    saveEditedSkinIfChanged ();
  else
    saveConfigPage ();

  closeColourPicker ();
  showKeyboard (false);
  _skinEditorOpen = false;
  _skinEditor->setVisible (false);

  // Opened from the panel's footer: back to the panel, with whatever the
  // list now holds -- a reset or a rename there replaced the document.
  if (_skinPanelOpen)
    {
      _skinPanel->setSkin (_skinEditor->getSkin (),
                           _skinEditor->getSkinName ());
      _skinPanel->setVisible (true);
      _skinPanel->toFront (true);
      _motionComponent->setSphereLeftInset (_skinPanel->getRight ());
      updateOverlayButtons ();
      return;
    }

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
  auto const edited = editedSkin ();

  // Everything the sphere reads from a skin, handed over the way the file
  // watcher hands it over — corona, glow, speaker light, the energy net, blob
  // and sphere size, the recording underlay. Only sphereScale used to come
  // through here, so every other one of those values sat unchanged while its
  // encoder turned and only appeared once the editor was closed. A value you
  // cannot see while you set it is a value you are setting blind.
  if (_motionComponent != nullptr)
    _motionComponent->applyVisualConfig (edited);

  // The rest of the interface catches up on the next tick, and **only once**
  // however many changes arrived in between.
  //
  // This used to be a callAsync per change, which does not coalesce: dragging
  // a colour queued one whole-tree theme apply per mouse sample -- loadTheme
  // reading 144 values, applyThemeToTree walking every component, a repaint
  // of the root and the bar's own re-layout, sixty times a second. The queue
  // could only fall behind the finger, which is what "zäh" was.
  //
  // The sphere is not in here on purpose: applyVisualConfig above is cheap
  // and direct, so what you are watching while you drag still follows the
  // finger at full rate.
  _skinToApply = edited;
  _skinApplyPending = true;
}

void
A3MotionUIComponent::saveEditedSkin ()
{
  auto const edited = editedSkinName ();
  auto const target = skinNameToWriteTo (edited);

  auto const file
      = skinFile (getConfigFile ().getParentDirectory (), target);

  // Rewritten whole, unlike config.json: a skin file is this editor's own
  // output, and its shape is generated rather than hand-arranged.
  writeTextFile (file, juce::JSON::toString (editedSkin (), false) + "\n");

  // The edits branched off the default, so the skin they landed in is the one
  // that should now be in force -- otherwise they would be written and then
  // immediately not shown.
  if (target != edited)
    applySkinNamed (target);

  _skinAsOpened = copyOfSkin (editedSkin ());
}

void
A3MotionUIComponent::saveEditedSkinIfChanged ()
{
  // A look is not an edit. Saving regardless wrote the file on every visit
  // and, on the shipped skin, branched the device off to "custom" (#53).
  if (!sameSkin (_skinAsOpened, editedSkin ()))
    saveEditedSkin ();
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

  persistSettings ();
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
  // Another slot shown is another slot touched: an armed DISCARD goes.
  _pendingTakes.disarm ();
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
  if (_overSphere == SphereOverlay::Files)
    refreshBrowser (BrowserSelection::PointAtTheSlot);

  // And so does the MIX page: it is one channel's strip, and which channel is
  // exactly what has just changed. Unconditionally, unlike the browser --
  // nothing here goes to disk, and a page told only while it is visible comes
  // back showing the channel of whoever was on show last.
  if (_mixerStrip)
    _mixerStrip->setChannel (static_cast<int> (channel));

  refreshTakeState ();
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
    return 4; // clip-bottom, clip-top, sway, elv
  if (menuIndex == ClipSettingsComponent::motionIndex)
    // In reading order: rot, spin, reach, swell, sqzX, strX, sqzY, strY,
    // fade, bias. The two lists went to Shape.
    return 10;
  if (menuIndex == ClipSettingsComponent::trajectoryIndex)
    return 4; // the picture, the clip field, then direction and end action
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

  // A knob playing a lane loses the lane, and keeps the value: what the two
  // taps take away is the recording, and the setting is what plays after it.
  // A take is its own clip, so this is also how a knob pass is thrown away
  // during a take. A knob without a lane goes back to the middle below.
  if (auto const knob = knobAt (section, sub); knob && pattern
                                                && pattern->hasLane (*knob))
    {
      pattern->clearLane (*knob);
      updateControlReadout (juce::String (knobName (*knob)).toUpperCase ()
                            + " LANE CLEARED");
      updateClipSettingsDisplay ();
      return;
    }

  switch (section)
    {
    case 0: // Shape: the turn. The section has one face now, so this knob is
            // always rot -- fade kept its own in Motion.
      if (sub != 1 || !pattern)
        return;

      // No turn at all is this knob's middle: the ring's twelve o'clock and
      // the shape as it was drawn are the same thing.
      pattern->setRotate (0.f);
      break;

    case 1: // Elevation
      if (!pattern)
        return;
      if (sub == 0)
        pattern->setClipBottom (0.f);
      else if (sub == 1)
        pattern->setClipTop (0.f);
      else if (sub == 2)
        // Off. The middle of a bipolar sweep is no sweep at all.
        pattern->setElevationLfo (0);
      else if (sub == 3)
        {
          // elv: the middle of what the clips leave, which is where the line
          // sat before anyone moved it.
          pattern->setElevationBase (defaultElevationBase (
              pattern->getClipTop (), pattern->getClipBottom ()));
          refreshPatternDisplay (pattern);
        }
      else
        return;
      break;

    case 2: // Motion — in reading order, the same numbering the value handler
            // and the painter use.
      if (!pattern)
        return;
      switch (sub)
        {
        // No turn at all is this knob's middle: the ring's twelve o'clock and
        // the shape as it was drawn are the same thing.
        case 0: pattern->setRotate (0.f); break;
        // Off. The middle of a bipolar sweep is no sweep at all, which is
        // also what a hand is reaching for when it double taps one.
        case 1: pattern->setSpin (0); break;
        // Its own direction kept: the reach is a signed size, and a reset
        // that flipped the sign turned the figure upside down.
        case 2: pattern->setReach (defaultReach (pattern->getReach ())); break;
        case 3: pattern->setReachLfo (0); break;
        // The figure as it was recorded.
        case 4: pattern->setSqueezeX (0.f); break;
        case 5: pattern->setSqueezeXLfo (0); break;
        case 6: pattern->setSqueezeY (0.f); break;
        case 7: pattern->setSqueezeYLfo (0); break;
        case 8: pattern->setFadeReach (ClipSettings{}.fadeReach); break;
        case 9:
          pattern->setBridgeBias (0);
          refreshPatternDisplay (pattern);
          break;
        // Upright, and no sweep.
        case 10: pattern->setTilt (0.f); break;
        case 11: pattern->setTiltLfo (0); break;
        case 12: pattern->setRoll (0.f); break;
        case 13: pattern->setRollLfo (0); break;
        default: return;
        }
      break;

    default:
      return;
    }

  updateControlReadout ("-- DEFAULT");
  updateClipSettingsDisplay ();
  // A reset is a value change like any other, and one that is not written is
  // one the next reload undoes -- which reads as the double tap not having
  // worked at all.
  scheduleSetSave ();
}

void
A3MotionUIComponent::setClipSettingsValue (index_t channel, int section,
                                           int sub, double value)
{
  if (channel != _clipSettingsChannel)
    return;

  auto &pattern = _patterns[channel][_clipSettingsSlot];
  if (!pattern)
    return;

  auto const level = static_cast<float> (value);
  auto const step = static_cast<int> (std::lround (value));

  // The Elevation and Motion sections are knobs; what is still a field on
  // this bar arrives as an increment, and so do the encoders.
  if (section == elevationSection)
    switch (sub)
      {
      case 0: pattern->setClipBottom (level); break;
      case 1: pattern->setClipTop (level); break;
      case 2: pattern->setElevationLfo (step); break;
      case 3:
        pattern->setElevationBase (elevationBaseForKnob (
            level, pattern->getClipTop (), pattern->getClipBottom ()));
        break;
      default: return;
      }
  else if (section == motionSection)
    switch (sub)
      {
      case 0: pattern->setRotate (level); break;
      case 1: pattern->setSpin (step); break;
      case 2: pattern->setReach (level); break;
      case 3: pattern->setReachLfo (step); break;
      case 4: pattern->setSqueezeX (level); break;
      case 5: pattern->setSqueezeXLfo (step); break;
      case 6: pattern->setSqueezeY (level); break;
      case 7: pattern->setSqueezeYLfo (step); break;
      case 8: pattern->setFadeReach (level); break;
      case 9: pattern->setBridgeBias (step); break;
      case 10: pattern->setTilt (level); break;
      case 11: pattern->setTiltLfo (step); break;
      case 12: pattern->setRoll (level); break;
      case 13: pattern->setRollLfo (step); break;
      default: return;
      }
  else
    return;

  refreshPatternDisplay (pattern);
  updateClipSettingsDisplay ();
  scheduleSetSave ();
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

  // An encoder has no touch: a step is the hand on the knob for a moment.
  // Stepped from where the knob is drawn, which a lane may have moved.
  if (auto const knob = knobAt (section, sub))
    {
      if (auto const &shown = _patterns[channel][_clipSettingsSlot])
        shown->takeOverKnob (*knob);
      _knobHold.nudge (*knob, juce::Time::getMillisecondCounterHiRes ());
      pushKnobHolds ();
    }

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
            // The clip field: the clips, each bringing the figure it names
            // and every value it carries. applyClip() is the same way in the
            // browser uses, so a clip means one thing however it is reached.
            auto const held
                = _patternLibrary->indexForClipFile (_slotClipFile[channel][slot]);
            auto const next = stepThroughLibrary (held, increment, true);
            if (next > 0)
              applyClip (channel, slot, next);
            break;
          }

        if (sub == 2)
          {
            params.direction
                = (params.direction + increment % value::numDirections
                   + value::numDirections)
                  % value::numDirections;
            applyMotionMode (channel, slot);
            break;
          }

        if (sub == 3)
          {
            // A drag on END while it says Clip walks the clip that follows;
            // otherwise it steps the end action, as a tap does -- see
            // handleClipSettingsToggle(), which is where a tap lands.
            if (params.endAction == static_cast<int> (EndAction::Clip))
              {
                stepFollowClip (channel, increment);
                break;
              }
            stepEndAction (channel, slot, increment);
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
        // And the knobs it plays, stretched onto the new figure.
        auto const heldLanes = pattern ? pattern->getLanes () : KnobLanes{};

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

        // A new shape in the slot replaces an unsaved take in it.
        dropPendingTake (channel, slot);

        if (newIndex == 0)
          {
            pattern = nullptr;
          }
        else
          {
            pattern = createPatternForIndex (newIndex, channel);
            if (pattern)
              {
                applyClipSettings (*pattern, held);
                applyLanes (*pattern, heldLanes);
              }
            refreshPatternDisplay (pattern);

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
    case 1: // Elevation — clip-bottom (0), clip-top (1), sway (2), elv (3).
            // The clips in the order they stand, because the floor is on the
            // left of a room drawn from the side. elv is where the middle of
            // the trajectory sits (the graphic's touch until 2026-09-26) and
            // sway is how fast that line travels.
      {
        auto &pattern = _patterns[channel][slot];
        if (!pattern)
          break;

        switch (sub)
          {
          case 0:
            pattern->setClipBottom (pattern->getClipBottom ()
                                    + increment * 0.05f);
            break;
          case 1:
            pattern->setClipTop (pattern->getClipTop () + increment * 0.05f);
            break;
          case 3:
            // elv, stepped the way the knob turns it: clockwise is higher.
            // A detent always moves it: the snap onto the poles and ear
            // height is as wide as one step, and used to pull every step back.
            pattern->setElevationBase (elevationBaseForEncoderStep (
                pattern->getElevationBase (), increment,
                pattern->getClipTop (), pattern->getClipBottom ()));
            refreshPatternDisplay (pattern);
            break;
          case 2:
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
    case 2: // Motion — in reading order: rot (0), spin (1), reach (2),
            // swell (3), sqzX (4), strX (5), sqzY (6), strY (7), fade (8),
            // bias (9). Renumbered when the two lists left for Shape:
            // renumbering a section means moving the layout, this handler,
            // the reset handler and the painter together -- see ARCHITECTURE.md --
            // and everything here had to move anyway.
      {
        auto &pattern = _patterns[channel][slot];
        if (!pattern)
          break;

        auto const stepped = [increment] (int step) {
          return std::clamp (step + increment, -lfoMaxStep, lfoMaxStep);
        };
        auto const sweepSaid = [this] (char const *what, int step) {
          // A step is an index into a table of powers of two and says nothing
          // to read, so the readout says the cycle it stands for.
          updateControlReadout (juce::String (what) + " "
                                + sweepReadout (step));
        };

        switch (sub)
          {
          case 0:
            // The standing angle the spin adds to -- both are summed at the
            // one place that turns anything.
            pattern->setRotate (pattern->getRotate () + increment * 0.02f);
            break;

          case 1:
            pattern->setSpin (stepped (pattern->getSpin ()));
            sweepSaid ("spin", pattern->getSpin ());
            break;

          case 2:
            // How far the trajectory's outer edge lands from the base the
            // elevation graphic sets.
            pattern->setReach (pattern->getReach () + increment * 0.05f);
            break;

          case 3:
            pattern->setReachLfo (stepped (pattern->getReachLfo ()));
            sweepSaid ("swell", pattern->getReachLfo ());
            break;

          case 4:
          case 6:
            {
              // The two squeezes. A tenth per step, which is a twentieth of
              // their ring: these run -1..1 where reach and the clips run
              // 0..1, so the same number would move half as far under the
              // same finger.
              auto const amount = 0.1f * static_cast<float> (increment);
              if (sub == 4)
                pattern->setSqueezeX (pattern->getSqueezeX () + amount);
              else
                pattern->setSqueezeY (pattern->getSqueezeY () + amount);
            }
            break;

          case 5:
            pattern->setSqueezeXLfo (stepped (pattern->getSqueezeXLfo ()));
            sweepSaid ("strX", pattern->getSqueezeXLfo ());
            break;

          case 7:
            pattern->setSqueezeYLfo (stepped (pattern->getSqueezeYLfo ()));
            sweepSaid ("strY", pattern->getSqueezeYLfo ());
            break;

          case 8:
            // How much of the take the joins take over. A twentieth per step,
            // the same as reach and the clips: it runs 0..1 like they do, and
            // a knob that needs two and a half times the finger for the same
            // travel reads as a different kind of control.
            //
            // A reading of the movement, not a change to it: nothing is
            // written into the ticks, so it can be turned down as freely as
            // up.
            pattern->setFadeReach (pattern->getFadeReach ()
                                   + 0.05f * static_cast<float> (increment));
            refreshPatternDisplay (pattern);
            break;

          case 9:
            // Where a drawn-through gap leads. Whole steps: nine positions,
            // and a finger should feel each one rather than slide past them.
            pattern->setBridgeBias (pattern->getBridgeBias () + increment);
            refreshPatternDisplay (pattern);
            break;

          case 10:
          case 12:
            {
              // The two leans, a tenth of a quarter turn per step like the
              // squeezes' tenth: rings of -2..2 that the setters wrap.
              auto const amount = 0.1f * static_cast<float> (increment);
              if (sub == 10)
                pattern->setTilt (pattern->getTilt () + amount);
              else
                pattern->setRoll (pattern->getRoll () + amount);
            }
            break;

          case 11:
            pattern->setTiltLfo (stepped (pattern->getTiltLfo ()));
            sweepSaid ("tswp", pattern->getTiltLfo ());
            break;

          case 13:
            pattern->setRollLfo (stepped (pattern->getRollLfo ()));
            sweepSaid ("rswp", pattern->getRollLfo ());
            break;

          default:
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

  // END is the one left: its tap steps the end action, while its drag walks
  // the follow when the end is Clip -- a tap arriving as a drag's first step
  // could not be told from that walk. See tapTogglesValue().
  if (section == ClipSettingsComponent::trajectoryIndex && sub == 3)
    stepEndAction (channel, _clipSettingsSlot, 1);
}

void
A3MotionUIComponent::stepEndAction (index_t channel, index_t slot,
                                    int increment)
{
  auto &params = _clipUIParams[channel][slot];
  params.endAction = (params.endAction + increment % value::numEndActions
                      + value::numEndActions)
                     % value::numEndActions;
  applyMotionMode (channel, slot);

  // Clip with nothing to hand over to is a stop, and the field says so; the
  // readout says how to give it something.
  if (params.endAction == static_cast<int> (EndAction::Clip))
    {
      armFollowClips ();
      if (followShownFor (channel).isEmpty ())
        updateControlReadout ("-- CLIP: DRAG END TO CHOOSE WHICH");
    }

  updateClipSettingsDisplay ();
  scheduleSetSave ();
}

SphereCamera
A3MotionUIComponent::sphereCamera () const
{
  return _motionComponent ? _motionComponent->getCamera () : SphereCamera{};
}

/** How far every channel's clip has got, gathered fresh, for the faces'
 *  progress bars.
 *
 *  Walked rather than remembered: a channel's clip can be stopped by an end
 *  action, by another slot being fired, or by the engine reaching the end of
 *  a one-shot, and none of those routes passes through here. Reading the
 *  patterns each time is what keeps a bar from being left filled on a
 *  channel that has already finished. */
void
A3MotionUIComponent::updateChannelProgress ()
{
  if (!_clipSettings)
    return;

  std::array<float, numChannelColumns> positions;
  positions.fill (-1.f);

  for (index_t channel = 0;
       channel < _patterns.size () && channel < (index_t)numChannelColumns;
       ++channel)
    for (index_t slot = 0; slot < _patterns[channel].size (); ++slot)
      {
        auto const &pattern = _patterns[channel][slot];
        if (pattern != nullptr
            && pattern->getStatus () == Pattern::Status::Playing)
          {
            positions[(size_t)channel] = pattern->getLapProgress ();
            break;
          }
      }

  _clipSettings->setChannelProgress (positions);
}

void
A3MotionUIComponent::chooseChannelFace (index_t channel, bool mayTurnOver)
{
  // Touching the face you are already on turns it over; reaching for its pot
  // does not. (PADS was the page where a face brought CLIP back instead; it
  // lies over the sphere since 2026-09-27, and every page left describes one
  // clip.)
  // Reaching for a pot on the face already shown chooses nothing again
  // (#64): selectClip() describes the bar afresh and the ACTION page lists
  // its files from disk, all before the drag's first step.
  auto const faceIsShown = channel == _clipSettingsChannel
                           && _clipSettingsSlot == _channelSlot[channel];
  if (!mayTurnOver && faceIsShown)
    return;

  if (mayTurnOver && channel == _clipSettingsChannel)
    _channelSlot[channel]
        = static_cast<index_t> ((_channelSlot[channel] + 1) % numPadSlots);

  selectClip (channel, _channelSlot[channel]);
}

void
A3MotionUIComponent::dragSpeedKey (int index, int increment)
{
  if (index < 0 || index >= numSpeedButtons || increment == 0)
    return;
  _pendingTakes.disarm ();
  refreshTakeState ();

  auto &carried = _speedButtonLog2[static_cast<size_t> (index)];
  auto const moved = draggedSpeedLog2 (carried, increment);
  if (moved == carried)
    return;

  carried = moved;
  _clipSettings->setSpeedButtons (_speedButtonLog2);
  persistSettings ();
  scheduleSetSave ();
  applySpeedLog2ToShownClip (moved);
}

void
A3MotionUIComponent::chooseSpeedKey (int index)
{
  if (index < 0 || index >= numSpeedButtons)
    return;
  _pendingTakes.disarm ();
  refreshTakeState ();

  applySpeedLog2ToShownClip (_speedButtonLog2[static_cast<size_t> (index)]);
}

EncoderClicks &
A3MotionUIComponent::encoderClicksOfPage ()
{
  static EncoderClicks none{};
  none = {};
  if (_barPage == BarPage::Motion)
    return _encoderClicksMotion;
  if (_barPage == BarPage::Record)
    return _encoderClicksRecord;
  return none;
}

void
A3MotionUIComponent::showEncoderMarks ()
{
  if (_clipSettings)
    _clipSettings->setEncoderMarks (
        encoderMarks (_barPage, encoderClicksOfPage ()));
}

EncoderTarget
A3MotionUIComponent::encoderTargetAt (int column, int row)
{
  return encoderTarget (
      _barPage, column, row,
      encoderClicksOfPage ()[static_cast<size_t> (column)]
                            [static_cast<size_t> (row)],
      // FPV has no page to edit, so the encoders act as with SHIFT: their own
      // column's FREQ/Q, never the hidden FULL page.
      isButtonPressed (Button::Shift) || _view == AppView::Fpv);
}

void
A3MotionUIComponent::handleEncoderTurn (int column, int row, int increment)
{
  if (auto const cursor = cursorKeyOfEncoder (keyboardShown (), column, row,
                                               increment))
    {
      for (int step = 0; step < std::abs (increment); ++step)
        typeKey (*cursor);
      return;
    }

  auto const target = encoderTargetAt (column, row);
  auto const shown = _clipSettingsChannel;

  switch (target.kind)
    {
    case EncoderTarget::Kind::None:
      return;

    case EncoderTarget::Kind::Control:
      // The rec mode is a key, stepped the way pressing it steps it.
      if (target.section == ClipSettingsComponent::globalIndex
          && target.sub == 0)
        {
          auto const count = static_cast<int> (recMenuModes.size ());
          auto const step = increment > 0 ? 1 : count - 1;
          applyRecMode ((recMenuIndex (_recMode) + step) % count);
          updateClipSettingsDisplay ();
          return;
        }
      handleClipSettingsValueChange (shown, target.section, target.sub,
                                     increment);
      return;

    case EncoderTarget::Kind::Speed:
      dragSpeedKey (target.speed, increment);
      return;

    case EncoderTarget::Kind::Mixer:
      {
        auto const channel = static_cast<int> (shown);
        auto const value = juce::jlimit (
            0.f, 1.f,
            _mixerState.channelValue (channel, target.mixer)
                + 0.02f * static_cast<float> (increment));
        _mixerState.setChannelFromTouch (channel, target.mixer, value);
        _mixer->syncControls ();
        _mixerStrip->syncControls ();
        updateControlReadout ("CH" + juce::String (channel + 1) + " "
                              + mixerControlLabel (target.mixer) + " "
                              + juce::String (value, 2));
        return;
      }

    case EncoderTarget::Kind::MixerKey:
      // A key is pressed, not turned -- see handleEncoderPress().
      return;

    case EncoderTarget::Kind::ActionList:
      {
        // A highlight, not an assignment: the press puts it on the button.
        auto const name = _action->moveListCursor (increment);
        updateControlReadout ("-- "
                              + (name.isEmpty () ? juce::String ("NO ACTION")
                                                 : name.toUpperCase ()));
        return;
      }

    case EncoderTarget::Kind::ActionKey:
      _action->moveKeyRing (increment);
      return;

    case EncoderTarget::Kind::ActionTile:
      _action->switchTile ();
      return;

    case EncoderTarget::Kind::ActionValue:
      _action->turnMarkedValue (target.sub, increment);
      return;

    case EncoderTarget::Kind::ActionButton:
      chooseActionButton (_chosenActionButton[shown] + increment);
      updateControlReadout (
          "A" + juce::String (_chosenActionButton[shown] + 1));
      return;

    case EncoderTarget::Kind::ColumnChannelPot:
      {
        auto const channel = static_cast<index_t> (column);
        handleChannelValueChange (channel, target.pot, increment);
        updateControlReadout ("CH" + juce::String (channel + 1) + " "
                              + channelPotLabel (target.pot) + " "
                              + juce::String (channelPotValue (channel,
                                                               target.pot),
                                              2));
        return;
      }
    }
}

void
A3MotionUIComponent::handleEncoderPress (int column, int row)
{
  disarmOnOtherInput ();

  // Nothing to click or choose in FPV: those belong to FULL's hidden page.
  if (_view == AppView::Fpv)
    return;

  // A click switches what the encoder turns, where there are two things
  // under it -- MOTION's rows, REC's fade|bias -- and says which it is now.
  if (encoderPressClicks (_barPage, column, row)
      && !isButtonPressed (Button::Shift))
    {
      auto &clicked = encoderClicksOfPage ()[static_cast<size_t> (column)]
                                            [static_cast<size_t> (row)];
      clicked = !clicked;
      showEncoderMarks ();
      persistSettings ();
      auto const target = encoderTargetAt (column, row);
      auto const spec = target.section == elevationSection
                            ? elevationKnobSpec (target.sub)
                            : motionKnobSpec (target.sub);
      updateControlReadout (juce::String ("-- ") + spec.label);
      return;
    }

  // A press on a length chooses it, as a tap does; on CUE or FX it flips it.
  auto const target = encoderTargetAt (column, row);
  if (target.kind == EncoderTarget::Kind::Speed)
    chooseSpeedKey (target.speed);
  // ACTION (2026-09-28): enc 2 assigns what it walked to, enc 3 presses the
  // ringed key, enc 4 switches the card, enc 5..8 mark the card's next row.
  if (target.kind == EncoderTarget::Kind::ActionList)
    _action->chooseListCursor ();
  if (target.kind == EncoderTarget::Kind::ActionKey)
    _action->pressKeyRing ();
  if (target.kind == EncoderTarget::Kind::ActionTile)
    _action->switchTile ();
  if (target.kind == EncoderTarget::Kind::ActionValue)
    _action->stepValueRow ();
  if (target.kind == EncoderTarget::Kind::MixerKey)
    {
      auto const channel = static_cast<int> (_clipSettingsChannel);
      auto const on = !_mixerState.channelToggle (channel, target.mixer);
      _mixerState.setChannelFromTouch (channel, target.mixer, on ? 1.f : 0.f);
      _mixer->syncControls ();
      _mixerStrip->syncControls ();
      updateControlReadout ("CH" + juce::String (channel + 1) + " "
                            + mixerControlLabel (target.mixer)
                            + (on ? " ON" : " OFF"));
    }
}

void
A3MotionUIComponent::pushKnobHolds ()
{
  auto const &shown = _patterns[_clipSettingsChannel][_clipSettingsSlot];
  auto const previous = _knobHoldPattern.lock ();

  if (previous != shown)
    {
      // Nobody's hand is on a clip that is not on show. The hands themselves
      // stay: a finger on a knob is on whatever clip that knob now shows --
      // which is how a take started under a held knob records it.
      if (previous)
        for (int k = 0; k < numKnobs; ++k)
          previous->setKnobHeld (static_cast<Knob> (k), false);
      _knobHoldPattern = shown;
    }

  if (!shown)
    return;

  auto const now = juce::Time::getMillisecondCounterHiRes ();
  for (int k = 0; k < numKnobs; ++k)
    {
      auto const knob = static_cast<Knob> (k);
      shown->setKnobHeld (knob, _knobHold.isHeld (knob, now));
    }
}

void
A3MotionUIComponent::updateClipSettingsDisplay ()
{
  // The bar does not exist yet while the set is being restored -- applySet()
  // runs before createMainUI(), so that the slots are already filled when the
  // UI is built. Everything else this reaches from there has this guard
  // (updateActionPage, updateControlReadout, refreshChannelValues); this one
  // did not, and the first set carrying an action segfaulted on startup.
  if (!_clipSettings)
    return;

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

  // The four faces: each channel's own colour and its clip's name, and which
  // of them the bar is describing.
  {
    std::array<juce::Colour, numChannelColumns> colours;
    std::array<juce::String, numChannelColumns> clipNames;
    for (size_t ch = 0; ch < numChannelColumns; ++ch)
      {
        // From the theme where a channel has no state of its own: a literal
        // here is a colour the skin cannot reach, which is what
        // NoColourLiterals exists to stop.
        colours[ch] = ch < _channelUIStates.size ()
                          ? _channelUIStates[ch]->colour
                          : toColour (theme ().textMuted);
        clipNames[ch] = ch < _slotClipFile.size () && !_slotClipFile[ch].empty ()
                            ? _slotClipFile[ch][0].getFileNameWithoutExtension ()
                            : juce::String ();
      }

    _clipSettings->setChannelFaces (colours, clipNames,
                                    static_cast<int> (_clipSettingsChannel));
  }
  // The coverage the hand set, and where the swell is holding it now.
  // The swept value comes from sweptElevation rather than from lfoSweep on
  // its own: the room the base leaves is part of what the sound is using, and
  // an arc drawn off the raw sweep would promise a reach the engine is not
  // playing. Shown while the base is sweeping too, because that is when the
  // two differ.
  // Every knob shows what the clip is playing with: a take's lane turns the
  // knob as a whole, as the hand did when it was recorded, and a small red dot
  // says the recording is doing it (setKnobsLaneDriven below). The arcs are
  // the sweeps' alone, drawn from wherever the knob stands.
  auto const knobOf = [&pattern] (Knob knob, float fallback) {
    return pattern ? pattern->getKnob (knob) : fallback;
  };
  auto const stepOf = [&pattern] (Knob knob, int fallback) {
    return pattern ? pattern->getKnobStep (knob) : fallback;
  };
  auto const swell = stepOf (Knob::Swell, 0);
  auto const sway = stepOf (Knob::Sway, 0);

  _clipSettings->setElevationReach (
      knobOf (Knob::Reach, 0.5f),
      pattern && (swell != 0 || sway != 0)
          ? sweptElevation (pattern->getElevationParams (), *pattern).reach
          : -2.f);
  // The line, and where the sway is holding it now -- the same pair the reach
  // above is given, and drawn the same way.
  _clipSettings->setElevationBase (
      knobOf (Knob::Elevation, 0.f),
      pattern && sway != 0
          ? lfoSweep (pattern->getKnob (Knob::Elevation), sway,
                      pattern->getElevationLfoPhase ())
          : -1.f);
  _clipSettings->setElevationClipTop (knobOf (Knob::ClipTop, 0.f));
  _clipSettings->setElevationClipBottom (knobOf (Knob::ClipBottom, 0.f));

  // The clips clipsToDraw() says -- every playing one, and the selected one
  // as its preview -- in the side-on circle, each with its sound running
  // along it (2026-09-27). The sphere above draws the same set. Mapped by
  // elevationFigureFor(), through exactly the calls the engine plays it
  // through, and projected from where the sphere above is being looked at, a
  // quarter turn behind it.
  {
    ClipGrid running{};
    ClipGrid filled{};
    for (std::size_t c = 0; c < numChannelColumns; ++c)
      for (std::size_t s = 0; s < numPadSlots; ++s)
        if (auto const &p = _patterns[c][s])
          {
            filled[c][s] = true;
            running[c][s] = patternIsRunning (p->getStatus ());
          }

    auto const camera = sphereCamera ();
    std::vector<ElevationChannel> clips;
    for (auto const &drawn : clipsToDraw (running, filled, channel, slot))
      {
        auto const &shown = _patterns[drawn.channel][drawn.slot];
        ElevationChannel into;
        into.colour = _channelUIStates[drawn.channel]->colour;
        into.selected = drawn.selected;
        into.figure = elevationFigureFor (*shown, *_patternLibrary,
                                          _engine.getHeightMap (), camera,
                                          elevationFigureSamples);

        // Only while that clip is the one being heard.
        auto const position = _engine.getChannelPosition (drawn.channel);
        into.headValid = shown->getStatus () == Pattern::Status::Playing
                         && position.isValid ();
        if (into.headValid)
          into.head = elevationSideView (position, camera);

        clips.push_back (std::move (into));
      }

    _clipSettings->setElevationChannels (clips);
    if (_motionComponent)
      _motionComponent->setSelectedPattern (pattern);
    _clipSettings->setSphereCamera (sphereCamera ());
  }

  // Motion/Filter: all sub-controls visible in parallel, like Elevation —
  // _clipSettingsSubIndex only picks which one is highlighted, and only
  // means anything while that section is actually selected.
  //
  // Speed is passed as an already-normalized knob fraction + the same words
  // the keys wear (speedKeyName) rather than the raw speedLog2 value, so
  // ClipSettingsComponent need not invert the range itself.
  // Inverted against the raw range: far left (frac 0) = speedLog2Max
  // ("16", slowest), far right (frac 1) = speedLog2Min ("1/128", fastest).
  auto const clipSpeedLog2 = pattern ? pattern->getSpeedLog2 () : 0;
  auto const speedRange
      = static_cast<float> (speedLog2Max - speedLog2Min);
  auto const speedFrac
      = speedRange > 0.f
            ? (speedLog2Max - clipSpeedLog2) / speedRange
            : 0.f;
  _clipSettings->setMotionSpeed (
      speedFrac,
      speedKeyName (clipSpeedLog2, getPatternLengthBeats (channel, slot)));
  // Read back off the pattern rather than from this table. The pattern is
  // where the engine looks and what the file carries, so a clip that came from
  // disk brings its own settings -- and the bar has to show those, not the
  // ones the last clip happened to leave in the table.
  if (pattern)
    {
      auto &editable = _clipUIParams[channel][slot];
      editable.direction = static_cast<int> (pattern->getPlayDirection ());
      editable.endAction = static_cast<int> (pattern->getEndAction ());
    }

  _clipSettings->setMotionDirection (_clipUIParams[channel][slot].direction);
  _clipSettings->setMotionEndAction (_clipUIParams[channel][slot].endAction,
                                     followShownFor (channel));
  _clipSettings->setMotionFadeReach (
      pattern ? pattern->getFadeReach () : ClipSettings{}.fadeReach);
  _clipSettings->setMotionBridgeBias (
      pattern ? pattern->getBridgeBias () : ClipSettings{}.bridgeBias);
  // Each squeeze, and where its own stretch is holding it now -- the same
  // pair reach is given, drawn the same way.
  auto const stretchX = stepOf (Knob::StretchX, ClipSettings{}.squeezeXLfo);
  auto const stretchY = stepOf (Knob::StretchY, ClipSettings{}.squeezeYLfo);
  _clipSettings->setMotionSqueeze (
      knobOf (Knob::SqueezeX, ClipSettings{}.squeezeX),
      knobOf (Knob::SqueezeY, ClipSettings{}.squeezeY),
      pattern && stretchX != 0
          ? lfoSweepBipolar (pattern->getKnob (Knob::SqueezeX), stretchX,
                             pattern->getSqueezeXLfoPhase ())
          : -2.f,
      pattern && stretchY != 0
          ? lfoSweepBipolar (pattern->getKnob (Knob::SqueezeY), stretchY,
                             pattern->getSqueezeYLfoPhase ())
          : -2.f);
  {
    std::array<bool, numKnobs> driven{};
    std::array<bool, numKnobs> writing{};
    if (pattern)
      for (int k = 0; k < numKnobs; ++k)
        {
          auto const knob = static_cast<Knob> (k);
          driven[static_cast<std::size_t> (k)]
              = pattern->getKnobPlayed (knob).has_value ();
          writing[static_cast<std::size_t> (k)] = pattern->isKnobWriting (knob);
        }
    _clipSettings->setKnobsLaneDriven (driven);
    _clipSettings->setKnobsWriting (writing);
  }
  _clipSettings->setMotionStretch (stretchX, stretchY);
  {
    // The two leans, and where each one's sweep is holding it now -- the
    // same pair the squeezes are given, drawn the same way.
    auto const tiltSweep = stepOf (Knob::TiltSweep, ClipSettings{}.tiltLfo);
    auto const rollSweep = stepOf (Knob::RollSweep, ClipSettings{}.rollLfo);
    auto const turn = pattern ? spaceTurnOf (*pattern) : SpaceTurn{};
    _clipSettings->setMotionLean (
        knobOf (Knob::Tilt, ClipSettings{}.tilt),
        knobOf (Knob::Roll, ClipSettings{}.roll),
        tiltSweep != 0 ? std::optional<float> (turn.tilt) : std::nullopt,
        rollSweep != 0 ? std::optional<float> (turn.roll) : std::nullopt,
        tiltSweep, rollSweep);
  }
  _clipSettings->setSweeps (stepOf (Knob::Spin, ClipSettings{}.spin), swell,
                            sway);
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
  _clipSettings->setShapeRotate (knobOf (Knob::Rotate, 0.f),
                                 pattern ? turnsOf (*pattern) : 0.f);

  _clipSettings->setBeatsPerBar (_engine.getBeatsPerBar ());

  // What the speed keys are a ratio of. Without it they cannot say how many
  // ticks they would run, because that is a property of the take, not of the
  // key -- see speedKeyName().
  _clipSettings->setPatternLengthBeats (
      getPatternLengthBeats (channel, slot));
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
    auto const status = pattern != nullptr ? pattern->getStatus ()
                                          : Pattern::Status::Empty;
    auto const isPlayingThis = status == Pattern::Status::Playing;

    // Pressed, and waiting for the beat to come round. Both ends of the
    // transport land on TempoClock::nextBeat(), so this is the window after
    // either a start or a stop -- up to half a second in which nothing has
    // happened yet, which without a sign reads as a key that did not work.
    // Waiting, in all three of its shapes: waiting to start, waiting to stop
    // on the beat, and -- the long one -- playing out a lap it has been asked
    // to finish. The last keeps the status Playing, because it *is* playing,
    // so the engine has to be asked separately or a press would leave no mark
    // at all for up to a whole phrase.
    auto const isScheduledThis
        = status == Pattern::Status::ScheduledForPlaying
          || status == Pattern::Status::ScheduledForIdle
          || (status == Pattern::Status::Playing
              && _engine.isStoppingAtEnd (channel));


    if (_statusBar)
      {
        // The fill is the take and only the take: one recording runs at a
        // time and it writes over something that does not come back, so it
        // keeps a shape of its own rather than becoming a fifth mark to
        // count in the dark. It shows the take wherever it runs, not only
        // when its clip is the one open (#16), in that channel's colour --
        // which is how the eye tells whose take it is.
        auto const running = _engine.isRecording () ? recording : nullptr;
        auto const armed = _engine.getScheduledForRecordingPattern ();
        auto const takeChannel
            = channelHoldingTake (_patterns, running ? running : armed);

        // Three states from one rule, so the bar and any later reader cannot
        // disagree about which of them is on. See RecordingIndicator.hh.
        auto const indicator
            = takeChannel < 0 ? RecordingIndicator::Off
                              : recordingIndicatorFor (armed != nullptr,
                                                       running != nullptr);
        auto const takeColour
            = takeChannel < 0 ? _channelUIStates[channel]->colour
                              : _channelUIStates[takeChannel]->colour;

        _statusBar->setCountingIn (indicator == RecordingIndicator::CountIn,
                                   takeColour);
        _statusBar->setRecordingProgress (
            indicator == RecordingIndicator::Running
                ? _engine.getRecordingProgress ()
                : -1.f,
            takeColour);

        updateChannelProgress ();
      }

    if (_clipSettings)
      {
        _clipSettings->setTransportState (isPlayingThis, isRecordingThis,
                                          isScheduledThis);
        refreshRecArmed ();

        // The strip's REC light follows the **engine**, not the key that
        // started the take. A recording ends by itself when it reaches its
        // length, and nothing is pressed at that moment -- which is why the
        // light used to stay on until somebody pressed REC again to turn it
        // off. Armed counts as on: the take is committed from the press, and
        // the light saying so before the downbeat is the only warning there is.
        _clipSettings->setRecording (
            _engine.isRecording ()
            || _engine.getScheduledForRecordingPattern () != nullptr);

        // What is still moving, not what is still held -- the accent outlives
        // the finger by its decay, and so does the action's hold on the clip's
        // settings.
        _clipSettings->setActionActive (
            _engine.isChannelAccentActive (channel));
      }
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

}

}
