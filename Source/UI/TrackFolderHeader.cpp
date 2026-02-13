#include "TrackFolderHeader.h"

TrackFolderHeader::TrackFolderHeader()
{
    addAndMakeVisible(collapseButton);
    collapseButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    collapseButton.setColour(juce::TextButton::textColourOnId, MidiSingLookAndFeel::textColour);
    collapseButton.setColour(juce::TextButton::textColourOffId, MidiSingLookAndFeel::textColour);
    collapseButton.onClick = [this]()
    {
        if (folderPtr != nullptr && onToggleCollapse)
            onToggleCollapse(folderIdx);
    };
}

void TrackFolderHeader::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Folder background
    g.setColour(juce::Colour(0xff3a3a4a));
    g.fillRect(bounds);

    if (folderPtr == nullptr)
        return;

    // Colour strip on left
    g.setColour(folderPtr->colour);
    g.fillRect(bounds.getX(), bounds.getY(), 4, bounds.getHeight());

    // Folder name
    g.setColour(MidiSingLookAndFeel::textColour);
    g.setFont(12.0f);
    g.drawText(folderPtr->name, 30, 0, bounds.getWidth() - 80, bounds.getHeight(),
               juce::Justification::centredLeft, true);

    // Track count badge
    juce::String badge = juce::String(folderPtr->getNumTracks()) + " tracks";
    g.setColour(MidiSingLookAndFeel::textDimColour);
    g.setFont(10.0f);
    g.drawText(badge, bounds.getWidth() - 70, 0, 60, bounds.getHeight(),
               juce::Justification::centredRight, true);

    // Bottom border
    g.setColour(MidiSingLookAndFeel::borderColour);
    g.drawHorizontalLine(bounds.getBottom() - 1, static_cast<float>(bounds.getX()), static_cast<float>(bounds.getRight()));
}

void TrackFolderHeader::resized()
{
    collapseButton.setBounds(6, 2, 20, getHeight() - 4);

    if (folderPtr != nullptr)
    {
        collapseButton.setButtonText(folderPtr->collapsed ? ">" : "v");
    }
}

void TrackFolderHeader::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
    {
        showContextMenu(e.getScreenPosition());
    }
}

void TrackFolderHeader::mouseDoubleClick(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    if (folderPtr != nullptr && onToggleCollapse)
        onToggleCollapse(folderIdx);
}

void TrackFolderHeader::setFolder(TrackFolder* folder, int folderIndex)
{
    folderPtr = folder;
    folderIdx = folderIndex;

    if (folderPtr != nullptr)
    {
        collapseButton.setButtonText(folderPtr->collapsed ? ">" : "v");
    }
    repaint();
}

void TrackFolderHeader::showContextMenu(juce::Point<int> position)
{
    juce::PopupMenu menu;
    menu.addItem(1, "Remove Folder");

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({ position.x, position.y, 1, 1 }),
        [this](int result)
        {
            if (result == 1 && onRemoveFolder)
                onRemoveFolder(folderIdx);
        });
}
