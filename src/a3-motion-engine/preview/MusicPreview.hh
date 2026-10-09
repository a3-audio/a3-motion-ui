/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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
#pragma once

#include <optional>
#include <string>

namespace a3
{

/** Where the music is, as StemDeck's preview (the `stemdeck.ahead` word)
 *  says. Motion analyses no audio: this is all it knows of the set's
 *  sections; the pilots time their games by it (MusicCue). */
enum class MusicSection
{
  Groove,
  Build,
  Drop,
  Breakdown,
};

/** The words StemDeck sends besides the four sections. */
constexpr char const *noMusicWord = "none"; // no audible deck, or not analysed
constexpr char const *setEndsWord = "end";  // no change before the set ends

/** "groove", "build", "drop", "breakdown"; nothing for any other word. */
std::optional<MusicSection> musicSectionFromWord (std::string const &word);
char const *wordOf (MusicSection section);

struct MusicAhead
{
  MusicSection section{ MusicSection::Groove };
  /** Nothing: the set ends before the music changes. */
  std::optional<MusicSection> next;
  /** Bars from the last downbeat to the change; -1: a loop holds it off. */
  int barsUntilNext{ 0 };
  /** 0-1, the bar's level against the set's loud bars. */
  float energy{ 0.f };
};

/** One `stemdeck.ahead` message's arguments as a preview. Nothing for words StemDeck
 *  does not send -- and for "none", which the caller asks first
 *  (isNoMusic), because it means something: the preview is gone. */
std::optional<MusicAhead> musicAheadFrom (std::string const &section,
                                          std::string const &next,
                                          int barsUntilNext, float energy);
bool isNoMusic (std::string const &section);

/** The last preview and how old it is. StemDeck sends one on every
 *  downbeat, so one older than staleAfterBars bars was not followed by
 *  another: StemDeck is gone, or stopped without saying so, and the preview
 *  counts as absent.
 *
 *  Not thread-safe: the caller serialises access (the OSC receiver hands the
 *  words to the message thread before calling receive()). */
class MusicPreview
{
public:
  static constexpr double staleAfterBars = 4.0;
  /** StemDeck's bars: four beats (its GridEdit::beatsPerBar). */
  static constexpr double beatsPerBar = 4.0;
  /** The tempo an age is counted in while Motion knows none. */
  static constexpr double tempoWhenUnknown = 120.0;

  void receive (MusicAhead const &ahead, double nowSeconds);
  void clear ();

  struct Current
  {
    MusicAhead ahead;
    double ageBars{ 0.0 };
  };

  /** The preview, if there is a fresh one, with its age in bars at `bpm`. */
  std::optional<Current> current (double nowSeconds, double bpm) const;

private:
  std::optional<MusicAhead> _ahead;
  double _receivedAt{ 0.0 };
};

}
