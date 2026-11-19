#include "PluginEditor.h"
#include <cmath>

FrozenTuneEditor::FrozenTuneEditor(FrozenTuneProcessor& p)
    : AudioProcessorEditor(&p), processor(p) {
    setLookAndFeel(&lnf);
    setSize(900, 560);

    vocalRangeBox.addItemList({"Soprano", "Alto / Tenor", "Low Male"}, 1);
    keyBox.addItemList({"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 1);
    scaleBox.addItemList({"Chromatic", "Major", "Minor", "Dorian", "Pentatonic", "Harmonic Minor", "Custom"}, 1);
    addAndMakeVisible(vocalRangeBox);
    addAndMakeVisible(keyBox);
    addAndMakeVisible(scaleBox);

    presetBox.addItemList({
        "Default Preset (Classic)",
        "01. Hard -5.0 Trap",
        "02. Melodic Rap + Doubler",
        "03. Natural Transparent",
        "04. Hyperpop 450Hz FX"
    }, 1);
    presetBox.setSelectedId(1, juce::dontSendNotification);
    presetBox.onChange = [this] { applyPresetIdx(presetBox.getSelectedId() - 1); };
    addAndMakeVisible(presetBox);

    auto setupPill = [this](juce::TextButton& b, juce::Colour onCol, juce::Colour onText = juce::Colour(0xff082f49)) {
        b.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff141922));
        b.setColour(juce::TextButton::buttonOnColourId, onCol);
        b.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff94a3b8));
        b.setColour(juce::TextButton::textColourOnId, onText);
        addAndMakeVisible(b);
    };

    setupPill(lowLatBtn, juce::Colour(0xff67e8f9));
    setupPill(hqBtn, juce::Colour(0xff67e8f9));
    lowLatBtn.onClick = [this] {
        if (auto* pr = processor.apvts.getParameter("lowLatency")) pr->setValueNotifyingHost(1.0f);
    };
    hqBtn.onClick = [this] {
        if (auto* pr = processor.apvts.getParameter("lowLatency")) pr->setValueNotifyingHost(0.0f);
    };

    setupPill(modernBtn, juce::Colour(0xff38bdf8), juce::Colours::white);
    setupPill(classicBtn, juce::Colour(0xff38bdf8), juce::Colours::white);
    modernBtn.onClick = [this] {
        if (auto* pr = processor.apvts.getParameter("formantLock")) pr->setValueNotifyingHost(1.0f);
    };
    classicBtn.onClick = [this] {
        if (auto* pr = processor.apvts.getParameter("formantLock")) pr->setValueNotifyingHost(0.0f);
    };

    setupPill(sibBtn, juce::Colour(0xff38bdf8), juce::Colours::white);
    sibBtn.setClickingTogglesState(true);

    setupPill(doublerBtn, juce::Colour(0xff38bdf8), juce::Colours::white);
    doublerBtn.setClickingTogglesState(true);
    doublerBtn.onClick = [this] {
        if (auto* pw = processor.apvts.getParameter("doublerWet"))
            pw->setValueNotifyingHost(doublerBtn.getToggleState() ? 0.30f : 0.0f);
    };

    setupPill(bypassBtn, juce::Colour(0xffef4444), juce::Colours::white);
    bypassBtn.setClickingTogglesState(true);

    auto setupDial = [this](juce::Slider& s, juce::Colour accent) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setColour(juce::Slider::rotarySliderFillColourId, accent);
        addAndMakeVisible(s);
    };
    setupDial(speedSlider, juce::Colour(0xff67e8f9));
    setupDial(sustainSlider, juce::Colour(0xff94a3b8));
    setupDial(amountSlider, juce::Colour(0xff94a3b8));
    setupDial(detuneSlider, juce::Colour(0xff67e8f9));

    tgLink.setFont(juce::FontOptions(11.5f, juce::Font::plain), false, juce::Justification::centredRight);
    tgLink.setColour(juce::HyperlinkButton::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(tgLink);

    rangeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "inputType", vocalRangeBox);
    keyAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "key", keyBox);
    scaleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "scale", scaleBox);
    sibAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "sibilanceGuard", sibBtn);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "bypass", bypassBtn);
    speedAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "retuneSpeed", speedSlider);
    sustainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "sustain", sustainSlider);
    amountAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "amount", amountSlider);
    detuneAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "pitchRef", detuneSlider);

    startTimerHz(60);
}

FrozenTuneEditor::~FrozenTuneEditor() {
    setLookAndFeel(nullptr);
}

void FrozenTuneEditor::applyPresetIdx(int idx) {
    auto setP = [this](const char* id, float val) {
        if (auto* p = processor.apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(val));
    };
    if (idx == 0) { setP("retuneSpeed", 14.0f); setP("sustain", 15.0f); setP("amount", 100.0f); setP("pitchRef", 440.0f); setP("doublerWet", 0.0f); doublerBtn.setToggleState(false, juce::dontSendNotification); }
    else if (idx == 1) { setP("retuneSpeed", -5.0f); setP("sustain", 20.0f); setP("amount", 100.0f); setP("pitchRef", 440.0f); setP("doublerWet", 0.0f); doublerBtn.setToggleState(false, juce::dontSendNotification); }
    else if (idx == 2) { setP("retuneSpeed", 6.0f);  setP("sustain", 25.0f); setP("amount", 95.0f);  setP("pitchRef", 440.0f); setP("doublerWet", 30.0f); doublerBtn.setToggleState(true, juce::dontSendNotification); }
    else if (idx == 3) { setP("retuneSpeed", 55.0f); setP("sustain", 10.0f); setP("amount", 75.0f);  setP("pitchRef", 440.0f); setP("doublerWet", 0.0f); doublerBtn.setToggleState(false, juce::dontSendNotification); }
    else if (idx == 4) { setP("retuneSpeed", -5.0f); setP("sustain", 0.0f);  setP("amount", 100.0f); setP("pitchRef", 450.0f); setP("doublerWet", 40.0f); doublerBtn.setToggleState(true, juce::dontSendNotification); }
}

void FrozenTuneEditor::timerCallback() {
    animPhase += 0.05f;
    if (animPhase > 6.283185f) animPhase -= 6.283185f;

    float inL = processor.uiInputLevel.load(std::memory_order_relaxed);
    float targetPulse = std::clamp(inL * 3.0f, 0.0f, 1.0f);
    heartPulse += 0.18f * (targetPulse - heartPulse);

    float targetCents = std::clamp(processor.uiCentsError.load(std::memory_order_relaxed), -100.0f, 100.0f);
    smoothCents += 0.25f * (targetCents - smoothCents);

    bool ll = *processor.apvts.getRawParameterValue("lowLatency") > 0.5f;
    lowLatBtn.setToggleState(ll, juce::dontSendNotification);
    hqBtn.setToggleState(!ll, juce::dontSendNotification);

    bool mod = *processor.apvts.getRawParameterValue("formantLock") > 0.5f;
    modernBtn.setToggleState(mod, juce::dontSendNotification);
    classicBtn.setToggleState(!mod, juce::dontSendNotification);

    bool byp = *processor.apvts.getRawParameterValue("bypass") > 0.5f;
    bypassBtn.setButtonText(byp ? "BYPASSED" : "ACTIVE");
    if (!byp) {
        bypassBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff102a36));
        bypassBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff67e8f9));
    }

    repaint();
}

void FrozenTuneEditor::mouseDown(const juce::MouseEvent& e) {
    for (int pc = 0; pc < 12; ++pc) {
        if (notePillRects[pc].contains(e.position)) {
            processor.customNoteEnabled[pc].store(!processor.customNoteEnabled[pc].load());
            scaleBox.setSelectedId(7, juce::sendNotification);
            repaint();
            return;
        }
    }
}

juce::String FrozenTuneEditor::midiToNoteNameOnly(int midi) {
    if (midi < 0) return "--";
    static const char* names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return names[midi % 12];
}

void FrozenTuneEditor::drawCrystalBrokenHeart(juce::Graphics& g, juce::Point<float> c, float scale, float crackGap, float glow) {
    juce::Path l, r;
    l.startNewSubPath(c.x - crackGap, c.y - 20.0f * scale);
    l.cubicTo(c.x - 13.0f * scale, c.y - 44.0f * scale, c.x - 54.0f * scale, c.y - 36.0f * scale, c.x - 48.0f * scale, c.y - 2.0f * scale);
    l.cubicTo(c.x - 44.0f * scale, c.y + 24.0f * scale, c.x - 16.0f * scale, c.y + 42.0f * scale, c.x - crackGap, c.y + 54.0f * scale);
    l.lineTo(c.x - crackGap - 6.0f * scale, c.y + 32.0f * scale);
    l.lineTo(c.x - crackGap + 4.0f * scale, c.y + 12.0f * scale);
    l.lineTo(c.x - crackGap - 6.0f * scale, c.y - 6.0f * scale);
    l.closeSubPath();

    r.startNewSubPath(c.x + crackGap, c.y - 20.0f * scale);
    r.cubicTo(c.x + 13.0f * scale, c.y - 44.0f * scale, c.x + 54.0f * scale, c.y - 36.0f * scale, c.x + 48.0f * scale, c.y - 2.0f * scale);
    r.cubicTo(c.x + 44.0f * scale, c.y + 24.0f * scale, c.x + 16.0f * scale, c.y + 42.0f * scale, c.x + crackGap, c.y + 54.0f * scale);
    r.lineTo(c.x + crackGap - 6.0f * scale, c.y + 32.0f * scale);
    r.lineTo(c.x + crackGap + 4.0f * scale, c.y + 12.0f * scale);
    r.lineTo(c.x + crackGap - 6.0f * scale, c.y - 6.0f * scale);
    r.closeSubPath();

    // Dark obsidian crystal left, frosted titanium right
    juce::ColourGradient gradL(juce::Colour(0xff27303f), c.x - 40.0f, c.y - 35.0f,
                               juce::Colour(0xff0b0f16), c.x, c.y + 50.0f, false);
    g.setGradientFill(gradL);
    g.fillPath(l);

    juce::ColourGradient gradR(juce::Colour(0xff64748b), c.x + 10.0f, c.y - 35.0f,
                               juce::Colour(0xff1e293b), c.x + 40.0f, c.y + 50.0f, false);
    g.setGradientFill(gradR);
    g.fillPath(r);

    // Luminous glacial-cyan crack glow
    g.setColour(juce::Colour(0xff67e8f9).withAlpha(0.25f + glow * 0.55f));
    g.strokePath(l, juce::PathStrokeType(2.2f));
    g.strokePath(r, juce::PathStrokeType(2.2f));
}

void FrozenTuneEditor::paint(juce::Graphics& g) {
    // Matte anodized black-titanium chassis
    g.fillAll(juce::Colour(0xff0d0f13));

    // Left smoked sapphire-glass display
    juce::Rectangle<float> leftGlass(12.0f, 12.0f, 558.0f, static_cast<float>(getHeight() - 24));
    juce::ColourGradient glassGrad(juce::Colour(0xff0b121c), 12.0f, 12.0f,
                                   juce::Colour(0xff070b11), 570.0f, 548.0f, false);
    g.setGradientFill(glassGrad);
    g.fillRoundedRectangle(leftGlass, 10.0f);
    g.setColour(juce::Colour(0xff262e3b));
    g.drawRoundedRectangle(leftGlass, 10.0f, 1.2f);

    // Right brushed titanium deck
    juce::Rectangle<float> rightDeck(582.0f, 12.0f, 306.0f, static_cast<float>(getHeight() - 24));
    juce::ColourGradient tiGrad(juce::Colour(0xff191c22), 582.0f, 12.0f,
                                juce::Colour(0xff101216), 888.0f, 548.0f, false);
    g.setGradientFill(tiGrad);
    g.fillRoundedRectangle(rightDeck, 10.0f);
    g.setColour(juce::Colour(0xff2b303a));
    g.drawRoundedRectangle(rightDeck, 10.0f, 1.2f);

    // Corner machined hex screws on titanium deck
    auto drawScrew = [&](float sx, float sy) {
        g.setColour(juce::Colour(0xff0b0d10));
        g.fillEllipse(sx - 4.0f, sy - 4.0f, 8.0f, 8.0f);
        g.setColour(juce::Colour(0xff374151));
        g.drawEllipse(sx - 4.0f, sy - 4.0f, 8.0f, 8.0f, 0.8f);
    };
    drawScrew(594.0f, 24.0f); drawScrew(876.0f, 24.0f);
    drawScrew(594.0f, 536.0f); drawScrew(876.0f, 536.0f);

    // Top left header
    g.setColour(juce::Colour(0xfff1f5f9));
    g.setFont(juce::FontOptions(21.0f, juce::Font::plain));
    g.drawText("FROZENTUNE", 28, 22, 170, 24, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff64748b));
    g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    g.drawText("by frozenmyheart", 28, 46, 150, 14, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xff64748b));
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("Vocal Range:", 204, 18, 100, 14, juce::Justification::centredLeft);
    g.drawText("Key & Scale:", 330, 18, 110, 14, juce::Justification::centredLeft);

    // Voice-reactive Circular Waveform Ring around Sculpted Crystal Heart
    juce::Point<float> hc(291.0f, 174.0f);
    constexpr float pi = 3.14159265358979323846f;
    const int numTicks = 64;
    for (int i = 0; i < numTicks; ++i) {
        float ang = (static_cast<float>(i) / numTicks) * 2.0f * pi;
        float wave = 0.5f + 0.5f * std::sin(ang * 5.0f + animPhase * 3.0f);
        float len = 4.0f + wave * (5.0f + heartPulse * 18.0f);
        float r1 = 82.0f;
        float r2 = r1 + len;
        g.setColour(juce::Colour(0xff67e8f9).withAlpha(0.35f + 0.55f * heartPulse));
        g.drawLine(hc.x + std::cos(ang) * r1, hc.y + std::sin(ang) * r1,
                   hc.x + std::cos(ang) * r2, hc.y + std::sin(ang) * r2, 1.6f);
    }

    // Broken heart closes crack when cents approach 0
    float centsAbs = std::min(1.0f, std::abs(smoothCents) / 50.0f);
    float crackGap = 1.6f + centsAbs * 3.2f;
    float breath = 1.0f + 0.03f * std::sin(animPhase * 2.0f) + 0.06f * heartPulse;
    drawCrystalBrokenHeart(g, hc, breath, crackGap, heartPulse);

    // Big Note Readout below heart
    int targetMidi = processor.uiTargetMidi.load(std::memory_order_relaxed);
    int targetPc = targetMidi >= 0 ? (targetMidi % 12) : -1;
    g.setColour(juce::Colour(0xfff1f5f9));
    g.setFont(juce::FontOptions(38.0f, juce::Font::bold));
    g.drawText(midiToNoteNameOnly(targetMidi), 241, 240, 100, 42, juce::Justification::centred);

    // Precision horizontal cents ruler (-100 .. +100)
    float rulerY = 290.0f;
    float rulerCx = 291.0f;
    g.setColour(juce::Colour(0xff334155));
    for (int t = -100; t <= 100; t += 10) {
        float tx = rulerCx + (static_cast<float>(t) / 100.0f) * 180.0f;
        float h = (t % 50 == 0) ? 10.0f : 5.0f;
        g.drawLine(tx, rulerY, tx, rulerY + h, 1.0f);
        if (t % 50 == 0) {
            g.setColour(juce::Colour(0xff64748b));
            g.setFont(juce::FontOptions(10.0f, juce::Font::plain));
            g.drawText((t > 0 ? "+" : "") + juce::String(t), static_cast<int>(tx - 16), static_cast<int>(rulerY + 12), 32, 12, juce::Justification::centred);
            g.setColour(juce::Colour(0xff334155));
        }
    }
    // Live needle on ruler
    float needleX = rulerCx + (smoothCents / 100.0f) * 180.0f;
    g.setColour(juce::Colour(0xff67e8f9));
    g.fillEllipse(needleX - 3.5f, rulerY - 4.0f, 7.0f, 7.0f);

    // Edit Notes section
    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::FontOptions(11.5f, juce::Font::plain));
    g.drawText("Edit Notes", 28, 344, 90, 16, juce::Justification::centredLeft);

    int root = keyBox.getSelectedId() > 0 ? keyBox.getSelectedId() - 1 : 0;
    auto scaleT = static_cast<frozen::ScaleType>(scaleBox.getSelectedId() > 0 ? scaleBox.getSelectedId() - 1 : 0);
    std::array<bool, 12> cMask{};
    for (int i = 0; i < 12; ++i) cMask[i] = processor.customNoteEnabled[i].load(std::memory_order_relaxed);

    auto drawNotePill = [&](int pc, const char* name, juce::Rectangle<float> r) {
        notePillRects[pc] = r;
        bool inScale = frozen::PitchEngine::isNoteInScale(pc, root, scaleT, cMask);
        bool playing = (pc == targetPc);

        juce::Colour bg = playing ? juce::Colour(0xff67e8f9)
                        : (inScale ? juce::Colour(0xffcbd5e1) : juce::Colour(0xff141922));
        juce::Colour border = playing ? juce::Colour(0xffa5f3fc)
                            : (inScale ? juce::Colour(0xffe2e8f0) : juce::Colour(0xff262e3b));
        g.setColour(bg);
        g.fillRoundedRectangle(r, 10.0f);
        g.setColour(border);
        g.drawRoundedRectangle(r, 10.0f, 1.2f);

        g.setColour((playing || inScale) ? juce::Colour(0xff0b0f16) : juce::Colour(0xff64748b));
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText(name, r, juce::Justification::centred);
    };

    const int topPc[5] = {1, 3, 6, 8, 10};
    const char* topLbl[5] = {"C#", "D#", "F#", "G#", "A#"};
    const float topX[5] = {74.0f, 148.0f, 296.0f, 370.0f, 444.0f};
    for (int i = 0; i < 5; ++i) drawNotePill(topPc[i], topLbl[i], {topX[i], 372.0f, 64.0f, 34.0f});

    const int botPc[7] = {0, 2, 4, 5, 7, 9, 11};
    const char* botLbl[7] = {"C", "D", "E", "F", "G", "A", "B"};
    for (int i = 0; i < 7; ++i) drawNotePill(botPc[i], botLbl[i], {36.0f + i * 74.0f, 416.0f, 66.0f, 42.0f});

    // Bottom left separator & Detune readout
    g.setColour(juce::Colour(0xff1e293b));
    g.drawLine(24.0f, 488.0f, 558.0f, 488.0f, 1.0f);

    float detHz = *processor.apvts.getRawParameterValue("pitchRef");
    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    g.drawText("Detune " + juce::String(detHz, 1) + " Hz", 282, 508, 120, 20, juce::Justification::centredLeft);

    // Right Titanium Deck Readouts
    float spd = *processor.apvts.getRawParameterValue("retuneSpeed");
    float sus = *processor.apvts.getRawParameterValue("sustain");
    float amt = *processor.apvts.getRawParameterValue("amount");

    g.setColour(juce::Colour(0xffcbd5e1));
    g.setFont(juce::FontOptions(13.0f, juce::Font::plain));
    g.drawText("Retune Speed", 596, 74, 278, 18, juce::Justification::centred);

    g.setColour(juce::Colour(0xff67e8f9));
    g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    g.drawText(juce::String(spd, 1), 695, 270, 80, 22, juce::Justification::centred);

    g.setColour(juce::Colour(0xffcbd5e1));
    g.setFont(juce::FontOptions(12.0f, juce::Font::plain));
    g.drawText("Sustain " + juce::String(static_cast<int>(sus)), 604, 392, 110, 18, juce::Justification::centred);
    g.drawText("Amount " + juce::String(static_cast<int>(amt)), 756, 392, 110, 18, juce::Justification::centred);

    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::FontOptions(11.5f, juce::Font::plain));
    g.drawText("Modern / Classic", 645, 456, 180, 18, juce::Justification::centred);
}

void FrozenTuneEditor::resized() {
    vocalRangeBox.setBounds(204, 34, 118, 26);
    keyBox.setBounds(330, 34, 66, 26);
    scaleBox.setBounds(402, 34, 152, 26);
    presetBox.setBounds(604, 26, 262, 28);

    speedSlider.setBounds(650, 96, 170, 170);
    sustainSlider.setBounds(612, 298, 94, 92);
    amountSlider.setBounds(764, 298, 94, 92);

    modernBtn.setBounds(636, 424, 96, 28);
    classicBtn.setBounds(738, 424, 96, 28);

    lowLatBtn.setBounds(24, 502, 140, 30);
    hqBtn.setBounds(170, 502, 46, 30);
    detuneSlider.setBounds(226, 496, 46, 42);
    tgLink.setBounds(410, 508, 144, 20);

    sibBtn.setBounds(600, 502, 66, 30);
    doublerBtn.setBounds(674, 502, 84, 30);
    bypassBtn.setBounds(766, 502, 104, 30);
}
