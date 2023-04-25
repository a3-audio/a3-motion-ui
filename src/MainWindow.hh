#pragma once

#include "MainComponent.hh"

#include <JuceHeader.h>

namespace a3
{

class MainWindow : public ResizableWindow, private KeyListener
{
public:
  MainWindow (String const &name);

  void userTriedToCloseWindow () override;
  void resized () override;

private:
  bool keyPressed (const KeyPress &k, Component *c) override;

  juce::Viewport viewport;
  MainComponent mainComponent;
};

}
