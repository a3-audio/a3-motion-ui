#pragma once

#include <JuceHeader.h>

namespace a3
{

/*
 * Our custom A3 LookAndFeel class.
 *
 * A note on the enum values for our custom ColourIds. JUCE uses
 * per-Component globally unique ColourIds, so we use the same
 * mechanism for our own Components. JUCE IDs start at 0x1000000, A3
 * IDs start at 0xa3000000.
 *
 * Formatting convention of IDs is as follows: 0xa3000204 identifies
 * color 4 of the A3 widget with running Component number 2. Every
 * newly introduced Component class increases the component number by
 * one. The per-Component custom colors are then numbered 00-ff in the
 * last 2 hex digits, making for 255 possible custom colors per custom
 * Component type.
 */
class LookAndFeel_A3 : public juce::LookAndFeel_V4
{
public:
  LookAndFeel_A3 ();
};

}
