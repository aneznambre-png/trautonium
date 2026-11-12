#pragma once
#include "TrautoniumDSP.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

class TrautoniumProcessor : public juce::AudioProcessor
{
public:
    TrautoniumProcessor();
    ~TrautoniumProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Trautonium"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleMidi (const juce::MidiMessage& m);

    traut::Engine engine;
    traut::Params params;
    std::array<std::atomic<float>*, traut::kNumParams> raw {};
    std::vector<float> scratch;
    float lastMixtureParam = -1.0f;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrautoniumProcessor)
};
