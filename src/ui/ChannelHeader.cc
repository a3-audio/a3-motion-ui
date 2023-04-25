#include "ChannelHeader.hh"

namespace a3
{

ChannelHeader::ChannelHeader (Channel const &channel) : channel (channel) {}

ChannelHeader::ChannelHeader (ChannelHeader &&rhs) : channel (rhs.channel) {}

void
ChannelHeader::paint (Graphics &g)
{
  Random rng;
  auto colour = Colour::fromHSL (rng.nextFloat (), 0.5f, 0.8f, 1.f);
  g.fillAll (colour);
}

}
