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

#include "ActionScript.hh"

#include <a3-motion-engine/Envelope.hh>
#include <a3-motion-engine/SpaceTurn.hh>
#include <a3-motion-engine/TempoLfo.hh>
#include <a3-motion-engine/util/SeedSpread.hh>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace a3
{

namespace
{

/** A value in a script: a number, or one of the words that name a list entry.
 *
 *  Whole and fractional are kept apart the way SuperCollider keeps them, so
 *  `rrand(-4, 4)` gives a step and `rrand(0.2, 0.6)` gives a distance. A
 *  script about `spin` would otherwise have to write `-4.0` and hope. */
struct Value
{
  enum class Kind
  {
    Number,
    Symbol,
  };

  Kind kind = Kind::Number;
  double number = 0.;
  bool whole = true;
  juce::String symbol;
};

Value
numberValue (double n, bool whole)
{
  return { Value::Kind::Number, n, whole, {} };
}

/** Thrown while reading a line; caught at the line's end and turned into one
 *  entry in the result's error list. Nothing escapes runActionScript(). */
struct ScriptError
{
  juce::String what;
};

[[noreturn]] void
fail (juce::String const &what)
{
  throw ScriptError{ what };
}

// ── Reading the text ─────────────────────────────────────────────────────

/** One line's worth of characters, walked left to right.
 *
 *  A hand-written reader rather than a grammar: the language is small enough
 *  that a parser generator would be more machinery than language, and the
 *  error messages a person reads on a device screen are the whole point of
 *  writing it out by hand. */
class Reader
{
public:
  Reader (juce::String const &text) : _text (text) {}

  void
  skipSpace ()
  {
    while (_at < _text.length () && juce::CharacterFunctions::isWhitespace (
                                        _text[_at]))
      ++_at;
  }

  bool
  atEnd ()
  {
    skipSpace ();
    return _at >= _text.length ();
  }

  juce::juce_wchar
  peek ()
  {
    skipSpace ();
    return _at < _text.length () ? _text[_at] : 0;
  }

  juce::juce_wchar
  take ()
  {
    skipSpace ();
    return _at < _text.length () ? _text[_at++] : 0;
  }

  bool
  takeIf (juce::juce_wchar c)
  {
    if (peek () != c)
      return false;

    ++_at;
    return true;
  }

  void
  expect (juce::juce_wchar c)
  {
    if (!takeIf (c))
      fail (juce::String ("expected '") + juce::String::charToString (c)
            + "'");
  }

  /** Letters and digits, for a name or a word. */
  juce::String
  takeWord ()
  {
    skipSpace ();

    auto const from = _at;
    while (_at < _text.length ()
           && (juce::CharacterFunctions::isLetterOrDigit (_text[_at])
               || _text[_at] == '_'))
      ++_at;

    return _text.substring (from, _at);
  }

  /** A number, and whether it was written with a point. */
  Value
  takeNumber ()
  {
    skipSpace ();

    auto const from = _at;
    auto sawPoint = false;
    while (_at < _text.length ()
           && (juce::CharacterFunctions::isDigit (_text[_at])
               || _text[_at] == '.'))
      {
        sawPoint = sawPoint || _text[_at] == '.';
        ++_at;
      }

    if (_at == from)
      fail ("expected a number");

    return numberValue (_text.substring (from, _at).getDoubleValue (),
                        !sawPoint);
  }

  /** Text between double quotes -- a clip's name, the only text there is. */
  juce::String
  takeString ()
  {
    expect ('"');
    auto const from = _at;
    while (_at < _text.length () && _text[_at] != '"')
      ++_at;
    if (_at >= _text.length ())
      fail ("the text has no closing \"");
    auto const text = _text.substring (from, _at);
    ++_at;
    return text;
  }

private:
  juce::String _text;
  int _at = 0;
};

// ── What a name is ───────────────────────────────────────────────────────

/** One settable value: how to read it out of a ClipSettings and how to put it
 *  back. Written out once here rather than as a switch in three places --
 *  reading a name, assigning to one, and writing the whole lot back out as a
 *  script are the same list seen from three sides. */
struct Field
{
  char const *name;
  std::function<Value (ClipSettings const &)> get;
  std::function<void (ClipSettings &, Value const &)> set;
};

int
clampStep (double v, int lo, int hi)
{
  return juce::jlimit (lo, hi, static_cast<int> (std::llround (v)));
}

float
clampUnit (double v)
{
  return juce::jlimit (0.f, 1.f, static_cast<float> (v));
}

/** A control whose middle is zero and whose ends are the same distance
 *  either side of it -- the two squeezes. */
float
clampBipolar (double v)
{
  return juce::jlimit (-1.f, 1.f, static_cast<float> (v));
}

/** A symbol expected out of a fixed list, or a clear complaint naming the
 *  list -- "\sideways is not one of \loop \stop \pause \bounce \random" tells
 *  a person what to type next, which "bad value" does not. */
int
symbolIndex (Value const &v, juce::StringArray const &words)
{
  if (v.kind != Value::Kind::Symbol)
    fail ("expected one of \\" + words.joinIntoString (" \\"));

  auto const at = words.indexOf (v.symbol);
  if (at < 0)
    fail ("\\" + v.symbol + " is not one of \\"
          + words.joinIntoString (" \\"));

  return at;
}

// In the enums' order. Bounce and random are directions since 2026-09-26;
// the end still accepts them from older scripts -- see its setter.
juce::StringArray const endActionWords{ "loop", "stop", "pause", "clip" };
juce::StringArray const directionWords{ "forward", "reverse", "bounce",
                                        "random" };
juce::StringArray const actModeWords{ "oneshot", "hold" };

Value
symbolValue (juce::String const &s)
{
  return { Value::Kind::Symbol, 0., true, s };
}

bool
truth (Value const &v)
{
  if (v.kind == Value::Kind::Symbol)
    {
      if (v.symbol == "true")
        return true;
      if (v.symbol == "false")
        return false;
    }

  fail ("expected true or false");
}

Value
boolValue (bool b)
{
  return symbolValue (b ? "true" : "false");
}

std::vector<Field> const &
fields ()
{
  auto const step = [] (int lo, int hi) { return std::pair<int, int>{ lo, hi }; };
  juce::ignoreUnused (step);

  static std::vector<Field> const list{
    { "speedLog2",
      [] (ClipSettings const &s) { return numberValue (s.speedLog2, true); },
      [] (ClipSettings &s, Value const &v) {
        s.speedLog2 = clampStep (v.number, speedLog2Min, speedLog2Max);
      } },
    { "rotate",
      [] (ClipSettings const &s) { return numberValue (s.rotate, false); },
      [] (ClipSettings &s, Value const &v) {
        // A rotation comes round to itself, so it wraps rather than clamps --
        // 1.2 is a fifth of a turn past the top, not "as far as it goes".
        auto const turns = static_cast<float> (v.number);
        s.rotate = turns - std::floor (turns);
      } },
    // How the figure is squeezed in its own plane. X is front-back, Y
    // left-right; the screen mirrors both, so sqzX is what you see as the
    // vertical -- see PlaneShaping.
    { "sqzX",
      [] (ClipSettings const &s) { return numberValue (s.squeezeX, false); },
      [] (ClipSettings &s, Value const &v) {
        s.squeezeX = clampBipolar (v.number);
      } },
    { "sqzY",
      [] (ClipSettings const &s) { return numberValue (s.squeezeY, false); },
      [] (ClipSettings &s, Value const &v) {
        s.squeezeY = clampBipolar (v.number);
      } },
    // Each squeeze's own sweep, the way swell is reach's.
    { "strX",
      [] (ClipSettings const &s) {
        return numberValue (s.squeezeXLfo, true);
      },
      [] (ClipSettings &s, Value const &v) {
        s.squeezeXLfo = clampStep (v.number, -lfoMaxStep, lfoMaxStep);
      } },
    { "strY",
      [] (ClipSettings const &s) {
        return numberValue (s.squeezeYLfo, true);
      },
      [] (ClipSettings &s, Value const &v) {
        s.squeezeYLfo = clampStep (v.number, -lfoMaxStep, lfoMaxStep);
      } },
    // How far the figure's plane is leant in the room, and each lean's sweep.
    // A lean is an angle on a ring, so it wraps like ~rotate: 3 quarter turns
    // is -1, not "as far as it goes".
    { "tilt",
      [] (ClipSettings const &s) { return numberValue (s.tilt, false); },
      [] (ClipSettings &s, Value const &v) {
        s.tilt = wrappedLean (static_cast<float> (v.number));
      } },
    { "roll",
      [] (ClipSettings const &s) { return numberValue (s.roll, false); },
      [] (ClipSettings &s, Value const &v) {
        s.roll = wrappedLean (static_cast<float> (v.number));
      } },
    { "tswp",
      [] (ClipSettings const &s) { return numberValue (s.tiltLfo, true); },
      [] (ClipSettings &s, Value const &v) {
        s.tiltLfo = clampStep (v.number, -lfoMaxStep, lfoMaxStep);
      } },
    { "rswp",
      [] (ClipSettings const &s) { return numberValue (s.rollLfo, true); },
      [] (ClipSettings &s, Value const &v) {
        s.rollLfo = clampStep (v.number, -lfoMaxStep, lfoMaxStep);
      } },
    { "reach",
      [] (ClipSettings const &s) { return numberValue (s.reach, false); },
      // -1..1 like the clip's own: negative spreads towards the ceiling
      // (#50 -- it was cut to 0..1).
      [] (ClipSettings &s, Value const &v) { s.reach = clampBipolar (v.number); } },
    { "clipTop",
      [] (ClipSettings const &s) { return numberValue (s.clipTop, false); },
      [] (ClipSettings &s, Value const &v) {
        s.clipTop = clampUnit (v.number);
      } },
    { "clipBottom",
      [] (ClipSettings const &s) { return numberValue (s.clipBottom, false); },
      [] (ClipSettings &s, Value const &v) {
        s.clipBottom = clampUnit (v.number);
      } },
    { "base",
      [] (ClipSettings const &s) { return numberValue (s.elevationBase, false); },
      [] (ClipSettings &s, Value const &v) {
        s.elevationBase = clampUnit (v.number);
      } },
    { "mirrorSouth",
      [] (ClipSettings const &s) { return boolValue (s.mirrorSouth); },
      [] (ClipSettings &s, Value const &v) { s.mirrorSouth = truth (v); } },
    { "flat", [] (ClipSettings const &s) { return boolValue (s.flat); },
      [] (ClipSettings &s, Value const &v) { s.flat = truth (v); } },
    { "flatElevation",
      [] (ClipSettings const &s) {
        return numberValue (s.flatElevation, false);
      },
      [] (ClipSettings &s, Value const &v) {
        s.flatElevation = clampUnit (v.number);
      } },
    { "spin", [] (ClipSettings const &s) { return numberValue (s.spin, true); },
      [] (ClipSettings &s, Value const &v) {
        s.spin = clampStep (v.number, -lfoMaxStep, lfoMaxStep);
      } },
    { "swell",
      [] (ClipSettings const &s) { return numberValue (s.reachLfo, true); },
      [] (ClipSettings &s, Value const &v) {
        s.reachLfo = clampStep (v.number, -lfoMaxStep, lfoMaxStep);
      } },
    // What swell is to reach: the elevation base's own slow sweep.
    { "sway",
      [] (ClipSettings const &s) {
        return numberValue (s.elevationLfo, true);
      },
      [] (ClipSettings &s, Value const &v) {
        s.elevationLfo = clampStep (v.number, -lfoMaxStep, lfoMaxStep);
      } },
    { "attack",
      [] (ClipSettings const &s) {
        return numberValue (s.envelopeAttack, true);
      },
      [] (ClipSettings &s, Value const &v) {
        s.envelopeAttack = clampStep (v.number, 0, envelopeMaxStep);
      } },
    { "decay",
      [] (ClipSettings const &s) {
        return numberValue (s.envelopeDecay, true);
      },
      [] (ClipSettings &s, Value const &v) {
        s.envelopeDecay = clampStep (v.number, 0, envelopeMaxStep);
      } },
    { "envelopeMax",
      [] (ClipSettings const &s) { return numberValue (s.envelopeMax, false); },
      [] (ClipSettings &s, Value const &v) {
        s.envelopeMax = clampUnit (v.number);
      } },
    { "freqAttack",
      [] (ClipSettings const &s) { return numberValue (s.freqAttack, true); },
      [] (ClipSettings &s, Value const &v) {
        s.freqAttack = clampStep (v.number, 0, envelopeMaxStep);
      } },
    { "freqDecay",
      [] (ClipSettings const &s) { return numberValue (s.freqDecay, true); },
      [] (ClipSettings &s, Value const &v) {
        s.freqDecay = clampStep (v.number, 0, envelopeMaxStep);
      } },
    { "freqMax",
      [] (ClipSettings const &s) { return numberValue (s.freqMax, false); },
      [] (ClipSettings &s, Value const &v) {
        s.freqMax = clampUnit (v.number);
      } },
    { "qAttack",
      [] (ClipSettings const &s) { return numberValue (s.qAttack, true); },
      [] (ClipSettings &s, Value const &v) {
        s.qAttack = clampStep (v.number, 0, envelopeMaxStep);
      } },
    { "qDecay",
      [] (ClipSettings const &s) { return numberValue (s.qDecay, true); },
      [] (ClipSettings &s, Value const &v) {
        s.qDecay = clampStep (v.number, 0, envelopeMaxStep);
      } },
    { "qMax",
      [] (ClipSettings const &s) { return numberValue (s.qMax, false); },
      [] (ClipSettings &s, Value const &v) { s.qMax = clampUnit (v.number); } },
    { "act",
      [] (ClipSettings const &s) {
        return symbolValue (actModeWords[s.actMode == ActMode::Hold ? 1 : 0]);
      },
      [] (ClipSettings &s, Value const &v) {
        s.actMode = symbolIndex (v, actModeWords) == 1 ? ActMode::Hold
                                                       : ActMode::OneShot;
      } },
    { "dir",
      [] (ClipSettings const &s) {
        return symbolValue (directionWords[static_cast<int> (s.direction)]);
      },
      [] (ClipSettings &s, Value const &v) {
        s.direction
            = static_cast<PlayDirection> (symbolIndex (v, directionWords));
      } },
    { "end",
      [] (ClipSettings const &s) {
        return symbolValue (endActionWords[static_cast<int> (s.endAction)]);
      },
      [] (ClipSettings &s, Value const &v) {
        // An older script's \bounce or \random as the end: the direction it
        // meant, looping -- the translation a file gets too.
        if (v.kind == Value::Kind::Symbol
            && (v.symbol == "bounce" || v.symbol == "random"))
          {
            auto const mode = playbackModeFromNames ("", v.symbol);
            s.direction = mode.direction;
            s.endAction = mode.endAction;
            return;
          }

        s.endAction
            = static_cast<EndAction> (symbolIndex (v, endActionWords));
      } },
    { "fade",
      [] (ClipSettings const &s) { return numberValue (s.fadeReach, false); },
      [] (ClipSettings &s, Value const &v) {
        s.fadeReach = clampUnit (v.number);
      } },
    { "bias",
      [] (ClipSettings const &s) {
        return numberValue (s.bridgeBias, true);
      },
      [] (ClipSettings &s, Value const &v) {
        s.bridgeBias = clampStep (v.number, -4, 4);
      } },
  };

  return list;
}

Field const *
findField (juce::String const &name)
{
  for (auto const &field : fields ())
    if (name == field.name)
      return &field;

  return nullptr;
}

// ── Working an expression out ────────────────────────────────────────────

class Evaluator
{
public:
  Evaluator (Reader &reader, ClipSettings const &so_far, juce::Random &random)
      : _reader (reader), _settings (so_far), _random (random)
  {
  }

  /** Lowest binding: a chain of + and -. */
  Value
  expression ()
  {
    auto left = product ();

    for (;;)
      {
        if (_reader.takeIf ('+'))
          left = combine (left, product (), 1.);
        else if (_reader.takeIf ('-'))
          left = combine (left, product (), -1.);
        else
          return left;
      }
  }

private:
  /** Tighter: a chain of * and /. */
  Value
  product ()
  {
    auto left = term ();

    for (;;)
      {
        if (_reader.takeIf ('*'))
          {
            auto const right = term ();
            left = numberValue (number (left) * number (right),
                                left.whole && right.whole);
          }
        else if (_reader.takeIf ('/'))
          {
            auto const right = term ();
            auto const divisor = number (right);
            if (divisor == 0.)
              fail ("divided by zero");

            // Always fractional: a division that came out whole by luck would
            // make the same script mean different things on different days.
            left = numberValue (number (left) / divisor, false);
          }
        else
          return left;
      }
  }

  Value
  term ()
  {
    auto const c = _reader.peek ();

    if (c == '-')
      {
        _reader.take ();
        auto const inner = term ();
        return numberValue (-number (inner), inner.whole);
      }

    if (c == '(')
      {
        _reader.take ();
        auto const inner = expression ();
        _reader.expect (')');
        return inner;
      }

    if (c == '~')
      {
        _reader.take ();
        auto const name = _reader.takeWord ();
        auto const *field = findField (name);
        if (field == nullptr)
          fail ("no such name: ~" + name);

        return field->get (_settings);
      }

    if (c == '\\')
      {
        _reader.take ();
        return symbolValue (_reader.takeWord ());
      }

    if (c == '[')
      return list ();

    if (juce::CharacterFunctions::isDigit (c))
      return _reader.takeNumber ();

    if (juce::CharacterFunctions::isLetter (c))
      return word ();

    if (c == '"')
      fail ("text goes on ~clip only; everything else is a number or a "
            "\\word");

    fail ("expected a value");
  }

  /** A list, which exists only to be chosen from. Anything else you might do
   *  with one needs a language this is deliberately not. */
  Value
  list ()
  {
    _reader.expect ('[');

    std::vector<Value> entries;
    if (_reader.peek () != ']')
      do
        entries.push_back (expression ());
      while (_reader.takeIf (','));

    _reader.expect (']');
    _reader.expect ('.');

    auto const method = _reader.takeWord ();
    if (method != "choose")
      fail ("a list can only be .choose'd, not ." + method);

    if (entries.empty ())
      fail ("nothing to choose from");

    return entries[static_cast<size_t> (
        _random.nextInt (static_cast<int> (entries.size ())))];
  }

  Value
  word ()
  {
    auto const name = _reader.takeWord ();

    if (name == "true" || name == "false")
      return symbolValue (name);

    if (name == "rrand")
      {
        _reader.expect ('(');
        auto const lo = expression ();
        _reader.expect (',');
        auto const hi = expression ();
        _reader.expect (')');

        // Two whole numbers give a whole number, as they do in
        // SuperCollider: spin is a step, and rrand(-4, 4) has to be able to
        // say so without writing -4.0 and hoping.
        auto const from = std::min (number (lo), number (hi));
        auto const to = std::max (number (lo), number (hi));

        if (lo.whole && hi.whole)
          return numberValue (
              std::floor (from)
                  + _random.nextInt (
                      static_cast<int> (std::floor (to) - std::floor (from))
                      + 1),
              true);

        return numberValue (from + _random.nextDouble () * (to - from), false);
      }

    fail ("no such function: " + name);
  }

  Value
  combine (Value const &left, Value const &right, double sign)
  {
    return numberValue (number (left) + sign * number (right),
                        left.whole && right.whole);
  }

  double
  number (Value const &v)
  {
    if (v.kind != Value::Kind::Number)
      fail ("\\" + v.symbol + " is a word, not a number");

    return v.number;
  }

  Reader &_reader;
  ClipSettings const &_settings;
  juce::Random &_random;
};

}

// ── The whole file ───────────────────────────────────────────────────────

ActionScriptResult
runActionScript (juce::String const &source, ClipSettings const &current,
                 juce::int64 seed)
{
  ActionScriptResult out;
  out.settings = current;

  juce::Random random (spreadSeed (seed));

  auto lineNumber = 0;
  for (auto const &raw : juce::StringArray::fromLines (source))
    {
      ++lineNumber;

      // Everything after // is for the person reading, not for us.
      auto line = raw.upToFirstOccurrenceOf ("//", false, false).trim ();
      if (line.isEmpty ())
        continue;

      try
        {
          Reader reader (line);

          if (!reader.takeIf ('~'))
            fail ("a line sets a name, so it starts with ~");

          auto const name = reader.takeWord ();

          // A Cue (library v2): the clip it puts on the channel, by name.
          // Not a ClipSettings field -- it says which clip, not how one plays.
          if (name == "clip")
            {
              reader.expect ('=');
              if (reader.peek () != '"')
                fail ("~clip takes a clip's name in quotes");
              auto const clip = reader.takeString ();
              reader.takeIf (';');
              if (!reader.atEnd ())
                fail ("more on the line than one assignment");
              out.clip = clip;
              continue;
            }

          // What fires when the accent is over (2026-09-29): a button of the
          // channel by its number. Not a ClipSettings field -- it says what
          // comes next, not how this one plays.
          if (name == "then")
            {
              reader.expect ('=');
              Evaluator evaluator (reader, out.settings, random);
              auto const value = evaluator.expression ();
              reader.takeIf (';');
              if (!reader.atEnd ())
                fail ("more on the line than one assignment");
              if (value.kind != Value::Kind::Number || !value.whole
                  || value.number < 1 || value.number > numActionButtons)
                fail ("~then takes a button, 1..6");
              out.then = static_cast<int> (value.number) - 1;
              out.assigned.addIfNotAlreadyThere (name);
              continue;
            }

          auto const *field = findField (name);
          if (field == nullptr)
            fail ("no such name: ~" + name);

          reader.expect ('=');

          // Against what the script has made of things so far, not against
          // what came in: two lines about one value read top to bottom like
          // every other line in the file.
          Evaluator evaluator (reader, out.settings, random);
          auto const value = evaluator.expression ();

          reader.takeIf (';');
          if (!reader.atEnd ())
            fail ("more on the line than one assignment");

          auto const directionBefore = out.settings.direction;
          field->set (out.settings, value);

          out.assigned.addIfNotAlreadyThere (name);
          // An older script's \bounce or \random as the end sets the
          // direction it meant, so that is set too.
          if (out.settings.direction != directionBefore)
            out.assigned.addIfNotAlreadyThere ("dir");
        }
      catch (ScriptError const &error)
        {
          out.errors.add ("line " + juce::String (lineNumber) + ": "
                          + error.what);
        }
    }

  return out;
}


std::vector<ActionScriptNote> const &
actionScriptNotes ()
{
  // **Built from the constants the reader clamps to, not typed out.** This
  // string is not a note to a maintainer: every shipped script carries it as a
  // comment on the parameter's line, so it is the interface, sitting where
  // somebody reads it while typing. It said -8..8 while the reader took -7..4,
  // so a script asking for 8 got 4 with the comment beside it claiming
  // otherwise -- in twenty-six scripts at once.
  //
  // The other ranges in this table are bounds of their own kind (0..1, a list
  // of words) and have no constant to derive from. This one does, and a range
  // that can be derived must be.
  static auto const speedRange
      = std::to_string (speedLog2Min) + ".." + std::to_string (speedLog2Max);

  // The reading order, which is the ACTION page's and the README's rather
  // than the field table's: shape, then where it sits, then how it moves,
  // then what ACT does to it. A performer learns the page and finds the same
  // order in the file.
  static std::vector<ActionScriptNote> const list{
    { "clip", "Cue", "\"name\"",
      "puts that clip on the channel, from the next downbeat" },
    { "speedLog2", "Shape", speedRange.c_str (),
      "how fast, as a power of two; -3 is the 1/8" },

    { "base", "Elevation", "0..1", "where the middle sits; 0 north, 1 south" },
    { "reach", "Elevation", "-1..1",
      "how far it spreads, sign says down or up" },
    { "clipTop", "Elevation", "0..1", "cut this much off the north side" },
    { "clipBottom", "Elevation", "0..1", "the same from the south side" },
    { "flat", "Elevation", "bool", "hold one elevation, ignore the reach" },
    { "flatElevation", "Elevation", "0..1", "the elevation it is held at" },

    { "rotate", "Motion", "0..1", "standing angle in revolutions; it wraps" },
    { "sqzX", "Motion", "-1..1", "squeeze front-back; 0 as recorded" },
    { "sqzY", "Motion", "-1..1", "the same left-right" },
    { "strX", "Motion", "-8..8", "~sqzX's own sweep, out and back" },
    { "strY", "Motion", "-8..8", "the same for ~sqzY" },
    { "tilt", "Motion", "-2..2",
      "the figure's plane leant forward (+) or back" },
    { "roll", "Motion", "-2..2",
      "the figure's plane leant to the left (+) or right" },
    { "tswp", "Motion", "-8..8", "turns ~tilt round, like spin" },
    { "rswp", "Motion", "-8..8", "turns ~roll round, like spin" },
    { "spin", "Motion", "-8..8", "bars per revolution, sign = direction" },
    { "swell", "Motion", "-8..8",
      "sweeps ~reach out of where it sits and back" },
    { "sway", "Motion", "-8..8",
      "sweeps ~base towards the pole the sign says" },
    { "fade", "Motion", "0..1", "how much of the take the joins take over" },
    { "bias", "Motion", "-4..4", "where a drawn-through gap leads" },
    { "dir", "Motion", "", "\\forward \\reverse" },
    { "end", "Motion", "", "\\loop \\stop \\pause \\bounce \\random" },

    { "attack", "Accent", "0..6", "rise while ACT is held, in bar fractions" },
    { "decay", "Accent", "0..6", "fall once ACT is let go" },
    { "envelopeMax", "Accent", "0..1", "the 3d it rises to" },
    { "freqAttack", "Accent", "0..6", "the same for the filter's cutoff" },
    { "freqDecay", "Accent", "0..6", "" },
    { "freqMax", "Accent", "0..1", "0 is off" },
    { "qAttack", "Accent", "0..6", "and for its resonance" },
    { "qDecay", "Accent", "0..6", "" },
    { "qMax", "Accent", "0..1", "0 is off" },
    { "act", "Accent", "", "\\oneshot \\hold" },
    { "then", "Accent", "1..6", "fires that button when the accent is over" },
  };

  return list;
}

namespace
{

/** A number as a script writes it: whole numbers plain, fractions without
 *  the zeros nobody typed. "0.550" reads as three decimals of precision
 *  somebody chose, and none of these values has that. */
juce::String
writtenNumber (Value const &value)
{
  if (value.whole)
    return juce::String (static_cast<int> (value.number));

  auto text = juce::String (value.number, 3);
  while (text.endsWithChar ('0'))
    text = text.dropLastCharacters (1);
  if (text.endsWithChar ('.'))
    text = text.dropLastCharacters (1);

  return text;
}

juce::String
writtenValue (Value const &value)
{
  if (value.kind != Value::Kind::Symbol)
    return writtenNumber (value);

  return value.symbol == "true" || value.symbol == "false"
             ? value.symbol
             : "\\" + value.symbol;
}

Field const *
fieldNamed (juce::String const &name)
{
  for (auto const &field : fields ())
    if (name == field.name)
      return &field;

  return nullptr;
}

/** The body both writers share: every parameter under its heading, the
 *  annotation in one column, and each line either live or commented out.
 *
 *  The column is what makes the file readable at a glance -- ranges that
 *  start at different places are ranges nobody scans. */
juce::String
renderScript (ClipSettings const &settings, bool commented)
{
  juce::StringArray lines;
  juce::String heading;

  for (auto const &note : actionScriptNotes ())
    {
      auto const *field = fieldNamed (note.name);
      auto const isClip = juce::String (note.name) == "clip";
      auto const isThen = juce::String (note.name) == "then";
      if (field == nullptr && !isClip && !isThen)
        continue;

      if (heading != note.heading)
        {
          heading = note.heading;
          if (!lines.isEmpty ())
            lines.add ("");
          lines.add ("// ---- " + heading + " "
                     + juce::String::repeatedString (
                         "-", juce::jmax (1, 64 - heading.length ())));
        }

      // A clip line has no value in a ClipSettings to write: it is offered,
      // commented out, as the line a Cue uncomments.
      auto assignment
          = isClip   ? juce::String ("//~clip = \"\";")
            : isThen ? juce::String ("//~then = 1;")
                     : juce::String (commented ? "//~" : "~") + note.name
                           + " = " + writtenValue (field->get (settings)) + ";";

      while (assignment.length () < scriptAnnotationColumn)
        assignment += " ";

      auto const annotation = scriptAnnotation (note);

      lines.add ((assignment + "// " + annotation).trimEnd ());
    }

  return lines.joinIntoString ("\n") + "\n";
}

}

juce::String
actionScriptTemplate ()
{
  return renderScript (ClipSettings{}, true);
}

juce::String
actionScriptFor (ClipSettings const &settings)
{
  return renderScript (settings, false);
}

juce::String
scriptAnnotation (ActionScriptNote const &note)
{
  auto annotation = juce::String (note.range);
  if (juce::String (note.hint).isNotEmpty ())
    {
      while (annotation.length () < 8)
        annotation += " ";
      annotation += note.hint;
    }
  return annotation;
}

juce::String
writtenSettingFor (ClipSettings const &settings, juce::String const &name)
{
  auto const *field = fieldNamed (name);
  return field == nullptr ? juce::String{} : writtenValue (field->get (settings));
}

juce::StringArray
actionScriptNames ()
{
  juce::StringArray names;
  for (auto const &field : fields ())
    names.add (field.name);
  // Not a setting, but a line a script may write (2026-09-29).
  names.add ("then");

  names.sort (false);
  return names;
}

ClipSettings
resolveActionAt (juce::String const &source, ClipSettings const &base,
                 juce::int64 seed, ActionFeel const &feel)
{
  return withFeel (runActionScript (source, base, seed).settings, feel);
}

ClipSettings
resolveActionAt (juce::String const &source, ClipSettings const &base,
                 juce::int64 seed, MotionOverrides const &motion,
                 ActionFeel const &feel)
{
  return withFeel (
      withMotion (runActionScript (source, base, seed).settings, motion),
      feel);
}

}
