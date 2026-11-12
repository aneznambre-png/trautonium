#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace traut;

juce::AudioProcessorValueTreeState::ParameterLayout TrautoniumProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    std::vector<std::unique_ptr<juce::AudioProcessorParameterGroup>> groups;
    std::vector<std::string> groupNames;

    for (const auto& s : paramSpecs())
    {
        size_t gi = 0;
        for (; gi < groupNames.size(); ++gi) if (groupNames[gi] == s.group) break;
        if (gi == groupNames.size())
        {
            groupNames.push_back (s.group);
            groups.push_back (std::make_unique<juce::AudioProcessorParameterGroup> ("grp" + juce::String ((int) gi), s.group, "|"));
        }

        const juce::ParameterID pid { s.id, 1 };
        if (s.type == ParamType::Float)
        {
            juce::NormalisableRange<float> range (s.lo, s.hi);
            if (s.skewMid > 0.0f) range.setSkewForCentre (s.skewMid);
            groups[gi]->addChild (std::make_unique<juce::AudioParameterFloat> (
                pid, s.name, range, s.def, juce::AudioParameterFloatAttributes().withLabel (s.unit)));
        }
        else if (s.type == ParamType::Int)
        {
            groups[gi]->addChild (std::make_unique<juce::AudioParameterInt> (
                pid, s.name, (int) s.lo, (int) s.hi, (int) s.def, juce::AudioParameterIntAttributes().withLabel (s.unit)));
        }
        else
        {
            juce::StringArray ch;
            for (auto& c : s.choices) ch.add (c);
            groups[gi]->addChild (std::make_unique<juce::AudioParameterChoice> (pid, s.name, ch, (int) s.def));
        }
    }
    for (auto& g : groups) layout.add (std::move (g));
    return layout;
}

TrautoniumProcessor::TrautoniumProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TRAUTONIUM", createLayout())
{
    for (int i = 0; i < kNumParams; ++i)
        raw[(size_t) i] = apvts.getRawParameterValue (paramSpecs()[(size_t) i].id);
}

bool TrautoniumProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void TrautoniumProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    lastMixtureParam = -1.0f;
}

void TrautoniumProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())                 engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())           engine.noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    else if (m.isPitchWheel())        engine.setPitchBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isChannelPressure())   engine.setPressure ((float) m.getChannelPressureValue() / 127.0f);
    else if (m.isController())
    {
        const float v = (float) m.getControllerValue() / 127.0f;
        switch (m.getControllerNumber())
        {
            case 1:  engine.setModWheel (v); break;                     // finger vibrato depth
            case 11: engine.setExpression (v); break;                   // foot pedal
            case 16: engine.setMixtureOverride (v < 0.34f ? 0 : (v < 0.67f ? 1 : 2)); break; // foot switch: mixture left / middle / right
            default: break;
        }
    }
}

void TrautoniumProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    for (int i = 0; i < kNumParams; ++i) params.v[(size_t) i] = raw[(size_t) i]->load();
    if (params[kMixture] != lastMixtureParam) { engine.setMixtureOverride (-1); lastMixtureParam = params[kMixture]; }
    engine.setParams (params);

    float* l = buffer.getWritePointer (0);
    float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    std::vector<float>& tmp = scratch;
    if ((int) tmp.size() < numSamples) tmp.resize ((size_t) numSamples);

    auto renderRange = [&] (int start, int count)
    {
        if (count <= 0) return;
        if (r != nullptr) engine.render (l + start, r + start, count);
        else
        {
            engine.render (l + start, tmp.data(), count);
            for (int i = 0; i < count; ++i) l[start + i] = 0.5f * (l[start + i] + tmp[(size_t) i]);
        }
    };

    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = std::min (numSamples, std::max (pos, meta.samplePosition));
        renderRange (pos, t - pos);
        pos = t;
        handleMidi (meta.getMessage());
    }
    renderRange (pos, numSamples - pos);
}

int TrautoniumProcessor::getNumPrograms() { return (int) presets().size(); }
const juce::String TrautoniumProcessor::getProgramName (int index)
{
    return juce::String (presets()[(size_t) juce::jlimit (0, getNumPrograms() - 1, index)].name);
}

void TrautoniumProcessor::setCurrentProgram (int index)
{
    index = juce::jlimit (0, getNumPrograms() - 1, index);
    currentProgram = index;
    const auto& specs = paramSpecs();
    for (size_t i = 0; i < specs.size(); ++i)
        if (auto* p = apvts.getParameter (specs[i].id))
            p->setValueNotifyingHost (p->convertTo0to1 (specs[i].def));
    for (auto& kv : presets()[(size_t) index].values)
        if (auto* p = apvts.getParameter (kv.first))
            p->setValueNotifyingHost (p->convertTo0to1 (kv.second));
}

void TrautoniumProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void TrautoniumProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) vt.getProperty ("program", 0);
            apvts.replaceState (vt);
        }
}

juce::AudioProcessorEditor* TrautoniumProcessor::createEditor() { return new TrautoniumEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TrautoniumProcessor(); }
