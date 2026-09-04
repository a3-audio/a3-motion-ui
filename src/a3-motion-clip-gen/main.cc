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

#include <JuceHeader.h>

#include <a3-motion-engine/ClipFile.hh>
#include <a3-motion-engine/ClipSettings.hh>
#include <a3-motion-engine/Envelope.hh>

#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace a3;

namespace
{

/** Constrained randomness, not uniform randomness.
 *
 *  The whole point of generating presets is that the good ones are found by
 *  ear rather than reasoned out. But a uniform draw across twelve controls
 *  produces mostly rubbish, and rubbish is expensive: every one has to be
 *  listened to before it can be thrown away.
 *
 *  So the ranges below are what a musician would actually reach for, the
 *  extremes are rare rather than as likely as the middle, and the parameters
 *  that belong together move together -- see couple(). What comes out is
 *  twenty things worth hearing rather than a hundred worth skipping.
 */
struct Ranges
{
  /** Speed as a power of two of a bar. The full range runs -7..4, but a
   *  preset at 1/128 of a bar is a blur and one at 16 bars does not appear to
   *  move; both are reachable by hand and neither is a starting point. */
  static constexpr int speedMin = -2;
  static constexpr int speedMax = 2;

  /** How far down the sphere the shape reaches. Under a quarter it is a dot
   *  around the pole whatever the trajectory says. */
  static constexpr float reachMin = 0.25f;
  static constexpr float reachMax = 1.0f;

  /** Spin and swell: mostly still. A preset where everything moves is a
   *  preset you cannot hear the trajectory through. */
  static constexpr int lfoReach = 4;
  static constexpr float lfoChance = 0.45f;

  /** The odd ones. Rare on purpose: they are strong, and something strong in
   *  half of all presets stops being a characteristic. */
  static constexpr float reverseChance = 0.15f;
  static constexpr float flatChance = 0.12f;
  static constexpr float mirrorChance = 0.15f;
  static constexpr float bounceChance = 0.2f;
};

std::mt19937 &
rng ()
{
  static std::mt19937 engine{ std::random_device{}() };
  return engine;
}

bool
chance (float probability)
{
  return std::uniform_real_distribution<float>{ 0.f, 1.f }(rng ())
         < probability;
}

int
between (int lo, int hi)
{
  return std::uniform_int_distribution<int>{ lo, hi }(rng ());
}

float
between (float lo, float hi)
{
  return std::uniform_real_distribution<float>{ lo, hi }(rng ());
}

/** Rounded to something a person could have dialled in, so a generated preset
 *  and a hand-made one look alike in the file. */
float
step (float value, float grain = 0.05f)
{
  return std::round (value / grain) * grain;
}

/** What belongs together moves together.
 *
 *  A fast clip covering the whole dome is mud -- the trajectory arrives
 *  everywhere at once and the ear hears no path. A slow one confined to the
 *  pole is a sound that barely moves. Neither is wrong as a hand setting, but
 *  as a *starting point* both are wasted: the listener throws them away
 *  without learning anything.
 */
void
couple (ClipSettings &s)
{
  if (s.speedLog2 <= -2)
    s.reach = std::min (s.reach, 0.7f);
  if (s.speedLog2 >= 2)
    s.reach = std::max (s.reach, 0.5f);

  // A spin on top of a fast clip is a second fast movement. One at a time.
  if (s.speedLog2 <= -2 && std::abs (s.spin) > 2)
    s.spin = s.spin > 0 ? 2 : -2;

  // Flat means one elevation for the whole shape, so reach says nothing.
  if (s.flat)
    s.reachLfo = 0;
}

ClipSettings
aPreset ()
{
  ClipSettings s;

  s.speedLog2 = between (Ranges::speedMin, Ranges::speedMax);
  s.rotate = step (between (0.f, 1.f), 0.125f);
  s.reach = step (between (Ranges::reachMin, Ranges::reachMax));

  s.spin = chance (Ranges::lfoChance)
               ? between (-Ranges::lfoReach, Ranges::lfoReach)
               : 0;
  s.reachLfo = chance (Ranges::lfoChance)
                   ? between (-Ranges::lfoReach, Ranges::lfoReach)
                   : 0;

  s.flat = chance (Ranges::flatChance);
  if (s.flat)
    s.flatElevation = step (between (0.2f, 0.8f));

  s.mirrorSouth = chance (Ranges::mirrorChance);

  if (chance (0.3f))
    s.clipTop = step (between (0.f, 0.35f));
  if (chance (0.3f))
    s.clipBottom = step (between (0.f, 0.35f));

  s.direction = chance (Ranges::reverseChance) ? PlayDirection::Reverse
                                               : PlayDirection::Forward;
  s.endAction = chance (Ranges::bounceChance) ? EndAction::Bounce
                                              : EndAction::Loop;

  s.envelopeAttack = between (0, envelopeMaxStep);
  s.envelopeDecay = between (0, envelopeMaxStep);
  s.envelopeMax = step (between (0.4f, 1.f));

  s.fadeSixteenths = between (0, 8);

  couple (s);
  return s;
}

/** A name you can read off a list and guess the sound from.
 *
 *  "Gen 7" tells you nothing, and a list of twenty of them is a list you have
 *  to listen through twice -- once to find out what they are, once to choose.
 *  Built from whatever is strongest about the preset instead.
 */
juce::String
describe (ClipSettings const &s)
{
  juce::StringArray words;

  if (s.speedLog2 <= -2)
    words.add ("Rush");
  else if (s.speedLog2 == -1)
    words.add ("Quick");
  else if (s.speedLog2 >= 2)
    words.add ("Drift");
  else if (s.speedLog2 == 1)
    words.add ("Slow");

  if (s.flat)
    words.add ("Flat");
  else if (s.reach >= 0.85f)
    words.add ("Wide");
  else if (s.reach <= 0.4f)
    words.add ("Tight");

  if (std::abs (s.spin) >= 3)
    words.add (s.spin > 0 ? "Whirl" : "Unwind");
  else if (s.spin != 0)
    words.add ("Turn");

  if (std::abs (s.reachLfo) >= 3)
    words.add ("Breathe");
  else if (s.reachLfo != 0)
    words.add ("Sway");

  if (s.direction == PlayDirection::Reverse)
    words.add ("Back");
  if (s.endAction == EndAction::Bounce)
    words.add ("Bounce");
  if (s.mirrorSouth)
    words.add ("Mirror");

  if (words.isEmpty ())
    words.add ("Plain");

  // Two words is a name; four is a description nobody reads in the dark.
  while (words.size () > 2)
    words.remove (words.size () - 1);

  return words.joinIntoString (" ");
}

void
printUsage ()
{
  std::cout
      << "Usage: a3-clip-gen [OPTIONS]\n\n"
      << "Writes clip setting presets -- constrained randomness, to be\n"
      << "listened through and thrown away from. Always writes Default.\n\n"
      << "  -n, --count <n>    how many to generate (default 20)\n"
      << "  -o, --output <dir> where to write (default pattern/clips)\n"
      << "  -h, --help\n";
}

}

int
main (int argc, char **argv)
{
  juce::ScopedJuceInitialiser_GUI juceInit;

  int count = 20;
  juce::File outDir ("/home/aaa/a3-system/a3-motion-ui/pattern/clips");

  for (int i = 1; i < argc; ++i)
    {
      juce::String const arg (argv[i]);
      if ((arg == "-n" || arg == "--count") && i + 1 < argc)
        count = juce::String (argv[++i]).getIntValue ();
      else if ((arg == "-o" || arg == "--output") && i + 1 < argc)
        outDir = juce::File (argv[++i]);
      else
        {
          printUsage ();
          return arg == "-h" || arg == "--help" ? 0 : 1;
        }
    }

  outDir.createDirectory ();

  // Default first and always. Something has to be the thing you go back to
  // when a preset turns out to be wrong for the take, and it must exist
  // whether or not anybody has generated anything.
  Clip fallback;
  fallback.name = "Default";
  if (!ClipFile::save (fallback, outDir.getChildFile ("Default.json")))
    {
      std::cerr << "cannot write Default.json" << std::endl;
      return 1;
    }
  std::cout << "  Default" << std::endl;

  for (int i = 0; i < count; ++i)
    {
      Clip clip;
      clip.settings = aPreset ();
      clip.name = freeClipName (outDir, describe (clip.settings))
                      .toStdString ();

      if (ClipFile::save (clip,
                          outDir.getChildFile (juce::String (clip.name)
                                               + ".json")))
        std::cout << "  " << clip.name << std::endl;
      else
        std::cerr << "  FAILED: " << clip.name << std::endl;
    }

  std::cout << count << " presets written to " << outDir.getFullPathName ()
            << "\nListen through them, keep what works, delete the rest."
            << std::endl;
  return 0;
}
