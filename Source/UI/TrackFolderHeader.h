#pragma once

#include "../Timeline/TrackFolder.h"
#include "LookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class TrackFolderHeader : public juce::Component
{
public:
    TrackFolderHeader();
    ~TrackFolderHeader() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    void setFolder(TrackFolder* folder, int folderIndex);
    TrackFolder* getFolder() const { return folderPtr; }
    int getFolderIndex() const { return folderIdx; }

    std::function<void(int)> onToggleCollapse;
    std::function<void(int)> onRemoveFolder;

    static constexpr int FOLDER_HEIGHT = 24;

private:
    void showContextMenu(juce::Point<int> position);

    TrackFolder* folderPtr = nullptr;
    int folderIdx = -1;
    juce::TextButton collapseButton { ">" };
};
