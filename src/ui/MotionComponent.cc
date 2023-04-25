#include "MotionComponent.hh"

namespace a3
{

MotionComponent::MotionComponent ()
{
  image.setImage (
      ImageFileFormat::loadFrom (File ("./resources/a3_logo-dark.png")));

  addChildComponent (image);
  image.setVisible (true);
}

void
MotionComponent::paint (Graphics &g)
{
}

void
MotionComponent::resized ()
{
  image.setBounds (getLocalBounds ());
}

}
