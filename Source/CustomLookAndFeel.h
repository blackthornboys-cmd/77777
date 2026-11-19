#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class FrozenLookAndFeel : public juce::LookAndFeel_V4 {
public:
    FrozenLookAndFeel() {
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff10141b));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff262d38));
        setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
        setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff94a3b8));
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff10141b));
        setColour(juce::PopupMenu::textColourId, juce::Colour(0xffe2e8f0));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff0284c7).withAlpha(0.38f));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(0xffffffff));
    }

    // Boutique brushed aluminum & dark titanium rotary dial
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override {
        auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                             static_cast<float>(width), static_cast<float>(height)).reduced(6.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        auto center = bounds.getCentre();
        auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);
        if (accent.isTransparent()) accent = juce::Colour(0xff67e8f9);

        // Recessed dark bezel ring
        juce::Path trackArc;
        trackArc.addCentredArc(center.x, center.y, radius - 2.0f, radius - 2.0f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff141820));
        g.strokePath(trackArc, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Razor-thin glacial cyan LED arc
        if (sliderPosProportional > 0.004f) {
            juce::Path valArc;
            valArc.addCentredArc(center.x, center.y, radius - 2.0f, radius - 2.0f, 0.0f, rotaryStartAngle, angle, true);
            g.setColour(accent.withAlpha(0.22f));
            g.strokePath(valArc, juce::PathStrokeType(8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(accent);
            g.strokePath(valArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Machined metallic dial cap
        auto innerR = radius * 0.78f;
        juce::ColourGradient capGrad(juce::Colour(0xff383f4b), center.x - innerR * 0.5f, center.y - innerR,
                                     juce::Colour(0xff171a20), center.x + innerR * 0.5f, center.y + innerR, false);
        g.setGradientFill(capGrad);
        g.fillEllipse(center.x - innerR, center.y - innerR, innerR * 2.0f, innerR * 2.0f);

        g.setColour(juce::Colour(0xff4b5563).withAlpha(0.7f));
        g.drawEllipse(center.x - innerR, center.y - innerR, innerR * 2.0f, innerR * 2.0f, 1.0f);

        // Machined indicator notch
        float r1 = innerR - 4.0f;
        float r2 = r1 - innerR * 0.34f;
        float sx = std::sin(angle), sy = -std::cos(angle);
        g.setColour(accent);
        g.drawLine(center.x + sx * r2, center.y + sy * r2,
                   center.x + sx * r1, center.y + sy * r1, 2.0f);
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int, int, int, int, juce::ComboBox& box) override {
        auto r = juce::Rectangle<float>(0.5f, 0.5f, static_cast<float>(width - 1), static_cast<float>(height - 1));
        g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle(r, 5.0f);
        g.setColour(box.findColour(juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle(r, 5.0f, 1.0f);

        juce::Path arrow;
        float ax = static_cast<float>(width - 13);
        float ay = static_cast<float>(height) * 0.5f - 2.0f;
        arrow.startNewSubPath(ax - 3.5f, ay);
        arrow.lineTo(ax, ay + 4.0f);
        arrow.lineTo(ax + 3.5f, ay);
        g.setColour(box.findColour(juce::ComboBox::arrowColourId));
        g.strokePath(arrow, juce::PathStrokeType(1.3f));
    }
};
