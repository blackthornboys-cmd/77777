#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PitchEngine.h"
#include <atomic>
#include <array>

class FrozenTuneProcessor : public juce::AudioProcessor {
public:
    FrozenTuneProcessor();
    ~FrozenTuneProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> uiDetectedHz { 0.0f };
    std::atomic<float> uiTargetHz { 0.0f };
    std::atomic<float> uiCentsError { 0.0f };
    std::atomic<int>   uiTargetMidi { -1 };
    std::atomic<float> uiInputLevel { 0.0f };
    std::atomic<float> uiOutputLevel { 0.0f };
    std::atomic<float> uiLatencyMs { 1.5f };
    std::atomic<bool>  uiSibilant { false };

    std::array<std::atomic<bool>, 12> customNoteEnabled;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    frozen::PitchEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FrozenTuneProcessor)
};
