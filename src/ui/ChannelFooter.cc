#include "ChannelFooter.hh"

namespace a3
{

ChannelFooter::ChannelFooter (Channel const &channel) : channel (channel) {}

ChannelFooter::ChannelFooter (ChannelFooter &&rhs) : channel (rhs.channel) {}

void
ChannelFooter::paint (Graphics &g)
{
  Random rng;
  auto colour = Colour::fromHSL (rng.nextFloat (), 0.5f, 0.8f, 1.f);
  g.fillAll (colour);
}

}
