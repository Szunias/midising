#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "LookAndFeel.h"

/**
 * Splash screen with loading progress for MidiSing DAW.
 * Shows branding and initialization progress during startup.
 */
class SplashScreen : public juce::Component,
                     public juce::Timer
{
public:
    /**
     * Callback function type for when splash screen is ready to close.
     */
    using OnReadyCallback = std::function<void()>;

    SplashScreen()
    {
        setOpaque(true);
        setSize(splashWidth, splashHeight);
    }

    ~SplashScreen() override
    {
        stopTimer();
    }

    /**
     * Set the callback to be invoked when the splash screen is ready to close.
     * @param callback The callback function to invoke.
     */
    void setOnReadyCallback(OnReadyCallback callback)
    {
        onReadyCallback = std::move(callback);
    }

    /**
     * Update the loading progress.
     * @param newProgress Progress value between 0.0 and 1.0.
     * @param newStatusMessage Status message to display.
     */
    void setProgress(double newProgress, const juce::String& newStatusMessage = {})
    {
        progress = juce::jlimit(0.0, 1.0, newProgress);

        if (newStatusMessage.isNotEmpty())
            statusMessage = newStatusMessage;

        repaint();

        // When progress reaches 1.0, start the fade-out after a short delay
        if (progress >= 1.0 && !fadeOutStarted)
        {
            fadeOutStarted = true;
            fadeOutDelay = 500; // ms before starting fade
            startTimerHz(60);
        }
    }

    /**
     * Get the current loading progress.
     * @return Progress value between 0.0 and 1.0.
     */
    double getProgress() const { return progress; }

    /**
     * Check if the splash screen has finished its display cycle.
     * @return True if ready to close.
     */
    bool isReadyToClose() const { return readyToClose; }

    void paint(juce::Graphics& g) override
    {
        // Apply fade-out alpha
        auto currentAlpha = static_cast<float>(fadeAlpha);

        // Background with gradient
        auto bounds = getLocalBounds().toFloat();
        juce::ColourGradient gradient(
            MidiSingLookAndFeel::backgroundDark.withAlpha(currentAlpha),
            bounds.getCentreX(), 0.0f,
            MidiSingLookAndFeel::backgroundMid.withAlpha(currentAlpha),
            bounds.getCentreX(), bounds.getHeight(),
            false);
        g.setGradientFill(gradient);
        g.fillAll();

        // Draw subtle border
        g.setColour(MidiSingLookAndFeel::borderColour.withAlpha(currentAlpha));
        g.drawRoundedRectangle(bounds.reduced(1.0f), 8.0f, 2.0f);

        // Draw decorative accent line at top
        g.setColour(MidiSingLookAndFeel::accentColour.withAlpha(currentAlpha));
        g.fillRect(bounds.removeFromTop(4.0f).reduced(40.0f, 0.0f));

        // Logo area
        auto contentBounds = getLocalBounds().reduced(40, 30);

        // Application icon/logo placeholder (musical note symbol)
        auto iconBounds = contentBounds.removeFromTop(80).toFloat();
        drawMusicIcon(g, iconBounds.withSizeKeepingCentre(60.0f, 60.0f), currentAlpha);

        contentBounds.removeFromTop(20);

        // Application name
        g.setColour(MidiSingLookAndFeel::textColour.withAlpha(currentAlpha));
        g.setFont(juce::Font(42.0f, juce::Font::bold));
        auto titleBounds = contentBounds.removeFromTop(50);
        g.drawText("MidiSing", titleBounds, juce::Justification::centred, false);

        // Tagline
        g.setColour(MidiSingLookAndFeel::textDimColour.withAlpha(currentAlpha));
        g.setFont(juce::Font(16.0f));
        auto taglineBounds = contentBounds.removeFromTop(25);
        g.drawText("Digital Audio Workstation", taglineBounds, juce::Justification::centred, false);

        contentBounds.removeFromTop(30);

        // Progress bar background
        auto progressBarBounds = contentBounds.removeFromTop(8).toFloat().reduced(30.0f, 0.0f);
        g.setColour(MidiSingLookAndFeel::sliderBackground.withAlpha(currentAlpha));
        g.fillRoundedRectangle(progressBarBounds, 4.0f);

        // Progress bar fill
        if (progress > 0.0)
        {
            auto progressFill = progressBarBounds.withWidth(
                static_cast<float>(progress) * progressBarBounds.getWidth());
            g.setColour(MidiSingLookAndFeel::accentColour.withAlpha(currentAlpha));
            g.fillRoundedRectangle(progressFill, 4.0f);
        }

        contentBounds.removeFromTop(15);

        // Status message
        g.setColour(MidiSingLookAndFeel::textDimColour.withAlpha(currentAlpha * 0.8f));
        g.setFont(juce::Font(13.0f));
        auto statusBounds = contentBounds.removeFromTop(20);
        g.drawText(statusMessage, statusBounds, juce::Justification::centred, false);

        // Version at bottom
        auto versionBounds = getLocalBounds().reduced(20).removeFromBottom(20);
        g.setColour(MidiSingLookAndFeel::textDimColour.withAlpha(currentAlpha * 0.6f));
        g.setFont(juce::Font(11.0f));
        g.drawText("Version 1.0.0", versionBounds, juce::Justification::centred, false);
    }

    void timerCallback() override
    {
        if (fadeOutDelay > 0)
        {
            fadeOutDelay -= 16; // Approximately 60 fps
            return;
        }

        // Fade out
        fadeAlpha -= fadeSpeed;

        if (fadeAlpha <= 0.0)
        {
            fadeAlpha = 0.0;
            stopTimer();
            readyToClose = true;

            if (onReadyCallback)
                onReadyCallback();
        }

        repaint();
    }

private:
    /**
     * Draw a simple music note icon.
     */
    void drawMusicIcon(juce::Graphics& g, juce::Rectangle<float> bounds, float alpha)
    {
        auto cx = bounds.getCentreX();
        auto cy = bounds.getCentreY();
        auto size = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.4f;

        // Draw a musical note shape
        juce::Path notePath;

        // Note head (ellipse)
        notePath.addEllipse(cx - size * 0.8f, cy + size * 0.3f, size * 1.2f, size * 0.8f);

        // Stem
        notePath.addRoundedRectangle(cx + size * 0.2f, cy - size * 1.2f, size * 0.2f, size * 1.6f, 2.0f);

        // Flag
        juce::Path flagPath;
        flagPath.startNewSubPath(cx + size * 0.4f, cy - size * 1.2f);
        flagPath.quadraticTo(cx + size * 1.2f, cy - size * 0.6f,
                            cx + size * 0.6f, cy - size * 0.2f);
        flagPath.quadraticTo(cx + size * 1.0f, cy - size * 0.5f,
                            cx + size * 0.4f, cy - size * 0.8f);
        flagPath.closeSubPath();

        g.setColour(MidiSingLookAndFeel::accentColour.withAlpha(alpha));
        g.fillPath(notePath);
        g.fillPath(flagPath);

        // Add subtle glow effect
        g.setColour(MidiSingLookAndFeel::accentColour.withAlpha(alpha * 0.3f));
        g.strokePath(notePath, juce::PathStrokeType(3.0f));
    }

    double progress = 0.0;
    juce::String statusMessage { "Initializing..." };

    bool fadeOutStarted = false;
    bool readyToClose = false;
    double fadeAlpha = 1.0;
    double fadeSpeed = 0.03;
    int fadeOutDelay = 0;

    OnReadyCallback onReadyCallback;

    static constexpr int splashWidth = 450;
    static constexpr int splashHeight = 320;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SplashScreen)
};

/**
 * Window to host the splash screen component.
 * Provides a borderless, centered splash window.
 */
class SplashScreenWindow : public juce::DocumentWindow
{
public:
    SplashScreenWindow()
        : DocumentWindow("MidiSing",
                        MidiSingLookAndFeel::backgroundDark,
                        0) // No title bar buttons
    {
        splashScreen = std::make_unique<SplashScreen>();

        setUsingNativeTitleBar(false);
        setContentNonOwned(splashScreen.get(), true);
        setResizable(false, false);
        setDropShadowEnabled(true);

        // Center on screen
        centreWithSize(splashScreen->getWidth(), splashScreen->getHeight());

        setAlwaysOnTop(true);
        setVisible(true);
    }

    ~SplashScreenWindow() override
    {
        splashScreen = nullptr;
    }

    /**
     * Get the splash screen component.
     * @return Pointer to the splash screen component.
     */
    SplashScreen* getSplashScreen() { return splashScreen.get(); }

    void closeButtonPressed() override
    {
        // Don't allow closing during splash
    }

private:
    std::unique_ptr<SplashScreen> splashScreen;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SplashScreenWindow)
};
