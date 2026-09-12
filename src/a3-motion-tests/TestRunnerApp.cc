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

#include <gtest/gtest.h>

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

class ScopedMessageThread : public juce::Thread
{
public:
  ScopedMessageThread () : juce::Thread ("message thread")
  {
    startThread ();
    // Waits without a timeout, so there is no outcome to inspect. JUCE 8 gave
    // the no-argument form a default timeout and therefore a bool; JUCE 9
    // makes it its own overload returning void. Calling it bare compiles
    // against both.
    semaphore.wait ();
  }

  ~ScopedMessageThread ()
  {
    juce::MessageManager::getInstance ()->stopDispatchLoop ();
    stopThread (10);
  }

private:
  void
  run () override
  {
    initializer.reset (new juce::ScopedJuceInitialiser_GUI);
    semaphore.signal ();
    juce::MessageManager::getInstance ()->runDispatchLoop ();
  }

  juce::WaitableEvent semaphore;
  std::unique_ptr<juce::ScopedJuceInitialiser_GUI> initializer;
};

int
main (int argc, char **argv)
{
  ScopedMessageThread t;
  ::testing::InitGoogleTest (&argc, argv);
  return RUN_ALL_TESTS ();
}
