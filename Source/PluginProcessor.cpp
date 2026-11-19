#include "PluginProcessor.h"
#include "PluginEditor.h"

FrozenTuneProcessor::FrozenTuneProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout()) {
    for (auto& b : customNoteEnabled) b.store(true);
}

juce::AudioProcessorValueTreeState::ParameterLayout FrozenTuneProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "key", "Key", juce::StringArray{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "scale", "Scale", juce::StringArray{"Chromatic", "Major", "Minor", "Dorian", "Pentatonic", "Harmonic Minor", "Custom"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "inputType", "Input Type", juce::StringArray{"Soprano", "Alto / Tenor", "Low Male"}, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "retuneSpeed", "Speed", juce::NormalisableRange<float>(-5.0f, 150.0f, 0.1f, 0.65f), 14.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "sustain", "Sustain", juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "amount", "Amount", juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 100.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "tracking", "Tracking", juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 55.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "stabilizer", "Note Stabilizer", juce::StringArray{"None", "Short", "Mid", "Long"}, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "doublerWet", "Doubler Wet", juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "doublerWidth", "Doubler Width", juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 50.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "pitchRef", "Pitch Reference", juce::NormalisableRange<float>(415.0f, 465.0f, 0.5f), 440.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>("lowLatency", "Low Latency", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>("sibilanceGuard", "Sibilance Guard", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>("formantLock", "Formant Lock", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>("bypass", "Bypass", false));

    return { params.begin(), params.end() };
}

void FrozenTuneProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    engine.prepare(sampleRate, samplesPerBlock);
    setLatencySamples(engine.getLatencySamples());
}

bool FrozenTuneProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet()
        && !layouts.getMainInputChannelSet().isDisabled();
}

void FrozenTuneProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;

    int key = static_cast<int>(*apvts.getRawParameterValue("key"));
    int scaleIdx = static_cast<int>(*apvts.getRawParameterValue("scale"));
    int rangeIdx = static_cast<int>(*apvts.getRawParameterValue("inputType"));
    float speed = *apvts.getRawParameterValue("retuneSpeed");
    float sustain = *apvts.getRawParameterValue("sustain");
    float amount = *apvts.getRawParameterValue("amount");
    float tracking = *apvts.getRawParameterValue("tracking");
    int stabMode = static_cast<int>(*apvts.getRawParameterValue("stabilizer"));
    float dblWet = *apvts.getRawParameterValue("doublerWet");
    float dblWidth = *apvts.getRawParameterValue("doublerWidth");
    float pitchRef = *apvts.getRawParameterValue("pitchRef");
    bool lowLat = *apvts.getRawParameterValue("lowLatency") > 0.5f;
    bool sibGuard = *apvts.getRawParameterValue("sibilanceGuard") > 0.5f;
    bool formLock = *apvts.getRawParameterValue("formantLock") > 0.5f;
    bool bypass = *apvts.getRawParameterValue("bypass") > 0.5f;

    std::array<bool, 12> cMask{};
    for (int i = 0; i < 12; ++i) cMask[i] = customNoteEnabled[i].load(std::memory_order_relaxed);

    engine.setParameters(key, static_cast<frozen::ScaleType>(scaleIdx),
                         static_cast<frozen::InputRange>(rangeIdx),
                         speed, sustain, amount, tracking, stabMode,
                         dblWet, dblWidth, pitchRef,
                         lowLat, sibGuard, formLock, cMask);
    setLatencySamples(bypass ? 0 : engine.getLatencySamples());

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numChannels == 0 || numSamples == 0) return;

    float inPeak = 0.0f, outPeak = 0.0f;
    auto* ch0 = buffer.getWritePointer(0);
    auto* ch1 = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i) {
        float inL = ch0[i];
        float inR = ch1 ? ch1[i] : inL;
        inPeak = std::max(inPeak, std::max(std::abs(inL), std::abs(inR)));

        if (bypass) {
            outPeak = inPeak;
            continue;
        }

        float outL = 0.0f, outR = 0.0f;
        engine.processStereoSample(inL, inR, outL, outR);
        ch0[i] = outL;
        if (ch1) ch1[i] = outR;
        outPeak = std::max(outPeak, std::max(std::abs(outL), std::abs(outR)));
    }

    auto tel = engine.getTelemetry();
    uiDetectedHz.store(tel.detectedHz, std::memory_order_relaxed);
    uiTargetHz.store(tel.targetHz, std::memory_order_relaxed);
    uiCentsError.store(tel.centsError, std::memory_order_relaxed);
    uiTargetMidi.store(tel.targetMidi, std::memory_order_relaxed);
    uiLatencyMs.store(tel.latencyMs, std::memory_order_relaxed);
    uiSibilant.store(tel.sibilantActive, std::memory_order_relaxed);
    uiInputLevel.store(inPeak, std::memory_order_relaxed);
    uiOutputLevel.store(outPeak, std::memory_order_relaxed);
}

void FrozenTuneProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void FrozenTuneProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessorEditor* FrozenTuneProcessor::createEditor() {
    return new FrozenTuneEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new FrozenTuneProcessor();
}
