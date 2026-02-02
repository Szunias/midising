#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
 * Custom LookAndFeel for MidiSing DAW.
 * Dark theme with modern styling.
 */
class MidiSingLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MidiSingLookAndFeel()
    {
        // Set colour scheme
        setColour(juce::ResizableWindow::backgroundColourId, backgroundDark);
        setColour(juce::TextButton::buttonColourId, buttonBackground);
        setColour(juce::TextButton::buttonOnColourId, accentColour);
        setColour(juce::TextButton::textColourOffId, textColour);
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        setColour(juce::Slider::backgroundColourId, sliderBackground);
        setColour(juce::Slider::thumbColourId, accentColour);
        setColour(juce::Slider::trackColourId, accentColour.darker(0.3f));
        setColour(juce::Label::textColourId, textColour);
        setColour(juce::ComboBox::backgroundColourId, buttonBackground);
        setColour(juce::ComboBox::textColourId, textColour);
        setColour(juce::ComboBox::outlineColourId, borderColour);
    }

    // Colour palette
    static inline juce::Colour backgroundDark   { 0xff1a1a1a };
    static inline juce::Colour backgroundMid    { 0xff252525 };
    static inline juce::Colour backgroundLight  { 0xff303030 };
    static inline juce::Colour borderColour     { 0xff404040 };
    static inline juce::Colour textColour       { 0xffe0e0e0 };
    static inline juce::Colour textDimColour    { 0xff808080 };
    static inline juce::Colour accentColour     { 0xff5a9fd4 };  // Blue
    static inline juce::Colour recordColour     { 0xffd4605a };  // Red
    static inline juce::Colour playColour       { 0xff6bba75 };  // Green
    static inline juce::Colour buttonBackground { 0xff3a3a3a };
    static inline juce::Colour sliderBackground { 0xff2a2a2a };
    static inline juce::Colour regionColour     { 0xff4a8db2 };  // Soft blue for regions

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool isMouseOverButton, bool isButtonDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
        auto baseColour = backgroundColour;

        if (isButtonDown)
            baseColour = baseColour.brighter(0.2f);
        else if (isMouseOverButton)
            baseColour = baseColour.brighter(0.1f);

        g.setColour(baseColour);
        g.fillRoundedRectangle(bounds, 4.0f);

        g.setColour(borderColour);
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        juce::ignoreUnused(minSliderPos, maxSliderPos);
        
        auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                             static_cast<float>(width), static_cast<float>(height));

        // Draw track background
        g.setColour(sliderBackground);
        
        if (style == juce::Slider::LinearHorizontal)
        {
            auto trackBounds = bounds.reduced(0, bounds.getHeight() * 0.35f);
            g.fillRoundedRectangle(trackBounds, 2.0f);

            // Draw filled portion
            g.setColour(accentColour);
            auto filledBounds = trackBounds.removeFromLeft(sliderPos - static_cast<float>(x));
            g.fillRoundedRectangle(filledBounds, 2.0f);

            // Draw thumb
            float thumbX = sliderPos - 6.0f;
            auto thumbBounds = juce::Rectangle<float>(thumbX, bounds.getY(), 12.0f, bounds.getHeight());
            g.setColour(slider.findColour(juce::Slider::thumbColourId));
            g.fillRoundedRectangle(thumbBounds.reduced(0, 2), 3.0f);
        }
        else if (style == juce::Slider::LinearVertical)
        {
            auto trackBounds = bounds.reduced(bounds.getWidth() * 0.35f, 0);
            g.fillRoundedRectangle(trackBounds, 2.0f);

            // Draw filled portion (from bottom)
            g.setColour(accentColour);
            float filledHeight = bounds.getBottom() - sliderPos;
            auto filledBounds = trackBounds.removeFromBottom(filledHeight);
            g.fillRoundedRectangle(filledBounds, 2.0f);

            // Draw thumb
            float thumbY = sliderPos - 6.0f;
            auto thumbBounds = juce::Rectangle<float>(bounds.getX(), thumbY, bounds.getWidth(), 12.0f);
            g.setColour(slider.findColour(juce::Slider::thumbColourId));
            g.fillRoundedRectangle(thumbBounds.reduced(2, 0), 3.0f);
        }
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override
    {
        juce::ignoreUnused(slider);
        
        auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                             static_cast<float>(width), static_cast<float>(height));
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f - 4.0f;
        auto centreX = bounds.getCentreX();
        auto centreY = bounds.getCentreY();
        auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // Draw background arc
        juce::Path backgroundArc;
        backgroundArc.addCentredArc(centreX, centreY, radius, radius, 0.0f,
                                     rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(sliderBackground);
        g.strokePath(backgroundArc, juce::PathStrokeType(4.0f));

        // Draw value arc
        juce::Path valueArc;
        valueArc.addCentredArc(centreX, centreY, radius, radius, 0.0f,
                               rotaryStartAngle, angle, true);
        g.setColour(accentColour);
        g.strokePath(valueArc, juce::PathStrokeType(4.0f));

        // Draw pointer
        juce::Path pointer;
        auto pointerLength = radius * 0.6f;
        pointer.addRectangle(-2.0f, -pointerLength, 4.0f, pointerLength);
        g.setColour(textColour);
        g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centreX, centreY));
    }
};
