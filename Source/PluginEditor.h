#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "CustomLookAndFeel.h"
#include <array>

class FrozenTuneEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit FrozenTuneEditor(FrozenTuneProcessor&);
    ~FrozenTuneEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void applyPresetIdx(int idx);
    void drawCrystalBrokenHeart(juce::Graphics& g, juce::Point<float> c, float scale, float crackGap, float glow);
    static juce::String midiToNoteNameOnly(int midi);

    FrozenTuneProcessor& processor;
    FrozenLookAndFeel lnf;

    juce::ComboBox vocalRangeBox, keyBox, scaleBox, presetBox;
    juce::TextButton lowLatBtn { "Low Latency  0.0 ms" }, hqBtn { "HQ" };
    juce::TextButton modernBtn { "Modern" }, classicBtn { "Classic" };
    juce::TextButton sibBtn { "Sib" }, doublerBtn { "Doubler" }, bypassBtn { "ACTIVE" };

    juce::Slider speedSlider, sustainSlider, amountSlider, detuneSlider;
    juce::HyperlinkButton tgLink { "t.me/heartmyfrozen", juce::URL("https://t.me/heartmyfrozen") };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rangeAtt, keyAtt, scaleAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> sibAtt, bypassAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> speedAtt, sustainAtt, amountAtt, detuneAtt;

    std::array<juce::Rectangle<float>, 12> notePillRects{};
    float animPhase = 0.0f;
    float heartPulse = 0.0f;
    float smoothCents = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FrozenTuneEditor)
};
