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
#include "ScriptLine.hh"

#include <a3-motion-engine/ActionScript.hh>

#include <optional>

namespace a3
{

namespace
{
/** Whether `line` is a `~name` line, and whether it is live. */
std::optional<bool>
assigns (juce::String const &line, juce::String const &name)
{
  auto text = line.trimStart ();
  auto live = true;
  if (text.startsWith ("//"))
    {
      live = false;
      text = text.substring (2).trimStart ();
    }
  if (!text.startsWith ("~" + name))
    return std::nullopt;

  auto const rest = text.substring (name.length () + 1);
  if (rest.isNotEmpty () && rest[0] != '='
      && !juce::CharacterFunctions::isWhitespace (rest[0]))
    return std::nullopt;
  return live;
}

/** The line to change: the last live one, else the last commented one. */
int
lineOf (juce::StringArray const &lines, juce::String const &name, bool liveOnly)
{
  int live = -1;
  int commented = -1;
  for (int i = 0; i < lines.size (); ++i)
    if (auto const found = assigns (lines[i], name))
      (*found ? live : commented) = i;
  return live >= 0 || liveOnly ? live : commented;
}

juce::String
indentOf (juce::String const &line)
{
  return line.substring (0, line.length () - line.trimStart ().length ());
}

/** The line's assignment without a leading `//`, and its comment. */
std::pair<juce::String, juce::String>
partsOf (juce::String const &line)
{
  auto body = line.trimStart ();
  if (body.startsWith ("//"))
    body = body.substring (2).trimStart ();
  auto const at = body.indexOf ("//");
  if (at < 0)
    return { body.trimEnd (), {} };
  return { body.substring (0, at).trimEnd (), body.substring (at) };
}

/** An assignment and its comment, the comment in the scripts' column. */
juce::String
joined (juce::String const &indent, juce::String assignment,
        juce::String const &comment)
{
  if (comment.isEmpty ())
    return indent + assignment;
  while (assignment.length () < scriptAnnotationColumn)
    assignment += " ";
  if (!assignment.endsWithChar (' '))
    assignment += " ";
  return indent + assignment + comment;
}

ActionScriptNote const *
noteFor (juce::String const &name)
{
  for (auto const &note : actionScriptNotes ())
    if (name == note.name)
      return &note;
  return nullptr;
}

/** Where a missing line goes: after the last line of its section. */
int
insertionPoint (juce::StringArray const &lines, ActionScriptNote const *note)
{
  if (note == nullptr)
    return lines.size ();
  auto const header = "// ---- " + juce::String (note->heading) + " ";
  auto at = -1;
  for (int i = 0; i < lines.size (); ++i)
    if (lines[i].startsWith (header))
      at = i;
  if (at < 0)
    return lines.size ();
  auto end = at + 1;
  while (end < lines.size () && lines[end].trim ().isNotEmpty ()
         && !lines[end].startsWith ("// ---- "))
    ++end;
  return end;
}

/** Split without losing whether the file ended in a newline. */
std::pair<juce::StringArray, bool>
linesOf (juce::String const &source)
{
  auto const endsInNewline = source.endsWithChar ('\n');
  auto const text = endsInNewline ? source.dropLastCharacters (1) : source;
  return { juce::StringArray::fromLines (text), endsInNewline };
}

juce::String
textOf (juce::StringArray const &lines, bool endsInNewline)
{
  return lines.joinIntoString ("\n") + (endsInNewline ? "\n" : "");
}
}

juce::String
setScriptLine (juce::String const &source, juce::String const &name,
               juce::String const &written)
{
  auto [lines, endsInNewline] = linesOf (source);
  auto const assignment = "~" + name + " = " + written + ";";

  auto const at = lineOf (lines, name, false);
  if (at >= 0)
    {
      auto const parts = partsOf (lines[at]);
      lines.set (at, joined (indentOf (lines[at]), assignment, parts.second));
      return textOf (lines, endsInNewline);
    }

  auto const *note = noteFor (name);
  auto const comment
      = note == nullptr ? juce::String{} : "// " + scriptAnnotation (*note);
  lines.insert (insertionPoint (lines, note),
                joined ({}, assignment, comment).trimEnd ());
  return textOf (lines, endsInNewline || source.isEmpty ());
}

juce::String
unsetScriptLine (juce::String const &source, juce::String const &name)
{
  auto [lines, endsInNewline] = linesOf (source);
  auto const at = lineOf (lines, name, true);
  if (at < 0)
    return source;

  auto const parts = partsOf (lines[at]);
  lines.set (at, joined (indentOf (lines[at]), "//" + parts.first, parts.second));
  return textOf (lines, endsInNewline);
}

}
