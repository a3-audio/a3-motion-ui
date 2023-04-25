#include <JuceHeader.h>

namespace a3
{

class MotionComponent : public juce::Component
{
public:
  MotionComponent ();

  void resized () override;
  void paint (Graphics &g) override;

private:
  ImageComponent image;
};

}
