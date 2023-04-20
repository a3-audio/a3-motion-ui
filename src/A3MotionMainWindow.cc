#include "A3MotionMainWindow.hh"

A3MotionMainWindow::
A3MotionMainWindow(String const & name) :
    ResizableWindow(name, true)
{
    // TODO create content
}

void
A3MotionMainWindow::
userTriedToCloseWindow()
{
    JUCEApplicationBase::quit();
}
