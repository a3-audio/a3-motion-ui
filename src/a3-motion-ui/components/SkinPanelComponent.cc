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

#include "SkinPanelComponent.hh"

#include <a3-motion-ui/theme/SkinParameters.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

#include <cmath>

namespace a3
{

namespace
{
juce::Font
bodyFont ()
{
  return juce::Font (juce::FontOptions (theme ().fontSize (FontRole::Body)));
}

juce::Font
headerFont ()
{
  return juce::Font (
      juce::FontOptions (theme ().fontSize (FontRole::Header)).withStyle ("Bold"));
}

/** The theme's defaults, for a value or colour the file does not state.
 *  Built from `Theme{}`, which never changes while the app runs. */
juce::var const &
defaults ()
{
  static juce::var const value = themeDefaultsVar ();
  return value;
}

int
rowGap ()
{
  return juce::roundToInt (theme ().paddingHair);
}
}

// ── the pieces a row is made of ─────────────────────────────────────────

/** A bar: a JUCE slider laid flat, whose drag is relative and one to one
 *  with its own width. */
class SkinPanelComponent::Bar : public juce::Slider
{
public:
  explicit Bar (SkinTunable const &tunable)
      : juce::Slider (juce::Slider::LinearBar, juce::Slider::NoTextBox),
        _tunable (tunable)
  {
    setName (tunable.label);
    setNormalisableRange (skinTunableRange (tunable));
    // Landing on a bar to read it must not change it.
    setSliderSnapsToMousePosition (false);
    // A drag that starts on a bar is the bar's, not the panel's scroll.
    setViewportIgnoreDragFlag (true);
    // The wheel scrolls the panel; a value is set by hand.
    setScrollWheelEnabled (false);
    textFromValueFunction = [tunable] (double value) {
      return skinTunableText (tunable, value);
    };
  }

  SkinTunable const &tunable () const { return _tunable; }

  void
  resized () override
  {
    juce::Slider::resized ();
    // The bar's own width is its whole range: the value moves with the
    // finger, one to one, whatever the panel's width. JUCE's default is a
    // constant 250 px, which is the pixel design juce-pro warns about.
    setMouseDragSensitivity (juce::jmax (1, getWidth ()));
  }

private:
  SkinTunable _tunable;
};

/** A section's name, and a tap that opens or closes it. */
class SkinPanelComponent::SectionHeader : public juce::Button
{
public:
  explicit SectionHeader (SkinSectionSpec const &spec)
      : juce::Button (spec.label), _section (spec.section)
  {
  }

  SkinSection section () const { return _section; }
  void setOpen (bool open)
  {
    _open = open;
    repaint ();
  }

  void
  paintButton (juce::Graphics &g, bool highlighted, bool down) override
  {
    juce::ignoreUnused (highlighted);
    auto const bounds = getLocalBounds ().toFloat ();

    // Lightness is elevation: the open section's header is lifted, the
    // others sit on the panel.
    g.setColour (toColour (theme ().textPrimary,
                           down || _open ? theme ().alphaFillEmphasis
                                         : theme ().alphaFill));
    g.fillRoundedRectangle (bounds, theme ().radiusControl);

    auto textArea = getLocalBounds ().reduced (
        juce::roundToInt (theme ().padding), 0);
    auto const marker = textArea.removeFromRight (textArea.getHeight () / 2);

    g.setColour (toColour (theme ().textPrimary));
    g.setFont (headerFont ());
    g.drawText (getButtonText (), textArea, juce::Justification::centredLeft,
                true);

    // ▸ closed, ▾ open.
    juce::Path chevron;
    auto const m = marker.toFloat ().withSizeKeepingCentre (
        marker.getWidth () * 0.6f, marker.getWidth () * 0.6f);
    if (_open)
      chevron.addTriangle (m.getX (), m.getY (), m.getRight (), m.getY (),
                           m.getCentreX (), m.getBottom ());
    else
      chevron.addTriangle (m.getX (), m.getY (), m.getRight (),
                           m.getCentreY (), m.getX (), m.getBottom ());
    g.setColour (toColour (theme ().textMuted));
    g.fillPath (chevron);
  }

private:
  SkinSection _section;
  bool _open = false;
};

/** A colour: its name, and a chip of it. A tap opens the picker. */
class SkinPanelComponent::Swatch : public juce::Button
{
public:
  Swatch (SkinColourRow const &row, SkinPanelComponent &owner)
      : juce::Button (row.label), _path (row.path), _owner (owner)
  {
  }

  juce::String const &path () const { return _path; }

  void
  paintButton (juce::Graphics &g, bool highlighted, bool down) override
  {
    juce::ignoreUnused (highlighted);
    auto const bounds = getLocalBounds ();

    g.setColour (toColour (theme ().textPrimary,
                           down ? theme ().alphaFillEmphasis
                                : theme ().alphaFill));
    g.fillRoundedRectangle (bounds.toFloat (), theme ().radiusControl);

    auto inner = bounds.reduced (juce::roundToInt (theme ().paddingSmall));
    auto const chip = inner.removeFromRight (inner.getHeight () * 2);

    g.setColour (_owner.colourAt (_path));
    g.fillRoundedRectangle (chip.toFloat (), theme ().radiusTick);
    g.setColour (toColour (theme ().textPrimary, theme ().alphaOutline));
    g.drawRoundedRectangle (chip.toFloat (), theme ().radiusTick,
                            theme ().strokeThin);

    g.setColour (toColour (theme ().textMuted));
    g.setFont (bodyFont ());
    g.drawText (getButtonText (),
                inner.reduced (juce::roundToInt (theme ().paddingSmall), 0),
                juce::Justification::centredLeft, true);
  }

private:
  juce::String _path;
  SkinPanelComponent &_owner;
};

/** What the viewport scrolls: every row's widgets, and the title. */
class SkinPanelComponent::Content : public juce::Component
{
public:
  /** One row's widgets. Built once for every section, and shown or hidden
   *  as sections open, so no widget is ever deleted from inside its own
   *  click. */
  struct RowWidgets
  {
    SkinPanelRow row;
    std::unique_ptr<juce::ToggleButton> toggle;
    std::unique_ptr<SectionHeader> header;
    std::unique_ptr<Bar> bar;
    std::unique_ptr<juce::TextButton> minus;
    std::unique_ptr<juce::TextButton> plus;
    std::unique_ptr<Swatch> swatch;

    std::vector<juce::Component *> all () const
    {
      std::vector<juce::Component *> result;
      for (juce::Component *c :
           { static_cast<juce::Component *> (toggle.get ()),
             static_cast<juce::Component *> (header.get ()),
             static_cast<juce::Component *> (bar.get ()),
             static_cast<juce::Component *> (minus.get ()),
             static_cast<juce::Component *> (plus.get ()),
             static_cast<juce::Component *> (swatch.get ()) })
        if (c != nullptr)
          result.push_back (c);
      return result;
    }
  };

  std::vector<RowWidgets> rows;
  juce::TextButton footer{ "All values and skin actions" };
  juce::String title;
  juce::Rectangle<int> titleArea;

  void
  paint (juce::Graphics &g) override
  {
    if (titleArea.isEmpty ())
      return;

    auto text = titleArea.reduced (juce::roundToInt (theme ().padding), 0);
    g.setFont (bodyFont ());
    g.setColour (toColour (theme ().textMuted));
    g.drawText ("SKIN", text, juce::Justification::centredLeft, true);

    g.setFont (headerFont ());
    g.setColour (toColour (theme ().textPrimary));
    g.drawText (title, text, juce::Justification::centredRight, true);
  }

  RowWidgets *
  find (SkinPanelRow const &row)
  {
    for (auto &widgets : rows)
      if (widgets.row.kind == row.kind && widgets.row.section == row.section
          && widgets.row.index == row.index)
        return &widgets;
    return nullptr;
  }
};

// ── the panel ───────────────────────────────────────────────────────────

SkinPanelComponent::SkinPanelComponent ()
    : _content (std::make_unique<Content> ())
{
  _viewport.setViewedComponent (_content.get (), false);
  _viewport.setScrollBarsShown (true, false);
  _viewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::all);
  addAndMakeVisible (_viewport);

  _content->addAndMakeVisible (_content->footer);
  _content->footer.onClick = [this] {
    if (onOpenFullList)
      onOpenFullList ();
  };

  rebuildRows ();
}

SkinPanelComponent::~SkinPanelComponent ()
{
  _viewport.setViewedComponent (nullptr, false);
}

void
SkinPanelComponent::rebuildRows ()
{
  auto &rows = _content->rows;
  rows.clear ();

  for (auto const &spec : skinSections ())
    {
      Content::RowWidgets header;
      header.row = { SkinPanelRowKind::SectionHeader, spec.section };
      header.header = std::make_unique<SectionHeader> (spec);
      header.header->onClick = [this, section = spec.section] {
        openSection (_open == section ? std::nullopt
                                      : std::optional<SkinSection> (section));
      };
      if (spec.hasSwitch)
        {
          header.toggle = std::make_unique<juce::ToggleButton> ();
          header.toggle->setTitle (juce::String (spec.label) + " on");
          header.toggle->onClick = [this, section = spec.section,
                                    toggle = header.toggle.get ()] {
            setSectionSwitch (section, toggle->getToggleState ());
          };
        }
      rows.push_back (std::move (header));

      auto const addBar = [this] (Content::RowWidgets &widgets,
                                  SkinTunable const &tunable) {
        widgets.bar = std::make_unique<Bar> (tunable);
        widgets.bar->onValueChange = [this, bar = widgets.bar.get ()] {
          setTunable (bar->tunable (), bar->getValue ());
        };
        widgets.minus = std::make_unique<juce::TextButton> (
            juce::String::fromUTF8 ("\xe2\x88\x92"));
        widgets.plus = std::make_unique<juce::TextButton> ("+");
        for (auto *key : { widgets.minus.get (), widgets.plus.get () })
          // Held, a key keeps stepping: the first repeat after a beat's
          // hesitation, then a steady run.
          key->setRepeatSpeed (400, 60);
        widgets.minus->onClick = [this, tunable] { stepTunable (tunable, -1); };
        widgets.plus->onClick = [this, tunable] { stepTunable (tunable, 1); };
      };

      for (int i = 0; i < static_cast<int> (spec.effects.size ()); ++i)
        {
          Content::RowWidgets effect;
          effect.row = { SkinPanelRowKind::Effect, spec.section, i };
          effect.toggle = std::make_unique<juce::ToggleButton> ();
          effect.toggle->setTitle (
              juce::String (spec.effects[static_cast<size_t> (i)].amount.label)
              + " on");
          effect.toggle->onClick = [this, section = spec.section, i,
                                    toggle = effect.toggle.get ()] {
            setEffectSwitch (section, i, toggle->getToggleState ());
          };
          addBar (effect, spec.effects[static_cast<size_t> (i)].amount);
          rows.push_back (std::move (effect));
        }

      for (int i = 0; i < static_cast<int> (spec.values.size ()); ++i)
        {
          Content::RowWidgets value;
          value.row = { SkinPanelRowKind::Value, spec.section, i };
          addBar (value, spec.values[static_cast<size_t> (i)]);
          rows.push_back (std::move (value));
        }

      for (int i = 0; i < static_cast<int> (spec.colours.size ()); ++i)
        {
          Content::RowWidgets colour;
          colour.row = { SkinPanelRowKind::Colour, spec.section, i };
          colour.swatch = std::make_unique<Swatch> (
              spec.colours[static_cast<size_t> (i)], *this);
          colour.swatch->onClick = [this, swatch = colour.swatch.get ()] {
            if (onColourPicked)
              onColourPicked (swatch->path (), colourAt (swatch->path ()));
          };
          rows.push_back (std::move (colour));
        }
    }

  for (auto &widgets : rows)
    for (auto *component : widgets.all ())
      _content->addChildComponent (component);
}

void
SkinPanelComponent::setSkin (juce::var skin, juce::String const &name)
{
  _skin = std::move (skin);
  _name = name;
  _content->title = name;

  _valueAtOpen.clear ();
  for (auto &widgets : _content->rows)
    {
      if (widgets.bar != nullptr)
        {
          auto const &tunable = widgets.bar->tunable ();
          auto const value = tunableValue (tunable);
          _valueAtOpen[tunable.path] = value;
          widgets.bar->setValue (value, juce::dontSendNotification);
          widgets.bar->setDoubleClickReturnValue (true, value);
        }
    }

  refreshEnabledStates ();
  _content->repaint ();
}

void
SkinPanelComponent::openSection (std::optional<SkinSection> section)
{
  _open = section;
  resized ();
}

void
SkinPanelComponent::setSectionSwitch (SkinSection section, bool on)
{
  setSkinSwitch (_skin, sectionSwitchPath (section), on);
  refreshEnabledStates ();
  valueChanged ();
}

void
SkinPanelComponent::setEffectSwitch (SkinSection section, int effect, bool on)
{
  auto const &effects = skinSectionSpec (section).effects;
  if (effect < 0 || effect >= static_cast<int> (effects.size ()))
    return;

  setSkinSwitch (_skin,
                 effectSwitchPath (section,
                                   effects[static_cast<size_t> (effect)].key),
                 on);
  refreshEnabledStates ();
  valueChanged ();
}

void
SkinPanelComponent::setTunable (SkinTunable const &tunable, double value)
{
  auto held = juce::jlimit (tunable.min, tunable.max, value);
  if (tunable.isWholeNumber)
    held = std::round (held);
  held = clampSkinValue (_skin, tunable.path, held);

  setSkinValue (_skin, tunable.path, held, tunable.isWholeNumber);

  // The bar shows what the file now holds, which a clamp may have moved.
  for (auto &widgets : _content->rows)
    if (widgets.bar != nullptr
        && juce::String (widgets.bar->tunable ().path) == tunable.path)
      widgets.bar->setValue (tunableValue (tunable),
                             juce::dontSendNotification);

  valueChanged ();
}

void
SkinPanelComponent::stepTunable (SkinTunable const &tunable, int steps)
{
  setTunable (tunable, stepSkinTunable (tunable, tunableValue (tunable), steps));
}

double
SkinPanelComponent::tunableValue (SkinTunable const &tunable) const
{
  if (skinHasValue (_skin, tunable.path))
    return skinValue (_skin, tunable.path);

  // What the app draws with while the file is silent, so a first step
  // starts from what is on screen rather than from zero.
  if (skinHasValue (defaults (), tunable.path))
    return skinValue (defaults (), tunable.path);

  return tunable.centre;
}

bool
SkinPanelComponent::isEffectBarEnabled (SkinSection section, int effect) const
{
  auto const &effects = skinSectionSpec (section).effects;
  if (effect < 0 || effect >= static_cast<int> (effects.size ()))
    return false;

  return skinEffectIsOn (_skin, section,
                         effects[static_cast<size_t> (effect)].key);
}

juce::Colour
SkinPanelComponent::colourAt (juce::String const &path) const
{
  auto const fallback = themeColour (defaults (), path, ThemeColour{});
  return toColour (themeColour (_skin, path, fallback));
}

void
SkinPanelComponent::refreshEnabledStates ()
{
  for (auto &widgets : _content->rows)
    {
      auto const &spec = skinSectionSpec (widgets.row.section);
      auto const sectionOn
          = skinSwitchIsOn (_skin, sectionSwitchPath (widgets.row.section));

      switch (widgets.row.kind)
        {
        case SkinPanelRowKind::SectionHeader:
          if (widgets.toggle != nullptr)
            widgets.toggle->setToggleState (sectionOn,
                                            juce::dontSendNotification);
          break;

        case SkinPanelRowKind::Effect:
          {
            auto const &effect
                = spec.effects[static_cast<size_t> (widgets.row.index)];
            widgets.toggle->setToggleState (
                skinSwitchIsOn (_skin, effectSwitchPath (widgets.row.section,
                                                         effect.key)),
                juce::dontSendNotification);
            // The switch rests while its section is off: what it says is
            // kept, and comes back with the section.
            widgets.toggle->setEnabled (sectionOn);

            auto const drawn = isEffectBarEnabled (widgets.row.section,
                                                   widgets.row.index);
            widgets.bar->setEnabled (drawn);
            widgets.minus->setEnabled (drawn);
            widgets.plus->setEnabled (drawn);
            break;
          }

        case SkinPanelRowKind::Title:
        case SkinPanelRowKind::Value:
        case SkinPanelRowKind::Colour:
        case SkinPanelRowKind::Footer:
          break;
        }
    }
}

void
SkinPanelComponent::valueChanged ()
{
  if (onValueChanged)
    onValueChanged ();
}

void
SkinPanelComponent::paint (juce::Graphics &g)
{
  // The panel's own ground, rounded where it meets the sphere and square
  // against the screen's edge.
  auto const bounds = getLocalBounds ().toFloat ();
  auto const radius = theme ().radiusPanel;

  juce::Path ground;
  ground.addRoundedRectangle (bounds.getX (), bounds.getY (),
                              bounds.getWidth (), bounds.getHeight (), radius,
                              radius, false, true, false, true);
  g.setColour (toColour (theme ().surface, theme ().overlayOpacity));
  g.fillPath (ground);
}

void
SkinPanelComponent::resized ()
{
  auto const inset = juce::roundToInt (theme ().padding);
  _viewport.setBounds (getLocalBounds ().reduced (inset));

  auto const rowHeight
      = skinPanelRowHeight (theme ().fontSize (FontRole::Body));
  auto const rows = skinPanelRows (_open);

  // The scroll bar takes its width only when there is something to scroll.
  auto const contentHeight = static_cast<int> (rows.size ()) * rowHeight;
  auto width = _viewport.getWidth ();
  if (contentHeight > _viewport.getHeight ())
    width -= _viewport.getScrollBarThickness ();

  for (auto &widgets : _content->rows)
    for (auto *component : widgets.all ())
      component->setVisible (false);
  _content->footer.setVisible (false);
  _content->titleArea = {};

  int y = 0;
  for (auto const &row : rows)
    {
      juce::Rectangle<int> const area (0, y, width, rowHeight);
      y += rowHeight;

      auto const inner = area.reduced (0, rowGap ());
      auto const parts = skinPanelRowParts (row.kind, inner);

      switch (row.kind)
        {
        case SkinPanelRowKind::Title:
          _content->titleArea = inner;
          continue;

        case SkinPanelRowKind::Footer:
          _content->footer.setBounds (parts.label);
          _content->footer.setVisible (true);
          continue;

        case SkinPanelRowKind::SectionHeader:
        case SkinPanelRowKind::Effect:
        case SkinPanelRowKind::Value:
        case SkinPanelRowKind::Colour:
          break;
        }

      auto *widgets = _content->find (row);
      if (widgets == nullptr)
        continue;

      auto const place = [] (juce::Component *component,
                             juce::Rectangle<int> bounds) {
        if (component == nullptr || bounds.isEmpty ())
          return;
        component->setBounds (bounds);
        component->setVisible (true);
      };

      if (widgets->header != nullptr)
        {
          widgets->header->setOpen (_open == row.section);
          // A section without a switch keeps the column: its name starts
          // where every other header's does.
          place (widgets->header.get (), parts.label);
        }
      place (widgets->toggle.get (), parts.toggle);
      place (widgets->bar.get (), parts.bar);
      place (widgets->minus.get (), parts.minus);
      place (widgets->plus.get (), parts.plus);
      place (widgets->swatch.get (), parts.swatch);
    }

  _content->setSize (width, y);
  _content->repaint ();
}

}
