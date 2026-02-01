#include "EffectRackUI.h"

//==============================================================================
EffectSlot::EffectSlot(Effect* effect, int idx, std::function<void(int)> onRemove)
    : effectPtr(effect), index(idx)
{
    addAndMakeVisible(nameLabel);
    nameLabel.setText(effect->getName(), juce::dontSendNotification);
    nameLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour);
    
    addAndMakeVisible(bypassButton);
    bypassButton.setToggleState(effect->isBypassed(), juce::dontSendNotification);
    bypassButton.onClick = [this, effect]() { effect->setBypass(bypassButton.getToggleState()); };
    
    addAndMakeVisible(removeButton);
    removeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red.withAlpha(0.6f));
    removeButton.onClick = [onRemove, idx]() { onRemove(idx); };
}

void EffectSlot::paint(juce::Graphics& g)
{
    g.fillAll(MidiSingLookAndFeel::backgroundDark.brighter(0.05f));
    g.setColour(MidiSingLookAndFeel::borderColour);
    g.drawRect(getLocalBounds());
}

void EffectSlot::resized()
{
    auto area = getLocalBounds().reduced(2);
    
    removeButton.setBounds(area.removeFromRight(20).reduced(2));
    area.removeFromRight(4);
    bypassButton.setBounds(area.removeFromRight(60));
    area.removeFromRight(4);
    nameLabel.setBounds(area);
}

//==============================================================================
EffectRackUI::EffectRackUI(EffectChain& chain) : effectChain(chain)
{
    addAndMakeVisible(viewport);
    viewport.setViewedComponent(&contentComp, false);
    
    addAndMakeVisible(addEffectCombo);
    addEffectCombo.addItem("Gain", 1);
    addEffectCombo.addItem("Simple EQ", 2);
    addEffectCombo.addItem("Reverb", 3);
    addEffectCombo.setTextWhenNothingSelected("Add Effect...");
    
    addAndMakeVisible(addButton);
    addButton.onClick = [this]()
    {
        int id = addEffectCombo.getSelectedId();
        std::unique_ptr<Effect> newEffect;
        
        switch (id)
        {
            case 1: newEffect = std::make_unique<GainEffect>(); break;
            case 2: newEffect = std::make_unique<SimpleEQEffect>(); break;
            case 3: newEffect = std::make_unique<ReverbEffect>(); break;
        }
        
        if (newEffect)
        {
            effectChain.addEffect(std::move(newEffect));
            refresh();
        }
    };
    
    refresh();
}

void EffectRackUI::paint(juce::Graphics& g)
{
    g.fillAll(MidiSingLookAndFeel::backgroundDark);
}

void EffectRackUI::resized()
{
    auto area = getLocalBounds();
    
    // Top bar for adding effects
    auto topBar = area.removeFromTop(30).reduced(2);
    addButton.setBounds(topBar.removeFromRight(30));
    topBar.removeFromRight(4);
    addEffectCombo.setBounds(topBar);
    
    viewport.setBounds(area);
    contentComp.setBounds(area); // Will assume width tracks viewport, height set in refresh
}

void EffectRackUI::refresh()
{
    slots.clear();
    contentComp.removeAllChildren();
    
    int y = 0;
    int h = 40;
    
    for (int i = 0; i < effectChain.getNumEffects(); ++i)
    {
        auto* fx = effectChain.getEffect(i);
        auto slot = std::make_unique<EffectSlot>(fx, i, [this](int idx)
        {
            effectChain.removeEffect(idx);
            refresh();
        });
        
        slot->setBounds(0, y, getWidth() - (viewport.getScrollBarThickness() + 4), h);
        contentComp.addAndMakeVisible(slot.get());
        slots.push_back(std::move(slot));
        
        y += h + 2;
    }
    
    contentComp.setSize(viewport.getWidth(), juce::jmax(viewport.getHeight(), y));
    resized(); // Ensure slots are proper width
}
