#pragma once

#include "../Effects/Effect.h"
#include "../Effects/EffectParameter.h"
#include "LookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <memory>
#include <functional>

/**
 * Auto-generated parameter editor for any Effect.
 * Creates sliders for all parameters returned by Effect::getParameters().
 * Supports a sidechain source ComboBox for effects that support sidechain.
 */
class EffectParameterEditor : public juce::Component,
                              public juce::Timer
{
public:
    EffectParameterEditor(Effect* effect);
    ~EffectParameterEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    Effect* getEffect() const { return effectPtr; }

    // Sidechain routing callback: (sourceTrackIndex) where -1 = none
    std::function<void(int)> onSidechainSourceChanged;

    // Set available track names for sidechain source selection
    void setSidechainTrackNames(const juce::StringArray& names);

    // Set current sidechain source index (-1 = none, 0+ = track index)
    void setSidechainSource(int trackIndex);

    static constexpr int ROW_HEIGHT = 28;
    static constexpr int LABEL_WIDTH = 100;
    static constexpr int VALUE_WIDTH = 60;
    static constexpr int HEADER_HEIGHT = 32;
    static constexpr int SIDECHAIN_HEIGHT = 32;

private:
    struct ParamRow
    {
        juce::String id;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> valueLabel;
    };

    Effect* effectPtr;
    juce::ToggleButton bypassToggle { "Bypass" };
    juce::Label titleLabel;

    // Sidechain controls
    juce::Label sidechainLabel;
    juce::ComboBox sidechainCombo;
    bool showSidechain = false;

    juce::Viewport viewport;
    juce::Component contentComp;
    std::vector<std::unique_ptr<ParamRow>> rows;

    void buildUI();
    void updateValues();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectParameterEditor)
};
