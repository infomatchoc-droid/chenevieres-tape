// Chènevières Tape — processeur du plugin
#pragma once
#include <JuceHeader.h>
#include "TapeEngine.h"

class TapeProcessor : public juce::AudioProcessor
{
public:
    TapeProcessor();
    ~TapeProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Chenevieres Tape"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.01; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // --- pour l'interface ---
    juce::AudioProcessorValueTreeState apvts;
    juce::RangedAudioParameter* param (int p) const { return params[(size_t) p]; }
    float value (int p) const { return raw[(size_t) p]->load(); }
    int choice (int p) const { return (int) std::lround (value (p)); }
    void setValue (int p, float v); // en unités réelles, notifie le DAW
    tape::Values values() const;

    float uiScale = 1.0f;
    // niveaux (RMS linéaire) : sur la bande et en sortie ; transport du DAW
    std::atomic<float> tapeRms[2] { 0.0f, 0.0f }, outRms[2] { 0.0f, 0.0f };
    std::atomic<bool> rolling { false };
    std::atomic<juce::uint32> lastBlockMs { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    std::array<juce::RangedAudioParameter*, tape::numParams> params {};
    std::array<std::atomic<float>*, tape::numParams> raw {};

    tape::Engine engine;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    int osFactor = 2;
};
