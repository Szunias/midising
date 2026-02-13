#pragma once

#include "../Effects/EffectChain.h"
#include "../Effects/GainEffect.h"
#include "../Effects/SimpleEQEffect.h"
#include "../Effects/ReverbEffect.h"
#include "../Effects/ParametricEQ.h"
#include "../Effects/VCACompressor.h"
#include "../Effects/StereoDelay.h"
#include "../Effects/NoiseGate.h"
#include "../Effects/Limiter.h"
#include "../Effects/ChorusFlanger.h"
#include "../Effects/Saturation.h"
#include "../Effects/Phaser.h"
#include "../Effects/MultibandCompressor.h"
#include "../Effects/DeEsser.h"
#include "LookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>

class EffectSlot : public juce::Component
{
public:
    EffectSlot(Effect* effect, int index, std::function<void(int)> onRemove);
    ~EffectSlot() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    Effect* effectPtr;
    int index;
    juce::Label nameLabel;
    juce::ToggleButton bypassButton { "Bypass" };
    juce::TextButton removeButton { "X" };
};

class EffectRackUI : public juce::Component
{
public:
    EffectRackUI(EffectChain& chain);
    ~EffectRackUI() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void refresh();

private:
    EffectChain& effectChain;
    juce::Viewport viewport;
    juce::Component contentComp;
    
    juce::ComboBox addEffectCombo;
    juce::TextButton addButton { "+" };
    
    std::vector<std::unique_ptr<EffectSlot>> slots;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectRackUI)
};
