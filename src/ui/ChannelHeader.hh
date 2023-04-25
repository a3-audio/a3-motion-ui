#include <JuceHeader.h>

namespace a3
{

class Channel;

class ChannelHeader : public juce::Component
{
public:
  ChannelHeader (Channel const &);
  ChannelHeader (ChannelHeader &&);

  void paint (Graphics &) override;

private:
  Channel const &channel;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelHeader)
};
}
