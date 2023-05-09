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

MainWindow::MainWindow (juce::String const &name)
    : juce::ResizableWindow (name, true), _mainComponent (numChannelsInitial)
{
  setWantsKeyboardFocus (true);

  setContentNonOwned (&_viewport, false /* resizeToFitWhenContentChangesSize */
  );
  _viewport.setViewedComponent (&_mainComponent);
  _viewport.setVisible (true);
  _mainComponent.setVisible (true);
}

bool
MainWindow::keyPressed (const juce::KeyPress &k)
{
  if (k.getKeyCode () == juce::KeyPress::escapeKey)
    {
      juce::JUCEApplicationBase::quit ();
      return true;
    }

  return false;
}

void
MainWindow::userTriedToCloseWindow ()
{
  juce::JUCEApplicationBase::quit ();
}

void
MainWindow::resized ()
{
  juce::ResizableWindow::resized ();

  auto const bounds = getLocalBounds ();
  _viewport.setBounds (bounds);

  // compute bounds for main component
  auto boundsContent
      = juce::Rectangle<int> (0.f, 0.f, //
                              juce::jmax (float (bounds.getWidth ()),
                                          _mainComponent.getMinimumWidth ()),
                              juce::jmax (float (bounds.getHeight ()),
                                          _mainComponent.getMinimumHeight ()));

  _mainComponent.setBounds (boundsContent);
}

}
