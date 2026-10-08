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

#include "SessionFile.hh"

#include <algorithm>

#include <a3-motion-engine/Playhead.hh>
#include <a3-motion-engine/TextFile.hh>


#include <iostream>

namespace a3
{

namespace
{
/** Grown to the shape of the device that is reading it, whatever shape it was
 *  written in. A set from a four-channel device on a six-channel one is four
 *  channels and two empty, not a refusal — hardware outlives file formats. */
void
fitToDevice (Session &set, int numChannels, int numSlots)
{
  set.channels.resize (static_cast<size_t> (juce::jmax (0, numChannels)));
  for (auto &channel : set.channels)
    channel.slots.resize (static_cast<size_t> (juce::jmax (0, numSlots)));
}

/** Only what differs from the defaults is written.
 *
 *  Against the defaults rather than against the clip: the clip can be changed
 *  by somebody else, or renamed, or be missing when the session is opened
 *  somewhere else -- and an override written relative to a moving reference
 *  means something different every time it is read.
 */
juce::var
writeOverrides (a3::ClipSettings const &settings)
{
  auto *object = new juce::DynamicObject ();
  a3::ClipSettings const defaults;

  auto const put = [object] (char const *key, auto value) {
    object->setProperty (key, value);
  };

  if (settings.speedLog2 != defaults.speedLog2)
    put ("speed", settings.speedLog2);
  if (settings.rotate != defaults.rotate)
    put ("rotate", settings.rotate);
  if (settings.squeezeX != defaults.squeezeX)
    put ("sqzX", settings.squeezeX);
  if (settings.squeezeY != defaults.squeezeY)
    put ("sqzY", settings.squeezeY);
  if (settings.squeezeXLfo != defaults.squeezeXLfo)
    put ("strX", settings.squeezeXLfo);
  if (settings.squeezeYLfo != defaults.squeezeYLfo)
    put ("strY", settings.squeezeYLfo);
  if (settings.tilt != defaults.tilt)
    put ("tilt", settings.tilt);
  if (settings.roll != defaults.roll)
    put ("roll", settings.roll);
  if (settings.tiltLfo != defaults.tiltLfo)
    put ("tswp", settings.tiltLfo);
  if (settings.rollLfo != defaults.rollLfo)
    put ("rswp", settings.rollLfo);
  if (settings.reach != defaults.reach)
    put ("reach", settings.reach);
  if (settings.clipTop != defaults.clipTop)
    put ("clipTop", settings.clipTop);
  if (settings.clipBottom != defaults.clipBottom)
    put ("clipBottom", settings.clipBottom);
  if (settings.mirrorSouth != defaults.mirrorSouth)
    put ("mirrorSouth", settings.mirrorSouth);
  // The base goes with it whenever the old flag is written, even at its
  // default. Loading migrates a set that says only mirrorSouth -- south meant
  // a base of 1 -- and a set that says both is a set the migration must not
  // touch. Without this the one state "mirrored, base at the north pole" came
  // back as "base at the south pole", and the slot then read as drifted for
  // ever after.
  if (settings.elevationBase != defaults.elevationBase
      || settings.mirrorSouth != defaults.mirrorSouth)
    put ("elevationBase", settings.elevationBase);
  if (settings.flat != defaults.flat)
    put ("flat", settings.flat);
  if (settings.flatElevation != defaults.flatElevation)
    put ("flatElevation", settings.flatElevation);
  if (settings.spin != defaults.spin)
    put ("spin", settings.spin);
  if (settings.reachLfo != defaults.reachLfo)
    put ("swell", settings.reachLfo);
  if (settings.elevationLfo != defaults.elevationLfo)
    put ("sway", settings.elevationLfo);
  if (settings.envelopeAttack != defaults.envelopeAttack)
    put ("envAttack", settings.envelopeAttack);
  if (settings.envelopeDecay != defaults.envelopeDecay)
    put ("envDecay", settings.envelopeDecay);
  if (settings.envelopeMax != defaults.envelopeMax)
    put ("envMax", settings.envelopeMax);
  if (settings.actMode != defaults.actMode)
    put ("actMode", a3::actModeToName (settings.actMode));
  if (settings.direction != defaults.direction)
    put ("direction", a3::playDirectionToName (settings.direction));
  if (settings.endAction != defaults.endAction)
    put ("endAction", a3::endActionToName (settings.endAction));
  if (settings.freqAttack != defaults.freqAttack)
    put ("freqAttack", settings.freqAttack);
  if (settings.freqDecay != defaults.freqDecay)
    put ("freqDecay", settings.freqDecay);
  if (settings.freqMax != defaults.freqMax)
    put ("freqMax", settings.freqMax);
  if (settings.qAttack != defaults.qAttack)
    put ("qAttack", settings.qAttack);
  if (settings.qDecay != defaults.qDecay)
    put ("qDecay", settings.qDecay);
  if (settings.qMax != defaults.qMax)
    put ("qMax", settings.qMax);
  if (settings.fadeReach != defaults.fadeReach)
    put ("fadeReach", settings.fadeReach);
  if (settings.bridgeBias != defaults.bridgeBias)
    put ("bridgeBias", settings.bridgeBias);

  return juce::var (object);
}

std::optional<a3::ClipSettings>
readOverrides (juce::var const &value)
{
  if (!value.isObject ())
    return {};

  a3::ClipSettings settings;
  auto const read = [&value] (char const *key, auto fallback) {
    auto const v = value.getProperty (key, juce::var ());
    return v.isVoid () ? fallback : static_cast<decltype (fallback)> (v);
  };

  settings.speedLog2 = read ("speed", settings.speedLog2);
  settings.rotate = read ("rotate", settings.rotate);
  settings.squeezeX = read ("sqzX", settings.squeezeX);
  settings.squeezeY = read ("sqzY", settings.squeezeY);
  settings.squeezeXLfo = read ("strX", settings.squeezeXLfo);
  settings.squeezeYLfo = read ("strY", settings.squeezeYLfo);
  settings.tilt = read ("tilt", settings.tilt);
  settings.roll = read ("roll", settings.roll);
  settings.tiltLfo = read ("tswp", settings.tiltLfo);
  settings.rollLfo = read ("rswp", settings.rollLfo);
  settings.reach = read ("reach", settings.reach);
  settings.clipTop = read ("clipTop", settings.clipTop);
  settings.clipBottom = read ("clipBottom", settings.clipBottom);
  settings.mirrorSouth = read ("mirrorSouth", settings.mirrorSouth);
  // Same migration as ClipFile::load(): a set written before the base says
  // only which pole, and south meant a base of 1.
  settings.elevationBase
      = read ("elevationBase",
              settings.mirrorSouth ? 1.f : settings.elevationBase);
  settings.flat = read ("flat", settings.flat);
  settings.flatElevation = read ("flatElevation", settings.flatElevation);
  settings.spin = read ("spin", settings.spin);
  settings.reachLfo = read ("swell", settings.reachLfo);
  settings.elevationLfo = read ("sway", settings.elevationLfo);
  settings.envelopeAttack = read ("envAttack", settings.envelopeAttack);
  settings.envelopeDecay = read ("envDecay", settings.envelopeDecay);
  settings.envelopeMax = read ("envMax", settings.envelopeMax);
  // "filter*" is what a set written before the split calls the cutoff's
  // envelope; the resonance had none to lose. See ClipFile::load().
  settings.freqAttack
      = read ("freqAttack", read ("filterAttack", settings.freqAttack));
  settings.freqDecay
      = read ("freqDecay", read ("filterDecay", settings.freqDecay));
  settings.freqMax = read ("freqMax", read ("filterMax", settings.freqMax));
  settings.qAttack = read ("qAttack", settings.qAttack);
  settings.qDecay = read ("qDecay", settings.qDecay);
  settings.qMax = read ("qMax", settings.qMax);
  settings.fadeReach = read ("fadeReach", settings.fadeReach);
  settings.bridgeBias = read ("bridgeBias", settings.bridgeBias);

  auto const named = [&value] (char const *key) {
    auto const v = value.getProperty (key, juce::var ());
    return v.isVoid () ? juce::String () : v.toString ();
  };

  if (auto const s = named ("actMode"); s.isNotEmpty ())
    settings.actMode = a3::actModeFromName (s);
  // Both through the one reader that knows Bounce and Random were end
  // actions; either left out keeps what the settings already say.
  auto const direction = named ("direction");
  auto const endAction = named ("endAction");
  if (direction.isNotEmpty () || endAction.isNotEmpty ())
    {
      auto const mode = a3::playbackModeFromNames (
          direction.isNotEmpty () ? direction
                                  : a3::playDirectionToName (settings.direction),
          endAction.isNotEmpty () ? endAction
                                  : a3::endActionToName (settings.endAction));
      settings.direction = mode.direction;
      settings.endAction = mode.endAction;
    }

  return settings;
}

}

namespace
{
/** All four or none: a hand-edited list of the wrong length is not
 *  half-applied. */
std::optional<std::array<int, numSpeedButtons> >
readSpeedKeys (juce::var const &value)
{
  auto const *list = value.getArray ();
  if (list == nullptr || list->size () != numSpeedButtons)
    return std::nullopt;

  std::array<int, numSpeedButtons> keys{};
  for (size_t i = 0; i < keys.size (); ++i)
    keys[i] = static_cast<int> ((*list)[static_cast<int> (i)]);
  return keys;
}
}

namespace
{
/** A channel's six actions, or -- in a set written before 2026-09-27, when a
 *  channel had two slots with an action each -- those two as A1 and A2. Read before fitToDevice cuts the
 *  second slot, or its action would be gone. */
void
readActions (Session::Channel &channel, juce::var const &value)
{
  if (auto const *entries = value.getArray ())
    {
      auto const count = juce::jmin (entries->size (), numActionButtons);
      for (int i = 0; i < count; ++i)
        {
          auto const &entry = entries->getReference (i);
          auto &action = channel.actions[static_cast<size_t> (i)];
          action.script = entry["script"].toString ().toStdString ();
        }
      return;
    }

  auto const count = juce::jmin (static_cast<int> (channel.slots.size ()),
                                 numActionButtons);
  for (int i = 0; i < count; ++i)
    {
      auto const &slot = channel.slots[static_cast<size_t> (i)];
      auto &action = channel.actions[static_cast<size_t> (i)];
      action.script = slot.action;
    }
}

juce::var
writeActions (std::array<Session::ActionEntry, numActionButtons> const &actions)
{
  juce::Array<juce::var> entries;
  for (auto const &action : actions)
    {
      auto *entry = new juce::DynamicObject ();
      if (!action.script.empty ())
        entry->setProperty ("script", juce::String (action.script));
      entries.add (juce::var (entry));
    }
  return entries;
}
}

Session
loadSession (juce::File const &file, int numChannels, int numSlots)
{
  Session set;

  // A missing or unreadable set is an empty one. A device somebody has not
  // brought a set to still has to come up, and a stray file on a stick must
  // not be the reason a gig does not start.
  auto const parsed = file.existsAsFile ()
                          ? juce::JSON::parse (file.loadFileAsString ())
                          : juce::var{};

  if (auto const *channels = parsed["channels"].getArray ())
    {
      for (auto const &entry : *channels)
        {
          Session::Channel channel;
          channel.threeD = static_cast<float> (entry["threeD"]);
          channel.freq = static_cast<float> (entry["freq"]);
          channel.q = static_cast<float> (entry["q"]);
          if (entry.hasProperty ("orbit"))
            channel.orbit = static_cast<bool> (entry["orbit"]);

          if (auto const *slots = entry["slots"].getArray ())
            for (auto const &slotEntry : *slots)
              {
                Session::Slot slot;
                slot.patternName
                    = slotEntry["pattern"].toString ().toStdString ();
                slot.recordLengthLog2
                    = static_cast<int> (slotEntry["recordLengthLog2"]);
                slot.clipFile = slotEntry["clip"].toString ().toStdString ();
                slot.action = slotEntry["action"].toString ().toStdString ();
                slot.endClip
                    = slotEntry["endClip"].toString ().toStdString ();
                slot.playing = slotEntry.getProperty ("playing", false);
                slot.overrides = readOverrides (slotEntry["overrides"]);
                channel.slots.push_back (slot);
              }

          readActions (channel, entry["actions"]);
          set.channels.push_back (channel);
        }
    }

  set.name = parsed.getProperty ("name", juce::var ()).toString ().toStdString ();
  set.speedButtonLog2 = readSpeedKeys (parsed["speedKeys"]);

  fitToDevice (set, numChannels, numSlots);

  return set;
}

bool
saveSession (juce::File const &file, Session const &set)
{
  juce::Array<juce::var> channels;

  for (auto const &channel : set.channels)
    {
      auto *entry = new juce::DynamicObject ();
      entry->setProperty ("threeD", channel.threeD);
      entry->setProperty ("freq", channel.freq);
      entry->setProperty ("q", channel.q);
      if (channel.orbit)
        entry->setProperty ("orbit", *channel.orbit);

      juce::Array<juce::var> slots;
      for (auto const &slot : channel.slots)
        {
          auto *slotEntry = new juce::DynamicObject ();
          slotEntry->setProperty ("pattern",
                                  juce::String (slot.patternName));
          slotEntry->setProperty ("recordLengthLog2", slot.recordLengthLog2);

          // Names, not paths: a set is carried between machines, and a path
          // would name a folder that is not there on the next one. Written
          // only when there is one, so a slot filled straight from a shape
          // does not carry two empty strings.
          if (!slot.clipFile.empty ())
            slotEntry->setProperty ("clip", juce::String (slot.clipFile));
          if (!slot.action.empty ())
            slotEntry->setProperty ("action", juce::String (slot.action));
          if (!slot.endClip.empty ())
            slotEntry->setProperty ("endClip", juce::String (slot.endClip));

          // Only when it was. A set full of "playing": false says the same
          // thing as a set that does not mention it, at four times the length.
          if (slot.playing)
            slotEntry->setProperty ("playing", true);

          if (slot.overrides)
            slotEntry->setProperty ("overrides",
                                    writeOverrides (*slot.overrides));
          slots.add (juce::var (slotEntry));
        }
      entry->setProperty ("slots", slots);
      // Only when a button has something: an empty list would tell the next
      // reading that there are no actions, and the slots' own -- a set from
      // before the buttons -- would no longer reach A1 and A2.
      auto const anyAction = std::any_of (
          channel.actions.begin (), channel.actions.end (),
          [] (Session::ActionEntry const &a) {
            return !a.script.empty ();
          });
      if (anyAction)
        entry->setProperty ("actions", writeActions (channel.actions));

      channels.add (juce::var (entry));
    }

  auto *root = new juce::DynamicObject ();
  root->setProperty ("channels", channels);
  if (!set.name.empty ())
    root->setProperty ("name", juce::String (set.name));
  if (set.speedButtonLog2)
    {
      juce::Array<juce::var> keys;
      for (auto const key : *set.speedButtonLog2)
        keys.add (key);
      root->setProperty ("speedKeys", keys);
    }

  if (!file.getParentDirectory ().createDirectory ())
    return false;

  return writeTextFile (file, juce::JSON::toString (juce::var (root)));
}


void
recordFlightModes (MotionEngine const &engine, Session &set)
{
  for (size_t ch = 0; ch < set.channels.size (); ++ch)
    set.channels[ch].orbit
        = engine.getFlightMode (static_cast<index_t> (ch))
          == FlightMode::Orbit;
}

void
restoreFlightModes (Session const &set, MotionEngine &engine)
{
  for (size_t ch = 0; ch < set.channels.size (); ++ch)
    if (auto const &orbit = set.channels[ch].orbit)
      engine.setFlightMode (static_cast<index_t> (ch),
                            *orbit ? FlightMode::Orbit : FlightMode::Clip);
}

bool
migrateSetToCurrent (juce::File const &root)
{
  auto const old = root.getChildFile ("set.json");
  auto const current = root.getChildFile ("current.json");

  if (!old.existsAsFile () || current.existsAsFile ())
    return false;

  if (!old.copyFileTo (current))
    {
      std::cerr << "SessionFile: cannot write " << current.getFullPathName ()
                << std::endl;
      return false;
    }

  std::cout << "SessionFile: the automatic set is now current.json"
            << std::endl;
  return true;
}

bool
migrateTwoSlotSets (juce::File const &root)
{
  auto const backup = root.getChildFile ("backup-two-slots");
  if (backup.exists ())
    return false;

  auto copied = false;
  auto const keep = [&] (juce::File const &file, juce::String const &relative) {
    if (!file.existsAsFile ())
      return;
    auto const target = backup.getChildFile (relative);
    target.getParentDirectory ().createDirectory ();
    copied = file.copyFileTo (target) || copied;
  };

  keep (root.getChildFile ("current.json"), "current.json");
  for (auto const *half : { "system", "user" })
    for (auto const &set : root.getChildFile ("sessions")
                               .getChildFile (half)
                               .findChildFiles (juce::File::findFiles, false,
                                                "*.json"))
      keep (set, juce::String ("sessions/") + half + "/" + set.getFileName ());

  if (copied)
    std::cout << "SessionFile: two-slot sets copied to "
              << backup.getFullPathName () << std::endl;
  return copied;
}

}
