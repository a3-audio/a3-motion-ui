#include <gtest/gtest.h>

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

class ScopedMessageThread : public juce::Thread
{
public:
  ScopedMessageThread () : juce::Thread ("message thread")
  {
    startThread ();
    auto wasSignaled = semaphore.wait ();
    juce::ignoreUnused (wasSignaled);
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
