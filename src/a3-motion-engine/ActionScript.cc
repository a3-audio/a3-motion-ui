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
#include <a3-motion-engine/TempoLfo.hh>
#include <a3-motion-engine/util/SeedSpread.hh>

#include <algorithm>
#include <functional>
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

juce::StringArray const endActionWords{ "loop", "stop", "pause", "bounce",
                                        "random" };
juce::StringArray const directionWords{ "forward", "reverse" };
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
        s.speedLog2 = clampStep (v.number, -8, 8);
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
    { "reach",
      [] (ClipSettings const &s) { return numberValue (s.reach, false); },
      [] (ClipSettings &s, Value const &v) { s.reach = clampUnit (v.number); } },
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
        return symbolValue (
            directionWords[s.direction == PlayDirection::Reverse ? 1 : 0]);
      },
      [] (ClipSettings &s, Value const &v) {
        s.direction = symbolIndex (v, directionWords) == 1
                          ? PlayDirection::Reverse
                          : PlayDirection::Forward;
      } },
    { "end",
      [] (ClipSettings const &s) {
        return symbolValue (endActionWords[static_cast<int> (s.endAction)]);
      },
      [] (ClipSettings &s, Value const &v) {
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

          field->set (out.settings, value);
        }
      catch (ScriptError const &error)
        {
          out.errors.add ("line " + juce::String (lineNumber) + ": "
                          + error.what);
        }
    }

  return out;
}

juce::String
actionScriptFor (ClipSettings const &settings)
{
  juce::StringArray lines;

  for (auto const &field : fields ())
    {
      auto const value = field.get (settings);

      auto written = value.kind == Value::Kind::Symbol
                         ? (value.symbol == "true" || value.symbol == "false"
                                ? value.symbol
                                : "\\" + value.symbol)
                         : (value.whole
                                ? juce::String (
                                      static_cast<int> (value.number))
                                : juce::String (value.number, 3));

      lines.add ("~" + juce::String (field.name) + " = " + written + ";");
    }

  return lines.joinIntoString ("\n") + "\n";
}

juce::StringArray
actionScriptNames ()
{
  juce::StringArray names;
  for (auto const &field : fields ())
    names.add (field.name);

  names.sort (false);
  return names;
}

}
