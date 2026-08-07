/*

  A3 Motion — System Pattern Generator

  Standalone CLI tool that generates the factory pattern SVG files.
  Run once (or whenever you want to regenerate); the main UI app
  just reads them from disk.

  Usage:
    a3-pattern-gen [OPTIONS]

  Options:
    -o, --output <dir>   Output directory (default: ../pattern/system
                         relative to the binary, or ./pattern/system)
    -a, --all            Generate all system patterns (default)
    -p, --pattern <name> Generate only the named pattern
    -l, --length <beats> Override length in beats for all patterns
    -r, --radius <float> Radius 0..1 (default 0.8)
    --list               List available pattern names and exit
    -h, --help           Show this help

  Patterns are saved as <beats>_<Name>.svg, sorted alphabetically.

*/

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include <memory>
#include <cstdlib>
#include <random>
#include <ctime>

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/PatternGenerator.hh>
#include <a3-motion-engine/PatternFile.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>

using namespace a3;

// ─── Pattern definition with per-pattern default length ───────────────

struct PatternDef
{
  std::string name;
  int defaultBeats;                                       // good default
  std::function<std::unique_ptr<Pattern> (int, float,
                                          HeightMap const &)> create;
};

static std::vector<PatternDef>
getPatternDefs ()
{
  // clang-format off
  return {
    { "Arc",       8,  [] (int b, float r, auto const &h) { return PatternGenerator::createArc       (b, r, h); }},
    { "Bounce",    8,  [] (int b, float r, auto const &h) { return PatternGenerator::createBounce    (b, r, h); }},
    { "Circle",   16,  [] (int b, float r, auto const &h) { return PatternGenerator::createCircle    (b, r, 360.f, h); }},
    { "Clover",   16,  [] (int b, float r, auto const &h) { return PatternGenerator::createClover    (b, r, h); }},
    { "Corner",    4,  [] (int b, float r, auto const &h) { return PatternGenerator::createCornerStep(b, r, h); }},
    { "Cross",     8,  [] (int b, float r, auto const &h) { return PatternGenerator::createCross     (b, r, h); }},
    { "Diamond",   4,  [] (int b, float r, auto const &h) { return PatternGenerator::createDiamond   (b, r, h); }},
    { "Ellipse",  16,  [] (int b, float r, auto const &h) { return PatternGenerator::createEllipse   (b, r, h); }},
    { "Figure 8", 16,  [] (int b, float r, auto const &h) { return PatternGenerator::createFigureOfEight (b, r, h); }},
    { "Heart",    16,  [] (int b, float r, auto const &h) { return PatternGenerator::createHeart     (b, r, h); }},
    { "Helix",    32,  [] (int b, float r, auto const &h) { return PatternGenerator::createHelix     (b, r, h); }},
    { "Hypo",         32, [] (int b, float r, auto const &h) { return PatternGenerator::createHypo       (b, r, 5.f, 3.f, 5.f, h); }},
    { "Hypo 7-2",     32, [] (int b, float r, auto const &h) { return PatternGenerator::createHypo       (b, r, 7.f, 2.f, 6.f, h); }},
    { "Hypo 8-3",     32, [] (int b, float r, auto const &h) { return PatternGenerator::createHypo       (b, r, 8.f, 3.f, 5.f, h); }},
    { "Epicycloid 3-1", 16, [] (int b, float r, auto const &h) { return PatternGenerator::createEpicycloid (b, r, 3.f, 1.f, 3.f, h); }},
    { "Epicycloid 5-2", 32, [] (int b, float r, auto const &h) { return PatternGenerator::createEpicycloid (b, r, 5.f, 2.f, 4.f, h); }},
    { "Epicycloid 7-3", 32, [] (int b, float r, auto const &h) { return PatternGenerator::createEpicycloid (b, r, 7.f, 3.f, 5.f, h); }},
    { "Infinity",     16, [] (int b, float r, auto const &h) { return PatternGenerator::createInfinity   (b, r, h); }},
    { "Lissajous",    16, [] (int b, float r, auto const &h) { return PatternGenerator::createLissajous  (b, r, 3.f, 2.f, pi<float> () / 2.f, h); }},
    { "Lissajous 1-2",16, [] (int b, float r, auto const &h) { return PatternGenerator::createLissajous  (b, r, 1.f, 2.f, pi<float> () / 2.f, h); }},
    { "Lissajous 2-3",16, [] (int b, float r, auto const &h) { return PatternGenerator::createLissajous  (b, r, 2.f, 3.f, pi<float> () / 2.f, h); }},
    { "Lissajous 3-4",16, [] (int b, float r, auto const &h) { return PatternGenerator::createLissajous  (b, r, 3.f, 4.f, pi<float> () / 2.f, h); }},
    { "Lissajous 3-5",16, [] (int b, float r, auto const &h) { return PatternGenerator::createLissajous  (b, r, 3.f, 5.f, pi<float> () / 2.f, h); }},
    { "Lissajous 5-4",16, [] (int b, float r, auto const &h) { return PatternGenerator::createLissajous  (b, r, 5.f, 4.f, pi<float> () / 2.f, h); }},
    { "Orbit",        32, [] (int b, float r, auto const &h) { return PatternGenerator::createOrbit      (b, r, h); }},
    { "Pendulum",      8, [] (int b, float r, auto const &h) { return PatternGenerator::createPendulum   (b, r, h); }},
    { "Petal",        16, [] (int b, float r, auto const &h) { return PatternGenerator::createPetal      (b, r, h); }},
    { "Random",        8, [] (int b, float r, auto const &h) { return PatternGenerator::createRandom     (b, r, h); }},
    { "Rose",         16, [] (int b, float r, auto const &h) { return PatternGenerator::createRose       (b, r, 3.f, h); }},
    { "Rose 4-Petal", 16, [] (int b, float r, auto const &h) { return PatternGenerator::createRose       (b, r, 2.f, h); }},
    { "Rose 5-Petal", 16, [] (int b, float r, auto const &h) { return PatternGenerator::createRose       (b, r, 5.f, h); }},
    { "Rose 7-Petal", 16, [] (int b, float r, auto const &h) { return PatternGenerator::createRose       (b, r, 7.f, h); }},
    { "Rose 8-Petal", 16, [] (int b, float r, auto const &h) { return PatternGenerator::createRose       (b, r, 4.f, h); }},
    { "Spiral",       32, [] (int b, float r, auto const &h) { return PatternGenerator::createSpiral     (b, r, h); }},
    { "Square",        4, [] (int b, float r, auto const &h) { return PatternGenerator::createSquare     (b, r, h); }},
    { "Star",          8, [] (int b, float r, auto const &h) { return PatternGenerator::createStar       (b, r, h); }},
    { "Triangle",      4, [] (int b, float r, auto const &h) { return PatternGenerator::createTriangle   (b, r, h); }},
    { "Wave",         16, [] (int b, float r, auto const &h) { return PatternGenerator::createWave       (b, r, h); }},
    { "Zigzag",        8, [] (int b, float r, auto const &h) { return PatternGenerator::createZigzag     (b, r, h); }},
  };
  // clang-format on
}

// ─── Helpers ──────────────────────────────────────────────────────────

static void
printUsage ()
{
  std::cout
      << "Usage: a3-pattern-gen [OPTIONS]\n"
      << "\n"
      << "  -o, --output <dir>   Output directory (default: pattern/system)\n"
      << "  -a, --all            Generate all system patterns [default]\n"
      << "  -p, --pattern <name> Generate only the named pattern\n"
      << "  -n, --count <N>      Generate N patterns (picks from pool or random)\n"
      << "  -l, --length <beats> Override length in beats (otherwise per-pattern default)\n"
      << "  -r, --radius <float> Radius 0..1 (default 0.8)\n"
      << "      --random         Generate random patterns (randomised type, radius,\n"
      << "                       length, rotation). Combine with -n for count.\n"
      << "      --seed <int>     RNG seed for --random (default: time-based)\n"
      << "      --list           List available pattern names and exit\n"
      << "  -h, --help           Show this help\n"
      << "\n"
      << "Examples:\n"
      << "  a3-pattern-gen                  # all system patterns\n"
      << "  a3-pattern-gen -n 8             # first 8 system patterns\n"
      << "  a3-pattern-gen --random -n 12   # 12 randomised patterns\n"
      << "  a3-pattern-gen --random -n 5 --seed 42\n";
}

static juce::String
safeFilename (std::string const &name)
{
  return juce::String (name)
      .replaceCharacters (" /\\:*?\"<>|", "__________");
}

// ─── Main ─────────────────────────────────────────────────────────────

int
main (int argc, char *argv[])
{
  // Initialise JUCE without a GUI
  juce::ScopedJuceInitialiser_GUI juceInit;

  std::string outputDir = "/home/aaa/a3-system/a3-motion-ui/pattern/system";
  std::string onlyPattern;
  int overrideLength = 0; // 0 = use per-pattern default
  float radius = 0.8f;
  int count = 0;          // 0 = all
  bool randomMode = false;
  unsigned int seed = static_cast<unsigned int> (std::time (nullptr));

  // Parse args
  for (int i = 1; i < argc; ++i)
    {
      std::string arg = argv[i];
      if ((arg == "-o" || arg == "--output") && i + 1 < argc)
        outputDir = argv[++i];
      else if ((arg == "-p" || arg == "--pattern") && i + 1 < argc)
        onlyPattern = argv[++i];
      else if ((arg == "-n" || arg == "--count") && i + 1 < argc)
        count = std::atoi (argv[++i]);
      else if ((arg == "-l" || arg == "--length") && i + 1 < argc)
        overrideLength = std::atoi (argv[++i]);
      else if ((arg == "-r" || arg == "--radius") && i + 1 < argc)
        radius = static_cast<float> (std::atof (argv[++i]));
      else if (arg == "--random")
        randomMode = true;
      else if (arg == "--seed" && i + 1 < argc)
        seed = static_cast<unsigned int> (std::atoi (argv[++i]));
      else if (arg == "--list")
        {
          for (auto const &d : getPatternDefs ())
            std::cout << "  " << d.name
                      << "  (default " << d.defaultBeats << " beats)\n";
          return 0;
        }
      else if (arg == "-h" || arg == "--help")
        {
          printUsage ();
          return 0;
        }
      else if (arg == "-a" || arg == "--all")
        {
          onlyPattern.clear ();
        }
      else
        {
          std::cerr << "Unknown option: " << arg << "\n";
          printUsage ();
          return 1;
        }
    }

  HeightMapSphere heightMap;
  auto defs = getPatternDefs ();

  // Ensure output directory exists
  juce::File outDir (outputDir);
  outDir.createDirectory ();

  int generated = 0;

  // ── Random mode ─────────────────────────────────────────────────
  if (randomMode)
    {
      int numToGenerate = count > 0 ? count : static_cast<int> (defs.size ());
      std::mt19937 rng (seed);

      std::cout << "Random mode  (seed=" << seed
                << ", count=" << numToGenerate << ")\n";

      // Distributions
      std::uniform_int_distribution<int> typeDist (
          0, static_cast<int> (defs.size ()) - 1);
      std::uniform_real_distribution<float> radiusDist (0.35f, 0.95f);
      std::uniform_real_distribution<float> rotDist (0.f, 360.f);

      // Allowed beat lengths (must be power-of-two friendly)
      std::vector<int> beatChoices = { 4, 8, 16, 32 };
      std::uniform_int_distribution<int> beatIdx (
          0, static_cast<int> (beatChoices.size ()) - 1);

      for (int n = 0; n < numToGenerate; ++n)
        {
          auto const &def = defs[static_cast<std::size_t> (typeDist (rng))];
          int beats
              = overrideLength > 0 ? overrideLength : beatChoices[static_cast<std::size_t> (beatIdx (rng))];
          float r = radiusDist (rng);
          float rotAngle = rotDist (rng);

          auto pattern = def.create (beats, r, heightMap);

          // Name: Rnd_<index>_<type> — set on the pattern itself too (not
          // just the filename below), so the saved SVG's data-name (shown
          // in the UI's Shape browser) matches instead of showing the
          // generator's generic internal name for every random pattern.
          auto const name = juce::String::formatted ("Rnd_%03d_%s", n + 1,
                                                      def.name.c_str ());
          pattern->setName (name.toStdString ());

          // Rotate all ticks by a random angle around the origin
          auto const numTicks = pattern->getNumTicks ();
          float cosA = std::cos (rotAngle * 3.14159265f / 180.f);
          float sinA = std::sin (rotAngle * 3.14159265f / 180.f);
          for (index_t t = 0; t < numTicks; ++t)
            {
              auto pos = pattern->getTick (t);
              float px = pos.x ();
              float py = pos.y ();
              float rx = px * cosA - py * sinA;
              float ry = px * sinA + py * cosA;
              // Clamp to unit disc
              float len = std::sqrt (rx * rx + ry * ry);
              if (len > 1.f)
                {
                  rx /= len;
                  ry /= len;
                }
              pattern->setTick (
                  t, Pos::fromCartesian (
                         rx, ry,
                         heightMap.computeHeight (
                             Pos::fromCartesian (rx, ry, 0.f))));
            }

          auto filename = juce::String::formatted ("%02d_", beats)
                          + safeFilename (name.toStdString ()) + ".svg";
          auto file = outDir.getChildFile (filename);

          if (PatternFile::save (
                  std::shared_ptr<Pattern> (std::move (pattern)), file))
            {
              std::cout << "  " << file.getFullPathName () << std::endl;
              ++generated;
            }
          else
            {
              std::cerr << "  FAILED: " << file.getFullPathName ()
                        << std::endl;
            }
        }

      std::cout << generated << " random patterns saved to " << outputDir
                << std::endl;
      return 0;
    }

  // ── Normal mode ─────────────────────────────────────────────────

  // Filter if --pattern was given
  if (!onlyPattern.empty ())
    {
      defs.erase (
          std::remove_if (defs.begin (), defs.end (),
                          [&] (auto const &d) {
                            return d.name != onlyPattern;
                          }),
          defs.end ());
      if (defs.empty ())
        {
          std::cerr << "Unknown pattern: " << onlyPattern << "\n";
          std::cerr << "Use --list to see available patterns.\n";
          return 1;
        }
    }

  // Limit count if -n was given
  if (count > 0 && count < static_cast<int> (defs.size ()))
    defs.resize (static_cast<std::size_t> (count));

  for (auto const &def : defs)
    {
      int beats = overrideLength > 0 ? overrideLength : def.defaultBeats;
      auto pattern = def.create (beats, radius, heightMap);
      // Distinct family members (e.g. the Lissajous/Rose/Hypo ratio
      // variants) all set the same generic internal name inside their
      // shared generator function — override with this PatternDef's own
      // name so the saved SVG's data-name (shown in the UI) matches.
      pattern->setName (def.name);

      auto filename = juce::String::formatted ("%02d_", beats)
                      + safeFilename (def.name) + ".svg";
      auto file = outDir.getChildFile (filename);

      if (PatternFile::save (std::shared_ptr<Pattern> (std::move (pattern)),
                             file))
        {
          std::cout << "  " << file.getFullPathName () << std::endl;
          ++generated;
        }
      else
        {
          std::cerr << "  FAILED: " << file.getFullPathName () << std::endl;
        }
    }

  std::cout << generated << " patterns saved to " << outputDir << std::endl;
  return 0;
}
