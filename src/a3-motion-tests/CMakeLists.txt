include(GoogleTest)

juce_add_console_app(a3-motion-tests
    COMPANY_NAME "a3-audio"
    PRODUCT_NAME "a3-motion-tests")
juce_generate_juce_header("a3-motion-tests")
a3_precompile_juce_header("a3-motion-tests")

target_sources("a3-motion-tests" PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/TestRunnerApp.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/PositionPacer.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/FlightField.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/BeatPulse.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/BaseOrbit.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ShipDynamics.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/FlightGravity.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/FlightWorld.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/FlightMotion.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ShipHearing.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/PilotDesk.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/Handover.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/FlightBodiesBox.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/FlightEngine.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/TruthKeeper.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/TempoClock.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/BeatTrace.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/RecordingTrace.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/TakeLaps.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/WindowReport.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/BeatSync.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ShippedCopies.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ExternalTempo.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/MusicPreview.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/Position.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/HeightMapSphere.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/MotionEngine.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SettingsPersistence.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SessionFile.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/PatternDir.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/AppPaths.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/PendingTakes.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/TakeUnderway.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/OscMessageHandler.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/BlobPunch.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/CoronaScaling.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/DragAccumulator.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SpeakerLightScaling.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ConeThinning.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ConfigFileWatcher.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/EnergyMap.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/DiscStepPieces.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SphereCamera.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SphereProjection.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/LineMapGeometry.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/LineMapStrokes.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/LineMapCapsules.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SphereLimb.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ElevationSideView.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/TrajectoryIcon.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/Listener.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/UnderlayLook.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ButtonLedColours.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/LedLoad.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/LedCache.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/TransportKeyGround.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/RecArming.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/KnobLanes.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/TakeBand.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/KnobAutomation.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/KnobHold.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/TakeSeed.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/TakeRecording.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/TakeProjection.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/EncoderMap.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/BarKeyboardLayout.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/BarKeyboardModel.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/PatternDisplay.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SphereMarks.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SpaceTurn.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/PadGlyphInk.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SceneLaunch.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/Theme.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ThemeColours.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/LookAndFeelTheme.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/NoColourLiterals.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/NoMetricLiterals.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/PadStatusColours.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/PatternRunning.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ChannelValueReset.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/CrossingWindow.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/RecordingIndicator.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ClockModeTempo.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/RecordingName.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/LibraryKeys.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ActionEditing.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/PadFunctions.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/PatternShapes.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ShippedLibrary.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ScriptPanelLayout.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ScriptPanel.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SplitFolder.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/TextFile.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/RecordingLength.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/FontScale.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ClipSettingsCaptions.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ClipSettingsLayout.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/BarPages.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/TrajectoryShaping.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/TrajectorySpin.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/TempoLfo.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/Envelope.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/Slew.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/BlobTrail.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/PlasmaSheath.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/BrowserLayout.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ControllerLayout.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ColumnBreak.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/OverlayStrips.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/MenuEditsInAMask.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/FingerLatch.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/FunctionKeys.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/FunctionKeyHold.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/MixerControls.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/MixerState.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/MixerLayout.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/FrameRate.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/FunctionKeyColours.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SkinGroups.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/CleanSkin.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SkinEditorScroll.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SkinSections.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/StemdeckSkin.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SkinPanelLayout.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SkinPanel.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/GlobalSettingsLayout.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/OscAddresses.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/DeviceHello.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/OscEndpoints.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/OscTruth.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/OscTruthContract.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/MasterColumnPaint.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ThemedComponent.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SkinSelection.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SkinParameters.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SkinFiles.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SkinNames.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/TextInput.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SkinEditorTyping.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/OnScreenKeyboard.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/PanelKeyboard.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/PanelOwnership.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/Workspaces.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/TouchGrabs.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ActionLayout.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/BridgeAgreement.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/TrajectoryBridges.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/TrajectoryShape.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/RecMode.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SystemPatternIcons.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SvgPathParsing.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/PatternFileJumps.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/PatternFileTakes.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SvgPathTokens.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/RecordingSpans.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/PatternWrittenTicks.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ClipFile.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ClipMigration.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/PatternFileGeometryOnly.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/PatternLibraryClips.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ClipSettings.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ActionFiring.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/EndActionClip.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/PauseResumes.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/PlayPausePress.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ActionScript.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/PilotOrder.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/FiredAction.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ScriptLine.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ActionMotion.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ActionMotionTile.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ActionEncoders.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ActionWritesScript.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/BlobPush.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ActionChain.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SerialReconnect.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SerialCandidates.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/PatternSpeedAndFade.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/PatternPlayback.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/RecordingSeam.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/PlaybackRate.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/BouncePlayback.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/PlayheadAdvance.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/MotionModePersistence.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/PatternProgressBar.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/VuMeter.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/DeskMeter.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/MeterBallistics.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/WaitUntil.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/MixerHitAreas.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/VuFader.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/PotKnob.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/BarRepaints.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ColourPickerUndo.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ChannelPotTurn.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/FirstSourceOnly.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/CameraFingers.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ChannelFaces.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/BarKnob.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ActionKnobs.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ClipKnobs.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/VuMeterView.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/StatusBarLayout.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/FpvLayout.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/FpvStripsPaint.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/FloorBodies.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/FloorGesture.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/FpvPagePress.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/FpvFloor.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ActionReach.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/BodyLook.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ShipShape.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/AppView.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/FittedFont.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/UnitScheduling.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/MotionIsPureOsc.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/TestsSendNothing.cc"
    )

# The corona visibility test reads the config that actually ships with the app,
# so a tuning change that hides the effect fails the suite rather than the eye.
# The shipped material as committed, not as the working copy holds it: on the
# rig ACTION writes into the scripts and the skin panel into custom.json while
# a set is played. test.sh exports HEAD's config/ and pattern/ here before
# every run (2026-09-29).
set(A3_COMMITTED_DIR "${CMAKE_BINARY_DIR}/committed")

# What the app is built from, for MotionIsPureOsc: the app target's
# definitions and link libraries as CMake ends up with them.
set(A3_UI_BUILD_REPORT "${CMAKE_CURRENT_BINARY_DIR}/a3-motion-ui-build-report.txt")
file(GENERATE OUTPUT "${A3_UI_BUILD_REPORT}" CONTENT
"compile definitions:
$<JOIN:$<TARGET_PROPERTY:a3-motion-ui,COMPILE_DEFINITIONS>,\n>
link libraries:
$<JOIN:$<TARGET_PROPERTY:a3-motion-ui,LINK_LIBRARIES>,\n>
")

target_compile_definitions(a3-motion-tests PRIVATE
    A3_UI_BUILD_REPORT="${A3_UI_BUILD_REPORT}"
    A3_CONFIG_JSON_PATH="${A3_COMMITTED_DIR}/config/config.json"
    A3_UI_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-ui"
    A3_ENGINE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-engine"
    A3_RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../resources"
    A3_PLATFORM_CONFIG_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../platform_config"
    # The four folders the shipped material lives in. Three of them are the
    # system half of a folder split into shipped and hand-made; pattern/system
    # is the shapes' half and predates the split. Written out together so the
    # next folder to be split is one line here, not a test found broken later
    # -- this list has been chased three times already.
    A3_PATTERN_SYSTEM_DIR="${A3_COMMITTED_DIR}/pattern/system"
    A3_PATTERN_USER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../pattern/user"
    # The instrument's own, which now live in the system half of a folder split
    # into shipped and hand-made. The performer's are in user/ beside them and
    # are nobody's to guarantee.
    A3_PATTERN_ACTIONS_DIR="${A3_COMMITTED_DIR}/pattern/actions/system"
    A3_PATTERN_CLIPS_DIR="${A3_COMMITTED_DIR}/pattern/clips/system"
    # The shipped half, like the actions above. The performer's sit in user/
    # beside them and are nobody's to guarantee.
    A3_PATTERN_SESSIONS_DIR="${A3_COMMITTED_DIR}/pattern/sessions/system")

target_include_directories("a3-motion-tests" PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(a3-motion-tests PUBLIC
    # The UI sources under test, compiled once with the app's flags.
    a3-motion-ui-shared
    a3-motion-engine
    # Colour work is most of the theme, and juce::Colour lives here. Without
    # it the LookAndFeel and every colour conversion would be untestable.
    juce::juce_graphics
    # LookAndFeel_V4 and the ColourIds it defines live here.
    juce::juce_gui_basics
    # The code editor the script panel is built on (CodeEditorComponent), so
    # the panel can be tested where it is built rather than only on the rig.
    juce::juce_gui_extra
    gtest
)

gtest_discover_tests(a3-motion-tests)
