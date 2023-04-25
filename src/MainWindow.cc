#include "MainWindow.hh"

#include "Config.hh"

namespace a3
{

MainWindow::MainWindow (String const &name)
    : ResizableWindow (name, true), mainComponent (numChannelsInitial)
{
  setWantsKeyboardFocus (true);
  addKeyListener (this);

  setContentNonOwned (&viewport, false /* resizeToFitWhenContentChangesSize */
  );
  viewport.setViewedComponent (&mainComponent);
  viewport.setVisible (true);
  mainComponent.setVisible (true);
}

bool
MainWindow::keyPressed (const KeyPress &k, Component *c)
{
  if (k.getKeyCode () == KeyPress::escapeKey)
    {
      JUCEApplicationBase::quit ();
      return true;
    }

  return false;
}

void
MainWindow::userTriedToCloseWindow ()
{
  JUCEApplicationBase::quit ();
}

void
MainWindow::resized ()
{
  ResizableWindow::resized ();

  auto const bounds = getLocalBounds ();
  viewport.setBounds (bounds);

  // compute bounds for main component
  auto boundsContent = Rectangle<int> (
      0.f, 0.f, //
      jmax (float (bounds.getWidth ()), mainComponent.getMinimumWidth ()),
      jmax (float (bounds.getHeight ()), mainComponent.getMinimumHeight ()));

  mainComponent.setBounds (boundsContent);
}

}
