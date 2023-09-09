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

#pragma once

#include <memory>

#include <JuceHeader.h>

#include <a3-motion-engine/backends/SpatBackend.hh>

namespace a3
{

class AsyncCommandQueue : public juce::Thread
{
public:
  AsyncCommandQueue (std::unique_ptr<SpatBackend> backend);
  ~AsyncCommandQueue ();

  void sendPosition (index_t channel, Pos position);
  void sendWidth (index_t channel, float width);
  void sendAmbisonicsOrder (index_t channel, int order);

  void run () override;

private:
  // TODO turn this into a proper union using std::variant
  struct Message
  {
    enum class Command
    {
      SendPosition,
      SendWidth,
      SendAmbisonicsOrder,
    } command;

    index_t channel;
    Pos position;
    float width;
    int order;
  };

  void submitMessage (Message &&message);
  void processFifo ();
  void processMessage (Message const &message);

  static constexpr int fifoSize = 32;
  juce::AbstractFifo _abstractFifo{ fifoSize };
  std::array<Message, fifoSize> _fifo;

  std::unique_ptr<SpatBackend> _backend;
};

}
