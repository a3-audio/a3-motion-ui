#include <JuceHeader.h>

namespace a3
{

class Channel;

class ChannelFooter : public juce::Component
{
public:
  ChannelFooter (Channel const &);
  ChannelFooter (ChannelFooter &&);

  void paint (Graphics &) override;

private:
  Channel const &channel;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelFooter)
};

}
