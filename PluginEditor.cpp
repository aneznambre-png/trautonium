#include "PluginEditor.h"

using namespace traut;

// ------------------------------------------------------------------ ParamPanel
ParamPanel::ParamPanel (juce::AudioProcessorValueTreeState& state)
{
    const auto& specs = paramSpecs();
    for (const auto& s : specs)
    {
        size_t si = 0;
        for (; si < sections.size(); ++si) if (sections[si].name == s.group) break;
        if (si == sections.size()) sections.push_back ({ s.group, {}, {} });

        auto c = std::make_unique<Cell>();
        c->label = std::make_unique<juce::Label>();
        c->label->setText (s.name, juce::dontSendNotification);
        c->label->setJustificationType (juce::Justification::centred);
        c->label->setFont (juce::Font (juce::FontOptions (11.5f)));
        addAndMakeVisible (*c->label);

        if (s.type == ParamType::Choice)
        {
            c->combo = std::make_unique<juce::ComboBox>();
            for (size_t i = 0; i < s.choices.size(); ++i) c->combo->addItem (s.choices[i], (int) i + 1);
            addAndMakeVisible (*c->combo);
            c->cAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, s.id, *c->combo);
        }
        else
        {
            c->slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
            c->slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 16);
            if (! s.unit.empty()) c->slider->setTextValueSuffix (" " + juce::String (s.unit));
            if (s.type == ParamType::Int) c->slider->setNumDecimalPlacesToDisplay (0);
            else c->slider->setNumDecimalPlacesToDisplay (s.hi >= 100.0f ? 0 : 2);
            addAndMakeVisible (*c->slider);
            c->sAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, s.id, *c->slider);
        }
        sections[si].cells.push_back (cells.size());
        cells.push_back (std::move (c));
    }
}

int ParamPanel::getRequiredHeight (int width) const
{
    const int perRow = std::max (1, width / cellW);
    int h = 8;
    for (const auto& s : sections)
        h += headerH + (((int) s.cells.size() + perRow - 1) / perRow) * cellH + 8;
    return h;
}

void ParamPanel::resized()
{
    const int perRow = std::max (1, getWidth() / cellW);
    int y = 8;
    for (auto& s : sections)
    {
        s.header = { 8, y, getWidth() - 16, headerH };
        y += headerH;
        int col = 0;
        for (size_t idx : s.cells)
        {
            auto& c = *cells[idx];
            const int x = col * cellW;
            c.label->setBounds (x, y, cellW, 16);
            if (c.slider) c.slider->setBounds (x + 4, y + 16, cellW - 8, cellH - 18);
            if (c.combo)  c.combo->setBounds  (x + 4, y + 30, cellW - 8, 24);
            if (++col == perRow) { col = 0; y += cellH; }
        }
        if (col != 0) y += cellH;
        y += 8;
    }
}

void ParamPanel::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xffd9a441));
    g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    for (const auto& s : sections)
    {
        g.drawText (juce::String (s.name).toUpperCase(), s.header, juce::Justification::centredLeft);
        g.setColour (juce::Colour (0x40d9a441));
        g.drawHorizontalLine (s.header.getBottom() - 2, (float) s.header.getX(), (float) s.header.getRight());
        g.setColour (juce::Colour (0xffd9a441));
    }
}

// ------------------------------------------------------------------ editor
TrautoniumEditor::TrautoniumEditor (TrautoniumProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), panel (p.apvts)
{
    title.setText ("TRAUTONIUM  -  Mixtur", juce::dontSendNotification);
    title.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
    title.setColour (juce::Label::textColourId, juce::Colour (0xffd9a441));
    addAndMakeVisible (title);

    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    viewport.setViewedComponent (&panel, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    setResizable (true, true);
    setResizeLimits (520, 400, 1400, 1400);
    setSize (820, 640);
}

void TrautoniumEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff17140f)); }

void TrautoniumEditor::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto top = r.removeFromTop (36);
    title.setBounds (top.removeFromLeft (top.getWidth() / 2));
    presetBox.setBounds (top.reduced (0, 4));
    viewport.setBounds (r);
    const int w = r.getWidth() - viewport.getScrollBarThickness();
    panel.setBounds (0, 0, w, panel.getRequiredHeight (w));
}
