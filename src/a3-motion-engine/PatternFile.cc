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

#include "PatternFile.hh"

#include <a3-motion-engine/tempo/TempoClock.hh>

#include <cmath>

namespace a3
{

bool
PatternFile::save (std::shared_ptr<Pattern> const &pattern,
                   juce::File const &file)
{
  if (!pattern)
    return false;

  auto ticks = pattern->getTicks ();

  auto *ticksArray = new juce::Array<juce::var> ();
  for (auto const &pos : ticks.positions)
    {
      if (!pos.isValid ())
        {
          ticksArray->add (juce::var ()); // null for invalid/jump ticks
        }
      else
        {
          auto *obj = new juce::DynamicObject ();
          obj->setProperty ("x", static_cast<double> (pos.x ()));
          obj->setProperty ("y", static_cast<double> (pos.y ()));
          obj->setProperty ("z", static_cast<double> (pos.z ()));
          ticksArray->add (juce::var (obj));
        }
    }

  auto *root = new juce::DynamicObject ();
  root->setProperty ("name", juce::String (pattern->getName ()));
  root->setProperty ("ticksPerBeat", TempoClock::getTicksPerBeat ());

  // Compute lengthBeats from tick count
  auto const numTicks = ticks.positions.size ();
  auto const lengthBeats
      = static_cast<int> (numTicks)
        / TempoClock::getTicksPerBeat ();
  root->setProperty ("lengthBeats", lengthBeats);

  // Move ticks array into a juce::var
  juce::var ticksVar;
  for (auto &item : *ticksArray)
    ticksVar.append (item);
  delete ticksArray;

  root->setProperty ("ticks", ticksVar);

  auto json = juce::JSON::toString (juce::var (root));

  // Ensure parent directory exists
  file.getParentDirectory ().createDirectory ();

  return file.replaceWithText (json);
}

std::shared_ptr<Pattern>
PatternFile::load (juce::File const &file)
{
  if (!file.existsAsFile ())
    return nullptr;

  auto json = file.loadFileAsString ();
  juce::var data;
  if (juce::JSON::parse (json, data).failed ())
    return nullptr;

  auto name = data["name"].toString ().toStdString ();
  auto lengthBeats = static_cast<int> (data["lengthBeats"]);
  auto ticksPerBeat = static_cast<int> (data["ticksPerBeat"]);
  auto ticksVar = data["ticks"];

  if (!ticksVar.isArray () || lengthBeats <= 0)
    return nullptr;

  auto pattern = std::make_shared<Pattern> ();
  pattern->setName (name);
  pattern->resize (static_cast<index_t> (lengthBeats));

  auto const *ticksArray = ticksVar.getArray ();

  // Handle possible tick count mismatch (file may have different PPQN)
  auto const fileTotalTicks = ticksArray->size ();
  auto const patternTotalTicks
      = static_cast<int> (pattern->getNumTicks ());

  if (ticksPerBeat == TempoClock::getTicksPerBeat ()
      && fileTotalTicks == patternTotalTicks)
    {
      // Direct 1:1 mapping
      for (int i = 0; i < fileTotalTicks; ++i)
        {
          auto const &tickVar = (*ticksArray)[i];
          if (tickVar.isVoid ())
            {
              // null → invalid tick (jump position)
              pattern->setTick (static_cast<index_t> (i), Pos::invalid);
            }
          else
            {
              auto x = static_cast<float> (tickVar["x"]);
              auto y = static_cast<float> (tickVar["y"]);
              auto z = static_cast<float> (tickVar["z"]);
              pattern->setTick (static_cast<index_t> (i),
                                Pos::fromCartesian (x, y, z));
            }
        }
    }
  else
    {
      // Resample: map file ticks to pattern ticks via linear index ratio
      for (int i = 0; i < patternTotalTicks; ++i)
        {
          auto const srcIdx
              = static_cast<int> (
                  static_cast<float> (i) / static_cast<float> (patternTotalTicks)
                  * static_cast<float> (fileTotalTicks));
          auto const clampedIdx = std::min (srcIdx, fileTotalTicks - 1);
          auto const &tickVar = (*ticksArray)[clampedIdx];
          if (tickVar.isVoid ())
            {
              pattern->setTick (static_cast<index_t> (i), Pos::invalid);
            }
          else
            {
              auto x = static_cast<float> (tickVar["x"]);
              auto y = static_cast<float> (tickVar["y"]);
              auto z = static_cast<float> (tickVar["z"]);
              pattern->setTick (static_cast<index_t> (i),
                                Pos::fromCartesian (x, y, z));
            }
        }
    }

  pattern->setStatus (Pattern::Status::Idle);
  return pattern;
}

std::string
PatternFile::peekNameAndTicks (juce::File const &file,
                               std::vector<Pos> &outTicks)
{
  outTicks.clear ();

  if (!file.existsAsFile ())
    return {};

  auto json = file.loadFileAsString ();
  juce::var data;
  if (juce::JSON::parse (json, data).failed ())
    return {};

  auto name = data["name"].toString ().toStdString ();
  auto ticksVar = data["ticks"];

  if (!ticksVar.isArray ())
    return name;

  auto const *ticksArray = ticksVar.getArray ();
  outTicks.reserve (static_cast<size_t> (ticksArray->size ()));

  for (auto const &tickVar : *ticksArray)
    {
      if (tickVar.isVoid ())
        {
          outTicks.push_back (Pos::invalid);
        }
      else
        {
          auto x = static_cast<float> (tickVar["x"]);
          auto y = static_cast<float> (tickVar["y"]);
          auto z = static_cast<float> (tickVar["z"]);
          outTicks.push_back (Pos::fromCartesian (x, y, z));
        }
    }

  return name;
}

}
