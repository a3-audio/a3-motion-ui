/*

  A3 Motion UI
  Copyright (C) 2023 Patric Schmitz

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

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
