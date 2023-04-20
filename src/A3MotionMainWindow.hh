#pragma once

#include <JuceHeader.h>


class A3MotionMainWindow :
    public ResizableWindow
{
public:
    A3MotionMainWindow(String const & name);

    void userTriedToCloseWindow() override;
};
