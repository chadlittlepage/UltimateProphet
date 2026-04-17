#pragma once
#include <JuceHeader.h>

class ProphetLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ProphetLookAndFeel()
    {
        // Dark Prophet-5 color scheme
        setColour(juce::Slider::rotarySliderFillColourId,   juce::Colour(0xffD4A843));
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2A2A2E));
        setColour(juce::Slider::thumbColourId,              juce::Colour(0xffE0E0E0));
        setColour(juce::Slider::textBoxTextColourId,        juce::Colour(0xffC0C0C0));
        setColour(juce::Slider::textBoxOutlineColourId,     juce::Colour(0x00000000));
        setColour(juce::Label::textColourId,                juce::Colour(0xffD4A843));
        setColour(juce::ComboBox::backgroundColourId,       juce::Colour(0xff2A2A2E));
        setColour(juce::ComboBox::textColourId,             juce::Colour(0xffD4A843));
        setColour(juce::ComboBox::outlineColourId,          juce::Colour(0xff4A4A50));
        setColour(juce::ComboBox::arrowColourId,            juce::Colour(0xffD4A843));
        setColour(juce::PopupMenu::backgroundColourId,      juce::Colour(0xff1A1A1E));
        setColour(juce::PopupMenu::textColourId,            juce::Colour(0xffD4A843));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff3A3020));
        setColour(juce::ToggleButton::textColourId,         juce::Colour(0xffD4A843));
        setColour(juce::ToggleButton::tickColourId,         juce::Colour(0xffD4A843));
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        auto radius = static_cast<float>(juce::jmin(width / 2, height / 2)) - 4.0f;
        auto centreX = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
        auto centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Outer ring (dark bezel)
        g.setColour(juce::Colour(0xff3A3A3E));
        g.fillEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);

        // Knob body (black)
        auto knobRadius = radius * 0.82f;
        g.setColour(juce::Colour(0xff1A1A1E));
        g.fillEllipse(centreX - knobRadius, centreY - knobRadius,
                      knobRadius * 2.0f, knobRadius * 2.0f);

        // Subtle rim highlight
        g.setColour(juce::Colour(0xff4A4A4E));
        g.drawEllipse(centreX - knobRadius, centreY - knobRadius,
                      knobRadius * 2.0f, knobRadius * 2.0f, 1.0f);

        // Position indicator arc (gold)
        juce::Path arcPath;
        arcPath.addCentredArc(centreX, centreY, radius - 1.0f, radius - 1.0f,
                              0.0f, rotaryStartAngle, angle, true);
        g.setColour(juce::Colour(0xffD4A843));
        g.strokePath(arcPath, juce::PathStrokeType(2.5f));

        // White indicator line
        juce::Path pointer;
        auto pointerLength = knobRadius * 0.7f;
        auto pointerThickness = 2.0f;
        pointer.addRectangle(-pointerThickness * 0.5f, -knobRadius + 4.0f,
                             pointerThickness, pointerLength);
        pointer.applyTransform(juce::AffineTransform::rotation(angle)
                                   .translated(centreX, centreY));
        g.setColour(juce::Colour(0xffE8E8E8));
        g.fillPath(pointer);
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

        auto bounds = button.getLocalBounds().toFloat();
        auto ledSize = 12.0f;
        auto ledX = bounds.getX() + 4.0f;
        auto ledY = bounds.getCentreY() - ledSize * 0.5f;

        // LED off state
        g.setColour(juce::Colour(0xff3A1A1A));
        g.fillEllipse(ledX, ledY, ledSize, ledSize);

        if (button.getToggleState())
        {
            // LED on: bright red with glow
            g.setColour(juce::Colour(0xffFF3030));
            g.fillEllipse(ledX + 1.0f, ledY + 1.0f, ledSize - 2.0f, ledSize - 2.0f);
            g.setColour(juce::Colour(0x40FF3030));
            g.fillEllipse(ledX - 2.0f, ledY - 2.0f, ledSize + 4.0f, ledSize + 4.0f);
        }

        // Label text
        g.setColour(juce::Colour(0xffD4A843));
        g.setFont(juce::Font(11.0f));
        g.drawText(button.getButtonText(), ledX + ledSize + 4.0f, bounds.getY(),
                   bounds.getWidth() - ledSize - 8.0f, bounds.getHeight(),
                   juce::Justification::centredLeft);
    }

    void drawLabel(juce::Graphics& g, juce::Label& label) override
    {
        g.setColour(label.findColour(juce::Label::textColourId));
        g.setFont(label.getFont());
        g.drawText(label.getText(), label.getLocalBounds(),
                   label.getJustificationType(), false);  // false = NO ellipsis ever
    }
};
