#include "MixerPanel.h"
#include "../Audio/MixGroup.h"
#include "../Audio/AudioTrack.h"
#include "../Plugins/PluginManager.h"
#include "../Plugins/PluginInstance.h"
#include "../Effects/GainEffect.h"
#include "../Effects/SimpleEQEffect.h"
#include "../Effects/ReverbEffect.h"
#include "../Effects/ParametricEQ.h"
#include "../Effects/VCACompressor.h"
#include "../Effects/StereoDelay.h"
#include <map>

MixerPanel::MixerPanel()
{
    // Master channel strip
    masterStrip = std::make_unique<ChannelStrip>();
    addAndMakeVisible(masterStrip.get());

    // Stereo correlation meter for master channel
    addAndMakeVisible(correlationMeter);

    // Correlation label
    correlationLabel.setText("CORR", juce::dontSendNotification);
    correlationLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textDimColour);
    correlationLabel.setJustificationType(juce::Justification::centred);
    correlationLabel.setFont(juce::Font(9.0f));
    addAndMakeVisible(correlationLabel);

    startTimer(50); // 20 FPS for meter updates
}

MixerPanel::~MixerPanel()
{
    stopTimer();
}

void MixerPanel::paint(juce::Graphics& g)
{
    g.fillAll(MidiSingLookAndFeel::backgroundDark);

    // Label for master section
    g.setColour(MidiSingLookAndFeel::textDimColour);
    g.setFont(10.0f);
    g.drawText("MASTER", getWidth() - MASTER_WIDTH, 4, MASTER_WIDTH, 16,
               juce::Justification::centred);
}

void MixerPanel::resized()
{
    updateChannelStrips();

    auto bounds = getLocalBounds();

    // Master section on right
    auto masterBounds = bounds.removeFromRight(MASTER_WIDTH);
    masterBounds.removeFromTop(20); // Space for "MASTER" label

    // Stereo correlation meter at bottom of master section
    auto correlationBounds = masterBounds.removeFromBottom(40);
    correlationLabel.setBounds(correlationBounds.removeFromTop(12));
    correlationMeter.setBounds(correlationBounds.reduced(4, 2));

    // Master strip fills the rest
    masterStrip->setBounds(masterBounds);

    // Channel strips fill the rest
    bounds.removeFromTop(20);
    int x = 0;
    for (auto& strip : channelStrips)
    {
        strip->setBounds(x, bounds.getY(), CHANNEL_WIDTH, bounds.getHeight());
        x += CHANNEL_WIDTH;
    }
}

void MixerPanel::timerCallback()
{
    if (mixerPtr != nullptr)
    {
        // Update master meters
        masterStrip->setLevel(mixerPtr->getPeakLevel(0), mixerPtr->getPeakLevel(1));

        // Update stereo correlation meter
        correlationMeter.setCorrelation(mixerPtr->getStereoCorrelation());
    }

    // Note: Individual track meters would need per-track peak tracking
    // For now, tracks just show 0 levels
}

void MixerPanel::updateChannelStrips()
{
    if (timelinePtr == nullptr)
        return;

    // Ensure we have the right number of channel strips
    while (static_cast<int>(channelStrips.size()) < timelinePtr->getNumTracks())
    {
        auto strip = std::make_unique<ChannelStrip>();
        addAndMakeVisible(strip.get());
        channelStrips.push_back(std::move(strip));
    }

    while (static_cast<int>(channelStrips.size()) > timelinePtr->getNumTracks())
    {
        channelStrips.pop_back();
    }

    // Configure strips with mix group linked fader support
    for (size_t i = 0; i < channelStrips.size(); ++i)
    {
        int trackIndex = static_cast<int>(i);
        Track* track = timelinePtr->getTrack(trackIndex);
        channelStrips[i]->setTrack(track);

        // Set up volume callback with mix group linking
        channelStrips[i]->onVolumeChanged = [this, trackIndex](Track* t, float newVolume)
        {
            if (timelinePtr == nullptr || t == nullptr)
                return;

            float oldVolume = t->getVolume();
            float volumeDelta = newVolume - oldVolume;

            // First set this track's volume
            t->setVolume(newVolume);

            // Then apply to linked tracks (excludes source track internally)
            MixGroup* mixGroup = timelinePtr->findMixGroupForTrack(trackIndex);
            if (mixGroup != nullptr && mixGroup->isEnabled())
            {
                // Apply delta to other tracks in the group
                const auto& indices = mixGroup->getTrackIndices();
                for (int idx : indices)
                {
                    if (idx != trackIndex && idx >= 0 && idx < timelinePtr->getNumTracks())
                    {
                        Track* linkedTrack = timelinePtr->getTrack(idx);
                        if (linkedTrack != nullptr)
                        {
                            float linkedVolume = linkedTrack->getVolume();
                            linkedTrack->setVolume(juce::jlimit(0.0f, 2.0f, linkedVolume + volumeDelta));
                        }
                    }
                }
            }
        };

        // Set up pan callback with mix group linking
        channelStrips[i]->onPanChanged = [this, trackIndex](Track* t, float newPan)
        {
            if (timelinePtr == nullptr || t == nullptr)
                return;

            float oldPan = t->getPan();
            float panDelta = newPan - oldPan;

            // First set this track's pan
            t->setPan(newPan);

            // Then apply to linked tracks if pan linking is enabled
            MixGroup* mixGroup = timelinePtr->findMixGroupForTrack(trackIndex);
            if (mixGroup != nullptr && mixGroup->isEnabled() && mixGroup->isPanLinked())
            {
                const auto& indices = mixGroup->getTrackIndices();
                for (int idx : indices)
                {
                    if (idx != trackIndex && idx >= 0 && idx < timelinePtr->getNumTracks())
                    {
                        Track* linkedTrack = timelinePtr->getTrack(idx);
                        if (linkedTrack != nullptr)
                        {
                            float linkedPan = linkedTrack->getPan();
                            linkedTrack->setPan(juce::jlimit(-1.0f, 1.0f, linkedPan + panDelta));
                        }
                    }
                }
            }
        };

        // Set up mute callback with mix group linking
        channelStrips[i]->onMuteChanged = [this, trackIndex](Track* t)
        {
            if (timelinePtr == nullptr || t == nullptr)
                return;

            bool shouldMute = t->isMuted();

            // Apply to linked tracks if mute linking is enabled
            MixGroup* mixGroup = timelinePtr->findMixGroupForTrack(trackIndex);
            if (mixGroup != nullptr && mixGroup->isEnabled() && mixGroup->isMuteLinked())
            {
                const auto& indices = mixGroup->getTrackIndices();
                for (int idx : indices)
                {
                    if (idx != trackIndex && idx >= 0 && idx < timelinePtr->getNumTracks())
                    {
                        Track* linkedTrack = timelinePtr->getTrack(idx);
                        if (linkedTrack != nullptr)
                        {
                            linkedTrack->setMuted(shouldMute);
                        }
                    }
                }
            }
        };

        // Set up solo callback with mix group linking
        channelStrips[i]->onSoloChanged = [this, trackIndex](Track* t)
        {
            if (timelinePtr == nullptr || t == nullptr)
                return;

            bool shouldSolo = t->isSoloed();

            // Apply to linked tracks if solo linking is enabled
            MixGroup* mixGroup = timelinePtr->findMixGroupForTrack(trackIndex);
            if (mixGroup != nullptr && mixGroup->isEnabled() && mixGroup->isSoloLinked())
            {
                const auto& indices = mixGroup->getTrackIndices();
                for (int idx : indices)
                {
                    if (idx != trackIndex && idx >= 0 && idx < timelinePtr->getNumTracks())
                    {
                        Track* linkedTrack = timelinePtr->getTrack(idx);
                        if (linkedTrack != nullptr)
                        {
                            linkedTrack->setSoloed(shouldSolo);
                        }
                    }
                }
            }
        };

        // Set up insert slot click callback to show plugin/effect selector
        channelStrips[i]->onInsertSlotClicked = [this](Track* t, int slotIndex)
        {
            if (t == nullptr)
                return;

            auto* audioTrack = dynamic_cast<AudioTrack*>(t);
            if (audioTrack == nullptr)
                return;

            // Build popup menu with built-in effects and VST plugins
            juce::PopupMenu menu;

            // Built-in effects submenu
            juce::PopupMenu builtInMenu;
            builtInMenu.addItem(101, "Gain");
            builtInMenu.addItem(102, "Simple EQ");
            builtInMenu.addItem(103, "Parametric EQ");
            builtInMenu.addItem(104, "VCA Compressor");
            builtInMenu.addItem(105, "Reverb");
            builtInMenu.addItem(106, "Stereo Delay");
            menu.addSubMenu("Built-in Effects", builtInMenu);

            // VST3 Plugins submenu
            auto& pluginManager = PluginManager::getInstance();
            auto effectPlugins = pluginManager.getEffectPlugins();

            if (!effectPlugins.isEmpty())
            {
                juce::PopupMenu vstMenu;

                // Group by manufacturer
                std::map<juce::String, juce::Array<juce::PluginDescription>> byManufacturer;
                for (const auto& desc : effectPlugins)
                {
                    juce::String mfr = desc.manufacturerName.isEmpty() ? "Unknown" : desc.manufacturerName;
                    byManufacturer[mfr].add(desc);
                }

                int vstId = 1000;
                for (auto& [manufacturer, plugins] : byManufacturer)
                {
                    juce::PopupMenu mfrMenu;
                    for (const auto& plugin : plugins)
                    {
                        mfrMenu.addItem(vstId++, plugin.name);
                    }
                    vstMenu.addSubMenu(manufacturer, mfrMenu);
                }

                menu.addSubMenu("VST3 Plugins", vstMenu);
            }

            menu.addSeparator();
            menu.addItem(1, "Scan for VST3 plugins...");

            // Show menu
            menu.showMenuAsync(juce::PopupMenu::Options(),
                [this, audioTrack, slotIndex, effectPlugins](int result)
                {
                    if (result == 0)
                        return;

                    if (result == 1)
                    {
                        // Scan for plugins
                        PluginManager::getInstance().startScan(
                            [](float progress, const juce::String& pluginName) {
                                DBG("Scanning: " + juce::String(progress * 100, 1) + "% - " + pluginName);
                            },
                            []() {
                                DBG("Plugin scan complete");
                            });
                        return;
                    }

                    // Built-in effects (101-199)
                    if (result >= 101 && result < 200)
                    {
                        std::unique_ptr<Effect> effect;
                        switch (result)
                        {
                            case 101: effect = std::make_unique<GainEffect>(); break;
                            case 102: effect = std::make_unique<SimpleEQEffect>(); break;
                            case 103: effect = std::make_unique<ParametricEQ>(); break;
                            case 104: effect = std::make_unique<VCACompressor>(); break;
                            case 105: effect = std::make_unique<ReverbEffect>(); break;
                            case 106: effect = std::make_unique<StereoDelay>(); break;
                        }
                        if (effect)
                        {
                            audioTrack->addInsertAtSlot(slotIndex, std::move(effect));
                        }
                        return;
                    }

                    // VST3 plugins (1000+)
                    if (result >= 1000)
                    {
                        int pluginIndex = result - 1000;
                        if (pluginIndex >= 0 && pluginIndex < effectPlugins.size())
                        {
                            const auto& desc = effectPlugins[pluginIndex];
                            
                            // Load plugin asynchronously
                            PluginManager::getInstance().loadPluginAsync(desc,
                                [audioTrack, slotIndex, desc](std::unique_ptr<juce::AudioPluginInstance> instance,
                                                              const juce::String& errorMessage)
                                {
                                    if (instance == nullptr)
                                    {
                                        juce::AlertWindow::showMessageBoxAsync(
                                            juce::MessageBoxIconType::WarningIcon,
                                            "Plugin Load Error",
                                            "Failed to load '" + desc.name + "':\n" + errorMessage);
                                        return;
                                    }

                                    // For now, just show that it loaded successfully
                                    // Full integration would require PluginEffect wrapper class
                                    DBG("Plugin loaded successfully: " + desc.name);
                                    juce::AlertWindow::showMessageBoxAsync(
                                        juce::MessageBoxIconType::InfoIcon,
                                        "Plugin Loaded",
                                        "Plugin '" + desc.name + "' loaded successfully.\n\n"
                                        "Note: Full VST insert integration is still in progress.");
                                });
                        }
                    }
                });
        };
    }
}
