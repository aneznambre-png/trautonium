#pragma once
#include "PluginProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>

// Auto-generated panel: one section per parameter group, built from traut::paramSpecs().
class ParamPanel : public juce::Component
{
public:
    explicit ParamPanel (juce::AudioProcessorValueTreeState& state);
    void resized() override;
    void paint (juce::Graphics&) override;
    int getRequiredHeight (int width) const;

private:
    struct Cell
    {
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cAtt;
    };
    struct Section { std::string name; std::vector<size_t> cells; juce::Rectangle<int> header; };

    std::vector<std::unique_ptr<Cell>> cells;
    std::vector<Section> sections;
    static constexpr int cellW = 100, cellH = 96, headerH = 26;
};

class TrautoniumEditor : public juce::AudioProcessorEditor
{
public:
    explicit TrautoniumEditor (TrautoniumProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    TrautoniumProcessor& proc;
    ParamPanel panel;
    juce::Viewport viewport;
    juce::ComboBox presetBox;
    juce::Label title;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrautoniumEditor)
};
