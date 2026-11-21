#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "SynthEngine.h"
#include "FxChain.h"

class AfterhoursProcessor : public juce::AudioProcessor
{
public:
    AfterhoursProcessor();
    ~AfterhoursProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Afterhours"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static constexpr int kChunk = 256;

    float get (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    void readParameters();
    void renderSegment (int start, int num);
    void handleMidi (const juce::MidiMessage& m);

    afterhours::SynthEngine engine;
    afterhours::FxChain fx;
    afterhours::Lfo lfo;
    afterhours::ModBuffers mod;
    afterhours::VoiceParams voiceParams;
    afterhours::FxParams fxParams;

    juce::AudioBuffer<float> synthBuffer;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> cutoffSm;
    juce::SmoothedValue<float> bendSm;
    float bendRange = 2.0f, modWheel = 0.0f;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AfterhoursProcessor)
};
