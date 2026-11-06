#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace tape;

juce::AudioProcessorValueTreeState::ParameterLayout TapeProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < numParams; ++i)
    {
        const auto& s = spec (i);
        const juce::ParameterID id { s.id, 1 };
        switch (s.kind)
        {
            case Kind::Float:
                layout.add (std::make_unique<juce::AudioParameterFloat> (id, s.name, juce::NormalisableRange<float> (s.min, s.max), s.def,
                    juce::AudioParameterFloatAttributes().withLabel (s.unit)));
                break;
            case Kind::Bool:
                layout.add (std::make_unique<juce::AudioParameterBool> (id, s.name, s.def >= 0.5f));
                break;
            case Kind::Choice:
                layout.add (std::make_unique<juce::AudioParameterChoice> (id, s.name,
                    juce::StringArray::fromTokens (s.choices, "|", ""), (int) s.def));
                break;
        }
    }
    return layout;
}

TapeProcessor::TapeProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TAPE", createLayout())
{
    for (int i = 0; i < numParams; ++i)
    {
        params[(size_t) i] = apvts.getParameter (spec (i).id);
        raw[(size_t) i] = apvts.getRawParameterValue (spec (i).id);
    }
}

bool TapeProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void TapeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int ch = juce::jmax (1, getMainBusNumInputChannels());
    // 44,1 / 48 kHz : suréchantillonnage ×2 ; 88,2 kHz et plus : fréquence d'origine
    osFactor = sampleRate < 70000.0 ? 2 : 1;
    if (osFactor > 1)
    {
        oversampling = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) ch, 1,
            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
        oversampling->initProcessing ((size_t) samplesPerBlock);
    }
    else
        oversampling.reset();
    engine.setParams (values());
    engine.prepare (sampleRate * osFactor, sampleRate, ch);
    const double osLat = oversampling ? oversampling->getLatencyInSamples() : 0.0;
    setLatencySamples ((int) std::lround (osLat) + engine.latencyBaseSamples());
}

Values TapeProcessor::values() const
{
    Values v;
    for (int i = 0; i < numParams; ++i) v[i] = raw[(size_t) i]->load();
    return v;
}

void TapeProcessor::setValue (int p, float v)
{
    if (auto* pr = params[(size_t) p])
    {
        const float n = pr->convertTo0to1 (v);
        if (std::abs (pr->getValue() - n) > 1e-7f)
        {
            pr->beginChangeGesture();
            pr->setValueNotifyingHost (n);
            pr->endChangeGesture();
        }
    }
}

void TapeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (2, getTotalNumInputChannels());
    for (int c = numCh; c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, buffer.getNumSamples());
    if (numCh == 0) return;

    bool play = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition()) play = pos->getIsPlaying();
    rolling.store (play);
    lastBlockMs.store (juce::Time::getMillisecondCounter());

    engine.setParams (values());
    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) buffer.getNumSamples());
    if (oversampling)
    {
        auto up = oversampling->processSamplesUp (block);
        float* ptrs[2] = { up.getChannelPointer (0), numCh > 1 ? up.getChannelPointer (1) : nullptr };
        engine.process (ptrs, numCh, (int) up.getNumSamples());
        oversampling->processSamplesDown (block);
    }
    else
    {
        float* ptrs[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };
        engine.process (ptrs, numCh, buffer.getNumSamples());
    }
    for (int c = 0; c < 2; ++c)
    {
        tapeRms[c].store (engine.tapeLevel (c));
        outRms[c].store (engine.outLevel (c));
    }
}

void TapeProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("uiScale", uiScale, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void TapeProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto t = juce::ValueTree::fromXml (*xml);
            uiScale = (float) t.getProperty ("uiScale", 1.0f);
            apvts.replaceState (t);
        }
}

juce::AudioProcessorEditor* TapeProcessor::createEditor() { return new TapeEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TapeProcessor(); }
