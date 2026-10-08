/*

  A3 Motion UI
  Copyright (C) 2026 Patric Schmitz

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

#include "ActionEditing.hh"

#include <a3-motion-engine/ActionScript.hh>
#include <a3-motion-engine/SplitFolder.hh>

namespace a3
{

std::vector<SlotRef>
slotsFiring (juce::File const &file,
             std::vector<std::vector<juce::File>> const &slotFiles)
{
  std::vector<SlotRef> firing;
  if (file == juce::File{})
    return firing;

  for (index_t channel = 0; channel < slotFiles.size (); ++channel)
    for (index_t slot = 0; slot < slotFiles[channel].size (); ++slot)
      if (slotFiles[channel][slot] == file)
        firing.push_back ({ channel, slot });
  return firing;
}

bool
listWaitsFor (bool scriptHasUnsavedChanges)
{
  return scriptHasUnsavedChanges;
}

std::optional<SlotRef>
takeEditOrigin (std::optional<SlotRef> &editOrigin)
{
  auto const taken = editOrigin;
  editOrigin.reset ();
  return taken;
}

PanelSync
panelSyncFor (juce::File const &chosen, juce::File const &inPanel,
              bool unsaved)
{
  if (chosen == inPanel)
    return PanelSync::Keep;
  return unsaved ? PanelSync::HoldRow : PanelSync::Reload;
}

juce::StringArray
fileErrorsOf (juce::String const &text, juce::File const &file)
{
  if (file.hasFileExtension ("json"))
    {
      juce::var parsed;
      auto const result = juce::JSON::parse (text, parsed);
      return result.failed () ? juce::StringArray{ result.getErrorMessage () }
                              : juce::StringArray{};
    }

  if (file.hasFileExtension ("svg"))
    {
      // JUCE's parser is lenient and hands back an element for a truncated
      // tag, so its error is asked as well as whether anything came back.
      juce::XmlDocument document (text);
      auto const element = document.getDocumentElement ();
      auto const error = document.getLastParseError ();
      if (element == nullptr || error.isNotEmpty ())
        return { error.isNotEmpty () ? error : juce::String{ "not XML" } };
      // Nor does it mind text that stops in the middle of a tag -- which is
      // what a file looks like when typing into it was interrupted.
      if (!text.trimEnd ().endsWithChar ('>'))
        return { "the file ends in the middle of a tag" };
      return {};
    }

  return scriptErrorsOf (text);
}

bool
errorsBlockSaving (juce::StringArray const &errors, juce::File const &file)
{
  return !errors.isEmpty ()
         && (file.hasFileExtension ("json") || file.hasFileExtension ("svg"));
}

juce::StringArray
scriptErrorsOf (juce::String const &script)
{
  // Run against the defaults with a fixed seed: only what is wrong with the
  // text is wanted here, not what it would do to any clip.
  return runActionScript (script, ClipSettings{}, 0).errors;
}

juce::String
copyBaseFor (juce::File const &from, juce::File const &folder,
             juce::String const &loadedSet)
{
  auto const list = folder.getFileName ();
  auto const isSets = list == "sessions";
  if (isSets && loadedSet.isNotEmpty ())
    return loadedSet;

  if (from != juce::File{})
    return from.getFileNameWithoutExtension ();

  // Sets live in sessions/, but a set is called a set -- the same word
  // saveCurrentSession() names a new one with.
  if (isSets)
    return "Set";
  if (list == "clips")
    return "Clip";
  return "Action";
}

bool
isActionScript (juce::File const &file)
{
  return file.hasFileExtension (".scd")
         && !file.getFileNameWithoutExtension ().equalsIgnoreCase ("README");
}

CueTarget
cueClipFor (std::optional<juce::String> const &clip,
            juce::File const &clipsDir)
{
  if (!clip.has_value ())
    return {};
  auto const file = namedFileIn (clipsDir, *clip, ".json");
  if (!file.existsAsFile ())
    return { {}, "no clip called " + *clip };
  return { file, {} };
}

CuePress
cuePressFor (bool isCue, bool clipExists, bool recording, bool takeWaiting)
{
  if (!isCue)
    return CuePress::NotACue;
  if (recording)
    return CuePress::Recording;
  if (takeWaiting)
    return CuePress::TakeWaiting;
  if (!clipExists)
    return CuePress::NoClip;
  return CuePress::Load;
}

ClipSettings
cuedClipSettings (juce::String const &source, ClipSettings const &clip,
                  juce::int64 seed)
{
  return resolveActionAt (source, clip, seed, actionFeelFrom (clip));
}

}
